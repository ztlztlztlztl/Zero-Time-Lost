"""Build-only, verified wheel cache; stdout is the selected cache directory."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import uuid


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def target():
    return f'{sys.implementation.cache_tag}-{sys.platform}-{platform.machine()}'


def valid(directory, requirements_hash, runtime):
    try:
        if directory.is_symlink():
            return False
        manifest = json.loads((directory / 'manifest.json').read_text('utf-8'))
        if (manifest.get('format') != 1 or manifest.get('requirements') != requirements_hash
                or manifest.get('runtime') != runtime):
            return False
        files = manifest['files']
        if not isinstance(files, dict) or not files:
            return False
        if {file.name for file in directory.iterdir()} != set(files) | {'manifest.json'}:
            return False
        for name, checksum in files.items():
            if Path(name).name != name or not name.endswith('.whl'):
                return False
            path = directory / name
            if path.is_symlink() or not path.is_file() or digest(path) != checksum:
                return False
        return True
    except (OSError, ValueError, KeyError, TypeError):
        return False


def prepare(requirements, cache_root, pip_wheel):
    requirements, cache_root = Path(requirements), Path(cache_root).resolve()
    requirements_hash, runtime = digest(requirements), target()
    identity = hashlib.sha256((requirements_hash + '\n' + runtime).encode()).hexdigest()
    cache_root.mkdir(parents=True, exist_ok=True)
    destination = cache_root / identity
    if valid(destination, requirements_hash, runtime):
        return destination
    # Keep a corrupt cache for diagnosis; never install from it.
    if destination.exists() or destination.is_symlink():
        destination.rename(cache_root / (identity + '.invalid-' + uuid.uuid4().hex))
    with tempfile.TemporaryDirectory(prefix='.download-', dir=cache_root) as temporary:
        stage = Path(temporary) / 'wheels'
        stage.mkdir()
        bootstrap = ('import sys,runpy; sys.path.insert(0,sys.argv.pop(1)); '
                     'runpy.run_module("pip",run_name="__main__")')
        # All direct and transitive dependencies are pinned in the requirements file.
        subprocess.run([sys.executable, '-I', '-B', '-c', bootstrap, str(pip_wheel),
                        'download', '--disable-pip-version-check', '--no-cache-dir',
                        '--only-binary=:all:', '--no-deps', '--dest', str(stage),
                        '-r', str(requirements.resolve())], check=True, stdout=sys.stderr)
        files = {path.name: digest(path) for path in stage.iterdir() if path.is_file()}
        manifest = dict(format=1, requirements=requirements_hash, runtime=runtime, files=files)
        (stage / 'manifest.json').write_text(json.dumps(manifest, sort_keys=True), 'utf-8')
        if not valid(stage, requirements_hash, runtime):
            raise ValueError('Downloaded wheel cache failed verification')
        try:
            stage.rename(destination)
        except OSError:
            # Another packager may have completed the identical immutable cache.
            if not valid(destination, requirements_hash, runtime):
                raise
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--requirements', required=True, type=Path)
    parser.add_argument('--cache-root', required=True, type=Path)
    parser.add_argument('--pip-wheel', required=True, type=Path)
    options = parser.parse_args()
    print(prepare(options.requirements, options.cache_root, options.pip_wheel))
