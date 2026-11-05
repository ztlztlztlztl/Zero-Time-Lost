"""Deploy to a personal host. Credentials are read locally, never passed in command arguments."""
import argparse
import json
from pathlib import Path
import re
import secrets

import paramiko

ROOT = Path(__file__).resolve().parents[1]


def connect(info, username=None):
    fields = dict((k.strip(), v.strip()) for k, v in re.findall(
        r"^([^\r\n:：]+)[:：]\s*([^\r\n]+)", Path(info).read_text(encoding="utf-8-sig"), re.M))
    host, user = fields["公网 IP"], username or fields["用户名"]
    known = ROOT / "build/cloud-known-hosts"
    known.parent.mkdir(exist_ok=True)
    client = paramiko.SSHClient()
    client.load_system_host_keys()
    if known.exists():
        client.load_host_keys(str(known))
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    options = dict(hostname=host, username=user, port=int(fields.get("SSH端口", fields.get("SSH 端口", 22))), timeout=30)
    if fields.get("私钥路径"):
        options["key_filename"] = fields["私钥路径"]
    else:
        options["password"] = fields["密码"]
    client.connect(**options)
    client.save_host_keys(str(known))
    return client, host


def run(client, command, timeout=900):
    _, out, err = client.exec_command(command, timeout=timeout)
    # Drain both streams together to avoid SSH channel deadlock during image builds.
    channel = out.channel
    import time
    while not channel.exit_status_ready() or channel.recv_ready() or channel.recv_stderr_ready():
        if channel.recv_ready():
            print(channel.recv(32768).decode(errors="replace"), end="", flush=True)
        if channel.recv_stderr_ready():
            print(channel.recv_stderr(32768).decode(errors="replace"), end="", flush=True)
        time.sleep(.1)
    if channel.recv_exit_status():
        raise RuntimeError("remote command failed")


def deploy(info, username, image_prefix):
    client, host = connect(info, username)
    try:
        import ipaddress
        ipaddress.ip_address(host)
        run(client, "mkdir -p /opt/ztl-cloud/server/{data,certs,backups} /opt/ztl-cloud/scripts; chmod 700 /opt/ztl-cloud/server/{data,certs,backups}")
        sftp = client.open_sftp()
        for local in list((ROOT / "server").glob("*.py")) + [ROOT / "server" / n for n in ("requirements.txt", "Dockerfile", "compose.yaml", "nginx.conf")] + [ROOT / "scripts/mail_worker.py", ROOT / "scripts/cloud_backup.py"]:
            sftp.put(str(local), "/opt/ztl-cloud/" + local.relative_to(ROOT).as_posix())
        remote_env = "/opt/ztl-cloud/server/.env"
        try:
            with sftp.open(remote_env) as f:
                env = f.read().decode()
            token = next(line.split("=", 1)[1] for line in env.splitlines() if line.startswith("ZTL_TOKEN="))
        except FileNotFoundError:
            token = secrets.token_urlsafe(48)
            with sftp.open(remote_env, "w") as f:
                f.write("ZTL_TOKEN=" + token + "\nZTL_PORT=443\n")
            sftp.chmod(remote_env, 0o600)
        if image_prefix:
            # This host cannot reach Docker Hub; the mirror remains deployment configuration.
            with sftp.open(remote_env) as f:
                existing = f.read().decode()
            existing = "\n".join(line for line in existing.splitlines() if not line.startswith(("ZTL_PYTHON_IMAGE=", "ZTL_NGINX_IMAGE=")))
            with sftp.open(remote_env, "w") as f:
                f.write(existing + f"\nZTL_PYTHON_IMAGE={image_prefix}/python:3.12-slim\nZTL_NGINX_IMAGE={image_prefix}/nginx:1.28-alpine\n")
        sftp.put(str(ROOT / ".dockerignore"), "/opt/ztl-cloud/.dockerignore")
        run(client, "export DEBIAN_FRONTEND=noninteractive; apt-get update -qq && apt-get install -y -qq docker.io docker-compose openssl python3; systemctl enable --now docker")
        run(client, f"""cd /opt/ztl-cloud/server/certs
if [ ! -f server.crt ]; then
 openssl req -x509 -newkey rsa:3072 -nodes -keyout ca.key -out ca.crt -days 3650 -subj '/CN=ZTL Personal Cloud CA' -addext 'basicConstraints=critical,CA:TRUE' 2>/dev/null
 openssl req -newkey rsa:3072 -nodes -keyout server.key -out server.csr -subj '/CN={host}' 2>/dev/null
 printf 'subjectAltName=IP:{host}\\nbasicConstraints=critical,CA:FALSE\\nextendedKeyUsage=serverAuth\\n' > leaf.ext
 openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 825 -extfile leaf.ext 2>/dev/null
 chmod 600 *.key
fi
cd /opt/ztl-cloud
docker-compose --env-file server/.env -f server/compose.yaml build
docker-compose --env-file server/.env -f server/compose.yaml stop
docker-compose --env-file server/.env -f server/compose.yaml rm -f
docker-compose --env-file server/.env -f server/compose.yaml up -d
""")
        local_ca = ROOT / "build/cloud-server-ca.crt"
        sftp.get("/opt/ztl-cloud/server/certs/ca.crt", str(local_ca))
        (ROOT / "build/cloud-live-test.json").write_text(json.dumps(dict(host=host, port=443, token=token, ca=str(local_ca))), encoding="utf-8")
        (ROOT / "cloud-connection.local.md").write_text(
            f"# Z.T.L 个人云端连接\n\n地址：{host}\n\nHTTPS 端口：443\n\n访问令牌：{token}\n\n信任证书：{local_ca}\n\n"
            "在设置 → Advance 中填写（自动应用），选择证书后测试连接，再启用同步。首次连接会上传本地公开业务数据及邮件任务设置。\n"
            "服务器服务目录：/opt/ztl-cloud；备份：/opt/ztl-cloud/server/backups。此文件不纳入 Git。\n", encoding="utf-8")
        run(client, "printf '23 3 * * * root python3 /opt/ztl-cloud/scripts/cloud_backup.py /opt/ztl-cloud/server/data/ztl.sqlite /opt/ztl-cloud/server/backups >> /opt/ztl-cloud/server/backups/backup.log 2>&1\\n' > /etc/cron.d/ztl-backup; chmod 644 /etc/cron.d/ztl-backup")
        run(client, "cd /opt/ztl-cloud; docker-compose --env-file server/.env -f server/compose.yaml ps; sleep 3; python3 scripts/cloud_backup.py server/data/ztl.sqlite server/backups")
        import httpx
        with httpx.Client(verify=str(local_ca), timeout=30, trust_env=False) as test:
            response = test.get(f"https://{host}/health", headers={"Authorization": "Bearer " + token})
            response.raise_for_status()
            print("HTTPS health verified; protocol", response.json()["protocol"])
        print("Connection details saved to ignored cloud-connection.local.md")
    finally:
        client.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--info", default=str(ROOT / "info.md"))
    parser.add_argument("--user")
    parser.add_argument("--image-prefix", default="")
    args = parser.parse_args()
    deploy(args.info, args.user, args.image_prefix)
