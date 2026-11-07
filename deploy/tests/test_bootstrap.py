"""Deployment/cleanup boundary tests. Never connects to a real server."""
import copy
import importlib.util
import io
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
from unittest.mock import Mock

import pytest

ROOT = Path(__file__).resolve().parents[2]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'deploy/scripts' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


remote = load('bootstrap_remote')
worker = load('bootstrap_worker')


@pytest.fixture(autouse=True)
def selected_payload(monkeypatch, tmp_path):
    from server.package import build_bundle
    bundle = tmp_path / 'server.zip'
    build_bundle(bundle, 'test-release')
    import sys
    monkeypatch.syspath_prepend(str(ROOT / 'deploy/scripts'))
    from release_bundle import extract_bundle
    payload, _ = extract_bundle(bundle, tmp_path / 'payload')
    monkeypatch.setattr(worker, 'PAYLOAD', payload)


@pytest.mark.parametrize('path', ['/', '/root', '/root/.ssh', '/root/.profile', '/root/.bashrc',
    '/root/.swas', '/root/snap', '/opt', '/opt/containerd', '/opt/ztl-cloud', '/etc', '/usr', '/var',
    '/root/../etc', '/root/x/../../etc', '/root//x', '/root/x/', '/root/x/nested',
    '/srv/aliyun-agent', '/root/.zshrc', 'relative', '/opt/docker'])
def test_system_and_ambiguous_paths_are_never_candidates(path):
    assert not remote.allowed_location(path)


@pytest.mark.parametrize('path', ['/root/bomb416', '/root/datalab', '/root/.cache', '/root/.config',
    '/root/.vscode-server', '/root/.bash_history', '/root/.local', '/root/.old-business', '/opt/myapp', '/srv/myapp', '/var/www/myapp'])
def test_business_and_development_paths_can_be_selected(path):
    assert remote.allowed_location(path)


def context(items=(), containers=()):
    return dict(machine='machine-a', items=list(items), containers=list(containers))


def item(kind='path', name='/root/old', identity='inode-a'):
    return dict(kind=kind, id=kind + ':' + name, name=name, identity=identity, detail='test')


def test_changed_or_fabricated_cleanup_plan_is_rejected(monkeypatch):
    a = item()
    monkeypatch.setattr(remote, 'permitted', lambda _: True)
    plan = context([a])
    for now, selected in [(context([dict(a, identity='new')]), [a['id']]),
                          (context(), [a['id']]), (context([a]), ['path:/etc']),
                          (dict(context([a]), machine='other'), [a['id']]),
                          (context([a]), [a['id'], a['id']])]:
        with pytest.raises(ValueError):
            remote.validate_selection(plan, selected, now)


def test_symlink_or_mount_replacement_blocks_cleanup(monkeypatch):
    a = item()
    monkeypatch.setattr(remote, 'permitted', lambda _: False)
    with pytest.raises(ValueError, match='禁止'):
        remote.validate_selection(context([a]), [a['id']], context([a]))


def test_unselected_container_using_directory_blocks_deletion(monkeypatch):
    a = item()
    monkeypatch.setattr(remote, 'permitted', lambda _: True)
    container = dict(Id='container-a', Mounts=[dict(Source='/root/old/data')])
    with pytest.raises(ValueError, match='容器'):
        remote.validate_selection(context([a]), [a['id']], context([a], [container]))


def test_volume_requires_all_attached_containers_selected():
    a = item('volume', 'old-data')
    container = dict(Id='container-a', Mounts=[dict(Name='old-data')])
    with pytest.raises(ValueError, match='数据卷'):
        remote.validate_selection(context([a]), [a['id']], context([a], [container]))


def test_custom_service_must_be_selected_with_its_program(monkeypatch):
    a = item()
    service = dict(item('service', 'old.service'), executable='/root/old/app')
    monkeypatch.setattr(remote, 'permitted', lambda _: True)
    with pytest.raises(ValueError, match='关联服务'):
        remote.validate_selection(context([a, service]), [a['id']], context([a, service]))


