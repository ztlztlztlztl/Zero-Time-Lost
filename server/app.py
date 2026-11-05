import hmac
import os
import hashlib
import json
from contextlib import asynccontextmanager
from typing import Literal

from fastapi import Depends, FastAPI, Header, HTTPException, Query
from pydantic import BaseModel, Field, model_validator

from .jobs import Jobs, worker
from .store import Store, encode


class Mutation(BaseModel):
    op: str = Field(min_length=1, max_length=128)
    kind: Literal["calendar", "note", "order", "settings", "mailflag"]
    id: str = Field(min_length=1, max_length=256)
    base: int = Field(ge=0)
    deleted: bool = False
    payload: dict

    @model_validator(mode="after")
    def check_payload(self):
        if len(encode(self.payload).encode()) > 4 * 1024 * 1024:
            raise ValueError("记录超过 4 MiB")
        if not self.deleted:
            if self.kind in ("calendar", "note") and self.payload.get("id") != self.id:
                raise ValueError("记录 ID 不匹配")
            if self.kind == "note" and not (self.payload.get("kind") == 2 or
                    self.payload.get("kind") == 1 and self.payload.get("targetType") == "website"):
                raise ValueError("只同步普通纸条和网站书签")
            if self.kind == "settings":
                p = self.payload
                if self.id != "shared" or not all(k in p for k in ("weather", "mail", "calendar", "noteTags", "weatherKey", "accounts")):
                    raise ValueError("共享设置字段不完整")
                if not all(isinstance(p[k], dict) for k in ("weather", "mail", "calendar")) or not isinstance(p["accounts"], list) or not isinstance(p["noteTags"], list):
                    raise ValueError("共享设置格式无效")
                interval = p["mail"].get("pollingInterval", 5)
                if not isinstance(interval, int) or interval != -1 and not 1 <= interval <= 1440:
                    raise ValueError("邮件轮询间隔无效")
                if len(p["accounts"]) > 100:
                    raise ValueError("邮箱数量超过限制")
                for account in p["accounts"]:
                    if not isinstance(account, dict) or not all(isinstance(account.get(k), str) and account[k] for k in ("address", "server", "port", "code")):
                        raise ValueError("邮箱任务设置不完整")
                    if not account["port"].isdigit() or not 1 <= int(account["port"]) <= 65535:
                        raise ValueError("邮箱端口无效")
            if self.kind == "order" and (self.id not in ("1", "2") or not isinstance(self.payload.get("ids"), list) or
                    not all(isinstance(i, str) and i for i in self.payload["ids"])):
                raise ValueError("排序数据无效")
            if self.kind == "mailflag":
                address, uid = self.payload.get("address", ""), self.payload.get("uid", "")
                if not isinstance(address, str) or not isinstance(uid, str) or not address or not uid or self.id != hashlib.sha256((address + "\0" + uid).encode()).hexdigest():
                    raise ValueError("邮件状态标识无效")
                if not all(isinstance(self.payload.get(k), bool) for k in ("read", "starred", "hidden")):
                    raise ValueError("邮件状态无效")
        return self


class MailRequest(BaseModel):
    address: str = Field(min_length=1, max_length=320)
    command: Literal["sync", "history", "body", "restore"]
    id: str = ""
    cursor: int = Field(default=0, ge=0)
    validity: str = ""
    before: int = Field(default=0, ge=0)
    force: bool = False


def create_app(path=None, token=None, background=True):
    store = Store(path or os.environ.get("ZTL_DB", "server/data/ztl.sqlite"))
    token = token if token is not None else os.environ.get("ZTL_TOKEN", "")
    jobs = Jobs(store)

    @asynccontextmanager
    async def lifespan(app):
        if not token or len(token) < 32:
            raise RuntimeError("Set a random ZTL_TOKEN (at least 32 characters)")
        if background:
            jobs.start()
        yield
        jobs.close()

    def authorized(authorization: str = Header(default="")):
        if not token or not hmac.compare_digest(authorization.encode(), ("Bearer " + token).encode()):
            raise HTTPException(401, "访问令牌无效")

    app = FastAPI(title="Z.T.L personal cloud", lifespan=lifespan,
                  dependencies=[Depends(authorized)], docs_url=None, redoc_url=None, openapi_url=None)
    app.state.store, app.state.jobs = store, jobs

    @app.get("/health")
    def health():
        with store.connect() as db:
            states = {r[0]: json.loads(r[1]) for r in db.execute("SELECT * FROM job_status")}
            head = db.execute("SELECT COALESCE(MAX(rev),0) FROM changes").fetchone()[0]
        return {"ok": True, "protocol": 1, "dataset": store.dataset, "head": head, "jobs": states}

    @app.post("/v1/mutate")
    def mutate(request: Mutation):
        try:
            return store.mutate(request.model_dump())
        except ValueError as error:
            raise HTTPException(409, str(error)) from error

    @app.get("/v1/changes")
    def changes(cursor: int = Query(0, ge=0), limit: int = Query(100, ge=1, le=500)):
        try:
            return store.changes(cursor, limit)
        except ValueError as error:
            raise HTTPException(409, str(error)) from error

    @app.post("/v1/mail")
    def mail(request: MailRequest):
        try:
            return {"ok": True, "data": jobs.mail_request(request.model_dump())}
        except Exception as error:
            return {"ok": False, "error": worker.failure_message(error)}

    @app.get("/v1/weather/{city}")
    def weather(city: str, refresh: bool = False):
        if not city.isascii() or not city.isdigit() or len(city) != 6:
            raise HTTPException(422, "城市编码无效")
        return jobs.weather(city, refresh)

    return app


app = create_app()
