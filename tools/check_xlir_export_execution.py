"""Actual generated foreign-export execution tracking on current-source products.

Build manifests must bind the full compiler/API/runtime source population.
Released TCC is a tool input only. Native hosts compile strictly; generated C
and old compiler contract fixtures retain recorded legacy unused-code warning
allowances. The module-read certificate is emitted; this is not restore cutover, shipping
library or full53 acceptance. Host fixture O0/O2 is not compiler/runtime O2.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--host-source', type=Path,
        help='Explicit corrected test-only host; compiler/runtime inputs stay frozen and are never replaced')
    parser.add_argument('--generated-host-source', type=Path,
        help='Explicit corrected test-only generated-C host; recorded and hashed')
    parser.add_argument('--transaction-host-source', type=Path,
        help='Explicit resident-free-aware transaction fixture; recorded and hashed')
    args = parser.parse_args()
    root, public, out = args.root.resolve(), args.public_directory.resolve(), args.out.resolve()
    manifest = public/'build.json'; build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
    if os.name != 'nt': assert build['Complete'] and build['InputsUnchanged']
    files = {Path(__file__).resolve(), Path(sys.executable), manifest}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']
        files.add(Path(row['Path']))
    driver = Path(build['Driver']); assert sha(driver).lower() == build['DriverSha256'].lower()
    cc = shutil.which('gcc'); assert cc
    runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
    files.update((driver, Path(cc), runtime))
    names = ['xlir_export_execution.xl', 'xlir_export_execution_host.c', 'xlir_export_execution_aot_host.c',
             'xlir_export_transaction.xl', 'xlir_export_transaction_contract.c',
             'xlir_export_transaction_host.c', 'xlir_export_transaction_runtime.c',
             'xlir_export_transaction_header_host.c']
    files.update(root/'test/cases'/name for name in names)
    host_source = args.host_source.resolve() if args.host_source else root/'test/cases/xlir_export_execution_host.c'
    generated_host = args.generated_host_source.resolve() if args.generated_host_source else root/'test/cases/xlir_export_execution_aot_host.c'
    files.update((host_source, generated_host))
    tx_host = args.transaction_host_source.resolve() if args.transaction_host_source else root/'test/cases/xlir_export_transaction_host.c'
    files.add(tx_host)
    files.update((root/'test/support/module_product_memory.c', root/'test/support/module_product_memory.h'))
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    rows, products, failures = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows,
              'Products': products, 'Failures': failures, 'Scope': __doc__,
              'GeneratedExportTracking': True, 'ActualTccExecution': True,
              'GeneratedReadCertificate': True, 'NativeRestoreCgenCutover': False,
              'ShippingLibraryAcceptance': False, 'Full53Acceptance': False,
              'LinkedCompilerRuntimeOptimization': 'O0', 'Sanitized': False}
    report['TestOnlyHostSource'] = str(host_source)
    report['TestOnlyGeneratedHostSource'] = str(generated_host)
    report['TestOnlyTransactionHostSource'] = str(tx_host)
    report['CleanupFailureInjection'] = 'GNU ld --wrap=xrtFree, exact armed bridge identities, always delegates real free; not OS cleanup exhaustion'
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(runtime, out/'xl.dll')
    env = os.environ.copy()
    if os.name != 'nt': env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I'+str(root), '-I'+str(root/'release'), '-I'+str(root/'release/include'),
             '-I'+str(root/'lib/tcc'), '-I'+str(public/'include')]
    legacy = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
              '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
              '-Wno-missing-field-initializers', '-Wno-missing-braces']
    links = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
             ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    if os.name == 'nt': links += [str(runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']
        links += [str(runtime), '-Wl,-rpath,'+str(runtime.parent), '-pthread', '-ldl', '-lm']
    def run(name, command):
        command = list(map(str, command))
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=120)
        data = result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode})
        print(name+': '+str(result.returncode), flush=True)
        assert result.returncode == 0, data[-6000:].decode(errors='replace')
        return data.decode(errors='replace')
    def product(path): products.append({'Path': str(path), 'Sha256': sha(path)})
    try:
        source = root/'test/cases/xlir_export_execution.xl'
        for mode in ('ir-lowered', 'cgen-default'):
            target = out/('execution.json' if mode == 'ir-lowered' else 'execution.c')
            run('execution.'+mode, [driver, source, mode, target]); product(target)
        ir = json.loads((out/'execution.json').read_text(encoding='utf-8-sig'))
        assert len(ir['exports']) == 5 and all(row['executionAbi'] == 1 for row in ir['exports'])
        repeat = out/'execution.repeat.c'
        run('execution.repeat', [driver, source, 'cgen-default', repeat]); product(repeat)
        assert sha(out/'execution.c') == sha(repeat)
        generated = (out/'execution.c').read_text(encoding='utf-8-sig')
        assert generated.count('xl6CodeScopeEnter(&execution, code)') == 5
        assert generated.count('xl6CodeScopeLeave(&execution)') == 10
        assert 'xl_script_module_ownership_read_v1' in generated
        for optimization in ('O0', 'O2'):
            suffix = '.exe' if os.name == 'nt' else ''
            memory = out/('memory-'+optimization+'.o')
            run(optimization+'.memory', [cc, *flags, '-'+optimization, '-c',
                root/'test/support/module_product_memory.c', '-o', memory]); product(memory)
            host = out/('tcc-'+optimization+suffix)
            run(optimization+'.host.compile', [cc, *flags, '-'+optimization,
                host_source, memory, *links, '-o', host]); product(host)
            tcc_lib = root/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
            text = run(optimization+'.host.run', [host, runtime, tcc_lib, out/('temps-'+optimization), public/'include'])
            assert 'actual TCC exports: 9 error/result vectors' in text and 'exact tool-only heap baseline' in text
            standalone = out/('generated-'+optimization+suffix)
            define = '-DXL6_EXECUTION_GENERATED_C="'+(out/'execution.c').as_posix()+'"'
            run(optimization+'.generated.compile', [cc, *flags, *legacy, '-'+optimization, define,
                generated_host, memory, *links, '-o', standalone]); product(standalone)
            assert 'balanced capability/heap' in run(optimization+'.generated.run', [standalone])
        contract = out/('contract'+('.exe' if os.name == 'nt' else ''))
        run('contract.compile', [cc, *flags, *legacy, '-Wno-pointer-sign', '-Wno-format-truncation',
            root/'test/cases/xlir_export_transaction_contract.c', *links, '-o', contract]); product(contract)
        tx_c, tx_h = out/'transaction.c', out/'transaction.h'
        text = run('contract.run', [contract, tx_c, tx_h]); product(tx_c); product(tx_h)
        assert '52 missing/mismatched transaction mutations' in text
        lowered = out/'transaction.json'
        run('transaction.lowered', [driver, root/'test/cases/xlir_export_transaction.xl', 'ir-lowered', lowered]); product(lowered)
        tx = json.loads(lowered.read_text(encoding='utf-8-sig'))['exports']
        assert len(tx) == 13 and all(row['executionAbi'] == 1 for row in tx)
        # The existing fixtures have a fixed generated-file layout. Stage fresh
        # compiler outputs there, never patch their C or use old generated code.
        # A repeated gate may reuse only byte-identical staged outputs; every
        # executable is still rebuilt and run in this gate's fresh directory.
        fixture_dir = root/'build/test'; fixture_dir.mkdir(parents=True, exist_ok=True)
        for src, name in ((tx_c, 'xlir_export_transaction.c'), (tx_h, 'xlir_export_transaction.h')):
            target = fixture_dir/name
            if target.exists(): assert target.read_bytes() == src.read_bytes(), target
            else: shutil.copy2(src, target)
            product(target)
        types = {row['cName']: row['returnType'] for row in tx}
        aliases = ['#define '+row['cName'][len('xl6_export_'):]+' '+row['cName'] for row in tx if row['cName'].startswith('xl6_export_tx_')]
        binding = fixture_dir/'xlir_export_transaction_bind.h'
        binding_text = '#define TX_PRODUCT xlir_product_t'+str(types['xl6_export_tx_product'])+'\n' \
            '#define TX_ACCEPT_PRODUCT xlir_product_t'+str(types['xl6_export_tx_accept_product'])+'\n'
        binding_text += '\n'.join(aliases)+'\n'
        binding_bytes = binding_text.replace('\n', os.linesep).encode('utf-8')
        if binding.exists(): assert binding.read_bytes() == binding_bytes, binding
        else: binding.write_bytes(binding_bytes)
        product(binding)
        for optimization in ('O0', 'O2'):
            binary = out/('transaction-'+optimization+('.exe' if os.name == 'nt' else ''))
            run(optimization+'.transaction.compile', [cc, *flags, *legacy, '-I'+str(fixture_dir), '-'+optimization,
                '-DXL6_TX_RESIDENT_FREE_WRAP', '-Wl,--wrap=xrtFree',
                '-DXL6_TX_BINDING_INCLUDE="'+binding.as_posix()+'"',
                '-DXL6_TX_GENERATED_INCLUDE="'+(fixture_dir/'xlir_export_transaction.c').as_posix()+'"',
                root/'test/cases/xlir_export_transaction_runtime.c', tx_host,
                root/'test/cases/xlir_export_transaction_header_host.c',
                *links, '-o', binary]); product(binary)
            assert '1100 real allocation failures' in run(optimization+'.transaction.run', [binary])
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged']
    except Exception as error:
        failures.append(repr(error)); print(repr(error), file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
