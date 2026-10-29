#!/usr/bin/env python3
"""Verify current resident container bridges using JIT and standalone AOT.

Ordinary local value, range, batch and persistence tests only. No network.
Static source contracts remain in check_xruntime_language_bridge.ps1.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--xl', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, xl, out = args.root.resolve(), args.xl.resolve(), args.out.resolve()
    if out == root or root in out.parents:
        raise ValueError('Validation products must be outside the source tree')
    out.mkdir(parents=True, exist_ok=False)
    cases = ['stdlib_dynamic_fields_resident', 'stdlib_stack_native',
             'stdlib_concurrent_queue_spsc_only', 'stdlib_concurrent_queue_mpsc_only',
             'stdlib_concurrent_queue_mpmc_only', 'stdlib_concurrent_queue_owned',
             'stdlib_container_projection_tree_only', 'stdlib_persistence_project_language']
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    inputs = {str(p): sha(p) for p in [xl, Path(__file__).resolve(),
              *[root / ('test/cases/' + case + '.xl') for case in cases]]}
    rows = []
    report = dict(complete=False, inputs=inputs, rows=rows, networkTests=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command):
        start = time.monotonic()
        result = subprocess.run(list(map(str, command)), cwd=out, capture_output=True, timeout=180)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        rows.append(dict(name=name, exit=result.returncode, seconds=time.monotonic()-start,
                         log=str(log), sha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(str(log))
        return (result.stdout + result.stderr).decode('utf-8', 'replace')

    save()
    for case in cases:
        source = root / ('test/cases/' + case + '.xl')
        run(case + '.jit', [xl, 'run', source])
        binary = out / (case + ('.exe' if os.name == 'nt' else ''))
        build_output = run(case + '.build', [xl, 'build', source, '-o', binary,
            *(['--save-temps'] if case == 'stdlib_concurrent_queue_spsc_only' else [])])
        run(case + '.aot', [binary])
        # This checks actual per-program closure. An unused MPMC declaration
        # in the SPSC fixture must not silently select all queue families.
        if case == 'stdlib_concurrent_queue_spsc_only':
            match = re.search(r'temporary files kept: (.+)', build_output)
            assert match, build_output
            selected = Path(match.group(1).strip()) / 'xl6_xrt_impl.c'
            text = selected.read_text(encoding='utf-8')
            (out / 'spsc-selected-native.c').write_text(text, encoding='utf-8')
            assert '#define XRUNTIME_MODULE_TYPED_QUEUE_SPSC' in text
            assert '#define XRUNTIME_MODULE_TYPED_QUEUE_MPMC' not in text
            assert '#define XRUNTIME_MODULE_ALL' not in text
    report['inputsUnchanged'] = all(sha(Path(p)) == digest for p, digest in inputs.items())
    assert report['inputsUnchanged']
    report['complete'] = True
    save()


if __name__ == '__main__':
    main()
