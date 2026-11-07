"""Desktop deployment worker. One stdin request; JSON Lines events on stdout.

Never accepts credentials in command arguments and never contacts a server on self-check.
"""
import base64
import hashlib
import ipaddress
import json
from pathlib import Path
import re
import secrets
import shlex
import socket
import sys
import time

# Embedded Python's _pth isolation omits the executed script's directory.
# Resolve only this bundled worker's sibling modules, independent of cwd/PYTHONPATH.
sys.path.insert(0, str(Path(__file__).resolve().parent))

ROOT = Path(__file__).resolve().parents[1]
REMOTE = Path(__file__).with_name('bootstrap_remote.py')
PAYLOAD = ROOT / 'unselected-release'


def event(kind, **values):
    print(json.dumps(dict(version=1, event=kind, **values), ensure_ascii=False), flush=True)


def progress(message):
    event('progress', message=message)


def payload_files():
    from release_bundle import FILES
    names = sorted(FILES)
    for name in names:
        if not (PAYLOAD / name).is_file():
            raise ValueError('部署资源缺失，请使用完整安装包：' + name)
    return names


def validate_connection(request):
    c = request.get('connection', {})
    host = str(c.get('host', '')).strip()
    try:
        ipaddress.ip_address(host)
    except ValueError:
        raise ValueError('自动部署请填写服务器 IPv4 或 IPv6 地址') from None
    port = c.get('sshPort', 22)
    if type(port) is not int or not 1 <= port <= 65535:
        raise ValueError('SSH 端口无效')
    if c.get('username', 'root') != 'root':
        raise ValueError('自动部署需要 root 登录')
    if not c.get('password') and not c.get('keyFile'):
        raise ValueError('请填写 SSH 密码或选择私钥')
    if c.get('keyFile') and not Path(c['keyFile']).is_file():
        raise ValueError('所选 SSH 私钥不存在')
    return dict(c, host=host, sshPort=port)


def fingerprint(key):
    return 'SHA256:' + base64.b64encode(hashlib.sha256(key.asbytes()).digest()).decode().rstrip('=')


def ssh_connect(c, data_dir, expected=''):
    import paramiko
    known_path = data_dir / 'bootstrap' / 'hosts.json'
    known = json.loads(known_path.read_text()) if known_path.exists() else {}
    identity = c['host'] + ':' + str(c['sshPort'])
    seen = []

    class Verify(paramiko.MissingHostKeyPolicy):
        def missing_host_key(self, client, hostname, key):
            value = fingerprint(key)
            if (known.get(identity) and known[identity] != value) or (expected and value != expected):
                raise ValueError('SSH 主机指纹发生变化，请核对服务器身份')
            seen.append(value)

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(Verify())
    try:
        client.connect(c['host'], port=c['sshPort'], username='root', password=c.get('password') or None,
                       key_filename=c.get('keyFile') or None, passphrase=c.get('passphrase') or None,
                       timeout=20, auth_timeout=25, banner_timeout=25, look_for_keys=False, allow_agent=False)
    except Exception:
        client.close()
        raise
    client.get_transport().set_keepalive(20)
    return client, seen[0], known, known_path, identity


def error_summary(value, redactions=(), lines=12, limit=1600):
    value = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', value)
    for secret in sorted((s for s in redactions if s), key=len, reverse=True):
        value = value.replace(secret, '[已隐藏]')
    value = re.sub(r'-----BEGIN [^-]*PRIVATE KEY-----.*?-----END [^-]*PRIVATE KEY-----',
                   '[私钥已隐藏]', value, flags=re.S)
    value = re.sub(r'(https?://)[^\s/@]+:[^\s/@]+@', r'\1[已隐藏]@', value)
    value = re.sub(r'(?im)(authorization\s*[:=]\s*).+$', r'\1[已隐藏]', value)
    value = re.sub(r'(?im)([\w-]*(?:token|password|passphrase|secret|api[_-]?key)[\w-]*\s*[:=]\s*).+$',
                   r'\1[已隐藏]', value)
    rows = [line.strip() for line in value.splitlines() if line.strip()]
    return '\n'.join(rows[-lines:])[-limit:]