def test_empty_selection_does_not_call_a_mutation(monkeypatch):
    monkeypatch.setattr(remote, 'inventory', lambda: context())
    execute = Mock()
    monkeypatch.setattr(remote, 'command', execute)
    assert remote.cleanup(context(), []) == dict(cleaned=0)
    execute.assert_not_called()


def test_ztl_and_vendor_containers_are_protected():
    assert remote.protected_container(dict(Name='/aliyun-agent'))
    assert remote.protected_container(dict(Config=dict(Labels={
        'com.docker.compose.project.working_dir': '/opt/ztl-cloud',
        'com.docker.compose.project.config_files': '/opt/ztl-cloud/server/compose.yaml'})))
    assert not remote.protected_container(dict(Name='/my-unused-app'))


def test_input_validation_does_not_allow_shell_injection(tmp_path):
    for c in [dict(host='127.0.0.1; rm -rf /', password='x'),
              dict(host='127.0.0.1', sshPort=0, password='x'),
              dict(host='127.0.0.1', sshPort=True, password='x'),
              dict(host='127.0.0.1', username='someone', password='x'),
              dict(host='127.0.0.1', keyFile=str(tmp_path / 'absent'))]:
        with pytest.raises(ValueError):
            worker.validate_connection(dict(connection=c))
    assert worker.validate_connection(dict(connection=dict(host='::1', password='x')))['host'] == '::1'


def test_deploy_requires_detection_and_explicit_reset(monkeypatch, tmp_path):
    c = dict(host='127.0.0.1')
    current = dict(machine='a', freeBytes=2*1024**3, portBusy=False, portOwned=False)
    monkeypatch.setattr(worker, 'remote', lambda *args: current)
    run = Mock()
    monkeypatch.setattr(worker, 'run', run)
    for request in [dict(plan=dict(machine='other'), mode='preserve'),
                    dict(plan=dict(machine='a'), mode='reset', confirmReset=False)]:
        request['cleanupPolicy'] = 'dedicated'
        with pytest.raises(ValueError):
            worker.deploy(Mock(), request, c, tmp_path)
    run.assert_not_called()


def test_dedicated_cleanup_selects_all_old_resources_and_keeps_ztl_mounts(monkeypatch):
    old_path = item(name='/root/old')
    shared = item(name='/root/ztl-shared')
    old_container = item('container', 'old-container', 'old-container-id')
    old_volume = item('volume', 'old-volume')
    old_image = item('image', 'old-image', 'old-image-id')
    old_network = item('network', 'old-network', 'old-network-id')
    ztl = dict(Id='ztl-container-id', Name='/ztl-api', Image='ztl-image-id', Mounts=[dict(Source='/root/ztl-shared')],
               Config=dict(Labels={'com.docker.compose.project.working_dir': '/opt/ztl-cloud',
                    'com.docker.compose.project.config_files': '/opt/ztl-cloud/server/compose.yaml'}))
    old = dict(Id='old-container-id', Image='old-image-id', Mounts=[dict(Name='old-volume', Source='/root/old')],
               NetworkSettings=dict(Networks={'old': dict(NetworkID='old-network-id')}))
    snapshot = context([old_path, shared, old_container, old_volume, old_image, old_network], [ztl, old])
    monkeypatch.setattr(remote, 'inventory', lambda: snapshot)
    monkeypatch.setattr(remote, 'permitted', lambda _: True)
    execute = Mock(return_value=dict(cleaned=5))
    monkeypatch.setattr(remote, 'execute_cleanup', execute)
    assert remote.cleanup_dedicated(snapshot)['cleaned'] == 5
    chosen = execute.call_args.args[0]
    assert {i['id'] for i in chosen} == {i['id'] for i in snapshot['items']} - {shared['id']}
    assert all('/opt/ztl-cloud' not in i['name'] for i in chosen)


