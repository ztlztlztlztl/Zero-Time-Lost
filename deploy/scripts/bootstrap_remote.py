"""Remote inventory and explicit cleanup. Executed over SSH, not installed as a service."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import shlex
import stat
import subprocess
import sys

ZTL = Path('/opt/ztl-cloud')
ROOT_KEEP = {'.ssh', '.bashrc', '.profile', '.bash_logout', '.swas', 'snap', '.zshrc', '.zprofile', '.zshenv'}
VENDOR = ('aliyun', 'aegis', 'cloudmonitor', 'cloud-assist', 'swas', 'containerd', 'docker', 'snap')


def command(args, check=True):
    p = subprocess.run(args, capture_output=True, text=True, timeout=40)
    if check and p.returncode:
        raise ValueError('服务器检查命令失败：' + args[0])
    return p.stdout


def mounts():
    # mountinfo escapes whitespace and backslashes as octal sequences.
    result = []
    for line in Path('/proc/self/mountinfo').read_text().splitlines():
        value = re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), line.split()[4])
        result.append(Path(value))
    return result


def beneath(path, parent):
    return path == parent or parent in path.parents


def allowed_location(path):
    p = PurePosixPath(path)
    if str(p) != path or not p.is_absolute() or '..' in p.parts:
        return False
    name = p.name.lower()
    if any(x in name for x in VENDOR):
        return False
    if p.parent == PurePosixPath('/root'):
        return p.name not in ROOT_KEEP
    return p.parent in (PurePosixPath('/opt'), PurePosixPath('/srv'), PurePosixPath('/var/www')) and p != PurePosixPath('/opt/ztl-cloud')


def permitted(path):
    p = Path(path)
    return (allowed_location(path) and not p.is_symlink() and p.resolve() == p
            and not any(beneath(m, p) for m in mounts()))


def file_item(p):
    if not permitted(str(p)):
        return None
    s = p.lstat()
    if not (stat.S_ISDIR(s.st_mode) or stat.S_ISREG(s.st_mode)):
        return None
    identity = f'{s.st_dev}:{s.st_ino}:{s.st_mtime_ns}:{s.st_mode}'
    return dict(kind='path', id='path:' + str(p), name=str(p), identity=identity,
                detail='目录及全部内容' if p.is_dir() else '文件')


def docker_inventory():
    if not shutil.which('docker'):
        return []
    ids = command(['docker', 'ps', '-aq'], False).split()
    if not ids:
        return []
    return json.loads(command(['docker', 'inspect', *ids]))


def is_ztl(container):
    labels = container.get('Config', {}).get('Labels') or {}
    directory = labels.get('com.docker.compose.project.working_dir', '')
    files = labels.get('com.docker.compose.project.config_files', '')
    if directory not in ('/opt/ztl-cloud', '/opt/ztl-cloud/server'):
        return False
    if '/opt/ztl-cloud/server/compose.yaml' in files.split(','):
        return True
    # Compose 1.x stores the supplied relative path, but working_dir may already
    # be the config's parent. Confirm actual bind mounts before accepting it.
    relative = {'server/compose.yaml', './server/compose.yaml'}
    if directory == '/opt/ztl-cloud/server':
        relative |= {'compose.yaml', './compose.yaml'}
    if not relative.intersection(files.split(',')):
        return False
    binds = {(m.get('Source'), m.get('Destination')) for m in container.get('Mounts', []) if m.get('Type') == 'bind'}
    if labels.get('com.docker.compose.service') == 'api':
        return ('/opt/ztl-cloud/server/data', '/data') in binds
    if labels.get('com.docker.compose.service') == 'https':
        return ('/opt/ztl-cloud/server/certs', '/certs') in binds and any(destination == '/etc/nginx/conf.d/default.conf' and (source == '/opt/ztl-cloud/server/nginx.conf' or re.fullmatch(r'/opt/ztl-cloud/releases/[a-f0-9]{64}/server/nginx\.conf', source)) for source, destination in binds)
    return False


def protected_container(container):
    return is_ztl(container) or any(x in container.get('Name', '').lower() for x in VENDOR)


def inventory():
    release = {}
    for line in Path('/etc/os-release').read_text().splitlines():
        if '=' in line:
            k, v = line.split('=', 1)
            release[k] = v.strip('"')
    if os.geteuid() != 0:
        raise ValueError('自动部署需要 root 登录')
    if release.get('ID') != 'ubuntu' or release.get('VERSION_ID') not in ('22.04', '24.04'):
        raise ValueError('首版仅支持 Ubuntu 22.04 / 24.04')
    items = []
    for base in ('/root', '/opt', '/srv', '/var/www'):
        if Path(base).is_dir():
            for p in sorted(Path(base).iterdir()):
                item = file_item(p)
                if item:
                    items.append(item)
    containers = docker_inventory()
    projects = {(c.get('Config', {}).get('Labels') or {}).get('com.docker.compose.project')
                for c in containers if is_ztl(c)} - {None, ''}
    if len(projects) > 1:
        raise ValueError('发现多套 Z.T.L 项目，请先人工整理后部署')
    project = next(iter(projects), 'ztl-cloud')
    conflict = any(not is_ztl(c) and (c.get('Config', {}).get('Labels') or {}).get('com.docker.compose.project')
                   in {project, 'ztl-cloud'} for c in containers)
    for c in containers:
        if protected_container(c):
            continue
        items.append(dict(kind='container', id='container:' + c['Id'], name=c['Name'].lstrip('/'),
                          identity=c['Id'], detail='Docker 容器（删除容器，不自动删除绑定目录或镜像）'))
    if shutil.which('docker'):
        for name in command(['docker', 'volume', 'ls', '-q'], False).split():
            attached = [c['Name'].lstrip('/') for c in containers
                        if any(m.get('Name') == name for m in c.get('Mounts', []))]
            if any(x in name.lower() for x in VENDOR) or any(protected_container(c) and any(m.get('Name') == name for m in c.get('Mounts', [])) for c in containers):
                continue
            info = json.loads(command(['docker', 'volume', 'inspect', name]))[0]
            identity = hashlib.sha256(json.dumps(info, sort_keys=True).encode()).hexdigest()
            items.append(dict(kind='volume', id='volume:' + name, name=name, identity=identity,
                              detail='Docker 数据卷；使用者：' + (', '.join(attached) or '无')))
        protected_images = {c.get('Image') for c in containers if protected_container(c)}
        image_ids = command(['docker', 'image', 'ls', '-aq', '--no-trunc'], False).split()
        if image_ids:
            images = json.loads(command(['docker', 'image', 'inspect', *sorted(set(image_ids))]))
            for image in images:
                repositories = [tag.rsplit(':', 1)[0].rsplit('/', 1)[-1] for tag in image.get('RepoTags') or []]
                if any(repo in ('python', 'nginx') or any(x in repo.lower() for x in VENDOR) for repo in repositories):
                    protected_images.add(image['Id'])
            while True:
                parents = {i.get('Parent') for i in images if i['Id'] in protected_images and i.get('Parent')}
                if parents.issubset(protected_images):
                    break
                protected_images.update(parents)
            for image in images:
                if image['Id'] in protected_images:
                    continue
                items.append(dict(kind='image', id='image:' + image['Id'], identity=image['Id'],
                    name=', '.join(image.get('RepoTags') or [image['Id'][:25]]), parent=image.get('Parent', ''), detail='旧应用镜像'))
        protected_networks = {n.get('NetworkID') for c in containers if protected_container(c)
                              for n in c.get('NetworkSettings', {}).get('Networks', {}).values()}
        network_ids = command(['docker', 'network', 'ls', '-q', '--no-trunc'], False).split()
        if network_ids:
            for network in json.loads(command(['docker', 'network', 'inspect', *network_ids])):
                if network['Name'] in ('bridge', 'host', 'none') or network['Id'] in protected_networks or any(x in network['Name'].lower() for x in VENDOR):
                    continue
                items.append(dict(kind='network', id='network:' + network['Id'], identity=network['Id'],
                                  name=network['Name'], detail='旧应用容器网络'))
    # Only locally authored regular unit files with an explicitly identifiable business executable.
    for p in sorted(Path('/etc/systemd/system').glob('*.service')):
        if p.is_symlink() or not p.is_file() or any(x in p.name.lower() for x in VENDOR):
            continue
        content = p.read_text(errors='replace')
        match = re.search(r'^ExecStart=[-+!:@]*([^\s]+)', content, re.M)
        if not match:
            continue
        line = re.search(r'^ExecStart=(.+)', content, re.M)[1]
        try:
            arguments = shlex.split(line)
        except ValueError:
            continue
        matches = [(arg, i['name']) for arg in arguments if arg.startswith('/') for i in items
                   if i['kind'] == 'path' and beneath(Path(arg), Path(i['name']))]
        owners = [owner for _, owner in matches]
        if owners:
            items.append(dict(kind='service', id='service:' + p.name, name=p.name,
                              identity=hashlib.sha256(p.read_bytes()).hexdigest(),
                              detail='停止、禁用并移除自定义服务；程序位于 ' + owners[0], executable=matches[0][0]))
    listeners = command(['ss', '-H', '-ltnp', 'sport = :443'], False).strip()
    ztl_listeners = any(is_ztl(c) and c.get('State', {}).get('Running') and
        any(x.get('HostPort') == '443' for rows in c.get('NetworkSettings', {}).get('Ports', {}).values()
            for x in (rows or [])) for c in containers)
    old_container_listener = any(not protected_container(c) and c.get('State', {}).get('Running') and
        any(x.get('HostPort') == '443' for rows in c.get('NetworkSettings', {}).get('Ports', {}).values()
            for x in (rows or [])) for c in containers)
    free = shutil.disk_usage('/opt').free
    machine = hashlib.sha256(Path('/etc/machine-id').read_bytes()).hexdigest()
    deployment = []
    for p in (ZTL, ZTL / 'server/.env'):
        if p.exists() or p.is_symlink():
            s = p.lstat()
            deployment.append([str(p), s.st_dev, s.st_ino, s.st_mtime_ns, s.st_size])
    return dict(os=release.get('PRETTY_NAME', 'Ubuntu'), machine=machine, items=items,
                existing=ZTL.exists(), portBusy=bool(listeners), portOwned=ztl_listeners,
                freeBytes=free, containers=containers, deployment=deployment,
                composeProject=project, projectConflict=conflict, portCleanup=old_container_listener)


def validate_selection(plan, selected, current):
    if plan.get('machine') != current['machine']:
        raise ValueError('服务器身份已变化，请重新检测')
    old = {i['id']: i for i in plan.get('items', [])}
    now = {i['id']: i for i in current['items']}
    if not isinstance(selected, list) or len(set(selected)) != len(selected):
        raise ValueError('无效清理清单')
    result = []
    for key in selected:
        if key not in old or key not in now or old[key] != now[key]:
            raise ValueError('清理对象已变化，请重新检测后确认')
        result.append(now[key])
    chosen_containers = {i['identity'] for i in result if i['kind'] == 'container'}
    for i in result:
        if i['kind'] == 'path':
            if not permitted(i['name']):
                raise ValueError('禁止清理该路径')
            for c in current['containers']:
                if c['Id'] not in chosen_containers and any(beneath(Path(m['Source']), Path(i['name']))
                         or beneath(Path(i['name']), Path(m['Source'])) for m in c.get('Mounts', []) if m.get('Source')):
                    raise ValueError('目录仍被未选中的容器使用：' + i['name'])
            for service in current['items']:
                if service['kind'] == 'service' and beneath(Path(service['executable']), Path(i['name'])) and service['id'] not in selected:
                    raise ValueError('请同时选择关联服务：' + service['name'])
        if i['kind'] == 'volume':
            for c in current['containers']:
                if c['Id'] not in chosen_containers and any(m.get('Name') == i['name'] for m in c.get('Mounts', [])):
                    raise ValueError('数据卷仍被未选中的容器使用：' + i['name'])
        if i['kind'] in ('image', 'network'):
            for c in current['containers']:
                used = c.get('Image') == i['identity'] if i['kind'] == 'image' else any(
                    n.get('NetworkID') == i['identity'] for n in c.get('NetworkSettings', {}).get('Networks', {}).values())
                if c['Id'] not in chosen_containers and used:
                    raise ValueError('资源仍被需保留的容器使用：' + i['name'])
    return result


def cleanup(plan, selected):
    current = inventory()
    items = validate_selection(plan, selected, current)
    return execute_cleanup(items)


def automatic_selection(current):
    """Select all old business resources, excluding dependencies of protected containers."""
    selected = []
    keep = [c for c in current['containers'] if protected_container(c)]
    for item in current['items']:
        if item['kind'] == 'path' and any(
            beneath(Path(m['Source']), Path(item['name'])) or beneath(Path(item['name']), Path(m['Source']))
            for c in keep for m in c.get('Mounts', []) if m.get('Source')):
            continue
        selected.append(item['id'])
    return selected


def cleanup_dedicated(plan, keep_images=()):
    current = inventory()
    if plan.get('machine') != current['machine'] or plan.get('deployment') != current.get('deployment'):
        raise ValueError('服务器或已有 Z.T.L 部署发生变化，请重新检测')
    if not isinstance(keep_images, (list, tuple)) or any(not isinstance(i, str) or not re.fullmatch(r'sha256:[a-f0-9]{64}', i) for i in keep_images):
        raise ValueError('需保留的预检镜像身份无效')
    protected = set(keep_images)
    while True:
        parents = {i.get('parent') for i in current['items'] if i['kind'] == 'image'
                   and i['identity'] in protected and i.get('parent')}
        if parents.issubset(protected):
            break
        protected.update(parents)
    current['items'] = [i for i in current['items'] if i['kind'] != 'image' or i['identity'] not in protected]
    # The user approves the dedicated-server policy, not an arbitrary client-supplied path list.
    selected = automatic_selection(current)
    items = validate_selection(current, selected, current)
    return execute_cleanup(items)


def execute_cleanup(items):
    image_map = {i['identity']: i for i in items if i['kind'] == 'image'}
    def depth(image):
        seen = set()
        while image.get('parent') in image_map and image['identity'] not in seen:
            seen.add(image['identity'])
            image = image_map[image['parent']]
        return len(seen)
    items = sorted(items, key=lambda i: depth(i) if i['kind'] == 'image' else 0, reverse=True)
    for kind in ('service', 'container', 'volume', 'network', 'image', 'path'):
        for item in items:
            if item['kind'] != kind:
                continue
            if kind == 'service':
                command(['systemctl', 'stop', item['name']])
                command(['systemctl', 'disable', item['name']])
                Path('/etc/systemd/system', item['name']).unlink()
                command(['systemctl', 'daemon-reload'])
            elif kind == 'container':
                command(['docker', 'rm', '-f', item['identity']])
            elif kind == 'volume':
                command(['docker', 'volume', 'rm', item['name']])
            elif kind == 'network':
                command(['docker', 'network', 'rm', item['identity']])
            elif kind == 'image':
                if command(['docker', 'image', 'inspect', item['identity']], False).strip():
                    command(['docker', 'image', 'rm', '-f', item['identity']])
            else:
                p = Path(item['name'])
                # Recheck directly before removal; rmtree does not follow nested symlinks.
                fresh = file_item(p)
                # Stopping a business service can update its directory's mtime. The inode,
                # device and type must still be the same; nested mounts/symlinks are rechecked.
                if fresh is None or fresh['identity'].split(':')[:2] != item['identity'].split(':')[:2] or fresh['identity'].split(':')[-1] != item['identity'].split(':')[-1]:
                    raise ValueError('目录在清理过程中发生变化，请重新检测：' + item['name'])
                if p.is_dir():
                    shutil.rmtree(p)
                else:
                    p.unlink()
    return dict(cleaned=len(items))


def main():
    request = json.load(sys.stdin)
    if request.get('action') == 'scan':
        result = inventory()
        selected = set(automatic_selection(result))
        result['items'] = [i for i in result['items'] if i['id'] in selected]
        result.pop('containers')
    elif request.get('action') == 'clean':
        if request.get('policy') != 'dedicated':
            raise ValueError('自动清理仅支持 Z.T.L 专用服务器策略')
        result = cleanup_dedicated(request['plan'], request.get('keepImages', []))
    else:
        raise ValueError('未知服务器检查操作')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    try:
        main()
    except ValueError as error:
        print(json.dumps({'error': str(error)}, ensure_ascii=False))
