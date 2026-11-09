"""Restore external development materials to their recorded repository paths."""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def destination(root, relative):
    name = PurePosixPath(relative)
    if (not relative or '\\' in relative or ':' in relative or name.is_absolute()
            or any(p in ('', '.', '..') for p in relative.split('/'))):
        raise ValueError('Invalid material path: ' + relative)
    root = root.resolve()
    path = root.joinpath(*name.parts)
    if not path.resolve().is_relative_to(root):
        raise ValueError('Material path escapes repository')
    for probe in [path, *path.parents]:
        if probe == root:
            break
        if probe.is_symlink() or getattr(probe, 'is_junction', lambda: False)():
            raise ValueError('Material path contains a link: ' + str(probe))
    return path


def git(repo, *args):
    return subprocess.check_output(['git', '--git-dir=' + str(repo), *args], stderr=subprocess.PIPE)


def obtain(index, cache, supplied=None):
    spec = index['bundle']
    if supplied:
        path = Path(supplied).resolve()
    else:
        cache.mkdir(parents=True, exist_ok=True)
        path = cache / (spec['sha256'] + '.bundle')
        if not path.exists():
            if not spec['url'].startswith('https://'):
                raise ValueError('Material download requires HTTPS')
            fd, temporary = tempfile.mkstemp(prefix='download-', dir=cache)
            temporary = Path(temporary)
            try:
                size = 0
                with os.fdopen(fd, 'wb') as output, urllib.request.urlopen(spec['url'], timeout=60) as response:
                    while block := response.read(1024 * 1024):
                        size += len(block)
                        if size > spec['bytes']:
                            raise ValueError('Material download exceeds declared size')
                        output.write(block)
                if size != spec['bytes'] or digest(temporary) != spec['sha256']:
                    raise ValueError('Material download checksum mismatch')
                os.replace(temporary, path)
            finally:
                temporary.unlink(missing_ok=True)
    if path.stat().st_size != spec['bytes'] or digest(path) != spec['sha256']:
        raise ValueError('Material bundle checksum mismatch')
    return path


def catalog(index, cache, supplied=None):
    bundle = obtain(index, cache, supplied)
    repo = cache / (index['bundle']['sha256'] + '.git')
    if not repo.exists():
        subprocess.run(['git', 'init', '--bare', '--template=', str(repo)], check=True, capture_output=True)
    expected = index['bundle']['commit']
    result = subprocess.run(['git', '--git-dir=' + str(repo), 'cat-file', '-e', expected + '^{commit}'], capture_output=True)
    if result.returncode:
        git(repo, 'fetch', '--no-tags', str(bundle), index['bundle']['ref'] + ':refs/heads/materials')
    if git(repo, 'rev-parse', 'refs/heads/materials').decode().strip() != expected:
        raise ValueError('Material commit does not match manifest')
    spec = index['catalog']
    if spec['bytes'] > 32 * 1024 * 1024:
        raise ValueError('Material catalog exceeds supported size')
    raw = git(repo, 'cat-file', 'blob', spec['gitBlob'])
    if len(raw) != spec['bytes'] or hashlib.sha256(raw).hexdigest() != spec['sha256']:
        raise ValueError('Material catalog checksum mismatch')
    data = json.loads(raw)
    for name, versions in index.get('historicalAliases', {}).items():
        if any(oid not in data['objects'] for oid in versions):
            raise ValueError('Historical alias references an unknown object')
        data['paths'][name] = versions
    return repo, data


def restore_blob(root, repo, relative, oid, spec, force=False):
    path = destination(root, relative)
    if path.exists():
        if path.is_file() and path.stat().st_size == spec['size'] and digest(path) == spec['sha256']:
            return 'already present'
        if not force:
            raise ValueError('Existing file differs; use --force to replace: ' + relative)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.material-', dir=path.parent)
    temporary = Path(temporary)
    process = None
    try:
        h = hashlib.sha256()
        size = 0
        with os.fdopen(fd, 'wb') as output:
            process = subprocess.Popen(['git', '--git-dir=' + str(repo), 'cat-file', 'blob', oid], stdout=subprocess.PIPE)
            while block := process.stdout.read(1024 * 1024):
                size += len(block)
                if size > spec['size']:
                    raise ValueError('Material exceeds declared size')
                h.update(block)
                output.write(block)
        if process.wait() or size != spec['size'] or h.hexdigest() != spec['sha256']:
            raise ValueError('Material object checksum mismatch')
        destination(root, relative)
        if path.exists() and not force:
            raise ValueError('Destination appeared during restoration: ' + relative)
        os.replace(temporary, path)
        return 'restored'
    finally:
        if process and process.poll() is None:
            process.kill()
            process.wait()
        if process:
            process.stdout.close()
        temporary.unlink(missing_ok=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--index', type=Path, default=ROOT / 'dev/release-materials/materials-index.json')
    p.add_argument('--root', type=Path, default=ROOT)
    p.add_argument('--cache', type=Path, default=ROOT / 'dev/local-state/material-cache')
    p.add_argument('--bundle', type=Path, help='Use a downloaded bundle instead of HTTPS')
    p.add_argument('--force', action='store_true')
    p.add_argument('--blob', help='Restore a historical version listed for the requested path')
    p.add_argument('--all-current', action='store_true')
    p.add_argument('action', choices=['list', 'restore'])
    p.add_argument('paths', nargs='*')
    args = p.parse_args()
    index = json.loads(args.index.read_text(encoding='utf-8'))
    if args.action == 'list':
        for name in sorted(index['current']):
            print(name)
        return
    names = sorted(index['current']) if args.all_current else args.paths
    if not names or (args.blob and len(names) != 1):
        p.error('Choose material paths, or --all-current; --blob requires one path')
    for name in names:
        destination(args.root, name)
    args.cache.mkdir(parents=True, exist_ok=True)
    repo, data = catalog(index, args.cache, args.bundle)
    for name in names:
        oid = args.blob or data['current'].get(name)
        if not oid or oid not in data['paths'].get(name, []):
            raise ValueError('Unknown material path/version: ' + name)
        print(name + ': ' + restore_blob(args.root, repo, name, oid, data['objects'][oid], args.force))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print('Material restore failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