def channel_run(client, command, data=None, timeout=120, step='', redactions=()):
    stdin, out, err = client.exec_command(command, timeout=30)
    if data is not None:
        stdin.write(data)
        stdin.flush()
    stdin.channel.shutdown_write()
    channel = out.channel
    output, errors = bytearray(), bytearray()
    deadline = time.monotonic() + timeout
    while not channel.exit_status_ready() or channel.recv_ready() or channel.recv_stderr_ready():
        if time.monotonic() > deadline:
            channel.close()
            detail = error_summary((errors + output).decode('utf-8', errors='replace'), redactions) if step else ''
            raise ValueError((step or '服务器操作') + '超时；请检查网络与镜像源后重新检测' + ('\n' + detail if detail else ''))
        if channel.recv_ready():
            output.extend(channel.recv(65536))
        if channel.recv_stderr_ready():
            errors.extend(channel.recv_stderr(65536))
        # Retain bounded tails. Only explicitly labelled deployment commands expose sanitized diagnostics.
        if len(output) > 2 * 1024 * 1024:
            del output[:-1024 * 1024]
        if len(errors) > 2 * 1024 * 1024:
            del errors[:-1024 * 1024]
        time.sleep(.03)
    code = channel.recv_exit_status()
    if code:
        detail = error_summary((output + errors).decode('utf-8', errors='replace'), redactions) if step else ''
        raise ValueError(f'{step or "服务器步骤"}执行失败（退出码 {code}）' + ('\n' + detail if detail else '；请检查网络、软件源或对应服务后重新检测'))
    return output.decode('utf-8', errors='replace')


def operation(client, request):
    source = Path(__file__).with_name('remote_operation.py').read_text(encoding='utf-8-sig')
    if request['action'] == 'start':
        request = dict(request, source=source)
    return json.loads(channel_run(client, 'python3 -c ' + shlex.quote(source), json.dumps(request), timeout=45))


def run(client, command, data=None, timeout=120, step='', redactions=()):
    if timeout < 180:
        return channel_run(client, command, data, timeout, step, redactions)
    label = step or '服务器维护步骤'
    identity = operation(client, dict(action='start', command=command, data=data, timeout=timeout,
        step=label, cancellable=any(word in label for word in ('预检', '构建', '下载镜像'))))['id']
    deadline = time.monotonic() + timeout + 40
    next_heartbeat = time.monotonic() + 10
    while time.monotonic() < deadline:
        state = operation(client, dict(action='status', id=identity, output=True))
        if state.get('state') in ('succeeded', 'failed'):
            output = state.get('output', '')
            if state['state'] != 'succeeded':
                raise ValueError(label + '失败；远端操作 ' + identity + '\n' + error_summary(output, redactions))
            return output
        if time.monotonic() >= next_heartbeat:
            age = max(0, int(time.time() - state.get('heartbeat', time.time())))
            progress(label + '仍在执行（操作 ' + identity[:8] + '，最近心跳 ' + str(age) + ' 秒前）')
            detail = error_summary(state.get('output', ''), redactions, lines=1, limit=300)
            if detail:
                progress(detail)
            next_heartbeat += 10
        time.sleep(1)
    raise ValueError(label + '等待超时；远端操作 ' + identity + '仍受服务器锁保护，请重新检测状态，勿重复启动')


def remote(client, request):
    source = REMOTE.read_text(encoding='utf-8')
    # Read-only inventory must remain available while a detached mutation holds the lock.
    result = run(client, 'python3 -c ' + shlex.quote(source), json.dumps(request), timeout=120 if request.get('action') == 'scan' else 180)
    value = json.loads(result)
    if value.get('error'):
        raise ValueError(value['error'])
    return value


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value), encoding='utf-8')
    temporary.replace(path)


def acquire_lock(client):
    stdin, out, err = client.exec_command(
        "flock -n /run/lock/ztl-bootstrap.lock sh -c 'flock -n /run/lock/ztl-command.lock true || exit 75; echo ZTL_LOCKED; cat >/dev/null'", timeout=15)
    if out.readline().strip() != 'ZTL_LOCKED':
        raise ValueError('另一个部署正在执行，请稍后重新检测')
    return stdin


def compose(client, project='ztl-cloud'):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,100}', project):
        raise ValueError('已有容器项目名称无效，请先人工整理')
    if run(client, 'if docker compose version >/dev/null 2>&1; then echo plugin; '
                   'elif command -v docker-compose >/dev/null; then echo legacy; else echo missing; fi').strip() == 'plugin':
        binary = 'docker compose'
    else:
        binary = 'docker-compose'
    return 'cd /opt/ztl-cloud && ' + binary + ' -p ' + shlex.quote(project) + ' --env-file server/.env -f server/compose.yaml '


def read_env(sftp):
    try:
        with sftp.open('/opt/ztl-cloud/server/.env') as f:
            return f.read().decode('utf-8')
    except FileNotFoundError:
        return ''


def env_values(value):
    return dict(line.split('=', 1) for line in value.splitlines() if '=' in line and not line.startswith('#'))


