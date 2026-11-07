"""Crash-recoverable program switch. Never replaces databases, certificates or backups."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid
import re


def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.switch-tmp')
    with temporary.open('wb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def save(root, state):
    atomic(root / 'deployment.json', json.dumps(state).encode())


def rollback(root, runner):
    root = Path(root)
    state = json.loads((root / 'deployment.json').read_text())
    if not re.fullmatch(r'[a-f0-9]{32}', state.get('id', '')):
        raise ValueError('Invalid rollback identity')
    directory = root / 'rollback' / state['id']
    if not directory.is_dir() or directory.is_symlink():
        raise ValueError('Rollback snapshot is missing')
    if state['previous']:
        for name in ('server/.env', 'server/compose.yaml'):
            atomic(root / name, (directory / name).read_bytes())
        runner('up -d --force-recreate')
        state['phase'] = 'rolled-back'
    else:
        runner('down')
        state['phase'] = 'failed-initial-deployment'
    save(root, state)
    return state


def apply(root, candidate, runner, health):
    root, candidate = Path(root), Path(candidate)
    if (root / 'deployment.json').exists():
        state = json.loads((root / 'deployment.json').read_text())
        if state['phase'] in ('switching', 'prepared', 'rollback-required'):
            rollback(root, runner)
    identity = uuid.uuid4().hex
    directory = root / 'rollback' / identity
    directory.mkdir(parents=True, mode=0o700)
    previous = all((root / name).is_file() for name in ('server/.env', 'server/compose.yaml'))
    if previous:
        for name in ('server/.env', 'server/compose.yaml'):
            destination = directory / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / name, destination)
    state = dict(id=identity, phase='prepared', previous=previous, candidate=candidate.name, updated=int(time.time()))
    save(root, state)
    try:
        state['phase'] = 'switching'
        save(root, state)
        for name in ('server/.env', 'server/compose.yaml'):
            atomic(root / name, (candidate / name).read_bytes())
        runner('up -d --force-recreate')
        health()
        state['phase'] = 'healthy'
        save(root, state)
        return state
    except BaseException:
        try:
            rollback(root, runner)
        except Exception:
            state['phase'] = 'rollback-required'
            save(root, state)
        raise


def main(request):
    import ssl
    import urllib.request
    root = Path('/opt/ztl-cloud')
    binary = 'docker compose' if subprocess.call('docker compose version >/dev/null 2>&1', shell=True) == 0 else 'docker-compose'
    project = request['project']
    import re
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,100}', project):
        raise ValueError('invalid project')
    command = binary + ' -p ' + project + ' --env-file server/.env -f server/compose.yaml '
    def run(args):
        # Compose 1.x cannot recreate containers against modern Docker metadata.
        if binary == 'docker-compose' and args.startswith('up'):
            subprocess.run(command + 'stop api https', cwd=root, shell=True, check=True, timeout=120)
            subprocess.run(command + 'rm -f api https', cwd=root, shell=True, check=True, timeout=120)
        subprocess.run(command + args, cwd=root, shell=True, check=True, timeout=240)
    def health():
        values = dict(line.split('=', 1) for line in (root / 'server/.env').read_text().splitlines() if '=' in line)
        context = ssl.create_default_context(cafile=str(root / 'server/certs/ca.crt'))
        context.check_hostname = False  # Loopback, still verifies the deployment CA.
        for attempt in range(20):
            try:
                req = urllib.request.Request('https://127.0.0.1/health', headers={'Authorization': 'Bearer ' + values['ZTL_TOKEN']})
                with urllib.request.urlopen(req, context=context, timeout=3) as response:
                    if json.load(response).get('protocol') == 1:
                        return
            except Exception:
                time.sleep(1)
        raise ValueError('Local HTTPS health check failed')
    if request['action'] == 'rollback':
        return rollback(root, run)
    digest = request['digest']
    if not re.fullmatch('[a-f0-9]{64}', digest):
        raise ValueError('invalid release digest')
    return apply(root, root / 'releases' / digest, run, health)


if __name__ == '__main__':
    import sys
    main(json.load(sys.stdin))
