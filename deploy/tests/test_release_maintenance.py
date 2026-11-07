import io
import json
import shlex
from pathlib import Path
import sqlite3
import sys
from unittest.mock import Mock
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'deploy/scripts'))
import bootstrap_worker as engine
import database_restore
import release_bundle
from server.package import build_bundle
from server.store import Store
from server.scripts.cloud_backup import backup


def test_server_release_has_independent_version_and_verified_contents(tmp_path):
    path = tmp_path / 'release.zip'
    build_bundle(path, '2.0.1-test')
    extracted, manifest = release_bundle.extract_bundle(path, tmp_path / 'cache')
    assert manifest['version'] == '2.0.1-test'
    assert json.loads((extracted / 'server/release.json').read_text())['version'] == '2.0.1-test'
    assert not list(extracted.rglob('.env'))
    assert (extracted / 'scripts/mail_worker.py').read_bytes() == (ROOT / 'shared/mail_worker.py').read_bytes()


def test_release_docker_context_includes_every_copy_source(tmp_path):
    path = tmp_path / 'release.zip'
    build_bundle(path, 'context-test')
    with zipfile.ZipFile(path) as archive:
        rules = archive.read('.dockerignore').decode().splitlines()
        repository_rules = (ROOT / '.dockerignore').read_text().splitlines()
        sources = []
        for line in archive.read('server/Dockerfile').decode().splitlines():
            parts = shlex.split(line, comments=True)
            if parts and parts[0].upper() == 'COPY':
                sources.extend(parts[1:-1])
        assert 'server/validation.py' in sources
        assert 'scripts/product-version.txt' in sources
        for name in sources:
            assert name in archive.namelist()
            assert '!' + name in rules, name
            assert '!' + name in repository_rules, name
            for parent in reversed(Path(name).parents):
                if parent.as_posix() != '.':
                    assert rules.index('!' + parent.as_posix() + '/') < rules.index('!' + name)
                    assert rules.index(parent.as_posix() + '/**') < rules.index('!' + name)


def test_packaging_rejects_missing_docker_source_before_creating_zip(tmp_path, monkeypatch):
    import server.package as package
    original = package.prepare_build_context
    def missing_source(files):
        files.pop('server/validation.py')
        original(files)
    monkeypatch.setattr(package, 'prepare_build_context', missing_source)
    target = tmp_path / 'invalid.zip'
    with pytest.raises(ValueError, match='server/validation.py'):
        package.build_bundle(target, 'missing-context')
    assert not target.exists()
    assert not target.with_suffix('.tmp').exists()


@pytest.mark.parametrize('change', ['extra', 'traversal', 'duplicate', 'tamper', 'symlink', 'protocol'])
def test_invalid_release_is_rejected_before_ssh(tmp_path, monkeypatch, change):
    path = tmp_path / 'release.zip'
    build_bundle(path, 'test')
    with zipfile.ZipFile(path) as archive:
        files = {n: archive.read(n) for n in archive.namelist()}
    if change in ('extra', 'traversal'):
        files['../outside' if change == 'traversal' else 'server/.env'] = b'unexpected'
    if change == 'tamper':
        files['server/app.py'] += b'\nchanged'
    if change == 'protocol':
        value = json.loads(files['release.json'])
        value['protocol'] = 99
        files['release.json'] = json.dumps(value).encode()
    with zipfile.ZipFile(path, 'w') as archive:
        for name, data in files.items():
            entry = zipfile.ZipInfo(name)
            if change == 'symlink' and name == 'server/app.py':
                entry.create_system = 3
                entry.external_attr = 0o120777 << 16
            archive.writestr(entry, data)
        if change == 'duplicate':
            archive.writestr('server/app.py', b'duplicate')
    ssh = Mock()
    monkeypatch.setattr(engine, 'ssh_connect', ssh)
    with pytest.raises(ValueError):
        engine.execute(dict(version=1, action='deploy', dataDir=str(tmp_path / 'data'), release=str(path)))
    ssh.assert_not_called()
    assert not (tmp_path / 'outside').exists()


@pytest.mark.parametrize('url', ['http://example.com/release.zip', 'https://user:password@example.com/a.zip', 'file:///etc/passwd'])
def test_release_download_requires_https_without_embedded_credentials(tmp_path, url):
    with pytest.raises(ValueError):
        release_bundle.download_bundle(url, tmp_path)


@pytest.mark.parametrize('redirect', ['https://cdn.example.test/release.zip', 'http://cdn.example.test/release.zip',
                                     'https://user:secret@cdn.example.test/release.zip'])
