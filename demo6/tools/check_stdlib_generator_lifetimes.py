"""Current closed IR generator storage, real callback-library unmapping and
managed graph retirement. Preserve all original populations on Windows/Linux
O0/O2; optionally instrument Linux selected native closure and callback library.
This is not arbitrary generated-module unloading or full53/shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt');cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    files=pinned|{Path(__file__).resolve(),Path(cc),runtime,root/'test/cases/xlir_artifact_runtime_contract.c'}
    for pattern in ('xlir_generator*.c','xlir_generator*.h','xlir_generator*.xl','xlir_*assert.h','xlir_continuation_owner_test.h','xlir_lifetime_platform.h'):
        files.update((root/'test/cases').glob(pattern))
    files.add(root/'src/runtime/xl6_ownership_plan.c')
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];products=[];proofs=[];failures=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'],XL6_TCC_LIB=str(Path(build['NativeRoot'])/'build/linux/tcc-dist'))
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Proofs=proofs,Failures=failures,Scope=__doc__,
        ComparedSourceInputs=compared,SelectedClosureSanitized=a.sanitize,SourceProviderOverrides=False,ExplicitSubset=False,
        CallbackLibraryUnmapAcceptance=False,GeneratedModuleUnmapAcceptance=False,Full53Acceptance=False,ShippingArtifact=False)
    def run(name,cmd):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,cmd)),Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert r.returncode==0,(name,data[-6000:]);return data.decode(errors='replace')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/xrt'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/tcc'),
        '-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    try:
        planner=out/('artifact'+ext);run('artifact-build',[cc,*flags,'-O0',root/'test/cases/xlir_artifact_runtime_contract.c',*objects,runtime,*links,'-o',planner]);product(planner)
        generated={};native={};sources=set()
        for family in ('storage','managed'):
            tool=out/(family+'-contract'+ext)
            run(family+'-contract-build',[cc,*flags,'-O0',root/'test/cases'/('xlir_generator_'+family+'_contract.c'),*objects,runtime,*links,'-o',tool]);product(tool)
            for repeat in ('','-repeat'):
                prefix=out/(family+repeat);run(family+repeat+'-generate',[tool,str(prefix)+'.c',str(prefix)+'.h',str(prefix)+'.json'])
            for suffix in ('c','h','json'):
                first=out/(family+'.'+suffix);other=out/(family+'-repeat.'+suffix);assert sha(first)==sha(other);product(first);product(other)
            generated[family]=out/(family+'.c')
            path=out/(family+'-native.c');text=run(family+'-plan',[planner,'--emit-native','test/cases/xlir_generator_'+family+'.xl',path])
            plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1
            native[family]=path;sources.update(plans[0]['Sources'])
        # The handwritten retirement host additionally exercises the production
        # shared Plan. This is an explicit harness dependency, not a Cgen override.
        sources.add('src/runtime/xl6_ownership_plan.c');report['HarnessProviders']=['src/runtime/xl6_ownership_plan.c']
        report['ProviderSources']=sorted(sources)
        path=out/'native.c';raw=native['managed'].read_text();modules=set()
        for source in native.values():
            text=source.read_text();assert 'XRT_MODULE_ALL' not in text and 'XRUNTIME_MODULE_ALL' not in text
            modules.update(re.findall(r'^#define ((?:XRT|XRUNTIME)_MODULE_\w+)',text,re.M))
        modules.update(('XRT_MODULE_MEMORY_DEBUG','XRT_MODULE_THREAD','XRT_MODULE_VALUE_GRAPH'))
        report['NativeModules']=sorted(modules)
        path.write_text(''.join('#define '+name+'\n' for name in sorted(modules))+raw);product(path)
        configs=[('O0',[]),('O2',[])]+([('san',['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'])] if a.sanitize else [])
        for opt,extra in configs:
            level='-O1' if opt=='san' else '-'+opt;sanlinks=['-fsanitize=address,undefined','-no-pie'] if opt=='san' else []
            if opt=='san':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            providers=[]
            for i,s in enumerate([path,*[root/f for f in sorted(sources)]]):
                obj=out/(opt+'-provider-'+str(i)+'.o');run(opt+'-provider-'+str(i),[cc,*flags,*extra,level,'-c',s,'-o',obj]);providers.append(obj);product(obj)
            library=out/(opt+'-callback'+('.dll' if os.name=='nt' else '.so'))
            # ASan/UBSan callback descriptors and code remain instrumented too.
            dllExtra=[x for x in extra if x!='-fno-pie']
            run(opt+'-callback-build',[cc,*flags,*dllExtra,level,'-shared',*([] if os.name=='nt' else ['-fPIC']),root/'test/cases/xlir_generator_storage_drop.c','-o',library]);product(library)
            specs=[('storage',['xlir_generator_storage_runtime.c','xlir_generator_storage_host.c'],[library],
                'Resident generators: 200 generated freezes, 200 active control/frame refusals, 200 real OOM rollbacks, 100 armed-OOM transfers, 100 actual DLL lifetimes with after-Close child leases; exact graphs and allocator balance, no native root registration'),
                ('managed',['xlir_generator_managed_runtime.c','xlir_generator_managed_host.c'],[],
                'Generated generator cycles: 140 complete six-node/seven-edge transactions, 280 finally steps, 140 finalizers, 140 context drops, 560 cross-thread freezes, 180 real-root deferrals, 140 missing-frame refusals, 3360 OOM budgets and '),
                ('lifetime',['xlir_generator_managed_lifetime.c'],[library],
                'Managed generator lifetimes: 150 last-owner reentrant closes, 150 sparse payload drops, 100 actual DLL unloads with separately held control/frame descriptors, 400 cross-thread freezes, 100 real OOM failures, 250 ABI refusals; exact memory balance')]
            for family,hosts,args,marker in specs:
                definitions=['-DXLIR_GENERATOR_STORAGE_SOURCE="'+generated['storage'].as_posix()+'"',
                    '-DXLIR_GENERATOR_STORAGE_BINDING="'+(out/'storage.h').as_posix()+'"',
                    '-DXLIR_GENERATOR_MANAGED_SOURCE="'+generated['managed'].as_posix()+'"',
                    '-DXLIR_GENERATOR_MANAGED_BINDING="'+(out/'managed.h').as_posix()+'"']
                binary=out/(family+'-'+opt+ext)
                try:
                    run(family+'-'+opt+'-build',[cc,*flags,*extra,level,*definitions,*[root/'test/cases'/h for h in hosts],*providers,*systems,*sanlinks,'-o',binary]);product(binary)
                    text=run(family+'-'+opt+'-execute',[binary,*args]);assert marker in text and ('exact memory balance' in text or 'exact graphs and allocator balance' in text)
                    if family=='lifetime':assert 'Current V3 descriptor lifetimes: 100 actual callback library unloads, independent control/frame last owners, 100 cross-thread freezes and exact memory balance' in text
                    proofs.append(dict(Family=family,Optimization=opt,Output=text))
                except AssertionError as error:
                    # Independent host families/configurations still execute;
                    # collect failures before one repair and unified rerun.
                    failures.append(dict(Family=family,Optimization=opt,Reason=str(error)))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['CallbackLibraryUnmapAcceptance']=not failures;report['Complete']=report['InputsUnchanged'] and not failures
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Generator lifecycle hosts: physical callback unmapping, cross-thread freeze and exact memory passed',flush=True)
if __name__=='__main__':main()
