"""Complete native graph clone recipes, restore isolation and exact pruning.

Fresh compiler contracts, deterministic C, JIT, independent strict O0/O2
products and complete allocation-failure prefixes. Linux ASan/UBSan covers O0
generated/runtime products only; this is not full53 or shipping acceptance.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true'); args=parser.parse_args()
    root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc
    driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=root/'test/cases'; fixtures=[cases/(name+'.xl') for name in
        ('stdlib_graph_clone','stdlib_graph_clone_dict_leaf','stdlib_graph_clone_continuation','stdlib_graph_clone_restore')]
    contract=cases/'xlir_graph_clone_contract.c'; host=cases/'xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,contract,host,memory,
        cases/'xlir_artifact_runtime_contract.c',*fixtures}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; plans=[]; products=[]; objects=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        ClosedPrefixes=prefixes,PrunedExecutions=plans,Products=products,Objects=objects,NegativeControls=controls,
        ShippingAcceptance=False,Full53Acceptance=False,PrunedGeneratedRuntimeSanitized=args.sanitize,
        SanitizedOptimizations=['O0'] if args.sanitize else [],WholeCompilerSanitized=False)
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if args.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),'-Wno-unused-function',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links+=[product]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])+systems
    sanitized=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.sanitize else []
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
        log=run('contract.run',[binary,*[fixture.as_posix() for fixture in fixtures]])
        match=re.search(r'operations=(\d+) isolations=(\d+) corrupt=(\d+)',log); assert match
        report['DeepCloneOperations'],report['RestoreIsolationNodes'],report['CorruptContractsRefused']=map(int,match.groups())
        selected=[json.loads(line) for line in log.splitlines() if line.startswith('{"Fixture":')]; assert len(selected)==4
        for index,(fixture,plan) in enumerate(zip(fixtures,selected)):
            tag=('mixed','dict-leaf','continuation','restore')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat])
            assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8-sig')
            assert 'xl6ValueDeepCloneNativeV1(' in body and 'xl6DictGraphProviderV1()' in body
            for legacy in ('xl6ValueCloneGraphV1','xl6ValueCloneSetGraphV1','xl6ValueCloneCollectionsV1'):
                assert legacy+'(' not in body
            if index!=0:
                assert 'xl6ListGraphProviderV1()' not in body and 'xl6SetGraphProviderV1()' not in body
            for opt in ('O0','O2'):
                san=sanitized if opt=='O0' else []; lane=tag+'.'+opt; impl=out/(lane+'.runtime.c')
                impl.write_text(''.join('#define '+module+'\n' for module in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                providers=[]
                for position,source in enumerate([impl,*[root/source for source in plan['Sources']],memory]):
                    obj=out/(lane+'.'+str(position)+'.o'); run(lane+'.provider.'+str(position),[cc,*flags,*san,'-'+opt,'-c',source,'-o',obj])
                    providers.append(obj); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
                binary=out/(lane+suffix)
                run(lane+'.build',[cc,*flags,*san,'-'+opt,'-include',generated,host,*providers,*systems,'-o',binary])
                log=run(lane+'.prefixes',[binary,'--all']); match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                runs,first=map(int,match.groups()); assert runs==first+1
                prefixes.append(dict(Fixture=tag,Optimization=opt,Runs=runs,FirstNoHit=first))
                plans.append(dict(Fixture=tag,Optimization=opt,Sanitized=bool(san),Sources=plan['Sources'],NativeModules=plan['NativeModules']))
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
                if index==1 and opt=='O0':
                    before='&(const XL6_ValueGraphPlanV1){sizeof(XL6_ValueGraphPlanV1),(const XL6_ValueGraphProviderV1*[]){xl6DictGraphProviderV1()},1,NULL,0}'
                    assert body.count(before)>=2
                    changed=out/'dict-provider-omitted.c'; changed.write_text(body.replace(before,
                        '&(const XL6_ValueGraphPlanV1){sizeof(XL6_ValueGraphPlanV1),NULL,0,NULL,0}'),encoding='utf-8')
                    broken=out/('dict-provider-omitted'+suffix)
                    run('dict-omitted.build',[cc,*flags,*san,'-O0','-include',changed,host,*providers,*systems,'-o',broken])
                    run('dict-omitted.reject',[broken,'999999'],True)
                    controls.append(dict(Name='dict-provider-omitted',Compiled=True,Rejected=True,Source=str(changed),SourceSha256=sha(changed),
                        Product=str(broken),ProductSha256=sha(broken)))
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        report['Complete']=len(prefixes)==8
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
