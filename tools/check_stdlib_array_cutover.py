#!/usr/bin/env python3
"""Batch Array producer/copy/dynamic/persistence discovery; never treats failures as acceptance."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--public-directory', required=True, type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    if out.exists(): parser.error('Use fresh evidence; do not overwrite a prior failing batch')
    manifest = args.public_directory.resolve() / 'build.json'
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    cli = Path(build['Driver'])
    fixtures = [
        'stdlib_array_create_language', 'stdlib_array_algebra_language', 'stdlib_array_import_language',
        'stdlib_array_methods', 'stdlib_array_methods_continuation',
        'stdlib_array_resident_defaults', 'stdlib_array_resident_defaults_continuation',
        'stdlib_array_extract', 'stdlib_array_extract_continuation', 'stdlib_array_extract_trivial',
        'stdlib_array_future_reference', 'stdlib_array_value_read', 'stdlib_array_value_read_continuation',
        'stdlib_array_copy_language', 'stdlib_array_clone_language',
        'stdlib_array_dynamic_language', 'stdlib_array_dynamic_algebra_language',
        'stdlib_array_persistence_language', 'stdlib_persistence_project_language',
        'stdlib_text_split', 'stdlib_text_binary', 'stdlib_text_join',
        'stdlib_array_create_oom', 'stdlib_dynamic_numeric_prune',
        'cgen_integer_division_errors', 'xlir_checked_integer_arithmetic_abi',
    ]
    files = {Path(__file__).resolve(), manifest, cli}
    for row in build['Inputs'] + build['Headers'] + build['Objects']:
        path = Path(row['Path'])
        if digest(path).lower() != row['Sha256'].lower(): parser.error('Stale compiler input: ' + str(path))
        files.add(path)
    files.update(root / 'test/cases' / (name + '.xl') for name in fixtures)
    inputs = [{'Path': str(path), 'Sha256': digest(path)} for path in sorted(files)]
    rows = []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__, 'Inputs': inputs, 'Rows': rows,
              'Sanitized': False, 'StrictC': False, 'AOT': False}
    out.mkdir(parents=True)
    env = dict(os.environ)
    for fixture in fixtures:
        source = root / 'test/cases' / (fixture + '.xl')
        for mode in ('ir-lowered', 'cgen-default', 'run'):
            target = out / (fixture + '.' + mode + ('.json' if mode == 'ir-lowered' else '.c' if mode == 'cgen-default' else ''))
            command = [str(cli), str(source), mode, str(target)]
            try:
                result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=180)
                data = result.stdout + result.stderr
                code = result.returncode
            except subprocess.TimeoutExpired as error:
                code, data = -1, (error.stdout or b'') + (error.stderr or b'') + b'\nTimed out\n'
            (out / (fixture + '.' + mode + '.log')).write_bytes(data)
            rows.append({'Fixture': fixture, 'Mode': mode, 'Exit': code, 'Complete': code == 0, 'Command': command})
            print(f'{fixture} {mode}: {code}', flush=True)
    report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
    report['Complete'] = report['InputsUnchanged'] and all(row['Complete'] for row in rows)
    report['Failures'] = [row for row in rows if not row['Complete']]
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'{len(rows)} checks, {len(report["Failures"])} failures; unchanged={report["InputsUnchanged"]}', flush=True)
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
