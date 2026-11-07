"""Wheel cache reuse, corruption rejection, and interrupted download isolation."""
import importlib.util
import json
from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('prepare_wheels', ROOT / 'scripts/prepare_wheels.py')
wheels = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wheels)


def fake_download(args, **kwargs):
    destination = Path(args[args.index('--dest') + 1])
    (destination / 'example-1.0-py3-none-any.whl').write_bytes(b'verified package payload')


def test_verified_cache_is_reusable_offline_and_isolated_by_requirements(tmp_path, monkeypatch):
    requirements = tmp_path / 'requirements.txt'
    requirements.write_text('example==1.0\n')
    monkeypatch.setattr(wheels.subprocess, 'run', fake_download)
    first = wheels.prepare(requirements, tmp_path / 'cache', tmp_path / 'pip.whl')
    monkeypatch.setattr(wheels.subprocess, 'run', lambda *a, **k: pytest.fail('offline cache must not download'))
    assert wheels.prepare(requirements, tmp_path / 'cache', tmp_path / 'pip.whl') == first
    requirements.write_text('example==2.0\n')
    monkeypatch.setattr(wheels.subprocess, 'run', fake_download)
    assert wheels.prepare(requirements, tmp_path / 'cache', tmp_path / 'pip.whl') != first


def test_corrupt_cache_is_quarantined_and_never_installed(tmp_path, monkeypatch):
    requirements = tmp_path / 'requirements.txt'
    requirements.write_text('example==1.0\n')
    monkeypatch.setattr(wheels.subprocess, 'run', fake_download)
    cached = wheels.prepare(requirements, tmp_path / 'cache', tmp_path / 'pip.whl')
    (cached / 'example-1.0-py3-none-any.whl').write_bytes(b'corrupt')
    monkeypatch.setattr(wheels.subprocess, 'run', lambda *a, **k: (_ for _ in ()).throw(subprocess.CalledProcessError(1, a)))
    with pytest.raises(subprocess.CalledProcessError):
        wheels.prepare(requirements, tmp_path / 'cache', tmp_path / 'pip.whl')
    assert not cached.exists()
    assert len(list(cached.parent.glob('*.invalid-*'))) == 1
    assert not list(cached.parent.glob('.download-*'))


def test_cache_manifest_cannot_point_outside_directory(tmp_path):
    outside = tmp_path / 'outside.whl'
    outside.write_bytes(b'outside')
    cache = tmp_path / 'cache'
    cache.mkdir()
    (cache / 'manifest.json').write_text(json.dumps(dict(format=1, runtime='test', requirements='hash',
                                                      files={'../outside.whl': wheels.digest(outside)})))
    assert not wheels.valid(cache, 'hash', 'test')