def test_dedicated_cleanup_rejects_replaced_server_before_deleting(monkeypatch):
    snapshot = dict(context(), deployment=['current'])
    monkeypatch.setattr(remote, 'inventory', lambda: snapshot)
    execute = Mock()
    monkeypatch.setattr(remote, 'execute_cleanup', execute)
    for plan in [dict(snapshot, machine='other'), dict(snapshot, deployment=['old'])]:
        with pytest.raises(ValueError, match='发生变化'):
            remote.cleanup_dedicated(plan)
    execute.assert_not_called()


def test_unconfirmed_dedicated_policy_never_reaches_server(monkeypatch, tmp_path):
    call = Mock()
    monkeypatch.setattr(worker, 'remote', call)
    with pytest.raises(ValueError, match='专用于'):
        worker.deploy(Mock(), dict(mode='preserve'), {}, tmp_path)
    call.assert_not_called()


def test_image_and_network_in_use_by_retained_container_are_not_deleted():
    for kind, container in [('image', dict(Id='keep', Image='resource-id')),
                            ('network', dict(Id='keep', NetworkSettings=dict(Networks={'net': dict(NetworkID='resource-id')})))]:
        resource = item(kind, 'old', 'resource-id')
        snapshot = context([resource], [container])
        with pytest.raises(ValueError, match='需保留的容器'):
            remote.validate_selection(snapshot, [resource['id']], snapshot)


def test_foreign_443_is_never_stopped(monkeypatch, tmp_path):
    monkeypatch.setattr(worker, 'remote', lambda *args: dict(machine='a', freeBytes=2*1024**3, portBusy=True, portOwned=False))
    run = Mock()
    monkeypatch.setattr(worker, 'run', run)
    with pytest.raises(ValueError, match='443'):
        worker.deploy(Mock(), dict(plan=dict(machine='a'), mode='preserve', cleanupPolicy='dedicated'), dict(host='127.0.0.1'), tmp_path)
    run.assert_not_called()


def test_missing_dependencies_and_payload_fail_before_ssh(monkeypatch, tmp_path):
    monkeypatch.setattr(worker, 'PAYLOAD', tmp_path)
    with pytest.raises(ValueError, match='资源缺失'):
        worker.payload_files()


class MemorySFTP:
    def __init__(self):
        self.files = {'/opt/ztl-cloud/server/.env': b'ZTL_TOKEN=existing-token\nZTL_PORT=443\nZTL_PYTHON_IMAGE=mirror/python:3.12-slim\n',
                      '/opt/ztl-cloud/server/certs/ca.crt': b'test-ca'}
    def open(self, path, mode='r'):
        if mode in ('r', 'rb'):
            if path not in self.files:
                raise FileNotFoundError(path)
            return io.BytesIO(self.files[path])
        owner = self
        class Writer(io.StringIO):
            def close(self):
                owner.files[path] = self.getvalue().encode()
                super().close()
        return Writer()
    def put(self, source, destination):
        self.files[destination] = Path(source).read_bytes()
    def posix_rename(self, source, destination):
        self.files[destination] = self.files.pop(source)
    def chmod(self, *args):
        pass
    def close(self):
        pass


