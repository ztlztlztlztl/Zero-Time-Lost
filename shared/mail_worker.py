"""Versioned, one-request-per-process IMAP adapter. Secrets only arrive on stdin."""
import email
import email.policy
import html
import imaplib
import json
import socket
import ssl
import sys
import re
import time
import threading
from datetime import datetime, timezone
from email.header import decode_header, make_header
from email.utils import parsedate_to_datetime
from pathlib import Path

PRODUCT_VERSION = Path(__file__).with_name('product-version.txt').read_text(encoding='ascii').strip()

MAX_MESSAGE = 8 * 1024 * 1024
socket.setdefaulttimeout(15)


class MailUnavailable(RuntimeError):
    """A missing message or changed mailbox identity, with no server text exposed."""


class MailAccessError(RuntimeError):
    """A local, safe explanation of a failed IMAP setup stage."""


def prepare_mailbox(mail, readonly=True):
    # Capabilities can change after authentication (notably NetEase's ID support).
    status, capabilities = mail.capability()
    if status != "OK":
        raise MailAccessError("邮箱已登录，但无法读取 IMAP 服务能力，请稍后重试")
    supported = b" ".join(value for value in capabilities if isinstance(value, bytes)).upper().split()
    if b"ID" in supported:
        status, _ = mail.xatom("ID", '("name" "Z.T.L" "version" "' + PRODUCT_VERSION + '" "vendor" "ZTL")')
        if status != "OK":
            raise MailAccessError("邮箱拒绝客户端身份声明，无法打开收件箱，请检查服务商的客户端访问设置")
    status, response = mail.select("INBOX", readonly=readonly)
    if status != "OK":
        unsafe = any(b"unsafe login" in value.lower() for value in response if isinstance(value, bytes))
        if unsafe:
            raise MailAccessError("邮箱登录成功，但服务商以 Unsafe Login 拒绝打开收件箱，请在邮箱网页版检查 IMAP/客户端安全设置后重试")
        raise MailAccessError("邮箱登录成功，但无法打开 INBOX 收件箱，请检查 IMAP 访问权限")


def decoded(value):
    try:
        return str(make_header(decode_header(value or "")))
    except (LookupError, UnicodeError):
        return str(value or "")


def identity(validity, uid):
    return str(validity) + ":" + str(uid)


def message_body(message):
    def render(part):
        if part.get_content_disposition() == "attachment" or part.get_filename():
            return ""
        kind = part.get_content_type()
        if kind == 'message/rfc822':
            return ""
        if part.is_multipart():
            children = list(part.iter_parts())
            if kind == "multipart/related":
                start = part.get_param("start")
                root = next((p for p in children if p.get("Content-ID") == start), children[0] if children else None)
                return render(root) if root else ""
            values = [(p.get_content_type(), render(p)) for p in children]
            if kind == "multipart/alternative":
                return next((v for k, v in reversed(values) if v.strip()), "")
            return "\n".join(v for _, v in values if v.strip())
        if kind not in ("text/plain", "text/html"):
            return ""
        data = part.get_payload(decode=True) or b""
        try:
            text = data.decode(part.get_content_charset() or "utf-8", errors="replace")
        except LookupError:
            text = data.decode("utf-8", errors="replace")
        if not text.strip():
            return ""
        if kind == "text/html":
            meaningful = re.sub(r"<[^>]*>", "", text)
            if not html.unescape(meaningful).strip() and not re.search(r"<(img|svg|video)\b", text, re.I):
                return ""
            return text
        return "<pre>" + html.escape(text) + "</pre>"
    return render(message)


def fetch_bytes(mail, uid, spec, metadata=None):
    status, response = mail.uid("fetch", str(uid), "(RFC822.SIZE)")
    if status != "OK":
        raise MailUnavailable()
    import re
    sizes = [int(x) for row in response if isinstance(row, bytes)
             for x in re.findall(rb"RFC822.SIZE (\d+)", row)]
    if not sizes:
        raise MailUnavailable()
    if max(sizes) > MAX_MESSAGE and "HEADER" not in spec:
        raise MailAccessError("邮件正文超过 8 MiB，建议在邮箱网页版查看")
    status, response = mail.uid("fetch", str(uid), spec)
    if status != "OK":
        raise RuntimeError("fetch failed")
    if metadata is not None:
        for row in response:
            header = row[0] if isinstance(row, tuple) else row
            if isinstance(header, bytes):
                match = re.search(rb'INTERNALDATE "([^"]+)"', header)
                if match:
                    metadata['internal_date'] = match[1].decode('ascii', errors='replace')
    payload = b"".join(row[1] for row in response if isinstance(row, tuple))
    if not payload:
        raise MailUnavailable()
    if len(payload) > MAX_MESSAGE:
        raise MailAccessError("邮件正文超过 8 MiB，建议在邮箱网页版查看")
    return payload


