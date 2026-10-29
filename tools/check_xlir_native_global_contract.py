"""Check private native-input declarations and rejection of mismatched C globals.

Negative executables are never run. JIT/module negatives run only after the
same source has been rejected by the standalone compiler before publication.
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
        if args.native_root is None: parser.error('Linux requires the genuine pinned TCC dependency')
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
    cases = work/'inputs'; cases.mkdir()
    for name in ('xlir_native_global_contract.xl', 'xlir_native_global_ir_contract.c', 'xlir_native_global_contract_host.c'):
        source, target = args.case_directory.resolve()/name, cases/name
        shutil.copyfile(source, target)
        inputs[str(source)], inputs[str(target)] = sha(source), sha(target)
    rows = []
    def run(name, command, marker=None, symbol=None, preserved=None):
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
        if symbol:
            passed = code == 1 and symbol in text and ('incompatible' in text or 'conflicting' in text)
        else:
            passed = code == 0 and (marker is None or marker in text)
        if preserved is not None: passed = passed and preserved.read_bytes() == b'KEEP'
        rows.append(dict(Name=name, Command=command, Exit=code, Passed=passed, Seconds=time.monotonic()-start,
            Marker=marker, RejectSymbol=symbol, Preserved=str(preserved) if preserved else None,
            Log=str(log), Sha256=sha(log)))
        (work/'progress.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(f'[native-global] {name}: {"passed" if passed else "FAILED"}', flush=True)
        if not passed: print(text[-2300:], flush=True)
        return passed
    def compile_c(name, arguments, symbol=None):
        response = work/(name+'.rsp')
        response.write_text('\n'.join('"'+str(v).replace('\\', '/').replace('"', '\\"')+'"'
            for v in arguments)+'\n', encoding='utf-8')
        return run(name, [cc, '@'+str(response)], symbol=symbol)
    objects = [item['Path'] for item in build['Objects'] if Path(item['Path']).name not in
        ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    archive = work/'resident.a'; response = work/'archive.rsp'
    response.write_text('\n'.join('"'+v.replace('\\', '/')+'"' for v in objects)+'\n', encoding='utf-8')
    archive_ready = run('resident-archive', [ar, 'rcs', archive, '@'+str(response)])
    flags = [v for v in build['CompileFlags'] if v not in ('-O0', '-O1', '-O2', '-O3', '-Os', '-Og')]
    flags += ['-I'+str(root), '-I'+str(compiler/'include'), '-I'+str(root/'release/include'),
              '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-label',
              '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-const-variable',
              '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Wno-pointer-sign', '-Wno-format-truncation']
    libraries = [root/'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if windows else [
        '-L'+str(native/'release'), '-lxl', '-Wl,-rpath,'+str(native/'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    if windows: shutil.copyfile(root/'release/xl.dll', work/'xl.dll')
    contract = work/('header-contract'+suffix)
    public_header, native_header = work/'public.h', work/'native.h'
    if archive_ready and compile_c('header-contract-build', [*flags, '-O2', cases/'xlir_native_global_ir_contract.c',
        archive, *libraries, '-o', contract]):
        run('header-contract', [contract, cases/'xlir_native_global_contract.xl', public_header, native_header],
            '12 mutations rejected before C/public/native output')
    source, host = cases/'xlir_native_global_contract.xl', cases/'xlir_native_global_contract_host.c'
    marker = 'exact imported storage and public header isolation passed'
    run('positive-jit', [driver, source, 'run', work/'positive-jit', host], marker)
    run('positive-module', [driver, source, 'module', work/'positive-module', host], marker)
    executable = work/('positive-aot'+suffix)
    if run('positive-aot-build', [driver, source, 'exe', executable.with_suffix('') if windows else executable, host]):
        run('positive-aot', [executable], marker)
    variants = {
        'width-kind': ('struct { int count; int padding; } native_global_counter = {40, 0};', 'bool', 'native_global_counter'),
        'unsigned': ('uint64_t native_global_counter = 40;', 'bool', 'native_global_counter'),
        'array': ('int64_t native_global_counter[1] = {40};', 'bool', 'native_global_counter'),
        'const': ('const int64_t native_global_counter = 40;', 'bool', 'native_global_counter'),
        'bool-width': ('int64_t native_global_counter = 40;', 'unsigned char', 'native_global_enabled')
    }
    for name, (declaration, boolean, symbol) in variants.items():
        bad = cases/('bad-'+name+'.c')
        bad.write_text('#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n'+declaration+'\n'+
            boolean+' native_global_enabled = 1;\nsize_t native_global_size = 3;\n', encoding='utf-8')
        inputs[str(bad)] = sha(bad)
        if native_header.exists():
            compile_c(name+'-gcc-reject', [*flags, '-include', native_header, '-fsyntax-only', bad], symbol)
        destination = work/(name+'-must-preserve'+suffix); destination.write_bytes(b'KEEP')
        if run(name+'-aot-reject', [driver, source, 'exe', destination.with_suffix('') if windows else destination, bad],
               symbol=symbol, preserved=destination):
            run(name+'-jit-reject', [driver, source, 'run', work/(name+'-jit'), bad], symbol=symbol)
            run(name+'-module-reject', [driver, source, 'module', work/(name+'-module'), bad], symbol=symbol)
    package_source = root/'test/cases/xlir_pkg_bad_global.xl'
    for path in [package_source, *sorted((root/'test/fixtures/xlir_pkg_bad_global').rglob('*'))]:
        if path.is_file(): inputs[str(path)] = sha(path)
    environment['XLANG_PACKAGE_PATH'] = str(root/'test/fixtures')
    destination = work/('package-must-preserve'+suffix); destination.write_bytes(b'KEEP')
    if run('package-aot-reject', [driver, package_source, 'exe', destination.with_suffix('') if windows else destination],
           symbol='xlir_pkg_bad_count', preserved=destination):
        run('package-jit-reject', [driver, package_source, 'run', work/'package-jit'], symbol='xlir_pkg_bad_count')
        run('package-module-reject', [driver, package_source, 'module', work/'package-module'], symbol='xlir_pkg_bad_count')
    changed = [path for path, digest in inputs.items() if sha(path) != digest]
    report = dict(Complete=bool(rows) and not changed and all(row['Passed'] for row in rows), Scope=__doc__,
        Inputs=inputs, ChangedInputs=changed, Rows=rows, NegativeExecutablesRun=False, ShippingAccepted=False)
    (work/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Native global contracts: {sum(row["Passed"] for row in rows)}/{len(rows)} checks passed', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    sys.exit(main())
