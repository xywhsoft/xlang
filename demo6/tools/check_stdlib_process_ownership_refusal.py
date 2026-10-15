"""Fail closed on the uncertified Process graph, using real source providers.

Three actual process states and a shared terminal Future; the original leaf
provider is an expected-rejection control. This does not certify Process/IO
graph collection, shipping/VFS rebuilding, or full53 acceptance.
"""
from pathlib import Path
import argparse, json, os, shutil, subprocess, sys
from xlir_public_build_inputs import sha, verify_public_build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'leaf-baseline-root', 'leaf-baseline-public', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    build, pinned, compared = verify_public_build(root, args.public_directory)
    baseline, old_inputs, _ = verify_public_build(args.leaf_baseline_root, args.leaf_baseline_public)
    pinned.update(old_inputs)
    cc = shutil.which('gcc')
    assert cc
    contract = root / 'test/support/process_ownership_refusal.c'
    numeric = root / 'test/support/native_scalar_convert_contract.c'
    memory = root / 'test/support/module_product_memory.c'
    child_source = root / 'test/cases/xlir_process_child.c'
    fixtures = [root / 'test/cases' / (name + '.xl') for name in (
        'stdlib_process_live_wait', 'stdlib_process_live_streams', 'stdlib_process_live_drop',
        'stdlib_process_future', 'stdlib_process_future_cancel', 'stdlib_process_future_drop',
        'stdlib_native_scalar_convert', 'stdlib_native_scalar_as', 'stdlib_native_scalar_errors')]
    runtime = root / 'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot']) / 'release/libxl.so'
    pinned.update((Path(__file__).resolve(), Path(sys.executable), Path(cc), contract, numeric, memory, child_source, runtime, *fixtures))
    inputs = [dict(Path=str(path), Sha256=sha(path)) for path in sorted(pinned)]
    objects = [Path(row['Path']) for row in build['Objects']
               if Path(row['Path']).stem not in ('xlir_public_source_driver', 'xlir_public_source_linux_driver')]
    old = [Path(row['Path']) for row in baseline['Objects'] if Path(row['Path']).stem == 'xl6_process_live']
    assert len(old) == 1 and sum(path.stem == 'xl6_process_live' for path in objects) == 1
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I' + str(root), '-I' + str(root / 'release'), '-I' + str(root / 'release/include')]
    libraries = [str(runtime)]
    if os.name == 'nt':
        libraries += ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']
        libraries += ['-pthread', '-ldl', '-lm', '-Wl,-rpath,' + str(runtime.parent)]
    out.mkdir(parents=True)
    if os.name == 'nt':
        shutil.copy2(runtime, out / runtime.name)
    rows, failures = [], []
    environment = dict(os.environ)
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Failures=failures, ComparedSources=compared, Scope=__doc__,
                  ProcessGraphAcceptance=False, Full53Acceptance=False, ShippingAcceptance=False,
                  ProvidersInstrumented=False, LeafBaselineRejected=False)
    suffix = '.exe' if os.name == 'nt' else ''

    def run(name, command, reject=False):
        command = list(map(str, command))
        process = subprocess.run(command, cwd=root, env=environment, capture_output=True, timeout=180)
        data = process.stdout + process.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        rows.append(dict(Name=name, Command=command, Exit=process.returncode,
                         ExpectedRejection=reject, Log=str(log), LogSha256=sha(log)))
        assert process.returncode == (1 if reject else 0), (name, data[-4000:].decode(errors='replace'))
        print(name + ': ' + str(process.returncode), flush=True)
        return data.decode(errors='replace')

    try:
        child = out / ('process-child' + suffix)
        run('child.build', [cc, '-std=c11', '-O2', *(['-municode'] if os.name == 'nt' else ['-D_GNU_SOURCE']),
                            child_source, '-o', child])
        environment['XL6_PROCESS_CHILD'] = str(child)
        if os.name != 'nt':
            environment['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
        for optimization in ('O0', 'O2'):
            binary = out / ('process.' + optimization + suffix)
            run(optimization + '.build', [cc, *flags, '-' + optimization, contract, *objects, *libraries, '-o', binary])
            output = run(optimization + '.run', [binary, child])
            assert output.count('direct-refused=1 snapshot-refused=1') == 3
            assert 'shared Future and ordinary destruction preserved' in output
            original = out / ('original-leaf.' + optimization + suffix)
            legacy = [old[0] if path.stem == 'xl6_process_live' else path for path in objects]
            run(optimization + '.original.build', [cc, *flags, '-' + optimization, contract, *legacy, *libraries, '-o', original])
            output = run(optimization + '.original.reject', [original, child], True)
            assert output.count('direct-refused=0 snapshot-refused=0') == 3
            assert 'uncertified process was accepted as a leaf' in output
            binary = out / ('numeric.' + optimization + suffix)
            run(optimization + '.numeric.build', [cc, *flags, '-' + optimization, numeric, memory, *objects, *libraries, '-o', binary])
            output = run(optimization + '.numeric.run', [binary])
            assert 'profiles=20 pairs=660 range-rejections=69' in output
            assert 'reflection exclusions=20' in output
        strict = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
                  '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                  '-Wno-missing-field-initializers', '-Wno-pointer-sign']
        for fixture in fixtures:
            generated = out / (fixture.stem + '.c')
            run(fixture.stem + '.cgen', [build['Driver'], fixture, 'cgen-default', generated])
            run(fixture.stem + '.strict', [cc, *flags, *strict, '-I' + str(args.public_directory / 'include'),
                                         '-O2', '-fsyntax-only', generated])
            for mode in ('run', 'module'):
                run(fixture.stem + '.' + mode, [build['Driver'], fixture, mode, out / (fixture.stem + '.' + mode)])
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report.update(Complete=True, ProcessLeafRefusalAcceptance=True, LeafBaselineRejected=True,
                      NumericConversionAcceptance=True, LanguageFixtureCount=len(fixtures))
    except Exception as error:
        failures.append(repr(error))
        print(repr(error), file=sys.stderr)
    report['Products'] = [dict(Path=str(path), Sha256=sha(path)) for path in sorted(out.iterdir()) if path.is_file()]
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
