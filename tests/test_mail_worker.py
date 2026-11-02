import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location("mail_worker", pathlib.Path(__file__).resolve().parents[1] / "scripts" / "mail_worker.py")
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)

RAW = b"From: sender@example.com\r\nSubject: hello\r\nDate: Tue, 20 Oct 2026 10:00:00 +0800\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nhello <student>"

class FakeIMAP:
    commands = []
    def __init__(self, *args, **kwargs): pass
    def login(self, *args): pass
    def capability(self): return "OK", [b"IMAP4rev1"]
    def select(self, *args, **kwargs):
        assert kwargs["readonly"] is True
        return "OK", []
    def response(self, *args): return "UIDVALIDITY", [b"9"]
    def logout(self): pass
    def uid(self, command, *args):
        self.commands.append((command, args))
        if command == "search": return "OK", [b"1 2 3 4"]
        if args[1] == "(RFC822.SIZE)": return "OK", [b"1 (RFC822.SIZE 200)"]
        return "OK", [(b"", RAW)]

class MailTests(unittest.TestCase):
    def setUp(self): FakeIMAP.commands = []
    def request(self, **extra):
        return {"account": {"address":"a@example.com","code":"x","server":"imap.example.com","port":"993"}, **extra}
    def test_initial_limit_and_true_identity(self):
        result = worker.execute(self.request(limit=2), FakeIMAP)
        self.assertEqual([m["id"] for m in result["messages"]], ["9:3","9:4"])
        self.assertEqual(result["cursor"], 4)
        self.assertFalse(any(c[0] == "store" for c in FakeIMAP.commands))
    def test_id_uses_post_login_capabilities_before_opening_inbox(self):
        class NetEaseIMAP(FakeIMAP):
            capabilities = (b"IMAP4REV1",)  # The pre-login greeting does not advertise ID.
            logged_in = False
            identified = False
            def login(self, *args): self.logged_in = True
            def capability(self):
                self_outer.assertTrue(self.logged_in)
                return "OK", [b"IMAP4rev1 ID"]
            def xatom(self, command, values):
                self_outer.assertEqual(command, "ID")
                self_outer.assertIn('"name" "ZTL Sidekick"', values)
                self_outer.assertNotIn("a@example.com", values)
                self.identified = True
                return "OK", [b"ID completed"]
            def select(self, *args, **kwargs):
                if not self.identified: return "NO", [b"Unsafe Login"]
                return super().select(*args, **kwargs)
        self_outer = self
        result = worker.execute(self.request(command="sync"), NetEaseIMAP)
        self.assertEqual(len(result["messages"]), 4)
        self.assertEqual(worker.execute(self.request(command="check"), NetEaseIMAP), {"mailbox":"INBOX"})
    def test_connection_check_rejects_login_only_success(self):
        class BlockedIMAP(FakeIMAP):
            def select(self, *args, **kwargs):
                return "NO", [b"Unsafe Login private-server-details"]
        with self.assertRaises(worker.MailAccessError) as caught:
            worker.execute(self.request(command="check"), BlockedIMAP)
        message = worker.failure_message(caught.exception)
        self.assertIn("Unsafe Login", message)
        self.assertNotIn("private-server-details", message)
    def test_non_id_server_never_receives_id(self):
        class StandardIMAP(FakeIMAP):
            def xatom(self, *args): raise AssertionError("ID was not advertised")
        self.assertEqual(worker.execute(self.request(command="check"), StandardIMAP), {"mailbox":"INBOX"})
    def test_rejected_id_has_specific_error(self):
        class RejectedIMAP(FakeIMAP):
            def capability(self): return "OK", [b"IMAP4rev1 ID"]
            def xatom(self, *args): return "NO", [b"private-server-details"]
        with self.assertRaises(worker.MailAccessError) as caught:
            worker.execute(self.request(command="check"), RejectedIMAP)
        self.assertIn("身份声明", worker.failure_message(caught.exception))
        self.assertNotIn("private-server-details", worker.failure_message(caught.exception))
    def test_first_sync_with_qt_null_state(self):
        # A missing SQLite row used to be serialized by Qt as explicit JSON nulls.
        result = worker.execute(self.request(command="sync", cursor=None, validity=None, limit=10), FakeIMAP)
        self.assertEqual(len(result["messages"]), 4)
        self.assertEqual(result["cursor"], 4)
        self.assertEqual(result["validity"], "9")
    def test_nullable_optional_batch_parameters(self):
        result = worker.execute(self.request(command="sync", cursor=None, validity="9", before=None, limit=None), FakeIMAP)
        self.assertEqual(len(result["messages"]), 4)
    def test_incremental_batch_does_not_skip(self):
        result = worker.execute(self.request(cursor=1, validity="9", limit=1), FakeIMAP)
        self.assertEqual(result["cursor"], 2)
        self.assertEqual(result["remaining"], 2)
    def test_history_uses_older_uids(self):
        result = worker.execute(self.request(command="history", cursor=4, validity="9", before=3, limit=1), FakeIMAP)
        self.assertEqual([m["id"] for m in result["messages"]], ["9:2"])
    def test_changed_validity_resets_identity(self):
        result = worker.execute(self.request(cursor=99, validity="old", limit=2), FakeIMAP)
        self.assertEqual(result["cursor"], 4)
    def test_body_is_peek_and_plaintext_escaped(self):
        result = worker.execute(self.request(command="body", id="9:3"), FakeIMAP)
        self.assertIn("&lt;student&gt;", result["body"])
        self.assertTrue(any("BODY.PEEK[]" in str(c) for c in FakeIMAP.commands))
    def test_reject_stale_body_identity(self):
        with self.assertRaises(RuntimeError):
            worker.execute(self.request(command="body", id="8:3"), FakeIMAP)
    def test_restore_fetches_headers_without_marking_server_read(self):
        result = worker.execute(self.request(command="restore", id="9:3"), FakeIMAP)
        self.assertEqual(result["message"]["id"], "9:3")
        self.assertEqual(result["message"]["from"], "sender@example.com")
        self.assertTrue(any("BODY.PEEK[HEADER]" in str(c) for c in FakeIMAP.commands))
        self.assertFalse(any(c[0] == "store" for c in FakeIMAP.commands))
    def test_restore_rejects_missing_server_message(self):
        class MissingIMAP(FakeIMAP):
            def uid(self, command, *args):
                return "OK", [None]
        with self.assertRaises(worker.MailUnavailable):
            worker.execute(self.request(command="restore", id="9:3"), MissingIMAP)
        self.assertIn("不存在", worker.failure_message(worker.MailUnavailable()))
    def test_attachment_download_command_removed(self):
        with self.assertRaises(ValueError):
            worker.execute(self.request(command="attachment", id="9:3", part=1), FakeIMAP)
    def test_header_decodes_all_parts(self):
        self.assertEqual(worker.decoded("=?utf-8?b?5L2g5aW9?= =?utf-8?b?5a2m55Sf?="), "你好学生")
    def test_verified_tls_and_trimmed_server(self):
        seen = {}
        class InspectIMAP(FakeIMAP):
            def __init__(self, host, port, **kwargs):
                seen.update(host=host, port=port, **kwargs)
            def login(self, address, code):
                seen.update(address=address, code=code)
        request = self.request(command="check")
        request["account"].update(server=" imap.example.com ", address=" a@example.com ")
        worker.execute(request, InspectIMAP)
        self.assertEqual(seen["host"], "imap.example.com")
        self.assertEqual(seen["address"], "a@example.com")
        self.assertEqual(seen["ssl_context"].verify_mode, worker.ssl.CERT_REQUIRED)
        self.assertTrue(seen["ssl_context"].check_hostname)
    def test_network_errors_do_not_echo_credentials(self):
        for error, expected in [(worker.socket.gaierror("private-value"), "域名解析"),
                                (TimeoutError("private-value"), "超时"),
                                (TypeError("private-value"), "数据格式"),
                                (worker.ssl.SSLCertVerificationError("private-value"), "证书"),
                                (worker.imaplib.IMAP4.error("private-value"), "认证")]:
            text = worker.failure_message(error)
            self.assertIn(expected, text)
            self.assertNotIn("private-value", text)

if __name__ == "__main__": unittest.main()
