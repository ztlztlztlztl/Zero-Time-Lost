"""GUI-only fake. Never imports SSH or accesses a network."""
import json
from pathlib import Path
import sys
r = json.load(sys.stdin)
assert r['version'] == 1
action = r['action']
if action == 'self-check':
    result = dict(adapter=1)
elif action in ('inspect-release', 'download-release'):
    result = dict(release=str(Path(r.get('release', 'test-release.zip')).resolve()), version='test-2.0', protocol=1)
elif action == 'scan':
    result = dict(machine='test', fingerprint='SHA256:fixture', os='Ubuntu test', existing=True, items=[], freeBytes=8*1024**3)
else:
    assert r['fingerprint'] == 'SHA256:fixture'
    if action == 'deploy':
        assert r['mode'] == 'preserve' and r['cleanupPolicy'] == 'none'
        result = dict(host=r['connection']['host'], port=443, token='test-token-'*5, ca='')
    elif action == 'list-backups':
        result = dict(backups=['ztl-20261106-030000-123456.sqlite'])
    else:
        result = dict(details='操作成功', health=dict(protocol=1, serverVersion='test-2.0'))
print(json.dumps(dict(version=1,event='result',data=result)))
