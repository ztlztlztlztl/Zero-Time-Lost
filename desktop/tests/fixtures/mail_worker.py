"""Offline IMAP fixture through the production JSON/process boundary."""
import importlib.util
import json
import pathlib
import sys
import os
import sqlite3

source = pathlib.Path(__file__).resolve().parents[3] / 'shared' / 'mail_worker.py'
spec = importlib.util.spec_from_file_location('production_mail_worker', source)
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)

RAW = (b'From: sender@example.test\r\nSubject: Integration mail\r\n'
       b'Date: Tue, 20 Oct 2026 10:00:00 +0800\r\n'
       b'Content-Type: text/plain; charset=utf-8\r\n\r\nIntegration body')


class FakeIMAP:
    def __init__(self, *args, **kwargs):
        self.host = args[0]
        if args and args[0] == 'offline.invalid':
            raise OSError('Fixture connection unavailable')

    def login(self, *args):
        self.logged_in = True
        self.identified = False

    def capability(self):
        assert self.logged_in
        return 'OK', [b'IMAP4rev1 ID']

    def xatom(self, command, values):
        assert command == 'ID' and 'Z.T.L' in values
        self.identified = True
        return 'OK', [b'ID completed']

    def select(self, folder, readonly=False):
        assert self.identified
        assert folder == 'INBOX'
        self.readonly = readonly
        return 'OK', [b'4']

    def response(self, name):
        assert name == 'UIDVALIDITY'
        return 'UIDVALIDITY', [b'9']

    def uid(self, command, *args):
        if command == 'search':
            return 'OK', [b'1 2 3 4']
        if command == 'store':
            assert not self.readonly
            root = pathlib.Path(os.environ['ZTL_DATA_DIR'])
            # The real process boundary must never mark an unsaved summary read.
            with sqlite3.connect(root / 'database' / 'mail.db') as db:
                for uid in args[0].split(','):
                    assert db.execute('SELECT 1 FROM mails WHERE uid=?', ('9:' + uid,)).fetchone()
            if self.host == 'fail-seen-once.test':
                marker = root / 'seen-attempt'
                if not marker.exists():
                    marker.write_text('failed', encoding='utf-8')
                    return 'NO', []
            return 'OK', []
        assert command == 'fetch'
        if args[0] == '99':
            return 'OK', [None]
        if args[1] == '(RFC822.SIZE)':
            return 'OK', [b'1 (RFC822.SIZE ' + str(len(RAW)).encode() + b')']
        assert args[1] in ('(BODY.PEEK[HEADER])', '(BODY.PEEK[HEADER] INTERNALDATE)', '(BODY.PEEK[])')
        return 'OK', [(b'', RAW)]

    def logout(self):
        pass


if __name__ == '__main__':
    sys.stdin.reconfigure(encoding='utf-8')
    sys.stdout.reconfigure(encoding='utf-8')
    try:
        request = json.load(sys.stdin)
        # Verify Qt's contract in addition to Python's compatibility with older null fields.
        if request.get('command') in ('sync', 'history'):
            assert isinstance(request.get('cursor'), int)
            assert isinstance(request.get('validity'), str)
        result = {'version': 1, 'ok': True, 'data': worker.execute(request, FakeIMAP)}
    except Exception as error:
        result = {'version': 1, 'ok': False, 'error': worker.failure_message(error)}
    print(json.dumps(result, ensure_ascii=False), flush=True)
