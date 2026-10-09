"""Source-built compiler/runtime and actual Cgen host execution activity.

Native OS-thread/real API hosts compile strictly. Generated C and compiler
fixtures retain explicit legacy unused-code warning allowances. Actual TCC is
a released tool input, not a rebuilt shipping product. O0/O2 lanes optimize
host/generated units, not the linked compiler/runtime O0 population. No read
certificate, native restore cutover, sanitizers or full53 acceptance claim.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--host-source', type=Path, help='Explicit test-only corrected actual API host, fully hashed')
    parser.add_argument('--generated-host-source', type=Path, help='Explicit test-only generated-C host, fully hashed')
    parser.add_argument('--ir-contract-source', type=Path, help='Explicit test-only compiler contract/output fixture, fully hashed')
    parser.add_argument('--sanitize-native', action='store_true', help='Linux only: instrument and run the entire selected native activity closure')
    parser.add_argument('--sanitized-xrt-source', type=Path, help='Explicit selected actual XRT implementation test TU, fully hashed')
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
    for name in ('xlir_host_activity_main.xl', 'xlir_host_activity_synthetic.xl', 'xlir_code_activity_contract.c',
                 'xlir_host_activity_ir_contract.c', 'xlir_host_activity_host.c', 'xlir_host_activity_generated_host.c'):
        path = root/'test/cases'/name
        if path.exists(): files.add(path)
    host = args.host_source.resolve() if args.host_source else root/'test/cases/xlir_host_activity_host.c'
    generated_host = args.generated_host_source.resolve() if args.generated_host_source else root/'test/cases/xlir_host_activity_generated_host.c'
    ir_contract = args.ir_contract_source.resolve() if args.ir_contract_source else root/'test/cases/xlir_host_activity_ir_contract.c'
    files.update((host, generated_host, ir_contract, root/'test/support/module_product_memory.c', root/'test/support/module_product_memory.h'))
    sanitized_xrt = args.sanitized_xrt_source.resolve() if args.sanitized_xrt_source else root/'test/support/code_activity_xrt.c'
    if args.sanitize_native:
        assert os.name != 'nt'; files.add(sanitized_xrt)
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    rows, products, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Products': products,
              'Errors': errors, 'Scope': __doc__, 'GeneratedReadCertificate': False, 'NativeRestoreCgenCutover': False,
              'ShippingLibraryAcceptance': False, 'Full53Acceptance': False, 'Sanitized': False,
              'SanitizedNativeClosure': False,
              'LinkedCompilerRuntimeOptimization': 'O0', 'NativeHostWarnings': 'C11 Wall Wextra Werror without exemptions',
              'Observer': 'ld wrap activity calls, always delegates to actual resident registry; no state-machine replacement'}
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(runtime, out/'xl.dll')
    env = os.environ.copy()
    if os.name != 'nt': env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I'+str(root), '-I'+str(root/'release'), '-I'+str(root/'release/include'),
             '-I'+str(root/'lib/tcc'), '-I'+str(public/'include')]
    legacy = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-label',
              '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Wno-missing-braces']
    report['GeneratedLegacyWarningAllowances'] = legacy
    links = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
             ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    if os.name == 'nt': links += [str(runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']; links += [str(runtime), '-Wl,-rpath,'+str(runtime.parent), '-pthread', '-ldl', '-lm']
    wrap = ['-Wl,--wrap=xl6CodeActivityEnterV1', '-Wl,--wrap=xl6CodeActivityLeaveV1']
    def run(name, command):
        command = list(map(str, command)); result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=120)
        data = result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode}); print(name+': '+str(result.returncode), flush=True)
        assert result.returncode == 0, data[-6000:].decode(errors='replace')
        return data.decode(errors='replace')
    def product(path): products.append({'Path': str(path), 'Sha256': sha(path)})
    try:
        contract = out/('ir-contract'+('.exe' if os.name == 'nt' else ''))
        run('ir-contract.compile', [cc, *flags, *legacy, '-Wno-pointer-sign', '-Wno-format-truncation',
            ir_contract, *links, '-o', contract]); product(contract)
        assert 'four missing/unknown ABI mutations' in run('ir-contract.run', [contract, out/'main.script.c', out/'synthetic.script.c'])
        product(out/'main.script.c'); product(out/'synthetic.script.c')
        run('ir-contract.repeat', [contract, out/'main.script.repeat.c', out/'synthetic.script.repeat.c'])
        for name in ('main', 'synthetic'):
            product(out/(name+'.script.repeat.c'))
            assert sha(out/(name+'.script.c')) == sha(out/(name+'.script.repeat.c'))
        for name in ('main', 'synthetic'):
            source = root/('test/cases/xlir_host_activity_'+name+'.xl')
            for mode in ('ir-typed', 'ir-lowered', 'cgen-default'):
                target = out/(name+('.c' if mode == 'cgen-default' else '.'+mode+'.json'))
                run(name+'.'+mode, [driver, source, mode, target]); product(target)
            typed = json.loads((out/(name+'.ir-typed.json')).read_text(encoding='utf-8-sig'))
            lowered = json.loads((out/(name+'.ir-lowered.json')).read_text(encoding='utf-8-sig'))
            assert typed['moduleHostExecutionAbi'] == 0 and lowered['moduleHostExecutionAbi'] == 1
            if name == 'synthetic':
                # Default C is a fragment without synthetic host adapters.
                # The exact verified IR fixture emits explicit script options.
                assert lowered['moduleEntry']['kind'] == 0
            generated = (out/(name+'.script.c')).read_text(encoding='utf-8-sig')
            assert 'xl_script_module_ownership_read_v1' not in generated
            assert 'xl6CodeActivityEnterV1(&execution' in generated
            repeat = out/(name+'.repeat.c'); run(name+'.repeat', [driver, source, 'cgen-default', repeat]); product(repeat)
            assert sha(repeat) == sha(out/(name+'.c'))
        for optimization in ('O0', 'O2'):
            suffix = '.exe' if os.name == 'nt' else ''
            memory = out/('memory-'+optimization+'.o')
            run(optimization+'.memory', [cc, *flags, '-'+optimization, '-c', root/'test/support/module_product_memory.c', '-o', memory]); product(memory)
            native = out/('native-'+optimization+suffix)
            run(optimization+'.native.compile', [cc, *flags, '-'+optimization, root/'test/cases/xlir_code_activity_contract.c', memory, *links, '-o', native]); product(native)
            assert 'zero XRT allocations' in run(optimization+'.native.run', [native])
            api = out/('tcc-'+optimization+suffix)
            run(optimization+'.api.compile', [cc, *flags, '-'+optimization, *wrap, host, memory, *links, '-o', api]); product(api)
            tcc_lib = root/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
            assert 'exact tool-only heap baseline' in run(optimization+'.api.run', [api, runtime, tcc_lib, out/('temps-'+optimization), public/'include'])
            for name in ('main', 'synthetic'):
                binary = out/(name+'-'+optimization+suffix)
                defines = ['-DXL6_ACTIVITY_GENERATED_C="'+(out/(name+'.script.c')).as_posix()+'"']
                if name == 'synthetic': defines += ['-DXL6_ACTIVITY_SYNTHETIC']
                run(optimization+'.'+name+'.compile', [cc, *flags, *legacy, '-'+optimization, *wrap, *defines,
                    generated_host, memory, *links, '-o', binary]); product(binary)
                assert 'balanced capability/heap' in run(optimization+'.'+name+'.run', [binary])
        if args.sanitize_native:
            # No uninstrumented runtime object or released library is linked.
            # The selected XRT TU uses only upstream module selection defines.
            sanitized_sources = [sanitized_xrt, root/'src/runtime/xl6_code_lease.c',
                root/'test/support/module_product_memory.c', root/'test/cases/xlir_code_activity_contract.c']
            sanitized_objects = []
            sanitizer = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined']
            for index, source in enumerate(sanitized_sources):
                obj = out/('sanitized-'+str(index)+'.o')
                run('sanitized.compile.'+str(index), [cc, *flags, *sanitizer, '-c', source, '-o', obj]); product(obj)
                sanitized_objects.append(obj)
            binary = out/'native-sanitized'
            run('sanitized.link', [cc, *sanitizer, *sanitized_objects, '-pthread', '-ldl', '-lm', '-o', binary]); product(binary)
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            assert 'zero XRT allocations' in run('sanitized.run', [binary])
            report['SanitizedNativeClosure'] = True
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged']
    except Exception as error: errors.append(repr(error)); print(repr(error), file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
