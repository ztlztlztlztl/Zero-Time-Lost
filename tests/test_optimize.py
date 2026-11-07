"""Regression coverage for storage, deployment, and background-service failures."""
import importlib.util
import io
import json
from pathlib import Path
import sqlite3
import subprocess
import threading
import time
import tracemalloc
import zipfile
from datetime import datetime, timedelta, timezone
from unittest.mock import Mock

import pytest
from server.store import Store
from server.jobs import Jobs, worker
from server.app import Mutation
from server.scripts.cloud_backup import backup as cloud_backup
from shared import local_backup

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_switch', ROOT / 'deploy/scripts/release_switch.py')
switch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(switch)


def test_manager_worker_resolves_sibling_modules_in_embedded_runtime(tmp_path):
    python = ROOT / 'build/ServerManager/Release/runtime/python.exe'
    if not python.exists():
        pytest.skip('packaged Windows runtime is not built')
    request = dict(version=1, action='self-check', dataDir=str(tmp_path))
    result = subprocess.run([str(python), '-B', str(ROOT / 'deploy/scripts/bootstrap_worker.py')],
                            cwd=tmp_path, input=json.dumps(request).encode(), capture_output=True, timeout=20)
    events = [json.loads(line) for line in result.stdout.decode('utf-8').splitlines()]
    assert result.returncode == 0 and events[-1]['event'] == 'result', events
    assert events[-1]['data']['adapter'] == 1


def test_local_backup_restore_preserves_unknown_data_and_wal(tmp_path):
    data = tmp_path / 'data'
    (data / 'database').mkdir(parents=True)
    (data / 'config').mkdir()
    (data / 'config/settings.json').write_text('{"setting":1}')
    (data / 'vault.ztlvault').write_text('encrypted-secret')
    (data / 'personal.txt').write_text('keep this')
    db = sqlite3.connect(data / 'database/tasks.db')
    db.execute('PRAGMA journal_mode=WAL')
    db.execute('CREATE TABLE tasks(title TEXT)')
    db.execute("INSERT INTO tasks VALUES('from WAL')")
    db.commit()
    archive = data / 'backups/test.ztlbackup'
    local_backup.backup(data, archive)
    db.close()
    (data / 'config/settings.json').write_text('{"setting":2}')
    snapshot = local_backup.restore(data, archive)
    assert snapshot.exists()
    assert json.loads((data / 'config/settings.json').read_text()) == {'setting': 1}
    assert (data / 'vault.ztlvault').read_text() == 'encrypted-secret'
    assert list((data / 'backups').glob('original-data-*/personal.txt'))[0].read_text() == 'keep this'
    with sqlite3.connect(data / 'database/tasks.db') as restored:
        assert restored.execute('SELECT title FROM tasks').fetchone()[0] == 'from WAL'
    # WAL contents belong in the verified snapshot rather than a copied sidecar.
    with zipfile.ZipFile(archive) as package:
        assert not any(name.endswith(('-wal', '-shm')) for name in package.namelist())


def test_local_backup_large_file_has_bounded_memory_and_valid_content(tmp_path):
    data = tmp_path / 'data'
    data.mkdir()
    payload = data / 'vault.large'
    with payload.open('wb') as stream:
        for _ in range(24):
            stream.write(b'x' * 1024 * 1024)
    archive = tmp_path / 'large.ztlbackup'
    tracemalloc.start()
    try:
        local_backup.backup(data, archive)
        _, peak = tracemalloc.get_traced_memory()
    finally:
        tracemalloc.stop()
    assert peak < 12 * 1024 * 1024
    assert local_backup.verify(archive)['files']['vault.large'] == local_backup.hashlib.sha256(b'x' * (24 * 1024 * 1024)).hexdigest()


def test_restore_rejects_tampering_without_touching_live_data(tmp_path):
    data = tmp_path / 'data'
    data.mkdir()
    original = data / 'vault.ztlvault'
    original.write_text('original')
    archive = tmp_path / 'bad.zip'
    with zipfile.ZipFile(archive, 'w') as package:
        package.writestr('vault.ztlvault', 'damaged')
        package.writestr('backup.json', json.dumps(dict(format=1, files={'vault.ztlvault': '0' * 64})))
    with pytest.raises(ValueError, match='校验'):
        local_backup.restore(data, archive)
    assert original.read_text() == 'original'


