"""Ordinary callable-result ABI, JIT/module/AOT and native ownership checks."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler-directory', 'case-directory', 'work-directory'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--baseline-directory', type=Path)
    parser.add_argument('--native-root', type=Path)
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--ar', default='ar')
    args = parser.parse_args()
    root, compiler, work = args.root.resolve(), args.compiler_directory.resolve(), args.work_directory.resolve()
    if work.exists(): parser.error('Choose a fresh work directory')
    work.mkdir(parents=True)
    windows = os.name == 'nt'; suffix = '.exe' if windows else ''
    cc, ar = shutil.which(args.cc), shutil.which(args.ar)
    if not cc or not ar: parser.error('C compiler and archive tool are required')
    environment = dict(os.environ)
    if not windows:
        if args.native_root is None: parser.error('Linux needs the genuine pinned TCC dependency')
        native = args.native_root.resolve()
        environment['XL6_SOURCE_NATIVE_ROOT'] = str(native)

    def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    manifest = compiler/'build.json'
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and not build.get('RuntimeOnly', False)
    driver = Path(build['Driver']); assert sha(driver).lower() == build['DriverSha256'].lower()
    inputs = {str(path): sha(path) for path in (manifest, driver, Path(__file__), Path(cc), Path(ar))}
    for section in ('Inputs', 'Headers', 'Objects'):
        for item in build.get(section, []):
            assert sha(item['Path']).lower() == item['Sha256'].lower(), item['Path']
            inputs[item['Path']] = sha(item['Path'])
    names = ('xlir_callable_container_results', 'cgen_typed_container_for_in_returned')
    cases = work/'inputs'; cases.mkdir()
    for name in (names[0]+'.xl', names[1]+'.xl', names[0]+'_contract.c', names[0]+'_host.c'):
        source, target = args.case_directory.resolve()/name, cases/name
        shutil.copyfile(source, target)
        inputs[str(source)], inputs[str(target)] = sha(source), sha(target)
    rows = []

    def run(name, command, marker=None, expected=0):
        command = list(map(str, command)); start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=environment, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=240)
            code, output = result.returncode, result.stdout
        except subprocess.TimeoutExpired as error:
            code, output = None, error.output or b''
        except OSError as error:
            code, output = -error.errno, str(error).encode('utf-8')
        text = output.decode('utf-8', errors='replace'); log = work/(name+'.log'); log.write_bytes(output)
        passed = code == expected and (marker is None or marker in text)
        rows.append(dict(Name=name, Command=command, Exit=code, ExpectedExit=expected, Passed=passed,
            Seconds=time.monotonic()-start, Marker=marker, Log=str(log), Sha256=sha(log)))
        (work/'progress.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(f'[callable-results] {name}: {"passed" if passed else "FAILED"}', flush=True)
        if not passed: print(text[-2600:], flush=True)
        return passed

    def compile_c(name, arguments):
        rsp = work/(name+'.rsp')
        rsp.write_text('\n'.join('"'+str(v).replace('\\', '/').replace('"', '\\"')+'"'
            for v in arguments)+'\n', encoding='utf-8')
        return run(name, [cc, '@'+str(rsp)])

    if args.baseline_directory:
        old_manifest = args.baseline_directory.resolve()/'build.json'
        before = json.loads(old_manifest.read_text(encoding='utf-8-sig'))
        old_driver = Path(before['Driver'])
        assert before['Complete'] and sha(old_driver).lower() == before['DriverSha256'].lower()
        inputs.update({str(old_manifest): sha(old_manifest), str(old_driver): sha(old_driver)})
        run('before-original-refusal', [old_driver, cases/(names[1]+'.xl'), 'cgen-default', work/'before.c'],
            'XLIR closure call has no exact signature and result ABI', expected=1)

    objects = [v['Path'] for v in build['Objects'] if Path(v['Path']).name not in
        ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    archive = work/'resident.a'; rsp = work/'archive.rsp'
    rsp.write_text('\n'.join('"'+v.replace('\\', '/')+'"' for v in objects)+'\n', encoding='utf-8')
    archive_ready = run('resident-archive', [ar, 'rcs', archive, '@'+str(rsp)])
    flags = [v for v in build['CompileFlags'] if v not in ('-O0', '-O1', '-O2', '-O3', '-Os', '-Og')]
    flags += ['-I'+str(root), '-I'+str(compiler/'include'), '-I'+str(root/'release/include'),
        '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-Wno-pointer-sign', '-Wno-format-truncation']
    libraries = [root/'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if windows else [
        '-L'+str(native/'release'), '-lxl', '-Wl,-rpath,'+str(native/'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    if windows: shutil.copyfile(root/'release/xl.dll', work/'xl.dll')
    contract = work/('callable-result-contract'+suffix)
    if archive_ready and compile_c('contract-build', [*flags, '-O2', cases/(names[0]+'_contract.c'), archive, *libraries, '-o', contract]):
        run('contract', [contract, cases/(names[0]+'.xl')], '105 mutations refused before C/public/native output')
    for name in names:
        source = cases/(name+'.xl'); generated = work/(name+'.c')
        emitted = run(name+'-emit', [driver, source, 'cgen-default', generated])
        repeated = work/(name+'.repeat.c')
        if run(name+'-emit-repeat', [driver, source, 'cgen-default', repeated]) and emitted:
            assert generated.read_bytes() == repeated.read_bytes()
        for mode in ('run', 'module'):
            run(name+'-'+mode, [driver, source, mode, work/(name+'-'+mode)])
        executable = work/(name+'-aot'+suffix)
        if run(name+'-aot-build', [driver, source, 'exe', executable.with_suffix('') if windows else executable]):
            run(name+'-aot', [executable])
        if emitted and archive_ready:
            for opt in ('-O0', '-O2'):
                executable = work/(name+opt+suffix)
                entry = ['-DCALLABLE_RESULT_LIBRARY'] if name == names[1] else []
                if compile_c(name+opt+'-build', [*flags, *entry, opt, '-include', generated,
                    cases/(names[0]+'_host.c'), archive, *libraries, '-o', executable]):
                    run(name+opt, [executable], 'three complete module lifetimes balanced')
    for name in ('xlir_function_value_calls', 'xlir_generic_closure_field', 'xlir_generic_callable_convention',
                 'xlir_property_overload_selection', 'xlir_nullable_getter'):
        source = root/'test/cases'/(name+'.xl'); inputs[str(source)] = sha(source)
        run(name+'-emit', [driver, source, 'cgen-default', work/(name+'.c')])
        run(name+'-run', [driver, source, 'run', work/(name+'-run')])
    changed = [p for p, h in inputs.items() if sha(p) != h]
    report = dict(Complete=bool(rows) and not changed and all(v['Passed'] for v in rows), Scope=__doc__,
        Rows=rows, Inputs=inputs, ChangedInputs=changed, ShippingAccepted=False, Full74GuardsPassed=False)
    (work/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Callable results: {sum(v["Passed"] for v in rows)}/{len(rows)} checks passed', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    sys.exit(main())
