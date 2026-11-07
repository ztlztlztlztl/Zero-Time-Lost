"""Cloud-owned IMAP polling and weather cache; no client needs to stay open."""
import importlib.util
import json
import logging
import threading
import time
import re
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager

import httpx

from .store import encode

mail_path = Path(__file__).resolve().parents[1] / "scripts/mail_worker.py"
if not mail_path.is_file():
    mail_path = Path(__file__).resolve().parents[1] / "shared/mail_worker.py"
spec = importlib.util.spec_from_file_location("ztl_mail_worker", mail_path)
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)
log = logging.getLogger("ztl.jobs")


class Jobs:
    def __init__(self, store):
        self.store = store
        self.lock = threading.RLock()
        self.account_locks = {}
        self.weather_lock = threading.RLock()
        self.pool = ThreadPoolExecutor(max_workers=4, thread_name_prefix="ztl-network")
        self.stop = threading.Event()
        self.thread = None
        self.network_slots = threading.BoundedSemaphore(4)

    def account_lock(self, address):
        with self.lock:
            return self.account_locks.setdefault(address, threading.RLock())

    def execute(self, request):
        if self.stop.is_set():
            raise TimeoutError("scheduler stopping")
        if not self.network_slots.acquire(timeout=1):
            raise worker.MailAccessError("后台网络任务繁忙，请稍后重试")
        try:
            return worker.execute(dict(request, _cancel=self.stop))
        finally:
            self.network_slots.release()

    @contextmanager
    def account_operation(self, address):
        lock = self.account_lock(address)
        if not lock.acquire(timeout=1):
            raise worker.MailAccessError("该邮箱正在收取或读取邮件，当前显示已保存内容，请稍后刷新")
        try:
            yield
        finally:
            lock.release()

    def config(self):
        record = self.store.get("settings", "shared")
        return record["payload"] if record and not record["deleted"] else {}

    def status(self, key, error=""):
        with self.store.connect() as db:
            db.execute("INSERT OR REPLACE INTO job_status VALUES(?,?)",
                       (key, encode({"updated": int(time.time()), "error": error})))

    def account(self, address):
        for account in self.config().get("accounts", []):
            if account.get("address") == address:
                return account
        raise ValueError("邮箱尚未同步到服务器，请先执行数据同步")

    def poll(self, account):
        address = account["address"]
        with self.account_operation(address):
            with self.store.connect() as db:
                row = db.execute("SELECT payload FROM mail_state WHERE address=?", (address,)).fetchone()
                state = json.loads(row[0]) if row else {}
            settings = self.config().get("mail", {})
            data = self.execute(dict(command="sync", account=account, cursor=state.get("cursor", 0),
                                       validity=state.get("validity", ""), retryUids=state.get("retryUids", []), limit=settings.get("initialNumber", 50)))
            self.save_mail(address, data, True)
            self.mark_seen(account)
            return data

    def save_mail(self, address, data, sync=False):
        with self.store.connect() as db:
            for message in data["messages"]:
                db.execute("INSERT OR IGNORE INTO mail(address,id,payload) VALUES(?,?,?)",
                           (address, message["id"], encode(message)))
            if not sync and "retryUids" in data:
                row = db.execute("SELECT payload FROM mail_state WHERE address=?", (address,)).fetchone()
                state = json.loads(row[0]) if row else {}
                if state.get("validity") == data.get("validity"):
                    state["retryUids"] = data["retryUids"]
                    db.execute("INSERT OR REPLACE INTO mail_state VALUES(?,?)", (address, encode(state)))
            if sync:
                db.execute("INSERT OR REPLACE INTO mail_state VALUES(?,?)", (address, encode(data | {"messages": []})))

    def mark_seen(self, account):
        address = account["address"]
        deadline = time.monotonic() + 25
        for _ in range(4):
            if self.stop.is_set() or time.monotonic() >= deadline:
                break
            with self.store.connect() as db:
                ids = [r[0] for r in db.execute("SELECT id FROM mail WHERE address=? AND seen=0 LIMIT 100", (address,))]
            if not ids:
                break
            result = self.execute(dict(command="markSeen", account=account, ids=ids))
            if result.get("acknowledged") != ids:
                raise ValueError("IMAP 已读确认无效")
            with self.store.connect() as db:
                db.executemany("UPDATE mail SET seen=1 WHERE address=? AND id=?", [(address, i) for i in ids])

    def history(self, account, request):
        with self.account_operation(account["address"]):
            with self.store.connect() as db:
                row = db.execute("SELECT payload FROM mail_state WHERE address=?", (account["address"],)).fetchone()
                state = json.loads(row[0]) if row else {}
            data = self.execute(dict(command="history", account=account, before=request.get("before", 0),
                validity=request.get("validity", ""), retryUids=state.get("retryUids", []),
                limit=self.config().get("mail", {}).get("earlierNumber", 10)))
            self.save_mail(account["address"], data)
            self.mark_seen(account)
            return data

    def mail_request(self, request):
        address = request["address"]
        account = self.account(address)
        command = request["command"]
        if command in ("body", "restore"):
            with self.account_operation(address):
                return self.execute(dict(command=command, account=account, id=request["id"]))
        if command == "history":
            return self.history(account, request)
        if command != "sync":
            raise ValueError("不支持的邮件命令")
        # First use and manual refresh can poll, while background polling works independently.
        warning = ""
        if request.get("force", False):
            try:
                result = self.poll(account)
                warning = (result or {}).get("warning", "")
                self.status("mail:" + address, warning)
            except Exception as error:
                warning = worker.failure_message(error)
                self.status("mail:" + address, warning)
        else:
            with self.store.connect() as db:
                status = db.execute("SELECT payload FROM job_status WHERE key=?", ("mail:" + address,)).fetchone()
                warning = json.loads(status[0]).get("error", "") if status else ""
        with self.store.connect() as db:
            row = db.execute("SELECT payload FROM mail_state WHERE address=?", (address,)).fetchone()
            state = json.loads(row[0]) if row else {}
            validity = state.get("validity", "")
            cursor = int(request.get("cursor") or 0) if request.get("validity") == validity else 0
            rows = db.execute("SELECT payload FROM mail WHERE address=? AND id LIKE ? "
                "AND CAST(substr(id,instr(id,':')+1) AS INTEGER)>? "
                "ORDER BY CAST(substr(id,instr(id,':')+1) AS INTEGER) LIMIT 100",
                (address, validity + ":%", cursor)).fetchall()
        messages = [json.loads(r[0]) for r in rows]
        end = max([cursor] + [int(m["id"].split(":")[1]) for m in messages])
        if not validity and warning:
            raise worker.MailAccessError(warning)
        return dict(messages=messages, validity=validity, cursor=end, remaining=int(len(messages) == 100), warning=warning)


    def weather(self, city, force=False):
        with self.store.connect() as db:
            row = db.execute("SELECT payload FROM weather WHERE city=?", (city,)).fetchone()
            cached = json.loads(row[0]) if row else {}
        if not force and time.time() - cached.get("attempted", 0) < 1200:
            return cached
        if self.stop.is_set():
            return dict(cached, error='后台任务正在停止')
        if not self.weather_lock.acquire(timeout=1):
            return dict(cached, error='天气正在刷新，当前显示已保存内容，请稍后重试')
        try:
            with self.store.connect() as db:
                row = db.execute("SELECT payload FROM weather WHERE city=?", (city,)).fetchone()
                cached = json.loads(row[0]) if row else {}
            if not force and time.time() - cached.get("attempted", 0) < 1200:
                return cached
            result = dict(cached, attempted=int(time.time()), error="")
            try:
                key = self.config().get("weatherKey", "")
                if not key:
                    raise ValueError("尚未配置高德天气 Key")
                response = httpx.get("https://restapi.amap.com/v3/weather/weatherInfo",
                                     params={"key": key, "city": city, "extensions": "base"}, timeout=15)
                response.raise_for_status()
                data = response.json()
                if data.get("status") != "1" or not data.get("lives"):
                    raise ValueError("天气服务拒绝请求，请检查 Web 服务 Key 与配额")
                live = data["lives"][0]
                result.update(temperature=live["temperature"], weather=live["weather"], updated=int(time.time()))
            except Exception as error:
                result["error"] = str(error) if isinstance(error, ValueError) else "天气连接失败，请稍后刷新"
            with self.store.connect() as db:
                db.execute("INSERT OR REPLACE INTO weather VALUES(?,?)", (city, encode(result)))
            self.status("weather:" + city, result.get("error", ""))
            return result
        finally:
            self.weather_lock.release()

    def tick(self):
        config = self.config()
        value = config.get("mail", {}).get("pollingInterval", 5)
        interval = max(60, min(86400, value * 60)) if type(value) is int else 300
        futures = []
        def poll_one(account):
            key = "mail:" + account["address"]
            warning = ""
            try:
                # Drain a bounded number of batches; persisted cursor resumes the next tick.
                for _ in range(3):
                    if self.stop.is_set():
                        return
                    data = self.poll(account)
                    warning = (data or {}).get("warning", "")
                    with self.store.connect() as db:
                        row = db.execute("SELECT payload FROM mail_state WHERE address=?", (account["address"],)).fetchone()
                    if not row or not json.loads(row[0]).get("remaining", 0):
                        break
                self.status(key, warning)
            except Exception as error:
                self.status(key, worker.failure_message(error))
                log.warning("mail job failed (%s)", type(error).__name__)
        if value != -1:
            for account in config.get("accounts", []):
                if self.stop.is_set():
                    break
                if not isinstance(account, dict) or not account.get("address"):
                    self.status("mail:invalid", "邮箱配置缺少 address")
                    continue
                key = "mail:" + account["address"]
                with self.store.connect() as db:
                    row = db.execute("SELECT payload FROM job_status WHERE key=?", (key,)).fetchone()
                    updated = json.loads(row[0]).get("updated", 0) if row else 0
                if time.time() - updated >= interval:
                    futures.append(self.pool.submit(poll_one, account))
        # Weather runs in its own scheduler lane while IMAP workers are active.
        for city in config.get("weather", {}).get("savedCities", []):
            if self.stop.is_set():
                break
            if not isinstance(city, dict) or not re.fullmatch(r"[0-9]{6}", str(city.get("code", ""))):
                self.status("weather:invalid", "城市配置缺少有效 code")
                continue
            try:
                self.weather(city["code"])
            except Exception:
                self.status("weather:" + city["code"], "天气任务失败")
        for future in futures:
            if self.stop.is_set():
                future.cancel()
            else:
                future.result()

    def start(self):
        def run():
            while not self.stop.is_set():
                try:
                    self.tick()
                except Exception as error:
                    log.warning("scheduler tick failed (%s)", type(error).__name__)
                self.stop.wait(30)
        self.thread = threading.Thread(target=run, name="ztl-jobs", daemon=True)
        self.thread.start()

    def close(self):
        self.stop.set()
        if self.thread:
            self.thread.join(timeout=18)
        self.pool.shutdown(wait=False, cancel_futures=True)
