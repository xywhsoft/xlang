"""Validate tagged-enum sequence schemas and independent Stack return owners.

Uses a coherent current-source compiler profile and ordinary local ABI,
lifetime and allocation-failure checks. Retains every result independently.
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
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--compiler-directory', type=Path, required=True)
    parser.add_argument('--case-directory', type=Path, required=True)
    parser.add_argument('--work-directory', type=Path, required=True)
    parser.add_argument('--native-root', type=Path)
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--ar', default='ar')
    args = parser.parse_args()
    root, compiler, work = args.root.resolve(), args.compiler_directory.resolve(), args.work_directory.resolve()
    if work.exists():
        parser.error('Choose a fresh work directory')
    work.mkdir(parents=True)
    windows = os.name == 'nt'
    suffix = '.exe' if windows else ''
    cc, ar = shutil.which(args.cc), shutil.which(args.ar)
    if not cc or not ar:
        parser.error('The C compiler and archive tool must be available')
    environment = dict(os.environ)
    if not windows:
        if args.native_root is None:
            parser.error('Linux requires --native-root for the genuine pinned TCC dependency')
        native = args.native_root.resolve()
        environment['XL6_SOURCE_NATIVE_ROOT'] = str(native)

    def sha(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

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
    names = ['xlir_tagged_enum_sequence', 'xlir_tagged_enum_sequence_oom', 'xlir_stack_return_erasure']
    files = [name+'.xl' for name in names] + [
        'xlir_tagged_enum_sequence_probe.c', 'xlir_tagged_enum_sequence_host.c',
        'xlir_tagged_enum_sequence_contract.c']
    for name in files:
        source = args.case_directory.resolve()/name
        target = cases/name
        shutil.copyfile(source, target)
        inputs[str(source)], inputs[str(target)] = sha(source), sha(target)
    rows = []

    def run(name, command, marker=None):
        command = list(map(str, command))
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=environment,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=240)
            code, output = result.returncode, result.stdout
        except subprocess.TimeoutExpired as error:
            code, output = None, error.output or b''
        except OSError as error:
            code, output = -error.errno, str(error).encode('utf-8')
        text = output.decode('utf-8', errors='replace')
        log = work/(name+'.log')
        log.write_bytes(output)
        passed = code == 0 and (marker is None or marker in text)
        rows.append(dict(Name=name, Command=command, Exit=code, Passed=passed,
                         Seconds=time.monotonic()-start, Marker=marker, Log=str(log), Sha256=sha(log)))
        (work/'progress.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(f'[enum-sequence] {name}: {"passed" if passed else "FAILED"}', flush=True)
        if not passed:
            print(text[-3000:], flush=True)
        return passed

    def compile_c(name, arguments):
        response = work/(name+'.rsp')
        response.write_text('\n'.join('"'+str(value).replace('\\', '/').replace('"', '\\"')+'"'
                                      for value in arguments)+'\n', encoding='utf-8')
        return run(name, [cc, '@'+str(response)])

    objects = [item['Path'] for item in build['Objects']
               if Path(item['Path']).name not in ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    archive = work/'resident.a'
    response = work/'archive.rsp'
    response.write_text('\n'.join('"'+value.replace('\\', '/')+'"' for value in objects)+'\n', encoding='utf-8')
    archive_ready = run('resident-archive', [ar, 'rcs', archive, '@'+str(response)])
    flags = [value for value in build['CompileFlags']
             if value not in ('-O0', '-O1', '-O2', '-O3', '-Os', '-Og')]
    flags += ['-I'+str(root), '-I'+str(compiler/'include'), '-I'+str(root/'release/include'),
              '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-label',
              '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-const-variable',
              '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
              '-Wno-pointer-sign', '-Wno-format-truncation']
    libraries = [root/'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32',
                 '-ladvapi32'] if windows else ['-L'+str(native/'release'), '-lxl',
                 '-Wl,-rpath,'+str(native/'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    if windows:
        shutil.copyfile(root/'release/xl.dll', work/'xl.dll')
    contract = work/('enum-sequence-contract'+suffix)
    if archive_ready and compile_c('contract-build', [*flags, '-O2', cases/'xlir_tagged_enum_sequence_contract.c',
                                                    archive, *libraries, '-o', contract]):
        run('contract', [contract, *[cases/(name+'.xl') for name in names]], 'mutations refused before C/header output')

    for name in names:
        source = cases/(name+'.xl')
        extra = [cases/'xlir_tagged_enum_sequence_probe.c'] if name == names[0] else []
        generated = work/(name+'-program.c')
        emitted = run(name+'-emit', [driver, source, 'cgen-default', generated])
        run(name+'-jit', [driver, source, 'run', work/(name+'-jit'), *extra])
        executable = work/(name+'-aot'+suffix)
        if run(name+'-aot-build', [driver, source, 'exe', executable.with_suffix('') if windows else executable, *extra]):
            run(name+'-aot', [executable])
        if emitted and archive_ready:
            for optimization in ('-O0', '-O2'):
                executable = work/(name+'-native'+optimization+suffix)
                # Compile the sidecar separately so the preincluded generated
                # module is not duplicated in a second translation unit.
                side_objects = []
                for sidecar in extra:
                    obj = work/(sidecar.stem+optimization+'.o')
                    if compile_c(sidecar.stem+optimization+'-build', [*flags, optimization, sidecar, '-c', '-o', obj]):
                        side_objects.append(obj)
                if len(side_objects) != len(extra):
                    continue
                if compile_c(name+optimization+'-build', [*flags, optimization, '-include', generated,
                    cases/'xlir_tagged_enum_sequence_host.c', *side_objects, archive, *libraries, '-o', executable]):
                    run(name+optimization, [executable], 'three complete module lifetimes balanced')

    for name in ['hir_flow_value_ownership_matrix', 'enum_recursive_container', 'enum_tagged_lifecycle',
                 'enum_async_closure', 'stdlib_stack_dynamic_surface']:
        source = root/'test/cases'/(name+'.xl')
        inputs[str(source)] = sha(source)
        run(name+'-emit', [driver, source, 'cgen-default', work/(name+'-generated.c')])
        run(name+'-jit', [driver, source, 'run', work/(name+'-jit')])
    changed = [path for path, digest in inputs.items() if sha(path) != digest]
    report = dict(Complete=bool(rows) and not changed and all(row['Passed'] for row in rows),
                  Scope=__doc__, Inputs=inputs, ChangedInputs=changed, Rows=rows, ShippingAccepted=False)
    (work/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Enum sequences: {sum(row["Passed"] for row in rows)}/{len(rows)} checks passed', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    sys.exit(main())