@pytest.mark.parametrize('moved', [False, True])
def test_interrupted_restore_recovers_from_directory_switch(tmp_path, moved):
    data = tmp_path / 'data'
    identity = 'a' * 32
    previous = tmp_path / ('data.previous-' + identity)
    stage = tmp_path / ('data.restore-' + identity)
    previous.mkdir()
    (previous / 'personal.txt').write_text('old')
    stage.mkdir()
    (stage / 'restore.marker').write_text(identity)
    (stage / 'vault.ztlvault').write_text('new')
    if moved:
        stage.rename(data)
    local_backup.atomic_json(local_backup.journal_path(data), dict(id=identity, phase='prepared'))
    local_backup.recover(data)
    assert (data / 'vault.ztlvault').read_text() == 'new'
    assert (data / 'backups' / ('original-data-' + identity) / 'personal.txt').read_text() == 'old'
    assert not local_backup.journal_path(data).exists()


def test_cloud_backup_daily_and_operation_retention_are_independent(tmp_path):
    source = tmp_path / 'db.sqlite'
    with sqlite3.connect(source) as db:
        db.execute('CREATE TABLE important(value)')
        db.execute('INSERT INTO important VALUES(42)')
    directory = tmp_path / 'backups'
    now = datetime(2026, 11, 6, 12, tzinfo=timezone.utc)
    for age in reversed(range(35)):
        cloud_backup(source, directory, 'daily', now - timedelta(days=age))
    for i in range(18):
        cloud_backup(source, directory, 'operation', now + timedelta(seconds=i))
    daily = [p for p in directory.glob('*.sqlite') if '-000000-000000' in p.name]
    operation = [p for p in directory.glob('*.sqlite') if p not in daily]
    assert len(daily) == 30
    assert len(operation) == 10
    with sqlite3.connect(daily[0]) as db:
        assert db.execute('SELECT value FROM important').fetchone()[0] == 42


def prepared_switch(tmp_path):
    root = tmp_path / 'installation'
    candidate = root / 'releases/new'
    for location, value in ((root, b'old'), (candidate, b'new')):
        (location / 'server').mkdir(parents=True)
        for name in ('.env', 'compose.yaml'):
            (location / 'server' / name).write_bytes(value)
    (root / 'server/data').mkdir()
    (root / 'server/data/ztl.sqlite').write_bytes(b'preserve business database')
    return root, candidate


@pytest.mark.parametrize('where', ['run', 'health'])
def test_server_switch_failure_restores_old_program_only(tmp_path, where):
    root, candidate = prepared_switch(tmp_path)
    calls = []
    def runner(command):
        calls.append(command)
        if where == 'run' and len(calls) == 1:
            raise RuntimeError('container could not start')
    def health():
        if where == 'health':
            raise RuntimeError('unhealthy candidate')
    with pytest.raises(RuntimeError):
        switch.apply(root, candidate, runner, health)
    assert (root / 'server/.env').read_bytes() == b'old'
    assert (root / 'server/compose.yaml').read_bytes() == b'old'
    assert (root / 'server/data/ztl.sqlite').read_bytes() == b'preserve business database'
    assert json.loads((root / 'deployment.json').read_text())['phase'] == 'rolled-back'
    assert calls == ['up -d --force-recreate'] * 2


def test_bad_rollback_identity_cannot_read_outside_snapshot(tmp_path):
    (tmp_path / 'deployment.json').write_text(json.dumps(dict(id='../other', previous=True)))
    with pytest.raises(ValueError):
        switch.rollback(tmp_path, Mock())


def test_date_header_epoch_is_valid_and_fallback_source_is_explicit():
    epoch = worker.message_brief(b'Date: Thu, 1 Jan 1970 00:00:00 +0000\r\n\r\n', '1', 1)
    assert epoch['date'] == 0 and epoch['dateSource'] == 'header'
    internal = worker.message_brief(b'Date: invalid\r\n\r\n', '1', 2, '01-Jan-2020 09:00:00 +0800')
    assert internal['dateSource'] == 'internal'
    assert worker.message_brief(b'\r\n', '1', 3)['dateSource'] == 'received'


def test_mime_empty_html_alternative_keeps_plain_and_ignores_forward_attachment():
    raw = b'Content-Type: multipart/mixed; boundary=m\r\n\r\n--m\r\nContent-Type: multipart/alternative; boundary=a\r\n\r\n--a\r\nContent-Type: text/plain\r\n\r\nactual text\r\n--a\r\nContent-Type: text/html\r\n\r\n<html><body></body></html>\r\n--a--\r\n--m\r\nContent-Type: message/rfc822\r\nContent-Disposition: attachment\r\n\r\nSubject: forwarded\r\n\r\nnot the body\r\n--m--'
    message = worker.email.message_from_bytes(raw, policy=worker.email.policy.default)
    body = worker.message_body(message)
    assert 'actual text' in body and 'not the body' not in body


