"""Check resolved property ownership with a complete current-source compiler.

Uses ordinary language/ABI/lifetime probes only. A fresh work directory retains
every independent result; failures do not suppress unrelated checks.
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
    parser.add_argument('--work-directory', type=Path, required=True)
    parser.add_argument('--native-root', type=Path)
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--ar', default='ar')
    args = parser.parse_args()
    root = args.root.resolve()
    compiler = args.compiler_directory.resolve()
    work = args.work_directory.resolve()
    if work.exists():
        parser.error('Choose a fresh work directory')
    work.mkdir(parents=True)
    windows = os.name == 'nt'
    suffix = '.exe' if windows else ''
    cc = shutil.which(args.cc)
    ar = shutil.which(args.ar)
    if not cc or not ar:
        parser.error('The C compiler and archive tool must be available')
    environment = dict(os.environ)
    if not windows:
        if args.native_root is None:
            parser.error('Linux requires --native-root for the pinned genuine TCC dependency')
        native = args.native_root.resolve()
        environment['XL6_SOURCE_NATIVE_ROOT'] = str(native)

    def sha(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    manifest = compiler / 'build.json'
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
    cases = root / 'test/cases'
    names = ['xlir_property_overload_selection', 'xlir_indexed_accessor',
             'xlir_property_assignment_transaction', 'xlir_property_update',
             'xlir_nullable_getter', 'xlir_native_factory_getter']
    for name in names:
        for ending in ('.xl', '_contract.c', '_host.c', '_runtime.c'):
            path = cases / (name + ending)
            if path.is_file():
                inputs[str(path)] = sha(path)
    simple = ['cgen_class_property_get_let', 'xlir_property_overload_selection',
              'xlir_nullable_getter', 'xlir_native_factory_getter']
    for name in simple:
        inputs[str(cases / (name + '.xl'))] = sha(cases / (name + '.xl'))
    host = cases / 'xlir_property_selection_host.c'
    inputs[str(host)] = sha(host)
    barrier = root / 'test/support/xlir_property_await_barrier.h'
    inputs[str(barrier)] = sha(barrier)
    rows = []

    def run(name, command, marker=None):
        command = list(map(str, command))
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, env=environment,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
            code, output = result.returncode, result.stdout
        except subprocess.TimeoutExpired as error:
            code, output = None, error.output or b''
        except OSError as error:
            code, output = -error.errno, str(error).encode('utf-8')
        text = output.decode('utf-8', errors='replace')
        log = work / (name + '.log')
        log.write_bytes(output)
        passed = code == 0 and (marker is None or marker in text)
        rows.append(dict(Name=name, Command=command, Exit=code, Passed=passed,
                         Seconds=time.monotonic()-start, Marker=marker,
                         Log=str(log), Sha256=sha(log)))
        (work / 'progress.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(f'[property] {name}: {"passed" if passed else "FAILED"}', flush=True)
        if not passed:
            print(text[-3000:], flush=True)
        return passed

    def compile_c(name, arguments):
        response = work / (name + '.rsp')
        response.write_text('\n'.join('"'+str(value).replace('\\', '/').replace('"', '\\"')+'"'
                                      for value in arguments)+'\n', encoding='utf-8')
        return run(name, [cc, '@'+str(response)])

    objects = [item['Path'] for item in build['Objects']
               if Path(item['Path']).name not in ('xlir_public_source_driver.o',
                                                  'xlir_public_source_linux_driver.o')]
    archive = work / 'resident.a'
    response = work / 'archive.rsp'
    response.write_text('\n'.join('"'+value.replace('\\', '/')+'"' for value in objects)+'\n',
                        encoding='utf-8')
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

    for name in names:
        contract = work/(name+'-contract'+suffix)
        ready = archive_ready and compile_c(name+'-contract-build',
            [*flags, '-O2', cases/(name+'_contract.c'), archive, *libraries, '-o', contract])
        prefix = work/name
        generated = [Path(str(prefix)+ending) for ending in ('.c', '.h', '.bind.h')]
        if name == 'xlir_property_overload_selection' or name == 'xlir_native_factory_getter':
            arguments = [cases/(name+'.xl')]
        elif name == 'xlir_nullable_getter':
            arguments = [cases/(name+'.xl'), generated[0]]
        else:
            arguments = generated
        ready = ready and run(name+'-contract', [contract, *arguments])
        if not ready or name not in names[1:4]:
            continue
        repeat = [Path(str(prefix)+'.repeat'+ending) for ending in ('.c', '.h', '.bind.h')]
        deterministic = run(name+'-repeat', [contract, *repeat])
        if deterministic:
            assert all(a.read_bytes() == b.read_bytes() for a, b in zip(generated, repeat))
        original = cases/(name+'_host.c')
        adapted = work/(name+'-host.c')
        text = original.read_text(encoding='utf-8-sig')
        for ending, target in (('.c', generated[0]), ('.bind.h', generated[2])):
            before = '../../build/test/'+name+ending
            assert text.count(before) == 1
            text = text.replace(before, target.as_posix())
        adapted.write_text(text, encoding='utf-8')
        inputs[str(adapted)] = sha(adapted)
        for optimization in ('-O0', '-O2'):
            executable = work/(name+'-native'+optimization+suffix)
            if compile_c(name+'-native'+optimization+'-build',
                         [*flags, optimization, cases/(name+'_runtime.c'), adapted,
                          archive, *libraries, '-o', executable]):
                marker = '100 complete module' if name == 'xlir_indexed_accessor' else 'initial allocation baseline restored'
                run(name+'-native'+optimization, [executable], marker)

    for name in simple:
        source = cases/(name+'.xl')
        generated = work/(name+'-program.c')
        emitted = run(name+'-emit', [driver, source, 'cgen-default', generated])
        run(name+'-jit', [driver, source, 'run', work/(name+'-jit')])
        executable = work/(name+'-aot'+suffix)
        if run(name+'-aot-build', [driver, source, 'exe', executable.with_suffix('') if windows else executable]):
            run(name+'-aot', [executable])
        if emitted and archive_ready:
            for optimization in ('-O0', '-O2'):
                executable = work/(name+'-program'+optimization+suffix)
                fragment = ['-DXL6_PROPERTY_MODULE_FRAGMENT'] if name in ('cgen_class_property_get_let', 'xlir_native_factory_getter') else []
                if compile_c(name+'-program'+optimization+'-build',
                             [*flags, optimization, *fragment, '-include', generated, host, archive,
                              *libraries, '-o', executable]):
                    run(name+'-program'+optimization, [executable], 'three complete module lifetimes balanced')

    changed = [path for path, digest in inputs.items() if sha(path) != digest]
    report = dict(Complete=bool(rows) and not changed and all(row['Passed'] for row in rows),
                  Scope=__doc__, Inputs=inputs, ChangedInputs=changed, Rows=rows,
                  NativeHostAdaptation='Only two generated include paths per retained C host',
                  ShippingAccepted=False)
    (work/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Property selection: {sum(row["Passed"] for row in rows)}/{len(rows)} checks passed', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    sys.exit(main())
