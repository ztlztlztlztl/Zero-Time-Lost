"""Verified SQLite snapshots: 30 daily calendar days and ten operation snapshots."""
import argparse
from datetime import datetime, timedelta, timezone
from pathlib import Path
import sqlite3
from contextlib import closing
import os
import tempfile


def backup(source, directory, kind="operation", now=None):
    source, directory = Path(source).resolve(), Path(directory).resolve()
    if not source.is_file():
        raise FileNotFoundError("database has not been initialized")
    if kind not in ("daily", "operation"):
        raise ValueError("unknown backup kind")
    now = now or datetime.now(timezone.utc)
    directory.mkdir(parents=True, exist_ok=True)
    stamp = now.strftime("%Y%m%d-000000-000000" if kind == "daily" else "%Y%m%d-%H%M%S-%f")
    target = directory / ("ztl-" + stamp + ".sqlite")
    fd, name = tempfile.mkstemp(prefix=".ztl-backup-", suffix=".tmp", dir=directory)
    os.close(fd)
    temporary = Path(name)
    try:
        with closing(sqlite3.connect(source.as_uri() + "?mode=ro", uri=True)) as live, closing(sqlite3.connect(temporary)) as copy:
            live.backup(copy)
            if copy.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
                raise RuntimeError("backup integrity check failed")
        temporary.chmod(0o600)
        with temporary.open("r+b") as stream:
            os.fsync(stream.fileno())
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)
    # Prune only after a verified snapshot has been published. Independent groups cannot crowd each other out.
    daily = sorted(directory.glob("ztl-????????-000000-000000.sqlite"), reverse=True)
    cutoff = (now - timedelta(days=29)).strftime("%Y%m%d")
    for old in daily:
        if old.name[4:12] < cutoff:
            old.unlink()
    operations = sorted((p for p in directory.glob("ztl-*.sqlite") if p not in daily), reverse=True)
    for old in operations[10:]:
        old.unlink()
    print("verified backup:", target.name)
    return target


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("directory")
    parser.add_argument("--kind", choices=("daily", "operation"), default="operation")
    args = parser.parse_args()
    backup(args.source, args.directory, args.kind)
