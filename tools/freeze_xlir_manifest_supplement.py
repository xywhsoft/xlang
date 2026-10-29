"""Add previously omitted manifest inputs to a frozen cohort, never overwrite.
The original frozen files and receipt stay intact; additions have a separate
receipt and must be present before the next source build begins.
"""
from pathlib import Path
import argparse, hashlib, json, shutil


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['root', 'frozen', 'receipt']:
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    root, frozen, receipt = args.root.resolve(), args.frozen.resolve(), args.receipt.resolve()
    assert not receipt.exists()
    original = json.loads(frozen.with_suffix('.json').read_text())
    assert original['Complete']
    for row in original['Inputs']:
        assert sha(frozen/row['Path']) == row['Sha256'], row
    added = []
    for name in ['tcc_vfs_manifest_win64_xlc.txt', 'tcc_vfs_manifest_linux_xlc.txt']:
        manifest = frozen/'tools'/name
        for line in manifest.read_text(encoding='utf-8-sig').splitlines():
            parts = line.split('|')
            if len(parts) < 3 or not parts[2].startswith('%XL6_ROOT%'):
                continue
            source = Path(parts[2].replace('%XL6_ROOT%', str(root)).replace('\\', '/'))
            assert source.is_relative_to(root)
            if parts[0] == 'dir':
                assert source.is_dir()
                files = [p for p in source.rglob('*') if p.is_file() and any(p.match(s) for s in parts[3].split(';'))]
            else:
                assert parts[0] == 'file'
                files = [source]
            for source in sorted(files):
                target = frozen/source.relative_to(root)
                if target.exists():
                    continue
                expected = sha(source)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
                assert sha(target) == sha(source) == expected
                added.append(dict(Path=str(target), Source=str(source), Sha256=expected))
    for row in original['Inputs']:
        assert sha(frozen/row['Path']) == row['Sha256']
    receipt.write_text(json.dumps(dict(Complete=True, OriginalUnchanged=True,
        OriginalReceipt=str(frozen.with_suffix('.json')), Additions=added), indent=2)+'\n', encoding='utf8')
    print('Added', len(added), 'manifest-only files without changing the original cohort')


if __name__ == '__main__':
    main()
