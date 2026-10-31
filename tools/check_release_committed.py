"""Audit exact committed delivery bytes; independent of offline runtime tests."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def check(root):
    receipt = root / 'dev/release-materials/provenance/current-delivery.json'
    data = json.loads(receipt.read_text(encoding='utf-8'))
    rows = data['products'] + data['dataFiles']
    paths = [row['path'] for row in rows]
    if len(set(paths)) != len(paths):
        raise ValueError('Duplicate delivery path')
    for row in rows:
        path = root / row['path']
        if not path.resolve().is_relative_to(root / 'release'):
            raise ValueError('Delivery path is outside release: ' + row['path'])
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(block)
        if path.stat().st_size != row['bytes'] or digest.hexdigest() != row['sha256']:
            raise ValueError('Delivery bytes changed: ' + row['path'])

    # 同时验证 Git 中的原始字节，防止忽略规则或换行转换遗漏运行成果。
    tracked = subprocess.check_output(
        ['git', 'ls-tree', '-r', '-z', 'HEAD', '--', 'release/data',
         *[row['path'] for row in data['products']]], cwd=root)
    blobs = {}
    modes = {}
    for record in tracked.split(b'\0'):
        if not record:
            continue
        metadata, path = record.split(b'\t', 1)
        mode, kind, oid = metadata.decode('ascii').split()
        if kind != 'blob':
            raise ValueError('Non-file delivery entry: ' + path.decode('utf-8'))
        name = path.decode('utf-8')
        blobs[name], modes[name] = oid, mode
    actual = subprocess.check_output(
        ['git', 'hash-object', '--no-filters', '--stdin-paths'],
        input=''.join(path + '\n' for path in paths).encode('utf-8'), cwd=root)
    hashes = actual.decode('ascii').splitlines()
    if len(hashes) != len(paths):
        raise ValueError('Incomplete Git byte audit')
    for path, oid in zip(paths, hashes):
        if blobs.get(path) != oid:
            raise ValueError('Uncommitted or transformed runtime file: ' + path)
    if modes.get('release/xl') != '100755':
        raise ValueError('Linux release/xl lacks its committed executable bit')
    return dict(status='passed',products=len(data['products']),
                dataFiles=len(data['dataFiles']),committedExactFiles=len(paths))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        result = check(args.root.resolve())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