def preflight_images(client, prefix='', values=None, redactions=(), check_dependencies=True):
    """Resolve and obtain both images, then verify Python dependencies before any business cleanup."""
    values = values or {}
    cached = set(run(client, "docker image ls --format '{{.Repository}}:{{.Tag}}'", step='检查镜像缓存', redactions=redactions).splitlines())
    images = {}
    for key, name in [('ZTL_PYTHON_IMAGE', 'python:3.12-slim'), ('ZTL_NGINX_IMAGE', 'nginx:1.28-alpine')]:
        configured = values.get(key, '').strip().strip('\"\'')
        if prefix:
            image = prefix + '/' + name
        elif configured:
            image = configured
        else:
            candidates = sorted(i for i in cached if i == name or i.endswith('/' + name))
            image = name if name in cached else next(iter(candidates), name)
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._:/-]{0,300}', image) or '..' in image or '://' in image:
            raise ValueError('服务器中保存的镜像名称无效，请在连接页填写镜像源前缀后重试')
        if image in cached:
            progress('复用服务器缓存镜像：' + image)
        else:
            progress('预检并下载镜像：' + image)
            try:
                run(client, 'docker pull ' + shlex.quote(image), timeout=300,
                    step='下载镜像 ' + image, redactions=redactions)
            except ValueError as error:
                raise ValueError(str(error) + '\n尚未清理旧业务。请在连接页填写可用的容器镜像源前缀，再重新检测部署。') from None
        images[key] = image
    if not check_dependencies:
        progress('日常更新由 Docker 构建校验依赖；构建失败时保留原服务')
        return images
    progress('预检 Python 依赖下载（在临时容器内，不改动现有服务）')
    code = ("import pathlib,subprocess,sys,tempfile; "
            "d=tempfile.TemporaryDirectory(); p=pathlib.Path(d.name); "
            "r=p/'requirements.txt'; r.write_text(sys.stdin.read()); "
            "sys.exit(subprocess.call([sys.executable,'-m','pip','download',"
            "'--disable-pip-version-check','--no-cache-dir','--timeout','15','--retries','1',"
            "'-r',str(r),'-d',str(p/'wheels')]))")
    # The container is stopped and removed even if the SSH channel times out/disconnects.
    container = 'ztl-preflight-' + secrets.token_hex(8)
    command = ('trap ' + shlex.quote('docker rm -f ' + container + ' >/dev/null 2>&1 || true') +
               ' EXIT HUP INT TERM; timeout --signal=TERM --kill-after=15s 570s docker run --rm -i --name ' + container + ' --entrypoint python ' +
               shlex.quote(images['ZTL_PYTHON_IMAGE']) + ' -c ' + shlex.quote(code))
    try:
        run(client, command, (PAYLOAD / 'server/requirements.txt').read_text(encoding='utf-8'),
            timeout=600, step='预检 Python 依赖下载', redactions=redactions)
    except ValueError as error:
        raise ValueError(str(error) + '\n尚未清理旧业务。请检查服务器访问 PyPI 及文件下载站点的网络后重试。') from None
    return images