@pytest.mark.parametrize('cleanup_policy', ['dedicated', 'none'])
@pytest.mark.parametrize('compose_kind', ['plugin', 'legacy'])
def test_preserve_deploy_reuses_token_ca_mirror_and_never_removes_data(monkeypatch, tmp_path, cleanup_policy, compose_kind):
    import httpx
    sftp = MemorySFTP()
    client = Mock()
    client.open_sftp.return_value = sftp
    snapshot = dict(machine='a', freeBytes=2*1024**3, portBusy=True, portOwned=True, existing=True)
    remote_calls = Mock(side_effect=lambda *args: snapshot)
    monkeypatch.setattr(worker, 'remote', remote_calls)
    commands = []
    def run(client, command, *args, **kwargs):
        commands.append(command)
        if 'echo plugin' in command:
            return compose_kind+'\n'
        if "docker image inspect --format '{{.Id}}'" in command:
            return 'sha256:' + 'a'*64 + '\nsha256:' + 'b'*64 + '\n'
        return ''
    monkeypatch.setattr(worker, 'run', run)
    monkeypatch.setattr(worker, 'prepare_certificates', lambda *args: None)
    class HTTP:
        def __init__(self, **kwargs):
            pass
        def __enter__(self):
            return self
        def __exit__(self, *args):
            pass
        def get(self, url, headers):
            assert headers['Authorization'] == 'Bearer existing-token'
            return httpx.Response(200, json=dict(protocol=1))
    monkeypatch.setattr(httpx, 'Client', HTTP)
    result = worker.deploy(client, dict(plan=dict(machine='a'), mode='preserve', cleanupPolicy=cleanup_policy), dict(host='127.0.0.1'), tmp_path)
    assert result['token'] == 'existing-token'
    assert result['ca'] == 'test-ca'
    assert any(call.args[1]['action'] == 'clean' for call in remote_calls.call_args_list) == (cleanup_policy == 'dedicated')
    assert any('docker run --rm -i' in c for c in commands)  # Dependency preflight applies to every update.
    assert b'ZTL_PYTHON_IMAGE=mirror/python:3.12-slim' in sftp.files['/opt/ztl-cloud/server/.env']
    assert not any('rm -rf' in c or ' down' in c or 'prune' in c for c in commands)
    assert any('build' in c for c in commands)
    assert next(i for i,c in enumerate(commands) if c.endswith('build')) < next(i for i,c in enumerate(commands) if 'up -d' in c)
    # The server-owned switch script performs and recovers container recreation.
    assert any('release_switch' in c or 'Crash-recoverable program switch' in c for c in commands)
    assert not any('--volumes' in c or re.search(r'\brm\s+[^;&]*\s-v(?:\s|$)', c) for c in commands)
    bash = Path('C:/Program Files/Git/bin/bash.exe')
    if bash.exists():
        for command in commands:
            checked = subprocess.run([str(bash), '-n', '-c', command], capture_output=True)
            assert checked.returncode == 0, checked.stderr


def test_certificate_commands_are_syntactically_valid_and_keep_existing_ca(monkeypatch):
    commands = []
    monkeypatch.setattr(worker, 'run', lambda client, command, **kwargs: commands.append(command))
    worker.prepare_certificates(Mock(), '2001:db8::1')
    assert '[ ! -f ca.crt ] && [ ! -f ca.key ]' in commands[0]
    assert '-checkip 2001:db8::1' in commands[0]
    assert '-CA ca.crt -CAkey ca.key' in commands[0]
    bash = Path('C:/Program Files/Git/bin/bash.exe')
    if bash.exists():
        assert subprocess.run([str(bash), '-n', '-c', commands[0]], capture_output=True).returncode == 0


def test_invalid_image_source_is_rejected_before_mutation(monkeypatch, tmp_path):
    execute = Mock()
    monkeypatch.setattr(worker, 'remote', execute)
    with pytest.raises(ValueError, match='镜像源'):
        worker.deploy(Mock(), dict(imagePrefix='bad; rm -rf /', cleanupPolicy='dedicated'), {}, tmp_path)
    execute.assert_not_called()


def test_changed_deployment_is_not_reset(monkeypatch, tmp_path):
    monkeypatch.setattr(worker, 'remote', lambda *args: dict(machine='a', deployment=['new'], freeBytes=2*1024**3))
    run = Mock()
    monkeypatch.setattr(worker, 'run', run)
    with pytest.raises(ValueError, match='部署'):
        worker.deploy(Mock(), dict(plan=dict(machine='a', deployment=['old']), mode='reset', confirmReset=True, cleanupPolicy='dedicated'), {}, tmp_path)
    run.assert_not_called()


