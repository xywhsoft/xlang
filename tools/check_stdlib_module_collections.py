"""Module-wide resident restore admission and observed collection loop sources.

Fresh compiler, deterministic C/JIT, strict pruned O0/O2 products and closed
allocation prefixes. Optional ASan/UBSan covers selected generated/runtime O0
and the focused native host gate, not the compiler or full53/shipping release.
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
    cc=shutil.which('gcc'); assert cc
    driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=root/'test/cases'; fixtures=[cases/(name+'.xl') for name in
        ('stdlib_restore_module_collections','stdlib_loop_field_source','stdlib_graph_clone_restore')]
    contract=cases/'xlir_module_collection_contract.c'; host=cases/'xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,contract,host,memory,
        root/'tools/check_stdlib_list_native.py',cases/'stdlib_restore_module_read_host.c',
        cases/'xlir_artifact_runtime_contract.c',*fixtures}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; prefixes=[]; plans=[]; products=[]; objects=[]; controls=[]; reports=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        ClosedPrefixes=prefixes,PrunedExecutions=plans,Products=products,Objects=objects,NegativeControls=controls,
        Reports=reports,ShippingAcceptance=False,Full53Acceptance=False,WholeCompilerSanitized=False,
        SanitizedOptimizations=['O0'] if args.sanitize else [])
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
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=1800)
        data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        print(name+': '+str(result.returncode),flush=True)
        assert (result.returncode!=0 if reject else result.returncode==0),(name,data[-8000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix); run('contract.build',[cc,*flags,'-O0',contract,*links,'-o',binary])
        log=run('contract.run',[binary,*[fixture.as_posix() for fixture in fixtures]])
        match=re.search(r'roots=(\d+) observations=(\d+) corrupt=(\d+)',log); assert match
        report['RestoreRoots'],report['ObservedSources'],report['CorruptContractsRefused']=map(int,match.groups())
        selected=[json.loads(line) for line in log.splitlines() if line.startswith('{"Fixture":')]; assert len(selected)==3
        for index,(fixture,plan) in enumerate(zip(fixtures,selected)):
            tag=('mixed','fields','dict-leaf')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat])
            assert sha(generated)==sha(repeat)
            body=generated.read_text(encoding='utf-8-sig')
            if index==0:
                assert 'ownership_adapters[] = {xl6ListOwnershipAdapterV1,xl6SetOwnershipAdapterV1,xl6DictOwnershipAdapterV1,}' in body
            if index==2:
                assert 'ownership_adapters[] = {xl6DictOwnershipAdapterV1,}' in body
                assert 'xl6ListOwnershipAdapterV1' not in body and 'xl6SetOwnershipAdapterV1' not in body
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
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        native=[sys.executable,root/'tools/check_stdlib_list_native.py','--root',root,'--module-read',
            '--modes','plan-module-read','--operations','plan-module-read,plan-module-clone','--batch-plan-prefixes']
        run('native-host',[*native,'--out',out/'native',*(['--sanitize'] if args.sanitize else [])])
        native_report=out/'native/results.json'; details=json.loads(native_report.read_text())
        assert details['Complete'] and details['InputsUnchanged']
        reports.append(dict(Path=str(native_report),Sha256=sha(native_report)))
        original=cases/'stdlib_restore_module_read_host.c'; broken=out/'omitted-host.c'
        text=original.read_text(); needle='plan.ownershipAdapterCount=mode==1 ? 0 : 1;'
        assert text.count(needle)==1; broken.write_text(text.replace(needle,'plan.ownershipAdapterCount=0;'),encoding='utf-8')
        log=run('adapter-omitted.reject',[*native,'--out',out/'omitted','--module-read-host',broken,'--operations',''],True)
        rejected=json.loads((out/'omitted/results.json').read_text()); assert not rejected['Complete']
        assert any(row['Name']=='O0.build' and row['Exit']==0 for row in rejected['Rows'])
        assert any(row['Name'].endswith('.plan-module-read') and row['Exit']!=0 for row in rejected['Rows'])
        control_product=out/'omitted'/('list-O0'+suffix); assert control_product.is_file()
        controls.append(dict(Name='resident-adapter-omitted',Compiled=True,Rejected=True,Source=str(broken),SourceSha256=sha(broken),
            Product=str(control_product),ProductSha256=sha(control_product)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        report['Complete']=len(prefixes)==6 and len(controls)==1
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
