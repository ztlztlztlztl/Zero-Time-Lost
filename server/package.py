"""Build an independent server release. Uses only Python's standard library."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def prepare_build_context(files):
    """Derive the ZIP's Docker allowlist from its verified file inventory.

    The release Dockerfile deliberately uses literal COPY paths. Validate those
    paths before writing a release, rather than discovering omissions over SSH.
    """
    for line in files['server/Dockerfile'].decode('utf-8-sig').splitlines():
        parts = shlex.split(line, comments=True)
        if not parts or parts[0].upper() != 'COPY':
            continue
        if len(parts) < 3 or any(p.startswith('--') for p in parts[1:]):
            raise ValueError('Release Dockerfile COPY must use explicit source paths')
        for source in parts[1:-1]:
            if source not in files:
                raise ValueError('Docker COPY source missing from release: ' + source)
    rules = ['**']
    directories = set()
    for name in sorted(files):
        if name == '.dockerignore':
            continue
        for parent in reversed(Path(name).parents):
            directory = parent.as_posix()
            if directory != '.' and directory not in directories:
                rules.append('!' + directory + '/')
                rules.append(directory + '/**')
                directories.add(directory)
        rules.append('!' + name)
    files['.dockerignore'] = ('\n'.join(rules) + '\n').encode('utf-8')


def build_bundle(destination, version):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', version):
        raise ValueError('Invalid release version')
    layout = json.loads((ROOT / 'shared/release-layout.json').read_text(encoding='utf-8-sig'))
    sources = {'scripts/product-version.txt': ROOT / 'shared/product-version.txt', 'scripts/mail_worker.py': ROOT / 'shared/mail_worker.py',
               'scripts/cloud_backup.py': ROOT / 'server/scripts/cloud_backup.py'}
    files = {name: sources.get(name, ROOT / name).read_bytes() for name in layout['files'] if name != 'server/release.json'}
    files['server/release.json'] = json.dumps(dict(version=version, protocol=1)).encode()
    prepare_build_context(files)
    manifest = dict(format=1, version=version, protocol=1,
                    files={name: hashlib.sha256(data).hexdigest() for name, data in files.items()})
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix('.tmp')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
        archive.writestr('release.json', json.dumps(manifest, indent=2))
    temporary.replace(destination)
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    destination.with_suffix(destination.suffix + '.sha256').write_text(digest + '\n', encoding='ascii')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', default=(ROOT / 'shared/product-version.txt').read_text().strip())
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    build_bundle(args.output, args.version)
    print(args.output)