def message_brief(payload, validity, uid, internal_date=None):
    message = email.message_from_bytes(payload, policy=email.policy.default)
    timestamp = None
    try:
        parsed = parsedate_to_datetime(message.get("Date", ""))
        if parsed.tzinfo is not None:
            timestamp = int(parsed.timestamp())
    except (ValueError, TypeError, OverflowError, OSError):
        pass
    source = "header" if timestamp is not None else "unknown"
    if timestamp is None and internal_date:
        try:
            timestamp = int(datetime.strptime(internal_date, "%d-%b-%Y %H:%M:%S %z").timestamp())
            source = "internal"
        except (ValueError, OverflowError, OSError):
            pass
    if timestamp is None:
        timestamp = int(time.time())
        source = "received"
    return {"id": identity(validity, uid), "from": decoded(message.get("From")),
            "subject": decoded(message.get("Subject")) or "无主题", "date": timestamp, "dateSource": source}


def fetch_brief(mail, validity, uid):
    metadata = {}
    payload = fetch_bytes(mail, uid, "(BODY.PEEK[HEADER] INTERNALDATE)", metadata)
    return message_brief(payload, validity, uid, metadata.get('internal_date'))


def execute(request, factory=imaplib.IMAP4_SSL, cancel=None, total_timeout=35):
    if request.get("command") == "self-check":
        return {"runtime": sys.version.split()[0], "ssl": ssl.OPENSSL_VERSION, "adapter": 1}
    cancel = cancel or request.get("_cancel")
    account = request["account"]
    port = int(account.get("port", "993"))
    if not 1 <= port <= 65535:
        raise ValueError("invalid port")
    deadline = time.monotonic() + total_timeout
    if cancel is not None and cancel.is_set():
        raise TimeoutError('mail task stopped')
    mail = factory(account["server"].strip(), port, timeout=15, ssl_context=ssl.create_default_context())
    finished = threading.Event()
    def interrupt():
        while not finished.wait(.1):
            if time.monotonic() >= deadline or cancel is not None and cancel.is_set():
                sock = getattr(mail, 'sock', None)
                if sock:
                    try:
                        sock.shutdown(socket.SHUT_RDWR)
                        sock.close()
                    except OSError:
                        pass
                return
    watchdog = threading.Thread(target=interrupt, daemon=True)
    watchdog.start()
    def budget():
        remaining = deadline - time.monotonic()
        if remaining <= 0 or cancel is not None and cancel.is_set():
            raise TimeoutError("mail task stopped")
        sock = getattr(mail, "sock", None)
        if sock:
            sock.settimeout(max(0.1, min(15, remaining)))
    original_uid = mail.uid
    def bounded_uid(*args, **kwargs):
        budget()
        return original_uid(*args, **kwargs)
    mail.uid = bounded_uid
    try:
        budget()
        mail.login(account["address"].strip(), account["code"])
        command = request.get("command", "sync")
        prepare_mailbox(mail, readonly=command != "markSeen")
        validity = (mail.response("UIDVALIDITY")[1] or [b"0"])[0].decode("ascii")
        if not validity.isdigit() or int(validity) == 0:
            raise MailAccessError("收件箱未返回有效 UIDVALIDITY，无法安全同步邮件")
        if command == "markSeen":
            ids = request.get("ids")
            if not isinstance(ids, list) or not 1 <= len(ids) <= 100:
                raise ValueError("invalid ids")
            uids = []
            for identity_value in ids:
                expected, uid = identity_value.split(":")
                if expected != validity:
                    continue
                if not uid.isascii() or not uid.isdigit() or int(uid) < 1:
                    raise ValueError("invalid uid")
                uids.append(str(int(uid)))
            if uids:
                status, _ = mail.uid("store", ",".join(uids), "+FLAGS.SILENT", r"(\Seen)")
                if status != "OK":
                    raise MailAccessError("摘要已保存，但服务器拒绝标记已读")
            return {"acknowledged": ids}
        if command == "check":
            return {"mailbox": "INBOX"}
        if command in ("body", "restore"):
            expected, uid = request["id"].split(":")
            if expected != validity or not uid.isdigit() or int(uid) < 1:
                raise MailUnavailable()
            if command == "restore":
                return {"message": fetch_brief(mail, validity, uid)}
            payload = fetch_bytes(mail, uid, "(BODY.PEEK[])")
            message = email.message_from_bytes(payload, policy=email.policy.default)
            return {"body": message_body(message)}
        if command not in ("sync", "history"):
            raise ValueError("unsupported command")
        # Older Qt builds explicitly send null for a mailbox with no local sync row.
        cursor = max(0, int(request.get("cursor") or 0))
        old_validity = request.get("validity") or ""
        if old_validity != validity:
            cursor = 0
        before = max(0, int(request.get("before") or 0))
        history = command == "history" and old_validity == validity and before > 0
        criteria = "UID 1:" + str(max(1, before - 1)) if history else ("ALL" if cursor == 0 else "UID " + str(cursor + 1) + ":*")
        status, response = mail.uid("search", None, criteria)
        if status != "OK":
            raise RuntimeError("search failed")
        raw_ids = response[0] if response and response[0] else b""
        uids = sorted({int(x) for x in raw_ids.split() if (int(x) < before if history else int(x) > cursor)})
        limit_value = request.get("limit")
        limit = max(1, min(int(50 if limit_value is None else limit_value), 100))
        chosen = uids[-limit:] if cursor == 0 or history else uids[:limit]
        messages, failed, missing = [], [], []
        retries = request.get("retryUids", []) if old_validity == validity else []
        retry_pool = sorted({int(i) for i in retries if str(i).isascii() and str(i).isdigit() and int(i)>0})
        normal_chosen = list(chosen)
        chosen = sorted(set(chosen) | set(retry_pool[:100]))
        failed.extend(uid for uid in retry_pool[100:] if uid not in chosen)
        for index, uid in enumerate(chosen):
            try:
                budget()
            except TimeoutError:
                failed.extend(chosen[index:])
                break
            try:
                messages.append(fetch_brief(mail, validity, uid))
            except MailUnavailable:
                # Confirm disappearance rather than treating a temporary fetch failure as deletion.
                try:
                    status, result = mail.uid("search", None, "UID " + str(uid))
                    if status == "OK" and str(uid).encode() not in (result[0] or b"").split():
                        missing.append(uid)
                    else:
                        failed.append(uid)
                except Exception:
                    failed.append(uid)
            except Exception:
                failed.append(uid)
        return {"messages": messages, "validity": validity,
                "cursor": max([cursor] + normal_chosen),
                "retryUids": failed, "missingUids": missing,
                "warning": "部分邮件暂未获取，已保留重试记录" if failed else "",
                "remaining": max(0, len(uids) - len(normal_chosen)) if cursor else 0}
    finally:
        finished.set()
        try:
            if getattr(mail, "sock", None):
                mail.sock.settimeout(1)
            mail.logout()
        except Exception:
            pass


