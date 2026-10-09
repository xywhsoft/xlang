"""Current-source raw C callback execution registration.

Actual public API/TCC and static generated C share saved-pointer, status/error,
closed/OOM and foreign-thread vectors. Native hosts are strict C11; generated
C/compiler fixtures retain recorded legacy warning allowances. Every linked
compiler/runtime object is current frozen source O0; host units are O0/O2.
Module read certificate emitted; no restore cutover, sanitizer or full53 claim.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'): parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--host-source', type=Path, help='Explicit test-only host override, fully hashed')
    parser.add_argument('--generated-host-source', type=Path, help='Explicit test-only static generated-C host override, fully hashed')
    parser.add_argument('--vectors-source', type=Path, help='Explicit test-only common observer/vectors override, fully hashed')
    args = parser.parse_args(); root, public, out = args.root.resolve(), args.public_directory.resolve(), args.out.resolve()
    manifest = public/'build.json'; build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
    if os.name != 'nt': assert build['Complete'] and build['InputsUnchanged']
    files = {Path(__file__).resolve(), Path(sys.executable), manifest}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']; files.add(Path(row['Path']))
    driver = Path(build['Driver']); assert sha(driver).lower() == build['DriverSha256'].lower()
    cc = shutil.which('gcc'); assert cc
    runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
    files.update((driver, Path(cc), runtime))
    for name in ('xlir_raw_activity.xl', 'xlir_raw_activity_vectors.h', 'xlir_raw_activity_host.c',
                 'xlir_raw_activity_generated_host.c', 'xlir_raw_activity_ir_contract.c',
                 'xlir_foreign_raw_callback_error.xl', 'xlir_foreign_raw_callback_error_host.c'):
        files.add(root/'test/cases'/name)
    files.update((root/'test/support/module_product_memory.c', root/'test/support/module_product_memory.h'))
    files.add(root/'test/cases/xlir_module_read_ir_contract.c')
    host = args.host_source.resolve() if args.host_source else root/'test/cases/xlir_raw_activity_host.c'
    generated_host = args.generated_host_source.resolve() if args.generated_host_source else root/'test/cases/xlir_raw_activity_generated_host.c'
    vectors = args.vectors_source.resolve() if args.vectors_source else root/'test/cases/xlir_raw_activity_vectors.h'
    files.update((host, generated_host, vectors))
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    rows, products, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Products': products,
              'Errors': errors, 'Scope': __doc__, 'GeneratedReadCertificate': True, 'NativeRestoreCgenCutover': False,
              'ShippingLibraryAcceptance': False, 'Full53Acceptance': False, 'Sanitized': False,
              'LinkedCompilerRuntimeOptimization': 'O0', 'NativeHostWarnings': 'C11 Wall Wextra Werror without exemptions',
              'Observer': 'ld wrap activity calls, always delegates to actual registry; no mirrored state machine'}
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(runtime, out/'xl.dll')
    env = os.environ.copy()
    if os.name != 'nt': env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I'+str(root),
             '-I'+str(root/'release'), '-I'+str(root/'release/include'), '-I'+str(root/'lib/tcc'),
             '-I'+str(public/'include'), '-I'+str(vectors.parent), '-I'+str(root/'test/cases')]
    legacy = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-label',
              '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Wno-missing-braces']
    report['GeneratedLegacyWarningAllowances'] = legacy
    links = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
             ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    if os.name == 'nt': links += [str(runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else: flags += ['-D_GNU_SOURCE']; links += [str(runtime), '-Wl,-rpath,'+str(runtime.parent), '-pthread', '-ldl', '-lm']
    wrap = ['-Wl,--wrap=xl6CodeActivityEnterV1', '-Wl,--wrap=xl6CodeActivityLeaveV1']
    def run(name, command):
        command = list(map(str, command)); result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=180)
        data = result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode}); print(name+': '+str(result.returncode), flush=True)
        assert result.returncode == 0, data[-6000:].decode(errors='replace')
        return data.decode(errors='replace')
    def product(path): products.append({'Path': str(path), 'Sha256': sha(path)})
    try:
        # The artifact certificate declaration has no full XRT/source-tree
        # include dependency. Compile the staged package header by itself.
        header = public/'include/__xl_runtime/xl6_ownership_read_abi.h'
        for language, standard in (('c', 'c11'), ('c++', 'c++17')):
            run('read-abi.'+language, [cc, '-std='+standard, '-Wall', '-Wextra', '-Werror',
                '-pedantic', '-x', language, '-fsyntax-only', header])
        contract = out/('ir-contract'+('.exe' if os.name == 'nt' else ''))
        run('ir-contract.compile', [cc, *flags, *legacy, '-Wno-pointer-sign', '-Wno-format-truncation',
            root/'test/cases/xlir_raw_activity_ir_contract.c', *links, '-o', contract]); product(contract)
        assert '24 missing/unknown mutations' in run('ir-contract.run', [contract])
        read_contract = out/('read-contract'+('.exe' if os.name == 'nt' else ''))
        run('read-contract.compile', [cc, *flags, *legacy, '-Wno-pointer-sign', '-Wno-format-truncation',
            root/'test/cases/xlir_module_read_ir_contract.c', *links, '-o', read_contract]); product(read_contract)
        assert '49 typed/module/export/raw/storage' in run('read-contract.run', [read_contract])
        for mode in ('ir-typed', 'ir-lowered', 'cgen-default'):
            target = out/('raw'+('.c' if mode == 'cgen-default' else '.'+mode+'.json'))
            run(mode, [driver, root/'test/cases/xlir_raw_activity.xl', mode, target]); product(target)
        typed = json.loads((out/'raw.ir-typed.json').read_text(encoding='utf-8-sig'))
        lowered = json.loads((out/'raw.ir-lowered.json').read_text(encoding='utf-8-sig'))
        assert all(row['rawExecutionAbi'] == 0 for row in typed['closureTargets'])
        assert typed['moduleReadExecutionAbi'] == 0 and lowered['moduleReadExecutionAbi'] == 1
        assert len(lowered['closureTargets']) == 12 and all(row['rawExecutionAbi'] == 1 for row in lowered['closureTargets'])
        assert 'xl_script_module_ownership_read_v1' in (out/'raw.c').read_text(encoding='utf-8-sig')
        run('repeat', [driver, root/'test/cases/xlir_raw_activity.xl', 'cgen-default', out/'raw.repeat.c']); product(out/'raw.repeat.c')
        assert sha(out/'raw.c') == sha(out/'raw.repeat.c')
        for optimization in ('O0', 'O2'):
            suffix = '.exe' if os.name == 'nt' else ''
            memory, original = out/('memory-'+optimization+'.o'), out/('original-'+optimization+'.o')
            run(optimization+'.memory', [cc, *flags, '-'+optimization, '-c', root/'test/support/module_product_memory.c', '-o', memory]); product(memory)
            run(optimization+'.original', [cc, *flags, '-'+optimization, '-c', root/'test/cases/xlir_foreign_raw_callback_error_host.c', '-o', original]); product(original)
            api = out/('tcc-'+optimization+suffix)
            run(optimization+'.api.compile', [cc, *flags, '-'+optimization, *wrap,
                host, original, memory, *links, '-o', api]); product(api)
            tcc_lib = root/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
            assert 'exact tool-only heap baseline' in run(optimization+'.api.run', [api, runtime, tcc_lib, out/('temps-'+optimization), public/'include'])
            binary = out/('generated-'+optimization+suffix)
            define = '-DXL6_RAW_ACTIVITY_GENERATED_C="'+(out/'raw.c').as_posix()+'"'
            run(optimization+'.generated.compile', [cc, *flags, *legacy, '-'+optimization, *wrap, define,
                generated_host, memory, *links, '-o', binary]); product(binary)
            assert 'no new capabilities and balanced heap' in run(optimization+'.generated.run', [binary])
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged']
    except Exception as error: errors.append(repr(error)); print(repr(error), file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
