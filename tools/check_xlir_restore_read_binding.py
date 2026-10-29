"""Current-source artifact/host read-capability binding, not restore cutover.

Typed/lowered mutation rejection, deterministic C/header, copied once before
entry, zero-allocation malformed/rebind paths, failed-init sealing, pruning,
actual API/TCC read Count/Trace and foreign refusal. Generated/native hosts are
O0/O2; linked complete compiler/runtime objects are source O0. No packaged
AOT, shipping library/VFS, sanitizer or full53 acceptance is claimed.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--before-root', type=Path)
    parser.add_argument('--before-public', type=Path)
    parser.add_argument('--test-root', type=Path, help='Explicit test-only fixture overrides, hashed; compiler/runtime remain frozen')
    args = parser.parse_args(); root, public, out = args.root.resolve(), args.public_directory.resolve(), args.out.resolve()
    tests = args.test_root.resolve() if args.test_root else root
    assert not out.exists() and bool(args.before_root) == bool(args.before_public)
    files = {Path(__file__).resolve(), Path(sys.executable)}
    def read_build(path):
        manifest = path/'build.json'; files.add(manifest)
        build = json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
        if os.name != 'nt': assert build['Complete'] and build['InputsUnchanged']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']; files.add(Path(row['Path']))
        files.add(Path(build['Driver'])); assert sha(build['Driver']).lower() == build['DriverSha256'].lower()
        return build
    build = read_build(public); before = None
    if args.before_root: before = (args.before_root.resolve(), args.before_public.resolve(), read_build(args.before_public.resolve()))
    cc, cxx = shutil.which('gcc'), shutil.which('g++'); assert cc and cxx
    files.update((Path(cc), Path(cxx)))
    runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'; files.add(runtime)
    names = ('xlir_restore_read_binding.xl', 'xlir_restore_read_binding_pruned.xl',
        'xlir_restore_read_binding_contract.c', 'xlir_restore_read_binding_api_host.c',
        'xlir_restore_read_binding_static_host.c', 'xlir_restore_read_binding_header_host.c')
    files.update(tests/'test/cases'/name for name in names)
    files.update((root/'test/support/module_product_memory.c', root/'test/support/module_product_memory.h'))
    if before:
        br, bp, bb = before
        files.add(br/'release/xl.dll' if os.name == 'nt' else Path(bb['NativeRoot'])/'release/libxl.so')
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    out.mkdir(parents=True); rows, products, errors = [], [], []
    if os.name == 'nt': shutil.copy2(runtime,out/'xl.dll')
    env = os.environ.copy()
    if os.name != 'nt': env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    suffix = '.exe' if os.name == 'nt' else ''
    legacy = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-label',
        '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers']
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Products': products,
        'Errors': errors, 'Scope': __doc__, 'NativeRestoreCgenCutover': False, 'PackagedAot': False,
        'ShippingLibraryAcceptance': False, 'Full53Acceptance': False, 'Sanitized': False,
        'ContinuousLanguageOomSweep': False, 'BindingZeroAllocationFailureProbe': True,
        'LinkedCompilerRuntimeOptimization': 'O0', 'GeneratedLegacyWarningAllowances': legacy,
        'NativeHostWarnings': 'C11 Wall Wextra Werror without exemptions', 'ExpectedBaselineAssertion': bool(before)}
    report['TestOnlyRoot'] = str(tests)
    def configuration(r,p,b):
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I'+str(r),
            '-I'+str(r/'release'), '-I'+str(r/'release/include'), '-I'+str(r/'lib/tcc'), '-I'+str(p/'include')]
        links = [row['Path'] for row in b['Objects'] if Path(row['Path']).name not in
            ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
        product = r/'release/xl.dll' if os.name == 'nt' else Path(b['NativeRoot'])/'release/libxl.so'
        if os.name == 'nt': links += [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        else: flags += ['-D_GNU_SOURCE']; links += [str(product), '-Wl,-rpath,'+str(product.parent), '-pthread', '-ldl', '-lm']
        return flags,links,product
    flags,links,_ = configuration(root,public,build)
    def run(name,command,cwd=root,environment=env,baseline=False):
        command = list(map(str,command)); options = {}
        if os.name != 'nt':
            import resource
            def no_core(): resource.setrlimit(resource.RLIMIT_CORE,(0,0))
            options['preexec_fn'] = no_core
        result = subprocess.run(command,cwd=cwd,env=environment,capture_output=True,timeout=240,**options)
        data = result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'ExpectedAssertion': baseline})
        print(name+': '+str(result.returncode),flush=True)
        if baseline: assert result.returncode != 0 and b'resident read boundary was bound' in data, data[-6000:]
        else: assert result.returncode == 0, data[-6000:].decode(errors='replace')
        return data.decode(errors='replace')
    def product(path): products.append({'Path': str(path), 'Sha256': sha(path)})
    try:
        contract = out/('contract'+suffix)
        run('contract.compile',[cc,*flags,*legacy,tests/'test/cases/xlir_restore_read_binding_contract.c',*links,'-o',contract]); product(contract)
        for label,source in (('restore','xlir_restore_read_binding.xl'), ('pruned','xlir_restore_read_binding_pruned.xl')):
            c,h,ir = out/(label+'.c'),out/(label+'.h'),out/(label+'.json')
            run(label+'.contract',[contract,tests/'test/cases'/source,c,h,ir])
            rc,rh,ri = out/(label+'.repeat.c'),out/(label+'.repeat.h'),out/(label+'.repeat.json')
            run(label+'.repeat',[contract,tests/'test/cases'/source,rc,rh,ri])
            for a,b in ((c,rc),(h,rh),(ir,ri)): assert sha(a) == sha(b); product(a); product(b)
            needed = label == 'restore'; module = json.loads(ir.read_text(encoding='utf-8-sig'))
            assert module['moduleReadBoundaryAbi'] == int(needed)
            assert ('xl_script_bind_ownership_read_v1' in c.read_text(encoding='utf-8-sig')) == needed
            assert ('xl_script_bind_ownership_read_v1' in h.read_text(encoding='utf-8-sig')) == needed
            if not needed: assert 'xlir_ownership_read_boundary' not in c.read_text(encoding='utf-8-sig')
            for standard,compiler in (('c11',cc),('c++17',cxx)):
                binary = out/(label+'.header.'+standard+suffix)
                hf = ['-std='+standard,'-Wall','-Wextra','-Werror','-pedantic-errors','-I'+str(public/'include')]
                define = '-DXL6_READ_BINDING_HEADER="'+h.as_posix()+'"'
                run(label+'.header.'+standard+'.compile',[compiler,*hf,define,tests/'test/cases/xlir_restore_read_binding_header_host.c','-o',binary]); product(binary)
                run(label+'.header.'+standard+'.run',[binary])
        for standard,compiler in (('c11',cc),('c++17',cxx)):
            binary = out/('private.header.'+standard+suffix)
            run('private.header.'+standard+'.compile',[compiler,'-std='+standard,'-Wall','-Wextra','-Werror','-pedantic-errors',
                '-I'+str(public/'include'),tests/'test/cases/xlir_restore_read_binding_header_host.c','-o',binary]); product(binary)
            run('private.header.'+standard+'.run',[binary])
        c,pc = out/'restore.c',out/'pruned.c'
        for optimization in ('O0','O2'):
            memory = out/('memory-'+optimization+'.o')
            run(optimization+'.memory.compile',[cc,*flags,'-'+optimization,'-c',root/'test/support/module_product_memory.c','-o',memory]); product(memory)
            static = out/('static-'+optimization+suffix)
            run(optimization+'.static.compile',[cc,*flags,*legacy,'-'+optimization,'-include',c,
                tests/'test/cases/xlir_restore_read_binding_static_host.c',memory,*links,'-o',static]); product(static)
            for mode in ('0','1','2'): assert 'balanced' in run(optimization+'.static.'+mode,[static,mode])
            api = out/('api-'+optimization+suffix)
            run(optimization+'.api.compile',[cc,*flags,'-'+optimization,tests/'test/cases/xlir_restore_read_binding_api_host.c',
                memory,*links,'-o',api]); product(api)
            tcc = root/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
            assert 'exact tool-only heap baseline' in run(optimization+'.api.run',[api,runtime,tcc,out/('temps-'+optimization),public/'include',c,pc])
        if before:
            br,bp,bb = before; bf,bl,bt = configuration(br,bp,bb); lane = out/'before'; lane.mkdir(); be = env.copy()
            if os.name == 'nt': shutil.copy2(bt,lane/'xl.dll')
            else: be['XL6_SOURCE_NATIVE_ROOT'] = bb['NativeRoot']
            binary = lane/('api'+suffix)
            run('before.api.compile',[cc,*bf,'-O0',tests/'test/cases/xlir_restore_read_binding_api_host.c',
                root/'test/support/module_product_memory.c',*bl,'-o',binary],br,be); product(binary)
            tcc = br/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(bb['NativeRoot'])/'build/linux/tcc-dist'
            run('before.api.run',[binary,bt,tcc,lane/'temps',public/'include',c,pc],br,be,True)
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged'] and not errors
    except Exception as error: errors.append(repr(error)); print(repr(error),file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__ == '__main__': raise SystemExit(main())