def prepare_certificates(client, host):
    # Preserve existing CA. Reissue the leaf only if expired, mismatched, or missing.
    ip = shlex.quote(host)
    run(client, "set -eu; umask 077; cd /opt/ztl-cloud/server/certs; "
        "if [ ! -f ca.crt ] && [ ! -f ca.key ]; then "
        "openssl req -x509 -newkey rsa:3072 -nodes -keyout ca.key -out ca.crt -days 3650 "
        "-subj '/CN=ZTL Personal Cloud CA' -addext 'basicConstraints=critical,CA:TRUE' "
        "-addext 'keyUsage=critical,keyCertSign,cRLSign' -addext 'subjectKeyIdentifier=hash' 2>/dev/null; fi; "
        "test -f ca.crt && test -f ca.key; openssl x509 -in ca.crt -checkend 86400 -noout >/dev/null; "
        "test \"$(openssl x509 -in ca.crt -pubkey -noout | sha256sum)\" = "
        "\"$(openssl pkey -in ca.key -pubout | sha256sum)\"; "
        "if ! openssl x509 -in ca.crt -noout -ext keyUsage | grep -q 'X509v3 Key Usage: critical' || "
        "! openssl x509 -in ca.crt -noout -ext keyUsage | grep -q 'Certificate Sign'; then "
        "printf '%s\\n' 'basicConstraints=critical,CA:TRUE' 'keyUsage=critical,keyCertSign,cRLSign' "
        "'subjectKeyIdentifier=hash' 'authorityKeyIdentifier=keyid:always' > ca.ext; "
        "openssl x509 -in ca.crt -signkey ca.key -days 3650 -clrext -extfile ca.ext -out ca.crt.bootstrap-tmp; "
        "openssl verify -x509_strict -check_ss_sig -CAfile ca.crt.bootstrap-tmp ca.crt.bootstrap-tmp; "
        "mv -- ca.crt.bootstrap-tmp ca.crt; fi; "
        "if [ ! -f server.crt ] || [ ! -f server.key ] || "
        "! openssl x509 -in server.crt -checkend 86400 -noout >/dev/null || "
        f"! openssl x509 -in server.crt -checkip {ip} -noout >/dev/null || "
        "! openssl verify -x509_strict -purpose sslserver -CAfile ca.crt server.crt >/dev/null 2>&1 || "
        "! openssl x509 -in server.crt -noout -ext keyUsage | grep -q 'Digital Signature' || "
        "[ \"$(openssl x509 -in server.crt -pubkey -noout | sha256sum)\" != "
        "\"$(openssl pkey -in server.key -pubout 2>/dev/null | sha256sum)\" ]; then "
        f"openssl req -newkey rsa:3072 -nodes -keyout server.key.bootstrap-tmp -out server.csr -subj '/CN={host}' 2>/dev/null; "
        f"printf '%s\\n' 'subjectAltName=IP:{host}' 'basicConstraints=critical,CA:FALSE' "
        "'keyUsage=critical,digitalSignature,keyEncipherment' 'extendedKeyUsage=serverAuth' "
        "'subjectKeyIdentifier=hash' 'authorityKeyIdentifier=keyid:always,issuer' > leaf.ext; "
        "openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt.bootstrap-tmp "
        "-days 825 -extfile leaf.ext 2>/dev/null; "
        f"openssl verify -x509_strict -purpose sslserver -verify_ip {ip} -CAfile ca.crt server.crt.bootstrap-tmp; "
        "mv -- server.key.bootstrap-tmp server.key; mv -- server.crt.bootstrap-tmp server.crt; fi; chmod 600 *.key",
        step='校验或修复 HTTPS 证书')


def verify_health(host, token, ca_path):
    import httpx
    authority = '[' + host + ']' if ':' in host else host
    last_error = ''
    with httpx.Client(verify=str(ca_path), timeout=10, trust_env=False) as http:
        for attempt in range(8):
            try:
                response = http.get('https://' + authority + '/health', headers={'Authorization': 'Bearer ' + token})
                if response.status_code == 200 and response.json().get('protocol') == 1:
                    return response.json()
                if response.status_code == 401:
                    raise ValueError('服务器 HTTPS 已连通，但访问令牌校验失败；请使用保留数据模式重新检测部署')
                last_error = '健康接口返回 HTTP ' + str(response.status_code) + '，或协议不匹配'
            except httpx.HTTPError as error:
                last_error = error_summary(str(error), (token,))
                if 'CERTIFICATE_VERIFY_FAILED' in last_error:
                    raise ValueError('HTTPS 证书校验失败；请使用保留数据模式重新检测部署以修复证书\n' + last_error) from None
            except (json.JSONDecodeError, AttributeError):
                last_error = '健康接口响应不是有效的 Z.T.L 协议数据'
            if attempt < 7:
                time.sleep(3)
    raise ValueError('服务器已执行启动，但本机 HTTPS 健康验证失败；请检查 443 端口、服务状态及网络\n' + last_error)


