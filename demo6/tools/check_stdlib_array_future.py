#!/usr/bin/env python3
"""Native Future Array slots: exact projection, explicit graph admission and full OOM prefixes.

The source language Array carrier is not migrated by this gate. Standalone lanes
compile the actual selected XRT/xruntime and resident implementation. Linux
sanitizers instrument that entire selection; --shared instruments the host only.
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
    parser.add_argument('--shared', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    objdump = shutil.which('objdump') if args.shared and os.name == 'nt' else None
    if out.exists() or not cc or not nm or (args.sanitize and os.name == 'nt') or (args.shared and os.name == 'nt' and not objdump):
        parser.error('Require gcc/nm, fresh evidence path, and Linux for sanitizer lanes')
    host = 'test/cases/stdlib_array_future_native_host.c'
    retired = 'test/cases/stdlib_sequence_retired_admission_abi.c'
    sources = [host, 'test/support/array_future_native.c',
        'src/runtime/xl6_sequence.c', 'src/runtime/xl6_array.c',
        'src/runtime/xl6_array_future_admission.c', 'src/runtime/xl6_array_erased.c',
        'src/runtime/xl6_array_value.c', 'src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_array_iterator.c', 'src/runtime/xl6_array_input.c',
        'src/runtime/xl6_string_type.c', 'src/runtime/xl6_nullable_string.c',
        'src/runtime/xl6_ownership_plan.c']
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / retired,
        root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h'}
    files.update(root / name for name in sources)
    for directory in ('src/runtime', 'release/include'):
        files.update((root / directory).rglob('*.h'))
    if args.shared:
        files.add(product)
        if objdump:
            files.add(Path(objdump))
    inputs = [{'Path': str(p.resolve()), 'Sha256': digest(p)} for p in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs,
        'Rows': rows, 'Sweeps': sweeps, 'SharedRuntime': args.shared,
        'Sanitized': args.sanitize, 'RuntimeInstrumented': args.sanitize and not args.shared,
        'Scope': __doc__}
    out.mkdir(parents=True)
    env = dict(os.environ)
    if args.shared and os.name == 'nt':
        shutil.copy2(product, out / 'xl.dll')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Irelease', '-Irelease/include']
    links = [] if os.name == 'nt' else ['-pthread', '-lm']
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

    def run(name, command, reject=False, timeout=180):
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out / (name + '.log')).write_bytes((error.stdout or b'') + (error.stderr or b''))
            rows.append({'Name': name, 'Command': command, 'Complete': False, 'TimedOut': True})
            raise
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        ok = result.returncode != 0 if reject else result.returncode == 0
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': ok, 'Rejection': reject})
        if not ok:
            raise AssertionError(name + ': ' + data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')

    try:
        for level in (0, 2):
            lane = f'O{level}'
            suffix = '.exe' if os.name == 'nt' else ''
            binary = out / (lane + suffix)
            selected = [host] if args.shared else sources
            linked = ([str(product)] if os.name == 'nt' else ['-Lrelease', '-lxl',
                '-Wl,-rpath,' + str(root / 'release'), *links]) if args.shared else links
            run(lane + '.build', [cc, *flags, f'-O{level}', *selected, *linked, '-o', str(binary)])
            symbols = run(lane + '.symbols', [objdump, '-p', str(product)] if objdump else
                [nm, *(['-D'] if args.shared and os.name != 'nt' else []),
                '--defined-only', str(product if args.shared else binary)])
            names = {line.split()[-1].lstrip('_') for line in symbols.splitlines() if line.split()}
            assert {'xl6SequenceFutureCreateV1', 'xl6ArrayFutureAdmissionOpsV2Get', 'xl6SequenceOwnershipAdapterV2',
                'xl6SequenceAdmissionTakeV3', 'xrtTypeFuture', 'xrtTypedArrayCreate'} <= names
            assert not {'xl6SequenceAdmissionCopyV1', 'xl6SequenceAdmissionTakeV1', 'xl6SequenceAdmissionDropV1',
                'xl6SequenceAdmissionCopyV2', 'xl6SequenceAdmissionTakeV2', 'xl6SequenceAdmissionDropV2'} & names
            if not args.shared:
                assert not any(re.match(r'(?:xl6(?:Regex|Log|Process|Stack)|xrt(?:Regex|Logger|Process|TypedStack))[A-Z]', n) for n in names)
                assert not {'xl6FutureRef', 'xl6FutureAggregatePolicyV1Get', 'xl6SequenceCallableCreateV1',
                    'xl6SequenceNominalCreateV1', 'xl6ClosureRuntimeV1Get', 'xl6CodeLeaseAcquire'} & names
            vectors = run(lane + '.vectors', [str(binary)])
            assert '200 physical cycles balanced' in vectors
            for version in (1, 2):
                obj = out / f'{lane}.retired-v{version}.o'
                defines = ['-DXL6_TEST_RETIRED_SEQUENCE_V2'] if version == 2 else []
                run(f'{lane}.retired-v{version}.compile', [cc, *flags, *defines, '-c', retired, '-o', str(obj)])
                retired_sources = [] if args.shared else [s for s in sources if s != host]
                rejection = run(f'{lane}.retired-v{version}.link', [cc, *flags, str(obj), *retired_sources,
                    *linked, '-o', str(out / f'{lane}.retired-v{version}{suffix}')], reject=True)
                assert all(f'xl6SequenceAdmission{op}V{version}' in rejection for op in ('Copy', 'Take', 'Drop'))
            for profile in (0, 1):
                operations = ['create', 'clone', 'cursor', 'read', 'erased-read', 'erased-take',
                    'snapshot', 'input', 'push', 'erased-push'] + (['resize'] if profile else [])
                for operation in operations:
                    for point in range(256):
                        text = run(f'{lane}.{profile}.{operation}.{point}', [str(binary), str(profile), operation, str(point)])
                        match = re.search(r'hit=([01]) balanced', text)
                        assert match, 'Missing actual allocation/lifetime outcome'
                        if match[1] == '0':
                            sweeps.append({'Lane': lane, 'Profile': profile, 'Operation': operation, 'FirstNoHit': point})
                            break
                    else:
                        raise AssertionError('Unclosed OOM prefix: ' + operation)
        assert len(sweeps) == 42
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Future Array: {len(rows)} checks, {len(sweeps)} complete prefixes, '
        f'{sum(row["FirstNoHit"]+1 for row in sweeps)} positions, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