def test_release_download_checks_redirect_before_request(tmp_path, monkeypatch, redirect):
    import httpx
    artifact = tmp_path / 'original.zip'
    build_bundle(artifact, 'redirect-test')
    seen = []
    def respond(request):
        seen.append(str(request.url))
        if len(seen) == 1:
            return httpx.Response(302, headers={'location': redirect})
        return httpx.Response(200, content=artifact.read_bytes())
    real_client = httpx.Client
    monkeypatch.setattr(httpx, 'Client', lambda **kw: real_client(transport=httpx.MockTransport(respond), **kw))
    if redirect.startswith('http:') or '@' in redirect:
        with pytest.raises(ValueError, match='重定向'):
            release_bundle.download_bundle('https://origin.example.test/release.zip', tmp_path / 'download')
        assert len(seen) == 1
        assert not list((tmp_path / 'download').glob('*.zip'))
    else:
        downloaded = release_bundle.download_bundle('https://origin.example.test/release.zip', tmp_path / 'download')
        assert downloaded.read_bytes() == artifact.read_bytes()
        assert len(seen) == 2


def setup_maintenance(monkeypatch):
    client = Mock()
    client.open_sftp.return_value.open.side_effect = lambda path: io.BytesIO(
        b'ZTL_TOKEN=private-token\n' if path.endswith('.env') else b'public-ca')
    monkeypatch.setattr(engine, 'remote', lambda *args: dict(existing=True, composeProject='ztl-cloud'))
    commands = []
    def run(client, command, *args, **kwargs):
        commands.append(command)
        return 'plugin\n' if 'echo plugin' in command else ''
    monkeypatch.setattr(engine, 'run', run)
    return client, commands


def test_connection_export_contains_no_ssh_credentials(monkeypatch, tmp_path):
    client, commands = setup_maintenance(monkeypatch)
    profile = engine.maintain(client, dict(action='export-connection'), dict(host='198.51.100.7', password='ssh-secret'), tmp_path)
    assert set(profile) == {'format', 'protocol', 'host', 'port', 'token', 'ca'}
    assert profile['token'] == 'private-token'
    assert 'ssh-secret' not in json.dumps(profile)
    assert not any('stop ' in c or 'rm ' in c for c in commands)


@pytest.mark.parametrize('name', ['../../etc/passwd', 'ztl-a.sqlite; touch /tmp/pwn', '/root/a.sqlite', ''])
def test_restore_refuses_arbitrary_path_before_stopping_service(monkeypatch, tmp_path, name):
    client, commands = setup_maintenance(monkeypatch)
    with pytest.raises(ValueError, match='备份名称无效'):
        engine.maintain(client, dict(action='restore', backup=name, confirmRestore=True), dict(host='198.51.100.7'), tmp_path)
    assert not any('stop api' in c for c in commands)


def test_restore_recovers_backup_and_changes_sync_identity(tmp_path):
    database = tmp_path / 'ztl.sqlite'
    store = Store(database)
    store.mutate(dict(op='one', kind='calendar', id='one', base=0, deleted=False, payload=dict(id='one', title='before')))
    source = backup(database, tmp_path / 'backups')
    dataset = store.dataset
    store.mutate(dict(op='two', kind='calendar', id='two', base=0, deleted=False, payload=dict(id='two', title='after')))
    prepared = database_restore.prepare(source, database)
    source.unlink()  # Backup retention cannot invalidate a prepared snapshot.
    database_restore.commit(prepared, database)
    restored = Store(database)
    assert restored.get('calendar', 'one')['payload']['title'] == 'before'
    assert restored.get('calendar', 'two') is None
    assert restored.dataset != dataset


def test_invalid_backup_leaves_live_database_unchanged(tmp_path):
    database = tmp_path / 'ztl.sqlite'
    store = Store(database)
    original = store.dataset
    invalid = tmp_path / 'invalid.sqlite'
    invalid.write_text('not sqlite')
    with pytest.raises((ValueError, sqlite3.DatabaseError)):
        database_restore.prepare(invalid, database)
    assert Store(database).dataset == original
    assert not list(tmp_path.glob('restore-*.sqlite'))


@pytest.mark.parametrize('failure', ['backup', 'stop', 'commit'])
def test_failed_restore_restarts_stopped_api_and_discards_snapshot(monkeypatch, tmp_path, failure):
    client, commands = setup_maintenance(monkeypatch)
    previous = engine.run
    operations = []
    def run(client, command, stdin=None, *args, **kwargs):
        operation = json.loads(stdin)['action'] if stdin else None
        if operation:
            operations.append(operation)
        if operation == 'prepare':
            return json.dumps(dict(prepared='restore-' + 'a' * 32 + '.sqlite'))
        if ((failure == 'backup' and 'cloud_backup.py' in command) or
            (failure == 'stop' and 'stop api' in command) or
            (failure == 'commit' and operation == 'commit')):
            commands.append(command)
            raise ValueError('injected ' + failure)
        return previous(client, command, *args, **kwargs)
    monkeypatch.setattr(engine, 'run', run)
    with pytest.raises(ValueError, match='injected'):
        engine.maintain(client, dict(action='restore', backup='ztl-20261106-120000-000001.sqlite', confirmRestore=True),
                        dict(host='198.51.100.7'), tmp_path)
    assert operations[-1] == 'discard'
    assert any('start api' in c for c in commands) == (failure != 'backup')
    assert not any('down ' in c or 'prune' in c or 'rm ' in c for c in commands)