def deploy(client, request, c, data_dir):
    payload_files()
    if request.get('cleanupPolicy') not in ('dedicated', 'none'):
        raise ValueError('请确认服务器专用于 Z.T.L 后再开始部署')
    prefix = request.get('imagePrefix', '').strip().rstrip('/')
    if prefix and (not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._:/-]{0,240}', prefix) or '..' in prefix or '://' in prefix):
        raise ValueError('镜像源前缀格式无效，请填写 registry.example.com/namespace，不含协议')
    plan = request.get('plan', {})
    current = remote(client, dict(action='scan'))
    if plan.get('machine') != current['machine']:
        raise ValueError('服务器身份已变化，请重新检测')
    if plan.get('deployment') != current.get('deployment'):
        raise ValueError('已有部署在检测后发生变化，请重新检测后确认')
    if current['portBusy'] and not current['portOwned'] and not current.get('portCleanup'):
        raise ValueError('443 被其他服务占用，请处理后重新检测')
    if request.get('mode') not in ('preserve', 'reset'):
        raise ValueError('请选择保留数据部署或清除后重建')
    if request.get('mode') == 'reset' and request.get('confirmReset') is not True:
        raise ValueError('清除旧 Z.T.L 部署需要明确确认')
    if request.get('mode') == 'reset' and request.get('cleanupPolicy') != 'dedicated':
        raise ValueError('重建必须在首次部署／清理入口明确确认')
    progress('检查部署目录及软件运行环境')
    # Refuse symlinked installation roots and nested mount points before any writes/reset.
    run(client, "set -eu; test ! -L /opt/ztl-cloud; "
        "if [ -d /opt/ztl-cloud ]; then "
        "test \"$(readlink -f /opt/ztl-cloud)\" = /opt/ztl-cloud; "
        "! findmnt -rn -o TARGET | grep -E '^/opt/ztl-cloud(/|$)'; "
        "! find /opt/ztl-cloud -type l -print -quit | grep .; fi")
    redactions = tuple(c.get(k, '') for k in ('password', 'passphrase'))
    progress('安装并检查运行环境（此阶段不清理旧业务）')
    run(client, "set -eu; export DEBIAN_FRONTEND=noninteractive; if ! command -v openssl >/dev/null || ! command -v python3 >/dev/null || ! command -v docker >/dev/null || ! command -v cron >/dev/null || ! (docker compose version >/dev/null 2>&1 || command -v docker-compose >/dev/null); then apt-get update -qq; "
        "apt-get install -y -qq openssl python3 ca-certificates cron; "
        "if ! command -v docker >/dev/null; then apt-get install -y -qq docker.io; fi; "
        "if ! docker compose version >/dev/null 2>&1 && ! command -v docker-compose >/dev/null; then "
        ". /etc/os-release; if [ \"$VERSION_ID\" = 24.04 ]; then apt-get install -y -qq docker-compose-v2; "
        "else apt-get install -y -qq docker-compose; fi; fi; "
        "fi; systemctl enable --now docker; systemctl enable --now cron", timeout=900,
        step='安装服务器依赖', redactions=redactions)
    sftp = client.open_sftp()
    try:
        previous_env = read_env(sftp)
        previous_values = env_values(previous_env)
    finally:
        sftp.close()
    redactions += (previous_values.get('ZTL_TOKEN', ''),)
    if request['mode'] == 'preserve' and previous_env and not previous_values.get('ZTL_TOKEN'):
        raise ValueError('已有部署缺少有效令牌；保留模式不会覆盖，请先修复配置或选择清除后重建')
    images = preflight_images(client, prefix, previous_values, redactions,
                              check_dependencies=True)
    image_ids = run(client, "docker image inspect --format '{{.Id}}' " +
                    ' '.join(shlex.quote(image) for image in images.values()),
                    step='确认预检镜像', redactions=redactions).splitlines()
    if len(image_ids) != 2 or any(not re.fullmatch(r'sha256:[a-f0-9]{64}', i) for i in image_ids):
        raise ValueError('无法确认预检镜像的身份，尚未清理旧业务，请重新检测')
    if current.get('existing'):
        progress('更新前备份现有数据库')
        run(client, "if [ -f /opt/ztl-cloud/server/data/ztl.sqlite ]; then "
            "cd /opt/ztl-cloud && python3 scripts/cloud_backup.py server/data/ztl.sqlite server/backups; fi",
            step='更新前备份', redactions=redactions)
    if request.get('cleanupPolicy') == 'dedicated':
        progress('镜像与依赖预检通过，正在自动清理旧应用和数据')
        remote(client, dict(action='clean', plan=plan, policy='dedicated', keepImages=image_ids))
    else:
        progress('镜像与依赖预检通过，保留其他业务与现有 Z.T.L 数据')
    after_cleanup = remote(client, dict(action='scan'))
    if after_cleanup['freeBytes'] < 1024 ** 3:
        raise ValueError('自动清理后 /opt 所在磁盘仍不足 1 GB，请释放空间后重试')
    if after_cleanup['portBusy'] and not after_cleanup['portOwned']:
        raise ValueError('443 仍被需保留的系统服务或未识别的程序占用，请处理后重新检测')
    if after_cleanup.get('projectConflict'):
        raise ValueError('容器项目名称仍与需保留的应用冲突，请先处理后重新检测')
    refreshed = remote(client, dict(action='scan'))
    if refreshed['portBusy'] and not refreshed['portOwned']:
        raise ValueError('443 被其他服务占用，请处理后重新检测')
    if refreshed.get('projectConflict'):
        raise ValueError('容器项目名称与其他应用冲突，请先处理后重新检测')
    if current['existing'] and request['mode'] == 'reset':
        progress('正在清除已明确确认的旧 Z.T.L 部署与数据')
        # Resource ownership is verified by inventory before compose can stop anything.
        check = remote(client, dict(action='scan'))
        if check['portBusy'] and not check['portOwned']:
            raise ValueError('443 占用已变化，请重新检测')
        run(client, "set -eu; if [ -f /opt/ztl-cloud/server/compose.yaml ] && "
            "[ -f /opt/ztl-cloud/server/.env ]; then " + compose(client, current.get('composeProject', 'ztl-cloud')) + "down; fi; "
            "rm -rf -- /opt/ztl-cloud; rm -f -- /etc/cron.d/ztl-backup", timeout=180)
    run(client, "set -eu; mkdir -p /opt/ztl-cloud/server/data /opt/ztl-cloud/server/certs "
        "/opt/ztl-cloud/server/backups /opt/ztl-cloud/scripts; chmod 700 /opt/ztl-cloud/server/data "
        "/opt/ztl-cloud/server/certs /opt/ztl-cloud/server/backups")
    sftp = client.open_sftp()
    try:
        env = read_env(sftp)
        values = env_values(env)
        token = values.get('ZTL_TOKEN', '')
        if env and not token:
            raise ValueError('已有部署缺少有效令牌；保留模式不会覆盖，请先修复配置或选择清除后重建')
        if not token:
            token = secrets.token_urlsafe(48)
        project = current.get('composeProject', 'ztl-cloud') if request['mode'] == 'preserve' else 'ztl-cloud'
        values.update(ZTL_TOKEN=token, ZTL_PORT='443', COMPOSE_PROJECT_NAME=project)
        values.update(images)
        redactions += (token,)
        progress('上传并校验独立的候选发布目录，旧程序继续运行')
        digest = hashlib.sha256(b''.join(name.encode()+hashlib.sha256((PAYLOAD/name).read_bytes()).digest() for name in payload_files())).hexdigest()
        stage = '/opt/ztl-cloud/releases/' + digest
        run(client, 'mkdir -p ' + stage + '/server ' + stage + '/scripts')
        for name in payload_files():
            destination = stage + '/' + name
            sftp.put(str(PAYLOAD / name), destination + '.upload')
            sftp.posix_rename(destination + '.upload', destination)
            with sftp.open(destination, 'rb') as uploaded:
                if hashlib.sha256(uploaded.read()).digest() != hashlib.sha256((PAYLOAD/name).read_bytes()).digest():
                    raise ValueError('候选发布文件校验失败，旧服务未切换')
        values['ZTL_API_IMAGE'] = 'ztl-cloud-api:' + digest[:32]
        compose_text = (PAYLOAD / 'server/compose.yaml').read_text(encoding='utf-8')
        compose_text = compose_text.replace('    build:', '    image: ' + values['ZTL_API_IMAGE'] + '\n    build:', 1)
        compose_text = compose_text.replace('context: ..', 'context: ' + stage)
        compose_text = compose_text.replace('env_file: .env', 'env_file: ' + stage + '/server/.env')
        for name in ('data', 'certs'):
            compose_text = compose_text.replace('./' + name + ':', '/opt/ztl-cloud/server/' + name + ':')
        compose_text = compose_text.replace('./nginx.conf:', stage + '/server/nginx.conf:')
        with sftp.open(stage + '/server/compose.yaml', 'w') as output:
            output.write(compose_text)
        with sftp.open(stage + '/server/.env', 'w') as output:
            output.write('\n'.join(k + '=' + v for k, v in values.items()) + '\n')
        sftp.chmod(stage + '/server/.env', 0o600)
        prepare_certificates(client, c['host'])
        prefix_command = compose(client, project)
        candidate_command = prefix_command.replace('--env-file server/.env -f server/compose.yaml',
            '--env-file ' + stage + '/server/.env -f ' + stage + '/server/compose.yaml')
        run(client, candidate_command + 'build', timeout=1800, step='构建候选服务器镜像（旧服务仍在运行）', redactions=redactions)
        progress('候选构建完成，切换运行程序；失败时自动恢复旧服务')
        switch = Path(__file__).with_name('release_switch.py').read_text(encoding='utf-8-sig')
        switch_command = 'python3 -c ' + shlex.quote(switch)
        run(client, switch_command, json.dumps(dict(action='apply', project=project, digest=digest)), timeout=900,
            step='切换并验证服务（支持自动回退）', redactions=redactions)
        # Compatibility maintenance scripts are updated only after the program switch succeeds.
        for name in ('scripts/cloud_backup.py',):
            sftp.put(str(PAYLOAD/name), '/opt/ztl-cloud/' + name + '.upload')
            sftp.posix_rename('/opt/ztl-cloud/' + name + '.upload', '/opt/ztl-cloud/' + name)
        run(client, "set -eu; printf '%s\\n' " + shlex.quote(
            '23 3 * * * root flock -n /run/lock/ztl-bootstrap.lock flock -n /run/lock/ztl-command.lock python3 /opt/ztl-cloud/scripts/cloud_backup.py /opt/ztl-cloud/server/data/ztl.sqlite '
            '/opt/ztl-cloud/server/backups --kind daily >> /opt/ztl-cloud/server/backups/backup.log 2>&1') +
            ' > /etc/cron.d/ztl-backup; chmod 644 /etc/cron.d/ztl-backup')
        ca = sftp.open('/opt/ztl-cloud/server/certs/ca.crt').read().decode('ascii')
    finally:
        sftp.close()
    ca_path = data_dir / 'bootstrap' / 'downloaded-ca.crt'
    ca_path.parent.mkdir(parents=True, exist_ok=True)
    ca_path.write_text(ca, encoding='ascii')
    progress('从本机验证 HTTPS、访问令牌和服务器健康状态')
    try:
        verify_health(c['host'], token, ca_path)
    except Exception:
        progress('本机 HTTPS 验证失败，正在恢复上一次程序配置；业务数据库不回退')
        run(client, switch_command, json.dumps(dict(action='rollback', project=project)), timeout=600, step='恢复旧服务', redactions=redactions)
        raise
    run(client, 'cd /opt/ztl-cloud && python3 scripts/cloud_backup.py server/data/ztl.sqlite server/backups')
    return dict(host=c['host'], port=443, token=token, ca=ca)


