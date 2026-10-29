"""Stage exactly the private headers selected by the shipping VFS manifest."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
from gen_tcc_vfs_resources import load_resources


def stage(root, out, manifest=None):
    root, out = Path(root).resolve(), Path(out).resolve()
    manifest = Path(manifest).resolve() if manifest is not None else root / 'tools/tcc_vfs_manifest_linux_xlc.txt'
    rows = []
    for line in manifest.read_text(encoding='utf-8-sig').splitlines():
        parts = line.strip().split('|')
        if len(parts) >= 3 and (parts[1] == 'include/__xl_runtime' or
                               parts[1].startswith('include/__xl_runtime/')):
            rows.append(line)
    assert rows, 'The shipping manifest must select runtime headers'
    out.mkdir(parents=True, exist_ok=True)
    selected = out / 'runtime-headers.manifest'
    assert not selected.exists(), 'Preserve previous header evidence'
    selected.write_text('\n'.join(rows) + '\n', encoding='utf-8', newline='\n')
    records = []
    for resource in load_resources(selected, root, root, root, out):
        source, target = resource.source.resolve(), out / resource.virtual
        assert source.is_relative_to(root) and source.suffix == '.h'
        assert target.resolve().is_relative_to(out / 'include/__xl_runtime')
        assert not target.exists(), 'Do not replace previously staged headers'
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
        records.append(dict(Source=str(source), Path=str(target), Sha256=digest))
    assert records
    (out / 'runtime-headers.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
    pinned = [Path(__file__).resolve(), root / 'tools/gen_tcc_vfs_resources.py', manifest,
              selected, *(Path(row['Path']) for row in records)]
    with (out / 'inputs.sha256').open('a', encoding='utf-8', newline='\n') as stream:
        for path in pinned:
            stream.write(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + str(path) + '\n')
    return records


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print('Staged runtime headers:', len(stage(args.root, args.out)))
