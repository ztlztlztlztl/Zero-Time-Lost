"""Cloud-owned IMAP polling and weather cache; no client needs to stay open."""
import importlib.util
import json
import logging
import threading
import time
from pathlib import Path

import httpx

from .store import encode

spec = importlib.util.spec_from_file_location("ztl_mail_worker", Path(__file__).resolve().parents[1] / "scripts/mail_worker.py")
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)
log = logging.getLogger("ztl.jobs")


class Jobs:
    def __init__(self, store):
        self.store = store
        self.lock = threading.RLock()
        self.stop = threading.Event()
        self.thread = None

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
        with self.lock:
            with self.store.connect() as db:
                row = db.execute("SELECT payload FROM mail_state WHERE address=?", (address,)).fetchone()
                state = json.loads(row[0]) if row else {}
            settings = self.config().get("mail", {})
            data = worker.execute(dict(command="sync", account=account, cursor=state.get("cursor", 0),
                                       validity=state.get("validity", ""), limit=settings.get("initialNumber", 50)))
            self.save_mail(address, data, True)
            self.mark_seen(account)

    def save_mail(self, address, data, sync=False):
        with self.store.connect() as db:
            for message in data["messages"]:
                db.execute("INSERT OR IGNORE INTO mail(address,id,payload) VALUES(?,?,?)",
                           (address, message["id"], encode(message)))
            if sync:
                db.execute("INSERT OR REPLACE INTO mail_state VALUES(?,?)", (address, encode(data | {"messages": []})))

    def mark_seen(self, account):
        address = account["address"]
        while True:
            with self.store.connect() as db:
                ids = [r[0] for r in db.execute("SELECT id FROM mail WHERE address=? AND seen=0 LIMIT 100", (address,))]
            if not ids:
                break
            result = worker.execute(dict(command="markSeen", account=account, ids=ids))
            if result.get("acknowledged") != ids:
                raise ValueError("IMAP 已读确认无效")
            with self.store.connect() as db:
                db.executemany("UPDATE mail SET seen=1 WHERE address=? AND id=?", [(address, i) for i in ids])

    def mail_request(self, request):
        with self.lock:
            address = request["address"]
            account = self.account(address)
            command = request["command"]
            if command in ("body", "restore"):
                return worker.execute(dict(command=command, account=account, id=request["id"]))
            if command == "history":
                data = worker.execute(dict(command="history", account=account, before=request.get("before", 0),
                    validity=request.get("validity", ""), limit=self.config().get("mail", {}).get("earlierNumber", 10)))
                self.save_mail(address, data)
                self.mark_seen(account)
                return data
            if command != "sync":
                raise ValueError("不支持的邮件命令")
            # First use and manual refresh can poll, while background polling works independently.
            warning = ""
            if request.get("force", False):
                try:
                    self.poll(account)
                    self.status("mail:" + address)
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
        with self.lock:
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
            return result

    def tick(self):
        config = self.config()
        interval = max(60, min(86400, config.get("mail", {}).get("pollingInterval", 5) * 60))
        if config.get("mail", {}).get("pollingInterval", 5) != -1:
            for account in config.get("accounts", []):
                key = "mail:" + account["address"]
                with self.store.connect() as db:
                    row = db.execute("SELECT payload FROM job_status WHERE key=?", (key,)).fetchone()
                    updated = json.loads(row[0])["updated"] if row else 0
                if time.time() - updated >= interval:
                    try:
                        self.poll(account)
                        self.status(key)
                    except Exception as error:
                        self.status(key, worker.failure_message(error))
                        log.warning("mail job failed (%s)", type(error).__name__)
        for city in config.get("weather", {}).get("savedCities", []):
            self.weather(city["code"])

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
            self.thread.join(timeout=5)
