"""Full local backup/restore helper. The GUI exits before this helper touches data."""
import argparse
from contextlib import contextmanager
import socket
from contextlib import closing
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sqlite3
import subprocess
import tempfile
import time
import uuid
import zipfile

DIRECTORIES = {'config', 'database', 'cloud'}


def included(name):
    path = PurePosixPath(name)
    return bool(path.parts) and (path.parts[0] in DIRECTORIES or path.parts[:2] == ('cache', 'drafts') or len(path.parts) == 1 and path.name.startswith('vault.'))


def checked(name):
    path = PurePosixPath(name)
    if '\\' in name or path.is_absolute() or '..' in path.parts or ':' in name or not included(name):
        raise ValueError('备份包含不支持的路径')
    return path


def backup(data, destination):
    data, destination = Path(data).resolve(), Path(destination).absolute()
    data.joinpath('tmp').mkdir(parents=True, exist_ok=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + '.partial-' + uuid.uuid4().hex)
    manifest = {}
    try:
        with tempfile.TemporaryDirectory(dir=data / 'tmp') as scratch, zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as archive:
            roots = [data / name for name in DIRECTORIES] + [data / 'cache/drafts'] + list(data.glob('vault.*'))
            files = []
            for root in roots:
                if root.is_symlink():
                    raise ValueError('数据目录包含链接')
                if root.is_dir():
                    files.extend(root.rglob('*'))
                elif root.is_file():
                    files.append(root)
            for file in sorted(files):
                if file.is_symlink():
                    raise ValueError('数据目录包含链接')
                if not file.is_file():
                    continue
                name = file.relative_to(data).as_posix()
                if not included(name) or file.name.endswith(('-wal', '-shm', '-journal')):
                    continue
                if file.is_symlink() or any(parent.is_symlink() for parent in file.parents if parent != data.parent):
                    raise ValueError('数据目录包含链接，未制作备份')
                source = file
                if file.suffix in ('.db', '.sqlite'):
                    source = Path(scratch) / (uuid.uuid4().hex + '.db')
                    with closing(sqlite3.connect(file.as_uri() + '?mode=ro', uri=True)) as live, closing(sqlite3.connect(source)) as target:
                        live.backup(target)
                        if target.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
                            raise ValueError('数据库完整性检查失败')
                checksum = hashlib.sha256()
                # Bound memory independently of the largest database or draft file.
                with source.open('rb') as stream, archive.open(name, 'w', force_zip64=True) as entry:
                    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                        checksum.update(chunk)
                        entry.write(chunk)
                manifest[name] = checksum.hexdigest()
            archive.writestr('backup.json', json.dumps(dict(format=1, files=manifest)))
        verify(temporary)
        temporary.replace(destination)
        return destination
    finally:
        temporary.unlink(missing_ok=True)


def verify(path):
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        names = [item.filename for item in entries]
        if len(names) != len(set(names)) or 'backup.json' not in names:
            raise ValueError('备份清单无效')
        if archive.getinfo('backup.json').file_size > 8 * 1024**2:
            raise ValueError('备份清单过大')
        manifest = json.loads(archive.read('backup.json'))
        if manifest.get('format') != 1 or not isinstance(manifest.get('files'), dict) or set(names) != set(manifest['files']) | {'backup.json'}:
            raise ValueError('备份文件与清单不一致')
        if sum(item.file_size for item in entries) > 16 * 1024**3:
            raise ValueError('备份超过 16 GiB')
        for name, digest in manifest['files'].items():
            checked(name)
            checksum = hashlib.sha256()
            with archive.open(name) as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    checksum.update(chunk)
            if checksum.hexdigest() != digest:
                raise ValueError('备份校验失败：' + name)
        return manifest


def atomic_json(path, value):
    temporary = path.with_suffix('.json.tmp')
    with temporary.open('w', encoding='utf-8') as stream:
        json.dump(value, stream)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def journal_path(data):
    return data.with_name(data.name + '.restore.json')


def recover(data):
    data = Path(data).absolute()
    journal = journal_path(data)
    if not journal.exists():
        return
    state = json.loads(journal.read_text(encoding='utf-8'))
    identity = state['id']
    if not isinstance(identity, str) or len(identity) != 32 or any(c not in '0123456789abcdef' for c in identity):
        raise ValueError('恢复日志身份无效，原文件保持不变')
    stage = data.with_name(data.name + '.restore-' + identity)
    previous = data.with_name(data.name + '.previous-' + identity)
    if any(p.is_symlink() for p in (data, previous, stage)):
        raise ValueError('恢复目录不可为链接')
    if not previous.exists():
        if (data / 'restore.marker').exists():
            (data / 'restore.marker').unlink()
            journal.unlink()
            return
        # Old directory was never moved: abandon candidate and retain the original.
        if data.exists():
            if stage.exists():
                shutil.rmtree(stage)
            journal.unlink()
            return
        raise ValueError('恢复目录不完整，请从 data.previous 目录手动恢复')
    if not data.exists():
        if stage.exists():
            stage.rename(data)
        else:
            previous.rename(data)
            journal.unlink()
            return
    marker = data / 'restore.marker'
    if not marker.exists() or marker.read_text() != identity:
        raise ValueError('恢复目标已被其他程序使用，原数据保留在 previous 目录')
    backups = data / 'backups'
    if (previous / 'backups').exists() and not backups.exists():
        (previous / 'backups').rename(backups)
    backups.mkdir(exist_ok=True)
    previous.rename(backups / ('original-data-' + identity))
    marker.unlink()
    journal.unlink()


