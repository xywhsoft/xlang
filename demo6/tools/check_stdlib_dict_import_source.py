"""Dict XLIR consuming import and closed JSON/XLON native restore.

Fresh full source compiler, pre-output corrupt IR refusal, deterministic strict
C, JIT and independent pruned O0/O2 runtime links with complete allocation
prefixes. Optional ASan/UBSan covers selected generated/runtime products, not
the whole compiler, shipping DLLs or the full 53-module objective.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc; driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=root/'test/cases'; fixtures=[cases/(s+'.xl') for s in ('stdlib_dict_import_source','stdlib_dict_import_leaf')]
    contract=cases/'xlir_dict_import_source_contract.c'; host=cases/'xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,contract,host,memory,
        cases/'xlir_artifact_runtime_contract.c',*fixtures}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(s),Sha256=sha(s)) for s in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; plans=[]; products=[]; objects=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        PrunedExecutions=plans,Products=products,Objects=objects,NegativeControls=controls,LanguageDictImportCutover=True,LanguageDictRestoreCutover=True,
        ShippingAcceptance=False,Full53Acceptance=False,PrunedGeneratedRuntimeSanitized=a.sanitize,
        SanitizedOptimizations=['O0'] if a.sanitize else [],WholeCompilerSanitized=False)
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if a.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),'-Wno-unused-function',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links+=[product]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])+systems
    sanitizer_flags=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
    def run(name,command,reject=False):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        print(name+': '+str(result.returncode),flush=True)
        assert (result.returncode!=0 if reject else result.returncode==0),(name,data[-8000:].decode(errors='replace'))
        if reject: assert b'assert' in data.lower(),(name,data[-1000:])
        return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix); run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        log=run('contract.run',[binary,*[f.as_posix() for f in fixtures]])
        found=re.search(r'imports=(\d+) restores=(\d+) mutations=(\d+)',log); assert found
        report['Imports'],report['NativeDictRestoreNodes'],report['CorruptContractsRefused']=map(int,found.groups())
        selected=[json.loads(s) for s in log.splitlines() if s.startswith('{"Fixture":')]; assert len(selected)==2
        for index,(fixture,plan) in enumerate(zip(fixtures,selected)):
            tag=('source','leaf')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated])
            run(tag+'.repeat',[driver,fixture,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            text=generated.read_text(encoding='utf-8-sig')
            assert 'xl6DictImportProviderV1()' in text and 'xl6CollectionImportChainSourceTakeV1(' in text
            assert 'xl6PersistenceRestoreParsedTakeV1(' in text
            assert 'xl6PersistenceRestoreCollectionV1(' not in text
            for opt in ('O0','O2'):
                # Keep optimized strict execution independent of sanitizer
                # instrumentation. O0 sanitizes every complete fault prefix
                # without an instrumentation-expanded O2 compiler bottleneck.
                san=sanitizer_flags if opt=='O0' else []
                lane=tag+'.'+opt; impl=out/(lane+'.runtime.c')
                impl.write_text(''.join('#define '+m+'\n' for m in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                providers=[]
                for i,source in enumerate([impl,*[root/s for s in plan['Sources']],memory]):
                    obj=out/(lane+'.'+str(i)+'.o'); run(lane+'.provider.'+str(i),[cc,*flags,*san,'-'+opt,'-c',source,'-o',obj]); providers.append(obj)
                    objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                binary=out/(lane+suffix)
                run(lane+'.build',[cc,*flags,*san,'-'+opt,'-DXL6_PRODUCT_USER_ENTRY','-include',generated,host,*providers,*systems,'-o',binary])
                log=run(lane+'.prefixes',[binary,'--all']); match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                runs,first=map(int,match.groups()); assert runs==first+1
                prefixes.append(dict(Fixture=tag,Optimization=opt,Runs=runs,FirstNoHit=first))
                plans.append(dict(Fixture=tag,Optimization=opt,Sanitized=bool(san),Sources=plan['Sources'],NativeModules=plan['NativeModules']))
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
                if index==1 and opt=='O0':
                    # An actual compiled omission must fail a restored class
                    # with a default native Dict; direct Dict-only graphs did
                    # not exercise this ownership read boundary.
                    source=root/'src/runtime/xl6_dict_import_chain.c'
                    body=source.read_text(encoding='utf-8'); before='.OwnershipAdapter=xl6DictOwnershipAdapterV1'
                    assert body.count(before)==1
                    changed=out/'dict-ownership-omitted.c'; changed.write_text(body.replace(before,'.OwnershipAdapter=NULL'),encoding='utf-8')
                    obj=out/'dict-ownership-omitted.o'; run('ownership-omitted.compile',[cc,*flags,*san,'-I'+str(root/'src/runtime'),'-O0','-c',changed,'-o',obj])
                    replaced=list(providers); position=plan['Sources'].index('src/runtime/xl6_dict_import_chain.c')+1
                    replaced[position]=obj; broken=out/('dict-ownership-omitted'+suffix)
                    run('ownership-omitted.build',[cc,*flags,*san,'-O0','-DXL6_PRODUCT_USER_ENTRY','-include',generated,host,*replaced,*systems,'-o',broken])
                    run('ownership-omitted.reject',[broken,'999999'],True)
                    objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                    controls.append(dict(Name='dict-ownership-omitted',Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),
                        Product=str(broken),ProductSha256=sha(broken)))
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs); assert report['InputsUnchanged']
        report['Complete']=len(prefixes)==4
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
