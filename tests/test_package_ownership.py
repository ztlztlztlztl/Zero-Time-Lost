"""Exercise install/clean/uninstall only inside pytest temporary directories."""
import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
SHELL = shutil.which('pwsh') or shutil.which('powershell')
pytestmark = pytest.mark.skipif(os.name != 'nt' or not SHELL, reason='Windows package scripts')


def run(script, *args, ok=True):
    result = subprocess.run([SHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), *map(str, args)], capture_output=True, timeout=30)
    if ok:
        assert result.returncode == 0, (result.stdout + result.stderr).decode(errors='replace')
    else:
        assert result.returncode != 0
    return result


def install(stage, target, ok=True):
    return run(ROOT / 'scripts/install-package.ps1', '-Stage', stage, '-Destination', target, '-Product', 'Z.T.L', ok=ok)


def test_updates_and_uninstall_keep_unknown_files_and_data(tmp_path):
    target, first, second = [tmp_path / name for name in ('installed', 'stage1', 'stage2')]
    for path in (target, first, second):
        path.mkdir()
    for stage in (first, second):
        shutil.copyfile(ROOT / 'scripts/uninstall.ps1', stage / 'uninstall.ps1')
    (target / 'personal.md').write_text('personal')
    (target / 'data').mkdir()
    (target / 'data/valuable').write_text('keep')
    (first / 'scripts').mkdir()
    (first / 'scripts/obsolete.py').write_text('old software')
    (first / 'Z.T.L.exe').write_text('first')
    install(first, target)
    (target / 'scripts/my-notes.txt').write_text('user file inside software folder')
    (second / 'Z.T.L.exe').write_text('second')
    install(second, target)
    assert (target / 'Z.T.L.exe').read_text() == 'second'
    assert not (target / 'scripts/obsolete.py').exists()
    assert (target / 'personal.md').read_text() == 'personal'
    assert (target / 'scripts/my-notes.txt').exists()
    assert (target / 'data/valuable').read_text() == 'keep'
    run(target / 'uninstall.ps1', '-Data', 'Keep')
    assert (target / 'personal.md').exists()
    assert (target / 'scripts/my-notes.txt').exists()
    assert (target / 'data/valuable').exists()
    assert not (target / 'Z.T.L.exe').exists()


def test_initial_install_refuses_to_take_ownership_of_unknown_collision(tmp_path):
    stage, target = tmp_path / 'stage', tmp_path / 'target'
    stage.mkdir()
    target.mkdir()
    (stage / 'personal.md').write_text('package file')
    (target / 'personal.md').write_text('my original')
    install(stage, target, ok=False)
    assert (target / 'personal.md').read_text() == 'my original'
    assert not (target / '.ztl-install.json').exists()


def test_clean_only_deletes_declared_intermediates(tmp_path):
    project = tmp_path / 'project'
    for name in ('scripts', 'desktop', '.git', 'build/Desktop/CMakeFiles', 'build/running/data'):
        (project / name).mkdir(parents=True)
    (project / 'CMakeLists.txt').write_text('project(test)')
    (project / 'desktop/main.cpp').write_text('source')
    (project / 'report.md').write_text('retain')
    (project / 'build/Desktop/CMakeFiles/intermediate').write_text('generated')
    (project / 'build/running/data/personal').write_text('valuable')
    shutil.copyfile(ROOT / 'scripts/clean.ps1', project / 'scripts/clean.ps1')
    run(project / 'scripts/clean.ps1', '-WhatIf')
    assert (project / 'build/Desktop/CMakeFiles/intermediate').exists()
    run(project / 'scripts/clean.ps1')
    assert not (project / 'build/Desktop/CMakeFiles').exists()
    assert (project / 'build/running/data/personal').read_text() == 'valuable'
    assert (project / 'report.md').read_text() == 'retain'