def test_known_host_changes_are_rejected_before_authentication(monkeypatch, tmp_path):
    import paramiko
    key = paramiko.RSAKey.generate(2048)
    worker.atomic_json(tmp_path / 'bootstrap/hosts.json', {'127.0.0.1:22': 'SHA256:old'})
    class Client:
        def set_missing_host_key_policy(self, policy):
            self.policy = policy
        def connect(self, *args, **kwargs):
            self.policy.missing_host_key(self, '127.0.0.1', key)
        def close(self):
            pass
    monkeypatch.setattr(paramiko, 'SSHClient', Client)
    with pytest.raises(ValueError, match='指纹'):
        worker.ssh_connect(dict(host='127.0.0.1', sshPort=22, password='test'), tmp_path)


def test_cached_mirror_images_work_without_contacting_docker_hub(monkeypatch):
    commands = []
    def run(client, command, data=None, **kwargs):
        commands.append((command, data))
        if 'image ls' in command:
            return 'mirror.example/library/python:3.12-slim\nmirror.example/library/nginx:1.28-alpine\n'
        return ''
    monkeypatch.setattr(worker, 'run', run)
    images = worker.preflight_images(Mock())
    assert images == dict(ZTL_PYTHON_IMAGE='mirror.example/library/python:3.12-slim', ZTL_NGINX_IMAGE='mirror.example/library/nginx:1.28-alpine')
    assert not any('docker pull' in command for command, _ in commands)
    probe, requirements = commands[-1]
    assert 'docker run --rm -i' in probe and 'trap ' in probe and 'timeout --signal=TERM' in probe
    assert '--entrypoint python mirror.example/library/python:3.12-slim' in probe
    assert requirements == (ROOT / 'server/requirements.txt').read_text(encoding='utf-8')
    bash = Path('C:/Program Files/Git/bin/bash.exe')
    if bash.exists():
        assert subprocess.run([str(bash), '-n', '-c', probe], capture_output=True).returncode == 0


def test_preflight_images_and_their_parents_survive_automatic_cleanup(monkeypatch):
    parent = dict(item('image', 'custom-base', 'sha256:' + 'a'*64), parent='')
    chosen = dict(item('image', 'custom-runtime', 'sha256:' + 'b'*64), parent=parent['identity'])
    old = dict(item('image', 'old-runtime', 'sha256:' + 'c'*64), parent='')
    snapshot = context([parent, chosen, old])
    monkeypatch.setattr(remote, 'inventory', lambda: copy.deepcopy(snapshot))
    execute = Mock(return_value=dict(cleaned=1))
    monkeypatch.setattr(remote, 'execute_cleanup', execute)
    assert remote.cleanup_dedicated(snapshot, [chosen['identity']]) == dict(cleaned=1)
    assert execute.call_args.args[0] == [old]
    with pytest.raises(ValueError, match='镜像身份无效'):
        remote.cleanup_dedicated(snapshot, ['/etc'])


def test_explicit_mirror_overrides_other_cached_images(monkeypatch):
    commands = []
    def run(client, command, *args, **kwargs):
        commands.append(command)
        return 'python:3.12-slim\nnginx:1.28-alpine\n' if 'image ls' in command else ''
    monkeypatch.setattr(worker, 'run', run)
    images = worker.preflight_images(Mock(), 'chosen.example/library', {'ZTL_PYTHON_IMAGE': 'old/python:3.12-slim'})
    assert images['ZTL_PYTHON_IMAGE'] == 'chosen.example/library/python:3.12-slim'
    assert 'docker pull chosen.example/library/python:3.12-slim' in commands
    assert 'docker pull chosen.example/library/nginx:1.28-alpine' in commands


