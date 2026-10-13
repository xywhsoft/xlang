#!/usr/bin/env python3
"""Native Stack projection, batch and owning two-result try foundation.
No dynamic try syntax, product IR/Cgen or full53 acceptance claim.
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
        parser.error('Require gcc/nm, a fresh output directory, and Linux for sanitizers')
    sources = ['test/cases/stdlib_stack_erased_native_host.c', 'test/support/array_value_native.c',
        'src/runtime/xl6_sequence.c', 'src/runtime/xl6_stack.c', 'src/runtime/xl6_stack_erased.c',
        'src/runtime/xl6_stack_erased_methods.c', 'src/runtime/xl6_stack_batch.c',
        'src/runtime/xl6_stack_erased_try.c',
        'src/runtime/xl6_char_value.c','src/runtime/xl6_char_type.c',
        'src/runtime/xl6_array.c', 'src/runtime/xl6_array_erased.c', 'src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_array_primitive_admission.c', 'src/runtime/xl6_array_reference_admission.c',
        'src/runtime/xl6_value_admission.c', 'src/runtime/xl6_string_type.c', 'src/runtime/xl6_string_value.c',
        'src/runtime/xl6_nullable_string.c', 'src/runtime/xl6_ownership_plan.c', 'src/api/xl_foreign_error.c']
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable),
        root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h', root / 'src/api/xl_foreign_error.h'}
    files.update(root / name for name in sources)
    for folder in ('src/runtime', 'release/include'):
        files.update((root / folder).rglob('*.h'))
    inputs = [{'Path': str(path.resolve()), 'Sha256': digest(path)} for path in sorted(files)]
    rows, sweeps, mutations = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Sweeps': sweeps,
        'Mutations': mutations,
        'LanguageExecution': False, 'Full53Acceptance': False, 'ShippingLibraryAcceptance': False,
        'RuntimeInstrumented': args.sanitize, 'Scope': __doc__}
    out.mkdir(parents=True)
    env = dict(os.environ)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL6_TEST_ERASED_STACK',
        '-I.', '-Irelease', '-Irelease/include']
    links = ['-Wl,--wrap=xrtTypedArrayTakeTail','-Wl,--wrap=xrtTypedArraySlice','-Wl,--wrap=xrtCallResultMove']
    if os.name!='nt': links += ['-pthread','-lm']
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

    def run(name: str, command: list[str], timeout=180) -> str:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': result.returncode == 0})
        assert result.returncode == 0, name + ': ' + data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')

    try:
        for level in (0, 2):
            lane = f'O{level}'
            binary = out / (lane + ('.exe' if os.name == 'nt' else ''))
            run(lane + '.build', [cc, *flags, f'-O{level}', *sources, *links, '-o', str(binary)])
            names = {line.split()[-1].lstrip('_') for line in run(lane + '.symbols',
                [nm, '--defined-only', str(binary)]).splitlines() if line.split()}
            assert {'xl6StackEraseV1', 'xl6StackErasedReadV1', 'xl6StackErasedPushV1',
                'xl6SequenceBeginErasedRoleV1', 'xl6SequenceBindProjectionRoleV1'} <= names
            assert 'Stack projection scalar/text/reference/role/failure/busy balanced' in run(lane + '.vectors', [str(binary)])
            for operation in ('peek', 'pop', 'push', 'clone', 'batch-peek', 'batch-pop','try-peek','try-pop','try-empty'):
                for point in range(257):
                    text = run(f'{lane}.{operation}.{point}', [str(binary), operation, str(point)])
                    match = re.search(r'hit=([01]) balanced', text)
                    assert match, 'Missing allocation/ownership outcome'
                    if match[1] == '0':
                        assert point > 0 or operation=='try-empty'
                        sweeps.append({'Lane': lane, 'Operation': operation, 'FirstNoHit': point})
                        break
                else:
                    raise AssertionError('Unclosed allocation failure prefix')
        assert len(sweeps) == 18
        # Compile actual broken implementations, never substitute a test stub
        # or count a compiler failure as a successful mutation detection.
        mutants = [
            ('try-ignores-independent-failure','src/runtime/xl6_stack_erased_try.c',
                ' || scope.failed', ''),
            ('try-leaks-published-on-no-diagnostic','src/runtime/xl6_stack_erased_try.c',
                'scope.failed && publicationAttempted','xrtGetError() && publicationAttempted'),
            ('try-null-value-becomes-missing','src/runtime/xl6_stack_erased_try.c',
                'if(!item || xrtGetError() || scope.failed)',
                'found=found && item && xrtValueType(item)!=XVALUE_NULL; if(!item || xrtGetError() || scope.failed)'),
            ('batch-result-leak-on-pending-failure', 'src/runtime/xl6_stack_batch.c',
                'if (scope.failed) { xrtValueRelease(result); result=NULL; }',
                'if (!success) { xrtValueRelease(result); result=NULL; }'),
            ('clone-loses-array-witness', 'src/runtime/xl6_sequence.c',
                'xl6SequenceBindStackResultArrayV1(target,owner->resultArrayIdentity)',
                'xl6SequenceBindStackResultArrayV1(target,owner->resultArrayIdentity+1)'),
            ('pop-after-failure', 'src/runtime/xl6_stack_erased.c',
                'result && pop && !scope.failed && !xrtTypedArrayRemove',
                'result && pop && !xrtTypedArrayRemove'),
            ('cross-role-probe', 'src/runtime/xl6_sequence.c',
                'if (owner->role==role) {',
                'if (owner->role==XL6_SEQUENCE_ARRAY_V1 || owner->role==XL6_SEQUENCE_STACK_V1) {'),
            ('clone-loses-projection', 'src/runtime/xl6_sequence.c',
                'xl6SequenceBindProjectionRoleV1(target,owner->role,owner->projection)',
                'xl6SequenceBindProjectionV1(target,owner->projection)'),
        ]
        for name, selected, before, after in mutants:
            original = (root / selected).read_text(encoding='utf-8-sig')
            assert original.count(before) >= 1 if name=='try-ignores-independent-failure' else original.count(before)==1, name
            mutant = out / (name + '.c')
            mutant.write_text(original.replace(before, after), encoding='utf-8')
            providers = [str(mutant) if path == selected else path for path in sources]
            binary = out / (name + ('.exe' if os.name == 'nt' else ''))
            run(name + '.build', [cc, *flags, '-O0', *providers, *links, '-o', str(binary)])
            result = subprocess.run([str(binary)], cwd=root, env=env, capture_output=True, timeout=180)
            rejected = result.stdout + result.stderr
            (out / (name + '.reject.log')).write_bytes(rejected)
            assert result.returncode != 0, 'Broken implementation passed: ' + name
            assert b'stdlib_stack_erased_native_host.c' in rejected and b'assert' in rejected.lower(), name
            mutations.append({'Name': name, 'Source': selected, 'MutantSha256': digest(mutant),
                'RuntimeExit': result.returncode, 'Rejected': True})
        assert len(mutations) == 8
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Stack projection: {len(rows)} checks, {len(sweeps)} closed allocation prefixes, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
