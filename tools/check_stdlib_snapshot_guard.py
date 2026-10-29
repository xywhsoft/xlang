"""Cross-platform snapshot guard, including rejecting real stale artifacts.

Copies only the repository-local validation closure to a fresh private directory.
Never changes the supplied source tree, OS settings or build products.
This proves snapshot validation, not language execution or release acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and root.is_dir()
    sys.path.insert(0, str(root / 'tools'))
    import gen_xrt_xlang_matrix as matrix
    names = [
        'lib/xrt/xrt.h', 'lib/xrt/xrt.snapshot.json',
        'lib/xruntime/xruntime.h', 'lib/xruntime/xruntime.snapshot.json',
        'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
        'release/include/xl_runtime.h', 'docs/XRT与xlang能力矩阵.md',
        'tools/xrt_xlang_deferred_apis.txt', 'tools/sync_xrt_snapshot.py',
        'tools/gen_xrt_symbol_table.py', 'tools/gen_xrt_xlang_matrix.py',
        'tools/gen_xrt_symbol_table.ps1', 'tools/check_xrt_snapshot.ps1',
    ]
    names.extend(sorted({witness for contract in matrix.XRUNTIME_CAPABILITY_CONTRACT.values()
                         for witness in contract['witnesses']}))
    files = {root / name for name in names}
    files.update(path for path in (root / 'src').rglob('*') if path.is_file())
    assert all(path.is_file() for path in files)
    tools = {Path(__file__).resolve(), Path(sys.executable).resolve()}
    powershell = shutil.which('powershell') if os.name == 'nt' else None
    if os.name == 'nt':
        assert powershell
        tools.add(Path(powershell).resolve())
    inputs = [dict(Path=str(path), Sha256=sha(path)) for path in sorted(files | tools)]
    out.mkdir(parents=True)
    copied = out / 'validation-source'
    for path in sorted(files):
        target = copied / path.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    rows = []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Scope=__doc__, SnapshotGuardAcceptance=False,
                  LanguageExecution=False, ShippingLibraryAcceptance=False, Full53Acceptance=False)

    def run(case, reject=False):
        commands = [('python', [sys.executable, copied / 'tools/sync_xrt_snapshot.py',
                                '--root', copied, '--check'])]
        if powershell:
            commands.append(('powershell', [powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass',
                            '-File', copied / 'tools/check_xrt_snapshot.ps1', '-Root', copied]))
        for validator, command in commands:
            process = subprocess.run(list(map(str, command)), cwd=copied, capture_output=True, timeout=60)
            data = process.stdout + process.stderr
            log = out / (case + '-' + validator + '.log')
            log.write_bytes(data)
            rows.append(dict(Case=case, Validator=validator, Exit=process.returncode,
                             ExpectedRejection=reject, Command=list(map(str, command)),
                             Log=str(log), LogSha256=sha(log)))
            assert process.returncode != 0 if reject else process.returncode == 0, (case, validator, data[-4000:])
            if reject:
                assert b'stale' in data.lower(), (case, validator, data[-4000:])

    try:
        run('current')
        changes = [
            ('capability-count', 'docs/XRT与xlang能力矩阵.md',
             lambda data: data.replace(b'| direct | 6 | 3 |', b'| direct | 6 | 1 |', 1)),
            ('deferred-list', 'tools/xrt_xlang_deferred_apis.txt', lambda data: data + b'\nxrtUnknownApi\n'),
            ('symbol-table', 'src/api/xl_xrt_symbols.inc',
             lambda data: data.replace(b'XL_XRT_SYMBOL(', b'XL_STALE_SYMBOL(', 1)),
            ('metadata', 'lib/xrt/xrt.snapshot.json',
             lambda data: json.dumps({**json.loads(data), 'publicApiCount': -1}).encode()),
        ]
        for case, name, mutate in changes:
            path = copied / name
            original = path.read_bytes()
            changed = mutate(original)
            assert changed != original
            path.write_bytes(changed)
            try:
                run(case, True)
            finally:
                path.write_bytes(original)
        assert all(sha(copied / path.relative_to(root)) == sha(path) for path in files)
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report.update(Complete=True, SnapshotGuardAcceptance=True, NegativeControls=4)
    except Exception as error:
        report['Error'] = repr(error)
        print(repr(error), flush=True)
    report['Products'] = [dict(Path=str(path), Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Snapshot validation phases: ' + str(len(rows)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