def restore(data, source):
    data = Path(data).absolute()
    recover(data)
    manifest = verify(source)
    identity = uuid.uuid4().hex
    stage = data.with_name(data.name + '.restore-' + identity)
    previous = data.with_name(data.name + '.previous-' + identity)
    stage.mkdir()
    published = False
    try:
        with zipfile.ZipFile(source) as archive:
            for name in manifest['files']:
                target = stage.joinpath(*checked(name).parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(name) as incoming, target.open('wb') as outgoing:
                    shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
                if target.suffix in ('.db', '.sqlite'):
                    with closing(sqlite3.connect(target.as_uri() + '?mode=ro', uri=True)) as db:
                        if db.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
                            raise ValueError('恢复数据库校验失败')
        snapshot = data / 'backups' / ('before-restore-' + identity + '.ztlbackup')
        backup(data, snapshot)
        (stage / 'restore.marker').write_text(identity)
        atomic_json(journal_path(data), dict(id=identity, phase='prepared'))
        published = True
        data.rename(previous)
        stage.rename(data)
        recover(data)
        return snapshot
    except BaseException:
        if published:
            recover(data)
        raise
    finally:
        if not published and stage.exists():
            shutil.rmtree(stage)


@contextmanager
def data_lock(data):
    if os.name == 'nt':
        kernel = ctypes.windll.kernel32
        kernel.CreateMutexW.restype = ctypes.c_void_p
        key = str(Path(data).absolute()).replace(chr(92), '/').lower()
        name = 'Local\\ZTLData-' + hashlib.sha256(key.encode('utf-8')).hexdigest()
        handle = kernel.CreateMutexW(None, False, name)
        if not handle:
            raise OSError('无法创建数据锁')
        acquired = kernel.WaitForSingleObject(ctypes.c_void_p(handle), 0) in (0, 0x80)
        try:
            if not acquired:
                raise RuntimeError('数据目录正在使用，本次操作未执行')
            yield
        finally:
            if acquired:
                kernel.ReleaseMutex(ctypes.c_void_p(handle))
            kernel.CloseHandle(ctypes.c_void_p(handle))
    else:
        import fcntl
        with Path(data).with_suffix('.maintenance.lock').open('a') as stream:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            yield


def wait_for_exit(pid, timeout=120):
    if os.name == 'nt':
        kernel = ctypes.windll.kernel32
        kernel.OpenProcess.restype = ctypes.c_void_p
        handle = kernel.OpenProcess(0x100000, False, pid)
        if not handle:
            return
        try:
            if kernel.WaitForSingleObject(ctypes.c_void_p(handle), int(timeout * 1000)) != 0:
                raise TimeoutError('客户端未退出，未执行备份或恢复')
        finally:
            kernel.CloseHandle(ctypes.c_void_p(handle))
    else:
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return
            time.sleep(.2)
        raise TimeoutError('client still running')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('backup', 'restore', 'recover'))
    parser.add_argument('data')
    parser.add_argument('archive', nargs='?')
    parser.add_argument('--wait-pid', type=int)
    parser.add_argument('--relaunch')
    parser.add_argument('--startup-recovery', action='store_true')
    args = parser.parse_args()
    data = Path(args.data).absolute()
    exited = not args.wait_pid
    result = dict(ok=False, action=args.action)
    try:
        if args.wait_pid:
            wait_for_exit(args.wait_pid)
            exited = True
        if args.action == 'recover' and args.startup_recovery:
            recover(data)
            return
        with data_lock(data):
            recover(data)
            if args.action != 'recover':
                (backup if args.action == 'backup' else restore)(data, args.archive)
            result.update(ok=True, archive=args.archive)
    except Exception as error:
        result['error'] = str(error)
        if args.action == 'recover':
            raise
    if exited and not journal_path(data).exists():
        report = data / 'logs' / 'backup-operation.json'
        report.parent.mkdir(parents=True, exist_ok=True)
        atomic_json(report, result)
        if args.relaunch:
            subprocess.Popen([args.relaunch], creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    elif exited:
        # Keep a readable failure report even if the data directory is between moves.
        atomic_json(data.with_name(data.name + '.backup-operation.json'), result)


if __name__ == '__main__':
    main()
