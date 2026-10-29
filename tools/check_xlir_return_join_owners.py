"""Check conditional/coalesce owner acquisition with a coherent source compiler.

Preserves the original product-owner mutation population and its 10,500-case
native lifetime/error ledger. Only generated include paths are adapted.
"""
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
    parser.add_argument('--native-root', type=Path)
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--ar', default='ar')
    args = parser.parse_args()
    root, compiler, work = args.root.resolve(), args.compiler_directory.resolve(), args.work_directory.resolve()
    if work.exists(): parser.error('Choose a fresh work directory')
    work.mkdir(parents=True)
    windows = os.name == 'nt'
    suffix = '.exe' if windows else ''
    cc, ar = shutil.which(args.cc), shutil.which(args.ar)
    if not cc or not ar: parser.error('The C compiler and archive tool must be available')
    environment = dict(os.environ)
    if not windows:
        if args.native_root is None: parser.error('Linux requires --native-root for the genuine pinned TCC dependency')
        native = args.native_root.resolve()
        environment['XL6_SOURCE_NATIVE_ROOT'] = str(native)
    def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    manifest = compiler/'build.json'
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and not build.get('RuntimeOnly', False)
    driver = Path(build['Driver'])
    assert sha(driver).lower() == build['DriverSha256'].lower()
    inputs = {}
    for section in ('Inputs', 'Headers', 'Objects'):
        for item in build.get(section, []):
            assert sha(item['Path']).lower() == item['Sha256'].lower(), item['Path']
            inputs[item['Path']] = sha(item['Path'])
    for path in (manifest, driver, Path(__file__), Path(cc), Path(ar)):
        inputs[str(path)] = sha(path)
    cases = work/'inputs'
    cases.mkdir()
    files = ['xlir_return_projection_owner_contract.c', 'xlir_return_projection_owner_runtime.c',
             'xlir_return_projection_owner_host.c', 'xlir_conditional_join_owners.xl',
             'xlir_tagged_enum_sequence_host.c']
    for name in files:
        source, target = args.case_directory.resolve()/name, cases/name
        shutil.copyfile(source, target)
        inputs[str(source)], inputs[str(target)] = sha(source), sha(target)
    rows = []
    def run(name, command, marker=None):
        command = list(map(str, command)); start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=environment,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=240)
            code, output = result.returncode, result.stdout
        except subprocess.TimeoutExpired as error:
            code, output = None, error.output or b''
        except OSError as error:
            code, output = -error.errno, str(error).encode('utf-8')
        text = output.decode('utf-8', errors='replace'); log = work/(name+'.log'); log.write_bytes(output)
        passed = code == 0 and (marker is None or marker in text)
        rows.append(dict(Name=name, Command=command, Exit=code, Passed=passed, Seconds=time.monotonic()-start,
                         Marker=marker, Log=str(log), Sha256=sha(log)))
        (work/'progress.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(f'[return-join] {name}: {"passed" if passed else "FAILED"}', flush=True)
        if not passed: print(text[-3000:], flush=True)
        return passed
    def compile_c(name, arguments):
        response = work/(name+'.rsp')
        response.write_text('\n'.join('"'+str(value).replace('\\', '/').replace('"', '\\"')+'"'
                                      for value in arguments)+'\n', encoding='utf-8')
        return run(name, [cc, '@'+str(response)])
    objects = [item['Path'] for item in build['Objects'] if Path(item['Path']).name not in
               ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    archive = work/'resident.a'; response = work/'archive.rsp'
    response.write_text('\n'.join('"'+value.replace('\\', '/')+'"' for value in objects)+'\n', encoding='utf-8')
    archive_ready = run('resident-archive', [ar, 'rcs', archive, '@'+str(response)])
    flags = [value for value in build['CompileFlags'] if value not in ('-O0', '-O1', '-O2', '-O3', '-Os', '-Og')]
    flags += ['-I'+str(root), '-I'+str(compiler/'include'), '-I'+str(root/'release/include'),
              '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-label',
              '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-const-variable',
              '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Wno-pointer-sign', '-Wno-format-truncation']
    libraries = [root/'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if windows else [
        '-L'+str(native/'release'), '-lxl', '-Wl,-rpath,'+str(native/'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    if windows: shutil.copyfile(root/'release/xl.dll', work/'xl.dll')
    contract = work/('return-projection-contract'+suffix)
    generated, header = work/'return-projection.c', work/'return-projection.h'
    contract_ready = archive_ready and compile_c('contract-build', [*flags, '-O2',
        cases/'xlir_return_projection_owner_contract.c', archive, *libraries, '-o', contract])
    if contract_ready and run('contract', [contract, generated, header], 'mutations rejected before C/header output'):
        repeat_c, repeat_h = work/'return-projection-repeat.c', work/'return-projection-repeat.h'
        if run('contract-repeat', [contract, repeat_c, repeat_h], 'skipped-acquisition CFG mutations rejected'):
            assert generated.read_bytes() == repeat_c.read_bytes() and header.read_bytes() == repeat_h.read_bytes()
        host = work/'return-projection-host.c'
        text = (cases/'xlir_return_projection_owner_host.c').read_text(encoding='utf-8-sig')
        for original, target in [('../../release/include/xrt_decl.h', root/'release/include/xrt_decl.h'),
                                 ('../../build/test/xlir_return_projection_owner.h', header)]:
            assert text.count(original) == 1
            text = text.replace(original, target.as_posix())
        host.write_text(text, encoding='utf-8'); inputs[str(host)] = sha(host)
        for optimization in ('-O0', '-O2'):
            executable = work/('return-projection'+optimization+suffix)
            if compile_c('return-projection'+optimization+'-build', [*flags, optimization, generated,
                cases/'xlir_return_projection_owner_runtime.c', host, archive, *libraries, '-o', executable]):
                run('return-projection'+optimization, [executable], '10500 cases, 3000 real diagnostic allocation failures')
    source = cases/'xlir_conditional_join_owners.xl'; generated = work/'join-program.c'
    emitted = run('join-emit', [driver, source, 'cgen-default', generated])
    run('join-jit', [driver, source, 'run', work/'join-jit'], '192 calls, shared identity')
    run('join-module', [driver, source, 'module', work/'join-module'], '192 calls, shared identity')
    executable = work/('join-aot'+suffix)
    if run('join-aot-build', [driver, source, 'exe', executable.with_suffix('') if windows else executable]):
        run('join-aot', [executable], '192 calls, shared identity')
    if emitted and archive_ready:
        for optimization in ('-O0', '-O2'):
            executable = work/('join-program'+optimization+suffix)
            if compile_c('join-program'+optimization+'-build', [*flags, optimization, '-include', generated,
                cases/'xlir_tagged_enum_sequence_host.c', archive, *libraries, '-o', executable]):
                run('join-program'+optimization, [executable], 'three complete module lifetimes balanced')
    source = root/'test/cases/cgen_var_return_exprs.xl'
    inputs[str(source)] = sha(source)
    run('original-return-emit', [driver, source, 'cgen-default', work/'original-return-generated.c'])
    # Only the expect_i64 sidecar is needed; its older string ABI is not used.
    sidecar = root/'test/cases/cgen_class_host.c'; inputs[str(sidecar)] = sha(sidecar)
    run('original-return-jit', [driver, source, 'run', work/'original-return-jit', sidecar])
    changed = [path for path, digest in inputs.items() if sha(path) != digest]
    report = dict(Complete=bool(rows) and not changed and all(row['Passed'] for row in rows), Scope=__doc__,
                  Inputs=inputs, ChangedInputs=changed, Rows=rows, NativeHostAdaptation='Two generated/public include paths only',
                  ShippingAccepted=False)
    (work/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Return joins: {sum(row["Passed"] for row in rows)}/{len(rows)} checks passed', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    sys.exit(main())