def maintain(client, request, c, data_dir):
    action = request['action']
    snapshot = remote(client, dict(action='scan'))
    if not snapshot.get('existing'):
        raise ValueError('未检测到 Z.T.L 服务，请先选择发布包进行部署')
    run(client, "set -eu; test ! -L /opt/ztl-cloud; test ! -L /opt/ztl-cloud/server; "
        "! find /opt/ztl-cloud -type l -print -quit | grep .")
    sftp = client.open_sftp()
    try:
        values = env_values(read_env(sftp))
        token = values.get('ZTL_TOKEN', '')
        ca = sftp.open('/opt/ztl-cloud/server/certs/ca.crt').read().decode('ascii')
    finally:
        sftp.close()
    if not token:
        raise ValueError('服务器令牌缺失，请先修复部署')
    redactions = (token, c.get('password', ''), c.get('passphrase', ''))
    if action == 'export-connection':
        return dict(format=1, protocol=1, host=c['host'], port=443, token=token, ca=ca)
    command = compose(client, snapshot.get('composeProject', 'ztl-cloud'))
    if action == 'logs':
        return dict(details=error_summary(run(client, command + 'logs --no-color --tail 100',
                    step='读取服务日志', redactions=redactions), redactions, lines=100, limit=16000))
    if action == 'backup':
        result = run(client, 'cd /opt/ztl-cloud && python3 scripts/cloud_backup.py server/data/ztl.sqlite server/backups',
                     step='创建数据库备份', redactions=redactions)
        return dict(details=result.strip())
    if action == 'list-backups':
        code = "from pathlib import Path;import json;print(json.dumps([p.name for p in sorted(Path('/opt/ztl-cloud/server/backups').glob('ztl-*.sqlite'),reverse=True) if p.is_file() and not p.is_symlink()]))"
        return dict(backups=json.loads(run(client, 'python3 -c ' + shlex.quote(code))))
    if action == 'restart':
        run(client, command + 'restart', timeout=180, step='重启 Z.T.L 服务', redactions=redactions)
    elif action == 'restore':
        if request.get('confirmRestore') is not True:
            raise ValueError('恢复数据库需要明确确认')
        name = request.get('backup', '')
        if not isinstance(name, str) or not re.fullmatch(r'ztl-[0-9]{8}-[0-9]{6}-[0-9]{6}\.sqlite', name):
            raise ValueError('备份名称无效')
        # Materialize a checked restore snapshot before backup retention can remove the selected source.
        source = Path(__file__).with_name('database_restore.py').read_text(encoding='utf-8')
        restore_command = 'python3 -c ' + shlex.quote(source)
        prepared = json.loads(run(client, restore_command, json.dumps(dict(action='prepare', backup=name)),
                                  step='校验恢复备份', redactions=redactions))['prepared']
        try:
            run(client, 'cd /opt/ztl-cloud && python3 scripts/cloud_backup.py server/data/ztl.sqlite server/backups',
                step='恢复前备份当前数据', redactions=redactions)
            try:
                run(client, command + 'stop api', step='停止 API 以恢复数据', redactions=redactions)
                run(client, restore_command, json.dumps(dict(action='commit', prepared=prepared)),
                    step='恢复数据库', redactions=redactions)
            finally:
                run(client, command + 'start api', step='启动恢复后的 API', redactions=redactions)
        finally:
            try:
                run(client, restore_command, json.dumps(dict(action='discard', prepared=prepared)),
                    step='清理恢复临时文件', redactions=redactions)
            except Exception:
                progress('恢复临时文件未能清理，请稍后检查服务器 data/restore-*.sqlite')
    elif action != 'status':
        raise ValueError('未知维护操作')
    details = run(client, command + 'ps', step='读取服务状态', redactions=redactions)
    ca_path = data_dir / 'downloaded-ca.crt'
    ca_path.write_text(ca, encoding='ascii')
    try:
        health = verify_health(c['host'], token, ca_path)
        return dict(details=details, health=health)
    except ValueError as error:
        return dict(details=details, healthError=str(error))


