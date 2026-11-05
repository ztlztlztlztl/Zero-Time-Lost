"""Run Qt sync against a disposable real HTTPS backend, including offline retry."""
import datetime
import ipaddress
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import time

import httpx
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(dir=ROOT / "build", prefix="https-test-") as temp:
        directory = Path(temp)
        key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "ZTL test")])
        now = datetime.datetime.now(datetime.timezone.utc)
        cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
                .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(minutes=5))
                .not_valid_after(now + datetime.timedelta(days=1))
                .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
                .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address("127.0.0.1"))]), False)
                .sign(key, hashes.SHA256()))
        pem = directory / "cert.pem"
        pem.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
        private = directory / "key.pem"
        private.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        token = secrets.token_urlsafe(48)
        config = directory / "config.json"
        config.write_text(json.dumps(dict(port=port, token=token, ca=str(pem))), encoding="utf-8")
        env = os.environ.copy()
        if os.name == "nt":
            env.setdefault("ZTL_TEST_TLS_BACKEND", "schannel")
        env.update(ZTL_DB=str(directory / "test.sqlite"), ZTL_TOKEN=token,
                   ZTL_CLOUD_TEST_CONFIG=str(config), QT_QPA_PLATFORM="offscreen",
                   ZTL_CLOUD_QA_IMAGE=str(ROOT / "build/cloud-settings.png"))
        env["PATH"] = "D:/Qt/6.11.0/mingw_64/bin;D:/Qt/Tools/mingw1310_64/bin;" + env["PATH"]
        log = (directory / "server.log").open("w")
        server = subprocess.Popen([sys.executable, "-m", "uvicorn", "server.app:app", "--host", "127.0.0.1", "--port", str(port),
            "--ssl-keyfile", str(private), "--ssl-certfile", str(pem), "--no-access-log"], cwd=ROOT, env=env, stdout=log, stderr=log,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            for _ in range(100):
                try:
                    with httpx.Client(verify=str(pem), trust_env=False, timeout=1) as client:
                        if client.get(f"https://127.0.0.1:{port}/health", headers={"Authorization": "Bearer " + token}).status_code == 200:
                            break
                except httpx.TransportError:
                    time.sleep(.1)
            else:
                raise RuntimeError("test server did not start")
            result = subprocess.run([str(ROOT / "build/Sidekick/ztl_cloud_tests.exe"), "-o", str(ROOT / "build/cloud-integration-results.txt") + ",txt"],
                cwd=ROOT, env=env, timeout=180, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            output = (ROOT / "build/cloud-integration-results.txt").read_text(encoding="utf-8", errors="replace")
            print("\n".join(line for line in output.splitlines() if line.startswith(("PASS", "FAIL", "SKIP", "Totals", "D:/"))))
            return result.returncode
        finally:
            server.terminate()
            server.wait(timeout=10)
            log.close()


if __name__ == "__main__":
    sys.exit(main())
