"""SQLite online backup, verification and bounded retention. Also usable before restores."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import sqlite3


def backup(source, directory):
    source, directory = Path(source).resolve(), Path(directory).resolve()
    if not source.is_file():
        raise FileNotFoundError("database has not been initialized")
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / ("ztl-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f") + ".sqlite")
    with sqlite3.connect(str(source)) as live, sqlite3.connect(str(target)) as copy:
        live.backup(copy)
        if copy.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("backup integrity check failed")
    target.chmod(0o600)
    for old in sorted(directory.glob("ztl-*.sqlite"), reverse=True)[30:]:
        old.unlink()
    print("verified backup:", target.name)
    return target


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("source")
    p.add_argument("directory")
    a = p.parse_args()
    backup(a.source, a.directory)
