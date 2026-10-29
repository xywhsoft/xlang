"""Resident dynamic bound-method lifetime and execution on full source builds.

The API fixture compiles two actual TCC modules; A's saved callable refers to
B's receiver, with no independent caller lease masking adapter ownership.
Continuous OOM prefixes, foreign-thread read refusal, close/last-drop and exact
heap identities are observed. Native host is strict C11; old generated/compiler
fixtures retain recorded legacy warning allowances. Host O0/O2 is not runtime
O2, a shipping compiler/library build, sanitizer, or full53 acceptance.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--host-source', type=Path, help='Explicit test-only corrected host, hashed; production stays frozen')
    parser.add_argument('--fixture-source', type=Path, help='Explicit test-only language fixture, hashed')
    parser.add_argument('--plan-source', type=Path, help='Explicit test-only dependency fixture, hashed')
    parser.add_argument('--before-root', type=Path, help='Optional full source-built baseline, expected real scope assertion')
    parser.add_argument('--before-public', type=Path)
    args = parser.parse_args(); root, public, out = args.root.resolve(), args.public_directory.resolve(), args.out.resolve()
    manifest = public/'build.json'; build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
    if os.name != 'nt': assert build['Complete'] and build['InputsUnchanged']
    files = {Path(__file__).resolve(), Path(sys.executable), manifest}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']; files.add(Path(row['Path']))
    cc = shutil.which('gcc'); assert cc
    runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
    driver = Path(build['Driver']); assert sha(driver).lower() == build['DriverSha256'].lower()
    files.update((Path(cc), runtime, driver))
    host_source = args.host_source.resolve() if args.host_source else root/'test/cases/xlir_bound_activity_host.c'
    fixture_source = args.fixture_source.resolve() if args.fixture_source else root/'test/cases/xlir_bound_activity.xl'
    plan_source = args.plan_source.resolve() if args.plan_source else root/'test/cases/xlir_bound_activity_plan.c'
    files.add(fixture_source)
    files.add(plan_source)
    files.add(host_source); before = None
    assert bool(args.before_root) == bool(args.before_public)
    if args.before_root:
        br, bp = args.before_root.resolve(), args.before_public.resolve(); bm = bp/'build.json'
        bb = json.loads(bm.read_text(encoding='utf-8-sig')); files.add(bm)
        assert bb['SourceXrt'] and not bb['RuntimeOnly'] and bb['EnableExeCompiler'] == 1
        if os.name != 'nt': assert bb['Complete'] and bb['InputsUnchanged']
        for row in bb['Inputs']+bb['Objects']+bb['Headers']:
            assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']; files.add(Path(row['Path']))
        bt = br/'release/xl.dll' if os.name == 'nt' else Path(bb['NativeRoot'])/'release/libxl.so'
        files.add(bt); before = (br, bp, bb, bt)
    names = ('xlir_bound_activity.xl', 'xlir_bound_activity_host.c', 'xlir_dynamic_bound_method.xl',
        'xlir_dynamic_bound_method_contract.c', 'xlir_dynamic_bound_method_host.c', 'xlir_dynamic_bound_method_runtime.c')
    files.update(root/'test/cases'/name for name in names)
    files.update((root/'test/support/module_product_memory.c', root/'test/support/module_product_memory.h',
        root/'test/cases/xlir_generated_module_host.c', root/'test/cases/cgen_class_host.c'))
    compat = ('cgen_dynamic_method_bound_lifetime', 'cgen_dynamic_method_inheritance',
        'cgen_dynamic_method_overload', 'cgen_generic_dynamic_method', 'cgen_dynamic_method_arguments')
    files.update(root/'test/cases'/(name+'.xl') for name in compat)
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    rows, products, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows,
        'Products': products, 'Errors': errors, 'Scope': __doc__, 'ActualTwoTccModules': True,
        'ContinuousAllocationFailure': True, 'GeneratedReadCertificate': True,
        'NativeRestoreCgenCutover': False, 'ShippingLibraryAcceptance': False, 'Full53Acceptance': False,
        'Sanitized': False, 'LinkedCompilerRuntimeOptimization': 'O0',
        'NativeHostWarnings': 'C11 Wall Wextra Werror without exemptions'}
    report['PackagedAotExecution'] = False
    report['ArtifactDependencyMetadataProof'] = True
    report['SourceDriverAotLimitations'] = 'Windows source driver has no current embedded compiler-source VFS; Linux source driver has no exe mode. Separate shipping product acceptance remains required.'
    report['ExpectedBaselineAssertion'] = bool(before)
    report['TestOnlyHostSource'] = str(host_source)
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(runtime, out/'xl.dll')
    env = os.environ.copy()
    if os.name != 'nt': env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I'+str(root),
        '-I'+str(root/'release'), '-I'+str(root/'release/include'), '-I'+str(root/'lib/tcc'), '-I'+str(public/'include')]
    legacy = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-label',
        '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
        '-Wno-missing-braces', '-Wno-pointer-sign', '-Wno-format-truncation']
    report['GeneratedLegacyWarningAllowances'] = legacy
    links = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    if os.name == 'nt': links += [str(runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else: flags += ['-D_GNU_SOURCE']; links += [str(runtime), '-Wl,-rpath,'+str(runtime.parent), '-pthread', '-ldl', '-lm']
    suffix = '.exe' if os.name == 'nt' else ''
    def run(name, command, cwd=root, environment=env, expected_assertion=False):
        command = list(map(str, command)); options = {}
        if os.name != 'nt':
            import resource
            def no_core(): resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            options['preexec_fn'] = no_core
        result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, timeout=180, **options)
        data = result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'ExpectedAssertion': expected_assertion}); print(name+': '+str(result.returncode), flush=True)
        if expected_assertion:
            assert result.returncode != 0 and b'xl6CodeOwnerScopeActive(owner) == active' in data, data[-6000:]
        else: assert result.returncode == 0, data[-6000:].decode(errors='replace')
        return data.decode(errors='replace')
    def product(path): products.append({'Path': str(path), 'Sha256': sha(path)})
    try:
        if before:
            br, bp, bb, bt = before; lane = out/'before'; lane.mkdir(); be = env.copy()
            bf = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', '-DXL_STATIC_EMBED',
                '-I'+str(br), '-I'+str(br/'release'), '-I'+str(br/'release/include'),
                '-I'+str(br/'lib/tcc'), '-I'+str(bp/'include')]
            bl = [row['Path'] for row in bb['Objects'] if Path(row['Path']).name not in
                ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
            if os.name == 'nt':
                shutil.copy2(bt, lane/'xl.dll'); bl += [str(bt), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
                tcc = br/'res/tcc/win64/x86_64'
            else:
                bf += ['-D_GNU_SOURCE']; be['XL6_SOURCE_NATIVE_ROOT'] = bb['NativeRoot']
                bl += [str(bt), '-Wl,-rpath,'+str(bt.parent), '-pthread', '-ldl', '-lm']
                tcc = Path(bb['NativeRoot'])/'build/linux/tcc-dist'
            binary = lane/('tcc'+suffix)
            define = '-DXL6_BOUND_ACTIVITY_SOURCE="'+fixture_source.as_posix()+'"'
            run('before.compile', [cc, *bf, define, host_source, br/'test/support/module_product_memory.c', *bl, '-o', binary], br, be); product(binary)
            run('before.run', [binary, bt, tcc, lane/'temps', bp/'include', '0'], br, be, True)
        contract = out/('contract'+suffix)
        run('contract.compile', [cc, *flags, *legacy, root/'test/cases/xlir_dynamic_bound_method_contract.c', *links, '-o', contract]); product(contract)
        c, header, bind = out/'bound.c', out/'bound.h', out/'bound.bind.h'
        run('contract.run', [contract, c, header, bind]); product(c); product(header); product(bind)
        run('contract.repeat', [contract, out/'repeat.c', out/'repeat.h', out/'repeat.bind.h'])
        for before, after in ((c, out/'repeat.c'), (header, out/'repeat.h'), (bind, out/'repeat.bind.h')):
            assert sha(before) == sha(after); product(after)
        text = c.read_text(encoding='utf-8-sig')
        assert 'xl6ClosureBoxErasedV1' in text and 'xlir_bound_method_layout' in text
        assert 'xlir_bound_method_drop' not in text and 'xlir_bound_method_trace' not in text
        for mode in ('ir-typed', 'ir-lowered'):
            target = out/(mode+'.json'); run(mode, [driver, fixture_source, mode, target]); product(target)
            ir = json.loads(target.read_text(encoding='utf-8-sig'))
            assert ir['boundMethodStorageAbi'] == (0 if mode == 'ir-typed' else 1)
        for optimization in ('O0', 'O2'):
            api = out/('tcc-'+optimization+suffix)
            define = '-DXL6_BOUND_ACTIVITY_SOURCE="'+fixture_source.as_posix()+'"'
            run(optimization+'.api.compile', [cc, *flags, '-'+optimization, '-DXL6_BOUND_ACTIVITY_ERASED_TEST', define,
                host_source, root/'test/support/module_product_memory.c', *links, '-o', api]); product(api)
            tcc = root/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
            assert 'exact tool-only heap baseline' in run(optimization+'.api.run', [api, runtime, tcc, out/('temps-'+optimization), public/'include', '1'])
            rich = out/('rich-'+optimization+suffix)
            defines = ['-DXL6_BOUND_GENERATED_C="'+c.as_posix()+'"', '-DXL6_BOUND_GENERATED_BIND="'+bind.as_posix()+'"']
            run(optimization+'.rich.compile', [cc, *flags, *legacy, '-'+optimization, *defines,
                root/'test/cases/xlir_dynamic_bound_method_host.c', root/'test/cases/xlir_dynamic_bound_method_runtime.c', *links, '-o', rich]); product(rich)
            assert '300 real async suspensions' in run(optimization+'.rich.run', [rich, '--smoke'])
        for name in compat:
            cfile = out/(name+'.c'); run(name+'.emit', [contract, root/'test/cases'/(name+'.xl'), cfile]); product(cfile)
            host = out/(name+'.o')
            run(name+'.compile', [cc, *flags, *legacy, '-O0', '-DXLIR_GENERATED_MODULE_USER_ENTRY',
                '-DXLIR_GENERATED_MODULE_PREINCLUDED', '-include', cfile, '-c', root/'test/cases/xlir_generated_module_host.c', '-o', host]); product(host)
            binary = out/(name+suffix)
            run(name+'.link', [cc, *flags, *legacy, host, root/'test/cases/cgen_class_host.c', *links, '-o', binary]); product(binary)
            run(name+'.run', [binary])
        # Prove canonical artifact dependencies, with no ordinary closure
        # target masking selection. This is explicitly NOT packaged AOT.
        plan = out/('plan'+suffix)
        run('plan.compile', [cc, *flags, *legacy, plan_source, *links, '-o', plan]); product(plan)
        assert 'direct dynamic call pruned' in run('plan.run', [plan, fixture_source])
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged']
    except Exception as error: errors.append(repr(error)); print(repr(error), file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
