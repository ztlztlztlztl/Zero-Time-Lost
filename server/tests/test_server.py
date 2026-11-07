import json
import sqlite3
import threading
from concurrent.futures import ThreadPoolExecutor

import pytest
from fastapi.testclient import TestClient

from server.app import create_app
from server.jobs import Jobs, worker
from server.store import Store, encode
from server.scripts.cloud_backup import backup

TOKEN = "test-token-" * 5


@pytest.fixture
def api(tmp_path):
    app = create_app(tmp_path / "cloud.db", TOKEN, background=False)
    with TestClient(app, headers={"Authorization": "Bearer " + TOKEN}) as client:
        yield client


def mutation(identity="one", base=0, op="op1", **kwargs):
    return dict(kind="calendar", id=identity, base=base, op=op, deleted=False,
                payload=dict(id=identity, title="事务", instances={"2030-11-05": {"done": True}},
                             hasDue=True, due="2030-11-05T10:00:00+08:00", repeat="none", type="ddl")) | kwargs


def test_authentication(api):
    assert api.get("/health").json()["protocol"] == 1
    assert api.get("/health", headers={"Authorization": "Bearer wrong"}).status_code == 401


def test_idempotency_and_conflict(api):
    first = api.post("/v1/mutate", json=mutation()).json()
    assert first["ok"]
    assert api.post("/v1/mutate", json=mutation()).json() == first
    assert len(api.get("/v1/changes").json()["records"]) == 1
    conflict = api.post("/v1/mutate", json=mutation(op="stale")).json()
    assert not conflict["ok"] and conflict["conflict"] == first["record"]
    changed = mutation(payload={"id": "one", "title": "different"})
    assert api.post("/v1/mutate", json=changed).status_code == 409


def test_paging_keeps_immutable_history_and_deletions(api):
    first = api.post("/v1/mutate", json=mutation()).json()["record"]
    deleted = mutation(base=first["rev"], op="delete", deleted=True, payload={})
    assert api.post("/v1/mutate", json=deleted).json()["ok"]
    page1 = api.get("/v1/changes?limit=1").json()
    assert page1["more"] and page1["records"][0] == first
    page2 = api.get(f"/v1/changes?cursor={page1['cursor']}&limit=1").json()
    assert page2["records"][0]["deleted"] and not page2["more"]
    assert api.get("/v1/changes?cursor=99999").status_code == 409


def test_validation_and_roundtrip(api):
    payload = mutation()["payload"]
    actual = api.post("/v1/mutate", json=mutation()).json()["record"]["payload"]
    assert actual == payload
    assert api.post("/v1/mutate", json=mutation(kind="note", payload={"id": "one", "kind": 0})).status_code == 422
    assert api.post("/v1/mutate", json=mutation(kind="note", payload={"id": "one", "kind": 1, "targetType": "file"})).status_code == 422


def test_concurrent_writers_only_one_wins(tmp_path):
    store = Store(tmp_path / "db")
    barrier = threading.Barrier(2)
    def write(i):
        barrier.wait()
        return store.mutate(mutation(op=f"writer{i}"))
    with ThreadPoolExecutor(2) as pool:
        results = list(pool.map(write, range(2)))
    assert sum(r["ok"] for r in results) == 1
    assert len(store.changes(0, 100)["records"]) == 1


def test_transaction_failure_does_not_leave_revision(tmp_path):
    store = Store(tmp_path / "db")
    with store.connect() as db:
        db.execute("CREATE TRIGGER fail_receipt BEFORE INSERT ON receipts BEGIN SELECT RAISE(ABORT,'fault'); END")
    with pytest.raises(sqlite3.IntegrityError):
        store.mutate(mutation())
    assert not store.get("calendar", "one")
    assert store.changes(0, 100)["records"] == []


def setup_jobs(tmp_path):
    store = Store(tmp_path / "db")
    account = dict(address="person@example.test", code="test", server="imap.example.test", port="993")
    store.mutate(mutation(kind="settings", identity="shared", payload={"accounts": [account], "mail": {"pollingInterval": 1}}))
    return store, Jobs(store), account


def test_mail_commits_before_seen_and_retries(monkeypatch, tmp_path):
    store, jobs, account = setup_jobs(tmp_path)
    seen = []
    def execute(request):
        if request["command"] == "sync":
            return dict(messages=[dict(id="42:9", subject="subject", **{"from": "sender"}, date=1)], validity="42", cursor=9)
        with store.connect() as db:
            assert db.execute("SELECT count(*) FROM mail").fetchone()[0] == 1
        seen.append(request["ids"])
        if len(seen) == 1:
            raise TimeoutError()
        return {"acknowledged": request["ids"]}
    monkeypatch.setattr(worker, "execute", execute)
    with pytest.raises(TimeoutError):
        jobs.poll(account)
    with store.connect() as db:
        assert db.execute("SELECT seen FROM mail").fetchone()[0] == 0
    jobs.poll(account)
    with store.connect() as db:
        assert db.execute("SELECT seen FROM mail").fetchone()[0] == 1
    assert len(seen) == 2


def test_weather_preserves_old_cache_on_failure(tmp_path, monkeypatch):
    store, jobs, _ = setup_jobs(tmp_path)
    old = dict(temperature="20", weather="晴", updated=100, attempted=100)
    with store.connect() as db:
        db.execute("INSERT INTO weather VALUES('110108',?)", (encode(old),))
    result = jobs.weather("110108", True)
    assert result["temperature"] == "20" and result["updated"] == 100 and result["error"]


def test_background_poll_and_cached_client_request(monkeypatch, tmp_path):
    store, jobs, account = setup_jobs(tmp_path)
    commands = []
    def execute(request):
        commands.append(request["command"])
        if request["command"] == "sync":
            return dict(messages=[dict(id="42:10", subject="offline delivery", **{"from": "sender"}, date=1)], validity="42", cursor=10)
        return {"acknowledged": request["ids"]}
    monkeypatch.setattr(worker, "execute", execute)
    # No desktop request is involved in the scheduler tick.
    jobs.tick()
    assert commands == ["sync", "markSeen"]
    cached = jobs.mail_request(dict(command="sync", address=account["address"], validity="42", cursor=0))
    assert cached["messages"][0]["id"] == "42:10"
    assert commands == ["sync", "markSeen"]
    jobs.tick()
    assert commands == ["sync", "markSeen"]


def test_invalid_shared_settings_rejected(api):
    assert api.post("/v1/mutate", json=mutation(kind="settings", identity="shared", payload={})).status_code == 422


def test_backup_restores_dataset_and_history(tmp_path):
    store = Store(tmp_path / "db")
    store.mutate(mutation())
    path = backup(store.path, tmp_path / "backup")
    restored = Store(path)
    assert restored.dataset == store.dataset
    assert restored.changes(0, 100) == store.changes(0, 100)
