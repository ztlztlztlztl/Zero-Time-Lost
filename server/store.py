"""Single-user revision log. Every mutation and its retry receipt commit together."""
import json
import sqlite3
import uuid
from contextlib import contextmanager
from pathlib import Path


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


class Store:
    def __init__(self, path):
        self.path = str(path)
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as db:
            db.executescript("""
                PRAGMA journal_mode=WAL;
                CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT);
                CREATE TABLE IF NOT EXISTS objects(kind TEXT,id TEXT,rev INTEGER,payload TEXT,
                    deleted INTEGER NOT NULL,PRIMARY KEY(kind,id));
                CREATE TABLE IF NOT EXISTS changes(rev INTEGER PRIMARY KEY AUTOINCREMENT,
                    kind TEXT,id TEXT,payload TEXT,deleted INTEGER);
                CREATE TABLE IF NOT EXISTS receipts(op TEXT PRIMARY KEY,request TEXT,response TEXT);
                CREATE TABLE IF NOT EXISTS mail(address TEXT,id TEXT,payload TEXT,seen INTEGER DEFAULT 0,
                    PRIMARY KEY(address,id));
                CREATE TABLE IF NOT EXISTS mail_state(address TEXT PRIMARY KEY,payload TEXT);
                CREATE TABLE IF NOT EXISTS job_status(key TEXT PRIMARY KEY,payload TEXT);
                CREATE TABLE IF NOT EXISTS weather(city TEXT PRIMARY KEY,payload TEXT);
            """)
            db.execute("INSERT OR IGNORE INTO meta VALUES('dataset',?)", (str(uuid.uuid4()),))

    @contextmanager
    def connect(self):
        db = sqlite3.connect(self.path, timeout=15)
        db.row_factory = sqlite3.Row
        try:
            with db:
                yield db
        finally:
            db.close()

    @property
    def dataset(self):
        with self.connect() as db:
            return db.execute("SELECT value FROM meta WHERE key='dataset'").fetchone()[0]

    @staticmethod
    def record(row):
        return dict(kind=row["kind"], id=row["id"], rev=row["rev"],
                    payload=json.loads(row["payload"]), deleted=bool(row["deleted"]))

    def get(self, kind, identity):
        with self.connect() as db:
            row = db.execute("SELECT * FROM objects WHERE kind=? AND id=?", (kind, identity)).fetchone()
            return self.record(row) if row else None

    def mutate(self, request):
        canonical = encode(request)
        with self.connect() as db:
            db.execute("BEGIN IMMEDIATE")
            receipt = db.execute("SELECT request,response FROM receipts WHERE op=?", (request["op"],)).fetchone()
            if receipt:
                if receipt["request"] != canonical:
                    raise ValueError("operation ID reused with different content")
                return json.loads(receipt["response"])
            row = db.execute("SELECT * FROM objects WHERE kind=? AND id=?",
                             (request["kind"], request["id"])).fetchone()
            actual = row["rev"] if row else 0
            if actual != request["base"]:
                result = {"ok": False, "conflict": self.record(row) if row else None}
            else:
                payload = encode(request["payload"])
                args = (request["kind"], request["id"], payload, int(request["deleted"]))
                revision = db.execute("INSERT INTO changes(kind,id,payload,deleted) VALUES(?,?,?,?)", args).lastrowid
                db.execute("INSERT INTO objects(kind,id,payload,deleted,rev) VALUES(?,?,?,?,?) "
                           "ON CONFLICT(kind,id) DO UPDATE SET payload=excluded.payload,deleted=excluded.deleted,rev=excluded.rev",
                           (*args, revision))
                result = {"ok": True, "record": dict(kind=request["kind"], id=request["id"],
                          rev=revision, payload=request["payload"], deleted=request["deleted"])}
            db.execute("INSERT INTO receipts VALUES(?,?,?)", (request["op"], canonical, encode(result)))
            return result

    def changes(self, cursor, limit):
        with self.connect() as db:
            high = db.execute("SELECT COALESCE(MAX(rev),0) FROM changes").fetchone()[0]
            if cursor > high:
                raise ValueError("cursor is ahead of server; restore requires reconnect review")
            rows = db.execute("SELECT * FROM changes WHERE rev>? ORDER BY rev LIMIT ?", (cursor, limit)).fetchall()
            end = rows[-1]["rev"] if rows else cursor
            return {"records": [self.record(r) for r in rows], "cursor": end, "more": end < high,
                    "dataset": self.dataset}