def execute(request):
    global PAYLOAD
    if request.get('version') != 1:
        raise ValueError('初始化协议版本不兼容')
    import paramiko
    import httpx
    from release_bundle import inspect_bundle, extract_bundle, download_bundle
    if request.get('action') == 'self-check':
        if not all(Path(__file__).with_name(n).is_file() for n in ('bootstrap_remote.py', 'release_bundle.py', 'database_restore.py', 'release_switch.py', 'remote_operation.py')):
            raise ValueError('缺少服务器检测脚本')
        return dict(adapter=1)
    data_dir = Path(request['dataDir']).resolve()
    data_dir.mkdir(parents=True, exist_ok=True)
    if request.get('action') in ('inspect-release', 'download-release'):
        path = request.get('release', '')
        if request['action'] == 'download-release':
            path = download_bundle(request.get('url', ''), data_dir / 'downloads', request.get('sha256', ''))
        manifest = inspect_bundle(path)
        return dict(release=str(Path(path).resolve()), version=manifest['version'], protocol=manifest['protocol'])
    if request.get('action') not in ('scan', 'deploy', 'status', 'logs', 'restart', 'backup', 'list-backups', 'restore', 'export-connection', 'cancel-operation'):
        raise ValueError('未知部署操作')
    if request['action'] == 'deploy':
        PAYLOAD, manifest = extract_bundle(request.get('release', ''), data_dir / 'releases')
    c = validate_connection(request)
    data_dir = Path(request['dataDir']).resolve()
    expected = request.get('fingerprint', '')
    if request['action'] != 'scan' and not expected:
        raise ValueError('请先检测并确认 SSH 指纹')
    progress('正在连接 SSH 并检查服务器身份')
    client, seen, known, known_path, identity = ssh_connect(c, data_dir, expected)
    lock = None
    try:
        if request['action'] == 'scan':
            progress('正在检测系统、443 端口及可清理资源')
            result = remote(client, dict(action='scan'))
            result['fingerprint'] = seen
            result['maintenance'] = operation(client, dict(action='status'))
            return result
        known[identity] = seen
        atomic_json(known_path, known)
        if request['action'] == 'cancel-operation':
            return dict(operation=operation(client, dict(action='cancel')))
        if request['action'] == 'status':
            active = operation(client, dict(action='status'))
            if active and active.get('state') in ('running', 'queued'):
                return dict(operation=active, details='远端维护步骤仍在执行；完成前不会启动新的修改')
        lock = acquire_lock(client)
        if request['action'] == 'deploy':
            return deploy(client, request, c, data_dir)
        return maintain(client, request, c, data_dir)
    finally:
        if lock:
            lock.close()
        client.close()


def main():
    try:
        request = json.loads(sys.stdin.read(2 * 1024 * 1024))
        result = execute(request)
        event('result', data=result)
    except Exception as error:
        # Only our intentional messages may reach the UI. Library exceptions can contain request data.
        import paramiko
        if isinstance(error, paramiko.AuthenticationException):
            message = 'SSH 认证失败，请核对 root 密码、私钥和私钥口令'
        elif isinstance(error, (socket.timeout, TimeoutError)):
            message = 'SSH 连接超时，请检查地址、SSH 端口和安全组'
        elif isinstance(error, ValueError) and not isinstance(error, json.JSONDecodeError):
            message = str(error)
        elif isinstance(error, FileNotFoundError):
            message = '所需文件或服务器命令缺失，请检查完整安装包和系统环境'
        else:
            message = '初始化操作失败（' + type(error).__name__ + '），请检查网络、服务器状态和安装包后重试'
        event('error', message=message)


if __name__ == '__main__':
    main()
