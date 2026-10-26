"""Copy a hash-checked source-driver cohort without changing the working tree.

Validation artifacts are kept outside the checkout so concurrent workspace
edits cannot silently change an in-flight build. This is not a shipping build.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and not out.is_relative_to(root)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    paths = set()
    for folder in ['src', 'lib', 'include', 'res/tcc', 'release/include', 'tools', 'test/cases', 'test/support']:
        paths.update(p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    paths.update(root/p for p in ['release/xl.dll', 'build_dll.bat', 'build_linux.sh'])
    # Native source package inputs outside src/lib are explicit manifest
    # dependencies too, even when this driver's executable never calls them.
    for name in ['tcc_vfs_manifest_win64_xlc.txt', 'tcc_vfs_manifest_linux_xlc.txt']:
        for line in (root/'tools'/name).read_text(encoding='utf-8-sig').splitlines():
            parts = line.split('|')
            if len(parts) < 3 or not parts[2].startswith('%XL6_ROOT%'):
                continue
            source = Path(parts[2].replace('%XL6_ROOT%', str(root)).replace('\\', '/'))
            assert source.is_relative_to(root)
            if parts[0] == 'file':
                paths.add(source)
            elif parts[0] == 'dir':
                patterns = parts[3].split(';')
                assert source.is_dir(), source
                paths.update(p for p in source.rglob('*') if p.is_file() and
                             any(p.match(pattern) for pattern in patterns))
    rows = [(p, sha(p)) for p in sorted(paths)]
    out.mkdir(parents=True)
    for source, expected in rows:
        target = out/source.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        assert sha(target) == expected, source
    stable = all(sha(p) == h for p, h in rows)
    report = dict(Complete=stable, HeadAtStart=head, Root=str(root), Frozen=str(out),
                  Inputs=[dict(Path=str(p.relative_to(root)), Sha256=h) for p, h in rows])
    out.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    assert stable, 'source changed while freezing; retain this evidence and choose a new output'
    print('Frozen', len(rows), 'files at', out, flush=True)


if __name__ == '__main__':
    main()
