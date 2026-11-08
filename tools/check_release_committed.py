"""Audit local delivery hashes and ensure runtime artifacts stay outside Git."""
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

    # Runtime bytes are verified against the delivery receipt, not stored in Git.
    tracked = subprocess.check_output(
        ['git', 'ls-files', '-z', '--', 'release'], cwd=root)
    if tracked:
        raise ValueError('Runtime artifacts must not be tracked; remove release/ from the index')
    ignored = subprocess.check_output(
        ['git', 'check-ignore', '--no-index', '-z', '--stdin'],
        input=b''.join(path.encode('utf-8') + b'\0' for path in paths), cwd=root)
    if set(ignored.decode('utf-8').split('\0')) - {''} != set(paths):
        raise ValueError('Runtime artifacts are not completely covered by .gitignore')
    return dict(status='passed', products=len(data['products']),
                dataFiles=len(data['dataFiles']), verifiedRuntimeFiles=len(paths),
                trackedRuntimeFiles=0)


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
