"""Validate server artifacts before SSH. Never extracts arbitrary ZIP paths."""
import hashlib
import json
from pathlib import Path
import re
import stat
import zipfile

LAYOUT = Path(__file__).resolve().parents[2] / 'shared/release-layout.json'
if not LAYOUT.is_file():
    LAYOUT = Path(__file__).with_name('release-layout.json')
FILES = set(json.loads(LAYOUT.read_text(encoding='utf-8-sig'))['files'])
LIMIT = 32 * 1024 * 1024


def inspect_bundle(path):
    path = Path(path)
    if not path.is_file() or path.stat().st_size > LIMIT:
        raise ValueError('服务端发布包不存在或超过 32 MiB')
    try:
        with zipfile.ZipFile(path) as archive:
            entries = archive.infolist()
            names = [i.filename for i in entries]
            if len(names) != len(set(names)) or set(names) != FILES | {'release.json'}:
                raise ValueError('发布包文件清单无效：存在重复、缺失或未声明文件')
            if sum(i.file_size for i in entries) > 64 * 1024 * 1024 or any(
                stat.S_ISLNK(i.external_attr >> 16) or i.file_size > 16 * 1024 * 1024 for i in entries):
                raise ValueError('发布包包含链接或超大文件')
            manifest = json.loads(archive.read('release.json'))
            if manifest.get('format') != 1 or manifest.get('protocol') != 1 or not re.fullmatch(
                r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', str(manifest.get('version', ''))):
                raise ValueError('发布包版本或协议不兼容')
            if set(manifest.get('files', {})) != FILES:
                raise ValueError('发布包校验清单不完整')
            for name in FILES:
                if hashlib.sha256(archive.read(name)).hexdigest() != manifest['files'][name]:
                    raise ValueError('发布包文件校验失败：' + name)
            release = json.loads(archive.read('server/release.json'))
            if release != dict(version=manifest['version'], protocol=1):
                raise ValueError('发布包内部版本信息不一致')
            return manifest
    except (zipfile.BadZipFile, json.JSONDecodeError, KeyError, TypeError, AttributeError) as error:
        raise ValueError('服务端发布包格式无效') from error


def extract_bundle(path, directory):
    manifest = inspect_bundle(path)
    digest = hashlib.sha256(Path(path).read_bytes()).hexdigest()
    target = Path(directory).resolve() / digest
    target.mkdir(parents=True, exist_ok=True)
    if target.is_symlink() or target.resolve().parent != Path(directory).resolve():
        raise ValueError('发布包缓存目录无效')
    with zipfile.ZipFile(path) as archive:
        for name in FILES:
            p = target / name
            p.parent.mkdir(parents=True, exist_ok=True)
            if p.is_symlink() or p.resolve() != p.absolute() or not p.resolve().is_relative_to(target):
                raise ValueError('发布包缓存路径存在链接')
            temporary = p.with_suffix(p.suffix + '.tmp')
            if temporary.is_symlink():
                raise ValueError('发布包缓存临时文件存在链接')
            temporary.write_bytes(archive.read(name))
            temporary.replace(p)
    return target, manifest


def download_bundle(url, directory, expected=''):
    import httpx
    from urllib.parse import urlsplit
    u = urlsplit(url)
    if u.scheme != 'https' or not u.hostname or u.username or u.password or u.fragment:
        raise ValueError('发布包地址必须为不含登录凭证的 HTTPS URL')
    if expected and not re.fullmatch('[a-fA-F0-9]{64}', expected):
        raise ValueError('外部 SHA-256 格式无效')
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    import tempfile
    fd, name = tempfile.mkstemp(prefix='release-', suffix='.zip', dir=directory)
    import os
    size = 0
    try:
        def validate_redirect(request):
            if request.url.scheme != 'https' or request.url.userinfo:
                raise ValueError('发布包重定向必须保持 HTTPS，且不能包含登录凭证')
        with os.fdopen(fd, 'wb') as out, httpx.Client(timeout=60, follow_redirects=True, max_redirects=5,
                event_hooks={'request': [validate_redirect]}) as client:
            with client.stream('GET', url) as response:
                response.raise_for_status()
                for chunk in response.iter_bytes():
                    size += len(chunk)
                    if size > LIMIT:
                        raise ValueError('发布包超过 32 MiB')
                    out.write(chunk)
        if expected and hashlib.sha256(Path(name).read_bytes()).hexdigest() != expected.lower():
            raise ValueError('发布包外部 SHA-256 不匹配')
        inspect_bundle(name)
        return Path(name)
    except Exception:
        Path(name).unlink(missing_ok=True)
        raise