@pytest.mark.parametrize('failure', ['pull', 'dependencies'])
def test_failed_preflight_never_cleans_or_resets_existing_business(monkeypatch, tmp_path, failure):
    snapshot = dict(machine='a', freeBytes=2*1024**3, portBusy=False, portOwned=False, existing=True)
    remote_calls = Mock(return_value=snapshot)
    monkeypatch.setattr(worker, 'remote', remote_calls)
    sftp = MemorySFTP()
    client = Mock()
    client.open_sftp.return_value = sftp
    commands = []
    def run(client, command, *args, **kwargs):
        commands.append(command)
        if failure == 'pull' and 'docker pull ' in command:
            raise ValueError('下载镜像失败：registry-1.docker.io i/o timeout')
        if failure == 'dependencies' and 'docker run ' in command:
            raise ValueError('依赖下载失败：pypi.org timeout')
        return ''
    monkeypatch.setattr(worker, 'run', run)
    with pytest.raises(ValueError, match='尚未清理旧业务'):
        worker.deploy(client, dict(plan=dict(machine='a'), mode='reset', confirmReset=True, cleanupPolicy='dedicated'),
                      dict(host='127.0.0.1'), tmp_path)
    assert all(call.args[1]['action'] == 'scan' for call in remote_calls.call_args_list)
    assert not any('rm -rf' in command or ' down' in command for command in commands)
    assert sftp.files['/opt/ztl-cloud/server/.env'].startswith(b'ZTL_TOKEN=existing-token')


def test_error_diagnostics_retain_network_failure_and_hide_credentials(monkeypatch):
    output = (b'\x1b[31mfailed to fetch https://registry-1.docker.io/v2/: i/o timeout\x1b[0m\n'
              b'password=ssh-secret\nZTL_TOKEN=private-token\nAuthorization: Bearer abc\n'
              b'https://user:pass@example.com/package\nraw-secret\n')
    channel = Mock()
    channel.exit_status_ready.return_value = True
    channel.recv_ready.side_effect = [True, True, False]
    channel.recv_stderr_ready.return_value = False
    channel.recv.return_value = output
    channel.recv_exit_status.return_value = 1
    client = Mock()
    client.exec_command.return_value = (Mock(), Mock(channel=channel), Mock())
    monkeypatch.setattr(worker.time, 'sleep', lambda _: None)
    with pytest.raises(ValueError) as caught:
        worker.run(client, 'docker pull python:3.12-slim', step='下载 Python 镜像', redactions=('raw-secret',))
    message = str(caught.value)
    assert '下载 Python 镜像执行失败' in message and 'i/o timeout' in message
    for secret in ('ssh-secret', 'private-token', 'Bearer abc', 'user:pass', 'raw-secret', '\x1b'):
        assert secret not in message
    assert len(worker.error_summary('x' * 20000)) <= 1600