def test_weather_cache_does_not_wait_for_network_lock(tmp_path):
    jobs = Jobs(Store(tmp_path / 'db'))
    with jobs.store.connect() as db:
        db.execute('INSERT INTO weather VALUES(?,?)', ('110108', json.dumps(dict(attempted=int(time.time()), temperature='12'))))
    ready, done = threading.Event(), threading.Event()
    def occupy():
        with jobs.weather_lock:
            ready.set()
            done.wait(3)
    thread = threading.Thread(target=occupy)
    thread.start()
    ready.wait(1)
    try:
        started = time.monotonic()
        assert jobs.weather('110108')['temperature'] == '12'
        assert time.monotonic() - started < .5
        started = time.monotonic()
        busy = jobs.weather('110108', force=True)
        assert busy['temperature'] == '12' and '正在刷新' in busy['error']
        assert time.monotonic() - started < 2
    finally:
        done.set()
        thread.join()
        jobs.close()


def test_business_validation_rejects_bad_interval_and_wrong_bool():
    base = dict(op='one', kind='calendar', id='one', base=0, payload=dict(id='one', title='test', type='interval', hasDue=True, repeat='none', start='2026-11-06T12:00:00+08:00', due='2026-11-06T11:00:00+08:00'))
    with pytest.raises(ValueError):
        Mutation(**base)
    base['payload'].update(type='ddl', paused='false')
    with pytest.raises(ValueError):
        Mutation(**base)


@pytest.mark.parametrize('rule,days', [('monthly', [29, 30, 31]), ('weekly', [1, 3, 7]), ('daily', None)])
def test_finite_recurrence_count_respects_last_supported_year(rule, days):
    from datetime import date
    from server.validation import calendar
    first, last = date(9998, 11, 20), date(9999, 12, 31)
    available = sum(rule == 'daily' or (d.day if rule == 'monthly' else d.isoweekday()) in days
                    for d in (first + timedelta(days=i) for i in range((last-first).days+1)))
    payload = dict(title='finite', type='ddl', hasDue=True, due=first.isoformat()+'T12:00:00+08:00',
                   zone='Asia/Shanghai', repeat=rule, countLimited=True, countLimit=available)
    if days is not None:
        payload['monthdays' if rule == 'monthly' else 'weekdays'] = days
    calendar(payload)
    with pytest.raises(ValueError, match='9999'):
        calendar(payload | dict(countLimit=available+1))


def test_monthly_legacy_default_uses_first_day_in_source_timezone():
    from server.validation import calendar
    payload = dict(title='monthly', hasDue=True, repeat='monthly', due='9999-12-30T12:00:00Z',
                   zone='Pacific/Kiritimati', countLimited=True, countLimit=1)
    calendar(payload)
    with pytest.raises(ValueError, match='9999'):
        calendar(payload | dict(countLimit=2))
    with pytest.raises(ValueError):
        calendar(payload | dict(monthdays=[]))


@pytest.mark.parametrize('locked', [False, True])
def test_stale_remote_operation_is_failed_only_when_command_lock_is_free(tmp_path, monkeypatch, locked):
    import sys
    import types
    def flock(*args):
        if locked:
            raise BlockingIOError()
    monkeypatch.setitem(sys.modules, 'fcntl', types.SimpleNamespace(LOCK_EX=2, LOCK_NB=4, flock=flock))
    spec = importlib.util.spec_from_file_location('remote_operation', ROOT / 'deploy/scripts/remote_operation.py')
    operation = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(operation)
    operation.ROOT, operation.LOCK = tmp_path, tmp_path / 'operation.lock'
    identity = 'a' * 32
    folder = tmp_path / identity
    folder.mkdir()
    operation.atomic(folder / 'status.json', dict(id=identity, state='running', heartbeat=time.time()-60))
    (folder / 'request.json').write_text('secret')
    result = operation.status(identity)
    assert result['state'] == ('running' if locked else 'failed')
    assert (folder / 'request.json').exists() == locked


def test_damaged_mail_does_not_drop_other_headers_or_advance_without_retry():
    class IMAP:
        def __init__(self, *args, **kwargs): pass
        def login(self, *args): pass
        def capability(self): return 'OK', [b'IMAP4rev1']
        def select(self, *args, **kwargs): return 'OK', []
        def response(self, *args): return 'UIDVALIDITY', [b'9']
        def logout(self): pass
        def uid(self, command, *args):
            if command == 'search': return 'OK', [b'1 2 3']
            if args[0] == '2': raise RuntimeError('transient failure')
            if args[1] == '(RFC822.SIZE)': return 'OK', [b'1 (RFC822.SIZE 20)']
            return 'OK', [(b'INTERNALDATE "06-Nov-2026 12:00:00 +0000"', b'Subject: ok\r\n\r\n')]
    result = worker.execute(dict(command='sync', account=dict(address='a', code='x', server='host')), IMAP)
    assert [m['id'] for m in result['messages']] == ['9:1', '9:3']
    assert result['retryUids'] == [2] and result['cursor'] == 3
    assert all(m['dateSource'] == 'internal' for m in result['messages'])
