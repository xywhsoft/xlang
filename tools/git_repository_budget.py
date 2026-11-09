"""Audit pushed history and measure a standalone pack against repository budgets."""
from pathlib import Path
import argparse
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ARCHIVES = ('.zip', '.tar', '.tar.gz', '.tar.xz', '.tar.zst', '.tgz', '.7z', '.bundle', '.gz', '.xz', '.lzma')
IMAGES = ('.png', '.jpg', '.jpeg', '.bmp', '.webp')


def external_reason(path, size=0):
    name = '/' + path.lower()
    if '/dev/extlibs/opencv/work/' in name:
        return 'OpenCV download/build work copy'
    if '/dev/extlibs/onnxruntime/smoke/' in name and name.endswith(('.dll', '.so', '.pdb', '.exe')):
        return 'downloadable native smoke dependency'
    if '/dev/extlibs/rapidocr/smoke/' in name and Path(path).name in ('onnxruntime.dll', 'onnxruntime_providers_shared.dll'):
        return 'downloadable native smoke dependency alias'
    if '/packages/rapidocr/models/' in name and name.endswith('.onnx'):
        return 'downloadable model'
    if '/release-materials/sources/' in name and name.endswith(ARCHIVES):
        return 'external source archive'
    if '/release-materials/provenance/' in name:
        if name.endswith(ARCHIVES + IMAGES):
            return 'external acceptance archive/image'
        if (size >= 1024 * 1024 and name.endswith(('.log', '.json', '.txt', '.rsp'))
                and Path(path).name not in ('manifest.json', 'receipt.json', 'current-delivery.json')):
            return 'external raw acceptance log'
    for prefix in ('release/', 'demo/release/', 'demo4/release/', 'demo5/release/', 'demo6/release/',
                   'dev/demo/release/', 'dev/demo4/release/', 'demo6/dev/xapi_legacy/demo6/release/', 'ide/release/'):
        if path.startswith(prefix):
            return 'runtime distribution'
    return None


def git(repo, *args, data=None):
    return subprocess.run(['git', *args], cwd=repo, input=data, capture_output=True, check=True).stdout


def inspect(repo, tips, policy):
    objects = {}
    for line in git(repo, 'rev-list', '--objects', *tips).decode('utf-8').splitlines():
        oid, _, path = line.partition(' ')
        objects[oid] = path
    sizes = {}
    raw = git(repo, 'cat-file', '--batch-check=%(objectname) %(objecttype) %(objectsize)',
              data=('\n'.join(objects) + '\n').encode())
    for line in raw.decode().splitlines():
        oid, kind, size = line.split()
        if kind == 'blob':
            sizes[oid] = int(size)
    errors = []
    approved = policy['approvedLargeBlobs']
    warnings = []
    for oid, size in sizes.items():
        if size > policy['fileLimitMiB'] * 1024 * 1024 and oid not in approved:
            errors.append('Unapproved large file: ' + objects[oid] + ' (' + str(size) + ' bytes)')
        elif size > policy.get('fileWarnMiB', 1) * 1024 * 1024 and oid not in approved:
            warnings.append(objects[oid])
    for path in warnings[:5]:
        print('WARNING: new file exceeds soft file budget: ' + path, file=sys.stderr)
    # Walk raw historical changes so aliases and pre-move paths cannot hide a forbidden archive.
    raw = git(repo, 'log', '--root', '-m', '--raw', '--no-abbrev', '--no-renames', '--format=', '-z', *tips)
    parts = raw.split(b'\0')
    i = 0
    rejected = set()
    while i < len(parts):
        header = parts[i].lstrip(b'\n')
        i += 1
        if not header.startswith(b':'):
            continue
        fields = header.split()
        path = parts[i].decode('utf-8')
        i += 1
        size = max(sizes.get(fields[2].decode(), 0), sizes.get(fields[3].decode(), 0))
        reason = external_reason(path, size)
        if reason:
            rejected.add(path + ': ' + reason)
    errors.extend(sorted(rejected))
    return errors, len(objects)


def pack_size(repo, tips, limit):
    process = subprocess.Popen(['git', '-c', 'pack.threads=2', 'pack-objects', '--revs', '--stdout'],
                               cwd=repo, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    total = 0
    try:
        process.stdin.write(('\n'.join(tips) + '\n').encode())
        process.stdin.close()
        while block := process.stdout.read(1024 * 1024):
            total += len(block)
            if total > limit:
                process.kill()
                process.wait()
                return total
        error = process.stderr.read()
        if process.wait():
            raise RuntimeError(error.decode(errors='replace'))
        return total
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        process.stdout.close()
        process.stderr.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=ROOT)
    parser.add_argument('--policy', type=Path, default=ROOT / 'tools/git_repository_policy.json')
    parser.add_argument('--pre-push', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    tips = []
    if args.pre_push:
        for line in sys.stdin:
            fields = line.split()
            if len(fields) == 4 and fields[1] != '0' * 40:
                tips.append(fields[1])
    else:
        tips = git(args.repo, 'for-each-ref', '--format=%(objectname)', 'refs/heads/master', 'refs/tags').decode().split()
    if not tips:
        return 0
    tips = list(dict.fromkeys(tips))
    policy = json.loads(args.policy.read_text(encoding='utf-8'))
    errors, objects = inspect(args.repo, tips, policy)
    limit = int(policy['hardLimitMiB'] * 1024 * 1024)
    size = pack_size(args.repo, tips, limit) if not errors else None
    if size is not None and size > limit:
        errors.append('Reachable standalone pack exceeds ' + str(policy['hardLimitMiB']) + ' MiB')
    report = dict(passed=not errors, tips=tips, objects=objects, packBytes=size,
                  packMiB=round(size / 1048576, 2) if size is not None else None, errors=errors)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if size is not None:
        print('Repository pack: %.2f MiB / %s MiB hard budget' % (size / 1048576, policy['hardLimitMiB']))
        if size > policy['warnMiB'] * 1024 * 1024:
            print('WARNING: repository exceeds warning budget', file=sys.stderr)
    for error in errors[:20]:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    raise SystemExit(main())