def failure_message(error):
    if isinstance(error, MailAccessError):
        return str(error)
    if isinstance(error, MailUnavailable):
        return "服务器原邮件已不存在或邮箱标识已改变，无法获取"
    if isinstance(error, ssl.SSLCertVerificationError):
        return "邮箱证书验证失败，请检查系统时间及网络代理证书"
    if isinstance(error, ssl.SSLError):
        return "邮箱 TLS 连接失败，请检查服务器及 SSL 端口（通常为 993）"
    if isinstance(error, socket.gaierror):
        return "邮箱域名解析失败，请检查服务器地址及网络/DNS"
    if isinstance(error, TimeoutError):
        return "邮箱连接或读取超时，请检查网络及端口是否可达"
    if isinstance(error, ConnectionError):
        return "邮箱连接被拒绝或中断，请检查服务器、端口及网络"
    if isinstance(error, imaplib.IMAP4.error):
        return "邮箱认证或 IMAP 操作失败，请检查授权码并开启 IMAP 服务"
    if isinstance(error, (TypeError, ValueError, KeyError)):
        return "邮件参数或数据格式异常 (" + type(error).__name__ + ")，请检查邮箱设置或更新程序"
    return "邮件请求失败 (" + type(error).__name__ + ")，请检查网络或重新同步"


def main():
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    try:
        request = json.loads(sys.stdin.read(1024 * 1024))
        data = execute(request)
        result = {"version": 1, "ok": True, "data": data}
    except Exception as error:
        # Do not echo credentials, headers or third-party exception text.
        result = {"version": 1, "ok": False, "error": failure_message(error)}
    print(json.dumps(result, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
