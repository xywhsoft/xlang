#!/usr/bin/env python3
"""Explicit archival of an unpublished test endpoint before formal version 1."""
import argparse
from datetime import datetime, timezone
import hashlib
from pathlib import Path, PurePosixPath
import secrets
import shlex
import sys
import update_manager as manager

def retire(store, platform, expected, apply=False):
    if type(expected) is not int or expected < 1:
        raise ValueError('--expect-version must name the current test iteration')
    remote = manager.SSH(manager.read(store / '.state/deploy.json'))
    if not remote.root.endswith('/update_' + platform):
        raise ValueError('wrong endpoint platform')
    active = remote.run('cat ' + shlex.quote(remote.root + '/version.txt'))
    current, _ = manager.version_marker(active)
    if current != expected:
        raise ValueError(f'expected test version {expected}, found {current}; nothing changed')
    token = secrets.token_hex(8)
    private = str(PurePosixPath(remote.root).parent) + '/.xlang-publish-' + hashlib.sha256(remote.root.encode()).hexdigest()[:16]
    archive = private + '/retired-test-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + token
    result = {'testVersion': current, 'archive': archive, 'remoteRoot': remote.root, 'applied': False}
    if not apply:
        return result
    q = shlex.quote
    expected_hash = hashlib.sha256(active.encode('utf-8')).hexdigest()
    manager.write(store / '.state/test-channel-retirement.json', {**result, 'status': 'pending'})
    script = f'''set -eu
root={q(remote.root)}
archive={q(archive)}
lock={q(private + '/lock')}
mkdir -p {q(private)}
mkdir "$lock" || exit 72
printf %s {q(token)} > "$lock/owner"
moved=0
finished=0
rollback() {{
  if test "$moved" = 1 && test "$finished" = 0; then
    if test -d "$root"; then mv -T -- "$root" "$archive.failed-new"; fi
    mv -T -- "$archive" "$root"
  fi
  rm -f -- "$lock/owner"
  rmdir -- "$lock" || true
}}
trap rollback EXIT
actual=$(sha256sum -- "$root/version.txt" | cut -d " " -f1)
test "$actual" = {q(expected_hash)}
test ! -e "$archive"
mv -T -- "$root" "$archive"
moved=1
mkdir -p "$root/update"
chmod 755 "$root" "$root/update"
cp -al -- "$archive/update/." "$root/update/"
finished=1
'''
    remote.run(script)
    result['applied'] = True
    manager.write(store / '.state/test-channel-retirement.json', {**result, 'status': 'retired'})
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform', choices=manager.PLATFORMS, default='win_x64')
    p.add_argument('--store', type=Path)
    p.add_argument('--expect-version', type=int, required=True)
    p.add_argument('--apply', action='store_true')
    a = p.parse_args()
    store = (a.store or manager.ROOT / 'release/update_info' / a.platform).resolve()
    try:
        with manager.lock(store):
            result = retire(store, a.platform, a.expect_version, a.apply)
        import json
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, RuntimeError) as e:
        print('ERROR:', e, file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
