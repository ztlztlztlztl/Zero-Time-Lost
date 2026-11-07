"""Detached, server-owned maintenance steps with a lock, heartbeat and bounded output."""
import fcntl
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path('/var/lib/ztl-maintenance')
LOCK = Path('/run/lock/ztl-command.lock')


def atomic(path, value):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value), encoding='utf-8')
    temp.replace(path)


def status(identity=None):
    if identity is None:
        file = ROOT / 'latest'
        if not file.exists():
            return {}
        identity = file.read_text().strip()
    if not re.fullmatch('[a-f0-9]{32}', identity):
        raise ValueError('invalid operation ID')
    file = ROOT / identity / 'status.json'
    state = json.loads(file.read_text()) if file.exists() else {}
    if state.get('state') in ('queued', 'running') and time.time() - state.get('heartbeat', 0) > 35:
        # A child retains this lock if the supervisor dies. Never report the
        # operation stopped while its command may still be changing the server.
        with LOCK.open('a') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return state
            state.update(state='failed', code=70, reason='维护进程已中断，请检查部署状态后重试', heartbeat=time.time())
            atomic(file, state)
            for name in ('request.json', 'input'):
                (file.parent / name).unlink(missing_ok=True)
    return state


def start(request):
    ROOT.mkdir(parents=True, exist_ok=True, mode=0o700)
    identity = uuid.uuid4().hex
    directory = ROOT / identity
    directory.mkdir(mode=0o700)
    source = request.pop('source')
    atomic(directory / 'request.json', request)
    os.chmod(directory / 'request.json', 0o600)
    atomic(directory / 'status.json', dict(id=identity, phase=request.get('step', ''), state='queued', cancellable=bool(request.get('cancellable')), heartbeat=time.time()))
    (ROOT / 'latest').write_text(identity)
    try:
        with open(os.devnull, 'r+b') as silent:
            subprocess.Popen([sys.executable, '-c', source, 'worker', identity], stdin=silent, stdout=silent, stderr=silent, start_new_session=True, close_fds=True)
    except Exception:
        atomic(directory / 'status.json', dict(id=identity, state='failed', code=70, heartbeat=time.time()))
        raise
    return dict(id=identity)


def run_work(identity):
    directory = ROOT / identity
    request = json.loads((directory / 'request.json').read_text())
    state = status(identity)
    with LOCK.open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            state.update(state='failed', code=75)
            atomic(directory / 'status.json', state)
            return
        with (directory / 'output.log').open('w+b') as output, (directory / 'input').open('wb+') as input_stream:
            input_stream.write((request.get('data') or '').encode())
            input_stream.seek(0)
            process = subprocess.Popen(request['command'], shell=True, stdin=input_stream, stdout=output, stderr=subprocess.STDOUT, start_new_session=True, pass_fds=(lock.fileno(),))
            deadline = time.monotonic() + min(3600, request['timeout'])
            state.update(state='running', pid=process.pid)
            while process.poll() is None:
                state['heartbeat'] = time.time()
                atomic(directory / 'status.json', state)
                stop = time.monotonic() >= deadline or request.get('cancellable') and (directory / 'cancel').exists()
                if stop:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=20)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                    state['reason'] = 'timeout' if time.monotonic() >= deadline else 'cancelled'
                    break
                if os.fstat(output.fileno()).st_size > 4 * 1024 * 1024:
                    output.seek(-1024 * 1024, 2)
                    tail = output.read()
                    output.seek(0)
                    output.write(tail)
                    output.truncate()
                time.sleep(1)
            state.update(state='succeeded' if process.returncode == 0 else 'failed', code=process.returncode, heartbeat=time.time())
            atomic(directory / 'status.json', state)
        (directory / 'request.json').unlink(missing_ok=True)
        (directory / 'input').unlink(missing_ok=True)


def work(identity):
    directory = ROOT / identity
    try:
        run_work(identity)
    except BaseException:
        state = status(identity)
        state.update(state='failed', code=70, heartbeat=time.time(), reason='maintenance worker interrupted')
        atomic(directory / 'status.json', state)
    finally:
        for name in ('request.json', 'input'):
            (directory / name).unlink(missing_ok=True)


def main(request):
    action = request['action']
    if action == 'start':
        return start(request)
    if action == 'status':
        state = status(request.get('id'))
        if state and request.get('output'):
            file = ROOT / state['id'] / 'output.log'
            if file.exists():
                with file.open('rb') as stream:
                    stream.seek(max(0, file.stat().st_size - (2*1024*1024 if state['state'] in ('succeeded', 'failed') else 4096)))
                    state['output'] = stream.read().decode('utf-8', errors='replace')
            else:
                state['output'] = ''
        return state
    if action == 'cancel':
        state = status(request.get('id'))
        accepted = state.get('cancellable', False) and state.get('state') in ('running', 'queued')
        if accepted:
            (ROOT / state['id'] / 'cancel').touch()
        state['cancelAccepted'] = accepted
        return state
    raise ValueError('unknown action')


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'worker':
        work(sys.argv[2])
    else:
        print(json.dumps(main(json.load(sys.stdin))))
