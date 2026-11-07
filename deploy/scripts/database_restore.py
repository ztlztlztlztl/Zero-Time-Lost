"""SQLite restore primitives, also executed remotely over SSH."""
import json
import os
import re
from pathlib import Path
import sqlite3
import sys
import uuid
from contextlib import closing


def prepare(source, database):
    source, database = Path(source), Path(database)
    if source.is_symlink() or not source.is_file():
        raise ValueError('备份文件不存在或是链接')
    target = database.with_name('restore-' + uuid.uuid4().hex + '.sqlite')
    try:
        with closing(sqlite3.connect(source.as_uri() + '?mode=ro', uri=True)) as live, closing(sqlite3.connect(target)) as copy:
            if live.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
                raise ValueError('备份完整性检查失败')
            if not {'objects', 'changes', 'meta'}.issubset({r[0] for r in live.execute("SELECT name FROM sqlite_master WHERE type='table'")}):
                raise ValueError('所选文件不是 Z.T.L 数据库')
            live.backup(copy)
            # Restoring a history is a new synchronization identity, even if its head happens to match.
            copy.execute("INSERT OR REPLACE INTO meta VALUES('dataset',?)", (str(uuid.uuid4()),))
            copy.commit()
        target.chmod(0o600)
        return target
    except Exception:
        target.unlink(missing_ok=True)
        raise


def commit(prepared, database):
    prepared, database = Path(prepared), Path(database)
    if prepared.parent != database.parent or not prepared.name.startswith('restore-') or prepared.is_symlink():
        raise ValueError('恢复临时文件无效')
    checked = sqlite3.connect(prepared.as_uri() + '?mode=ro', uri=True)
    try:
        if checked.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
            raise ValueError('恢复临时文件损坏')
    finally:
        checked.close()
    if database.exists():
        live = sqlite3.connect(database, timeout=15)
        try:
            if live.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()[0] != 0:
                raise ValueError('数据库仍在使用，未执行恢复')
        finally:
            live.close()
    # API has been stopped and its WAL is fully checkpointed before replacement.
    for suffix in ('-wal', '-shm'):
        Path(str(database) + suffix).unlink(missing_ok=True)
    os.replace(prepared, database)


if __name__ == '__main__':
    r = json.load(sys.stdin)
    database = Path('/opt/ztl-cloud/server/data/ztl.sqlite')
    if r['action'] == 'prepare':
        print(json.dumps(dict(prepared=prepare(Path('/opt/ztl-cloud/server/backups') / r['backup'], database).name)))
    elif r['action'] in ('commit', 'discard'):
        name = r['prepared']
        if not re.fullmatch(r'restore-[a-f0-9]{32}\.sqlite', name):
            raise ValueError('恢复临时文件名称无效')
        if r['action'] == 'commit':
            commit(database.with_name(name), database)
        else:
            database.with_name(name).unlink(missing_ok=True)
