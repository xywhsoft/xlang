#!/usr/bin/env python3
"""Native Sequence structural identity and complete equality allocation prefix.

This gate compiles the actual selected XRT/xruntime/resident code. It does not
claim language C/JIT/AOT execution or a complete class identity/float redesign.
Linux --sanitize instruments the complete selected implementation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    if out.exists() or not cc or not nm or (args.sanitize and os.name == 'nt'):
        parser.error('Require gcc/nm, fresh evidence directory and Linux for sanitizers')
    sources = ['test/cases/stdlib_sequence_identity_native_host.c', 'test/support/sequence_identity_native.c',
        'src/runtime/xl6_sequence.c', 'src/runtime/xl6_array.c', 'src/runtime/xl6_stack.c',
        'src/runtime/xl6_array_primitive_admission.c', 'src/runtime/xl6_array_reference_admission.c',
        'src/runtime/xl6_array_future_admission.c', 'src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_string_type.c', 'src/runtime/xl6_nullable_string.c',
        'src/runtime/xl6_nullable_i64.c', 'src/runtime/xl6_ownership_plan.c']
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable),
        root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h'}
    files.update(root / name for name in sources)
    for directory in ('src/runtime', 'release/include'): files.update((root / directory).rglob('*.h'))
    inputs = [{'Path': str(path.resolve()), 'Sha256': digest(path)} for path in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Sweeps': sweeps,
        'LanguageExecution': False, 'RuntimeInstrumented': args.sanitize, 'Scope': __doc__}
    out.mkdir(parents=True)
    env = dict(os.environ)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Irelease', '-Irelease/include']
    links = ['-lws2_32'] if os.name == 'nt' else ['-pthread', '-lm']
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name, command, timeout=120):
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': result.returncode == 0})
        assert result.returncode == 0, name + ': ' + data[-5000:].decode(errors='replace')
        return data.decode(errors='replace')
    try:
        for level in (0, 2):
            lane = f'O{level}'; binary = out / (lane + ('.exe' if os.name == 'nt' else ''))
            run(lane + '.build', [cc, *flags, f'-O{level}', *sources, *links, '-o', str(binary)], timeout=300)
            names = {line.split()[-1].lstrip('_') for line in run(lane + '.symbols',
                [nm, '--defined-only', str(binary)]).splitlines() if line.split()}
            assert {'xl6SequenceCreateAdmittedV1', 'xl6ArrayEqualsV1', 'xl6StackEqualsV1', 'xrtTypeFuture'} <= names
            assert not {'xl6ArrayFromValueV1', 'xl6ArrayToValueV1', 'xl6SequenceNominalCreateV1'} & names
            vectors = run(lane + '.vectors', [str(binary)])
            assert '30 cycles, scalar/text/managed/future/error/graph balanced' in vectors
            assert '80 simultaneous threads, colliding lanes, independent call paths' in vectors
            for point in range(513):
                text = run(f'{lane}.equal.{point}', [str(binary), str(point)])
                match = re.search(r'hit=([01]) balanced', text); assert match
                if match[1] == '0':
                    assert point > 0
                    sweeps.append({'Lane': lane, 'FirstNoHit': point}); break
            else: raise AssertionError('Unclosed equality allocation failure prefix')
        assert len(sweeps) == 2
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error); print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Sequence identity: {len(rows)} checks, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__ == '__main__': raise SystemExit(main())