@pytest.mark.parametrize('legacy', [False, True])
def test_generated_and_repaired_certificates_pass_packaged_python_strict_tls(monkeypatch, tmp_path, legacy):
    from cryptography import x509
    from cryptography.hazmat.primitives import serialization
    from http.server import BaseHTTPRequestHandler, HTTPServer
    import ssl
    import threading
    bash = Path('C:/Program Files/Git/bin/bash.exe')
    runtime = ROOT / 'build/ZTL-ServerManager/runtime/python.exe'
    if not bash.exists() or not runtime.exists():
        pytest.skip('Windows packaged runtime integration')
    environment = dict(os.environ, MSYS_NO_PATHCONV='1')
    def shell(command):
        checked = subprocess.run([str(bash), '-c', command], env=environment, cwd=tmp_path, capture_output=True, timeout=40)
        assert checked.returncode == 0, checked.stderr.decode(errors='replace')
    if legacy:
        shell("openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -days 3650 "
              "-subj '/CN=Original Personal CA' -addext 'basicConstraints=critical,CA:TRUE'")
        old_key = (tmp_path / 'ca.key').read_bytes()
        old_cert = x509.load_pem_x509_certificate((tmp_path / 'ca.crt').read_bytes())
    def run(client, command, **kwargs):
        shell(command.replace('cd /opt/ztl-cloud/server/certs', 'cd ' + shlex.quote(tmp_path.as_posix())))
    monkeypatch.setattr(worker, 'run', run)
    worker.prepare_certificates(Mock(), '127.0.0.1')
    cert = x509.load_pem_x509_certificate((tmp_path / 'ca.crt').read_bytes())
    usage = cert.extensions.get_extension_for_class(x509.KeyUsage)
    assert usage.critical and usage.value.key_cert_sign and usage.value.crl_sign
    if legacy:
        assert (tmp_path / 'ca.key').read_bytes() == old_key
        assert cert.subject == old_cert.subject
        assert cert.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == old_cert.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    stable = {name: (tmp_path / name).read_bytes() for name in ('ca.crt', 'ca.key', 'server.crt', 'server.key')}
    worker.prepare_certificates(Mock(), '127.0.0.1')
    assert stable == {name: (tmp_path / name).read_bytes() for name in stable}
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'{"protocol":1}')
        def log_message(self, *args):
            pass
    server = HTTPServer(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(tmp_path / 'server.crt', tmp_path / 'server.key')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        code = ("import httpx,ssl,sys; c=ssl.create_default_context(cafile=sys.argv[1]); "
                "c.verify_flags |= ssl.VERIFY_X509_STRICT; "
                "r=httpx.get(sys.argv[2],verify=c,trust_env=False,timeout=5); "
                "assert r.status_code==200 and r.json()['protocol']==1")
        result = subprocess.run([str(runtime), '-B', '-c', code, str(tmp_path / 'ca.crt'),
                                 'https://127.0.0.1:' + str(server.server_port) + '/health'], capture_output=True, timeout=15)
        assert result.returncode == 0, result.stderr.decode(errors='replace')
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)


@pytest.mark.parametrize('error', ['certificate', 'token', 'unavailable'])
def test_health_check_reports_specific_tls_auth_and_http_errors(monkeypatch, tmp_path, error):
    import httpx
    calls = []
    class HTTP:
        def __init__(self, **kwargs):
            assert kwargs['verify'] == str(tmp_path / 'ca.crt')
            assert kwargs['trust_env'] is False
        def __enter__(self):
            return self
        def __exit__(self, *args):
            pass
        def get(self, *args, **kwargs):
            calls.append(1)
            if error == 'certificate':
                raise httpx.ConnectError('[SSL: CERTIFICATE_VERIFY_FAILED] CA cert does not include key usage extension')
            return httpx.Response(401 if error == 'token' else 502)
    monkeypatch.setattr(httpx, 'Client', HTTP)
    monkeypatch.setattr(worker.time, 'sleep', lambda _: None)
    expected = {'certificate': 'key usage extension', 'token': '访问令牌校验失败', 'unavailable': 'HTTP 502'}
    with pytest.raises(ValueError, match=expected[error]):
        worker.verify_health('127.0.0.1', 'private-token', tmp_path / 'ca.crt')
    assert len(calls) == (8 if error == 'unavailable' else 1)
@pytest.mark.parametrize('directory,config', [('/opt/ztl-cloud','server/compose.yaml'),
    ('/opt/ztl-cloud/server','server/compose.yaml'),('/opt/ztl-cloud/server','./compose.yaml')])
@pytest.mark.parametrize('service', ['api','https'])
def test_legacy_relative_compose_identity_requires_actual_mounts(directory, config, service):
    labels = {'com.docker.compose.project.working_dir': directory,
              'com.docker.compose.project.config_files': config,
              'com.docker.compose.service': service}
    mounts = [dict(Type='bind',Source='/opt/ztl-cloud/server/data',Destination='/data')] if service == 'api' else [
        dict(Type='bind',Source='/opt/ztl-cloud/server/certs',Destination='/certs'),
        dict(Type='bind',Source='/opt/ztl-cloud/server/nginx.conf',Destination='/etc/nginx/conf.d/default.conf')]
    container = dict(Config=dict(Labels=labels),Mounts=mounts)
    assert remote.is_ztl(container)
    container['Mounts'] = [dict(Type='bind',Source='/root/other-app',Destination='/data')]
    assert not remote.is_ztl(container)
