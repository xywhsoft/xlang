"""Actual native List language producers and checked sparse imports.

Fresh complete source compiler; strict deterministic C, corrupt-IR refusal,
real pruned O0/O2 execution, whole module allocation prefixes and public JIT.
No compiler hooks, test factories, runtime overrides or shipping/full53 claim.
Includes closed Array/List mixed factories and shared nested language imports;
does not claim complete container, shipping-product or full53 acceptance.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    cc=shutil.which('gcc'); assert cc
    driver=Path(build['Driver']); assert sha(driver).lower()==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=root/'test/cases'
    fixtures=[cases/(name+'.xl') for name in ('stdlib_list_source_native','stdlib_list_source_factory_only',
        'stdlib_list_methods_array_prune','stdlib_list_source_mixed')]
    contract=cases/'xlir_list_source_contract.c'; host=cases/'xlir_module_product_oom_host.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),manifest,driver,product,
        contract,host,memory,cases/'xlir_artifact_runtime_contract.c',*fixtures}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True); rows=[]; sweeps=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        ClosedPrefixes=sweeps,PrunedExecutions=[],LanguageListProducersMigrated=True,
        LanguageListMigrationComplete=False,Full53Complete=False,ShippingProductAcceptance=False,
        PrunedGeneratedRuntimeSanitized=a.sanitize,WholeCompilerSanitized=False)
    env=os.environ.copy(); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if a.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    full=flags+['-DXL_STATIC_EMBED','-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links+=[str(product)]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(product.parent)])+systems
    san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Complete=not result.returncode,Seconds=time.monotonic()-start))
        print(name+': '+str(result.returncode),flush=True)
        assert not result.returncode,(name,data[-8000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix)
        run('contract.build',[cc,*full,'-O0',contract,*links,'-o',binary])
        log=run('contract.run',[binary,*[f.as_posix() for f in fixtures]])
        match=re.search(r'creates=(\d+) imports=(\d+) mutations=(\d+)',log); assert match
        report['Producers'],report['Imports'],report['CorruptContractsRefused']=map(int,match.groups())
        plans=[json.loads(line) for line in log.splitlines() if line.startswith('{"Fixture":')]; assert len(plans)==4
        for index,(fixture,plan) in enumerate(zip(fixtures,plans)):
            tag=('source','factory','array','mixed')[index]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c')
            lowered=out/(tag+'.lowered.json')
            run(tag+'.lowered',[driver,fixture,'ir-lowered',lowered])
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat])
            assert sha(generated)==sha(repeat)
            text=generated.read_text(encoding='utf-8-sig')
            if index!=2:
                assert 'xl6ListCreateTakeV1(' in text and 'xrtValueIntMap()' not in text
                assert not re.search(r'=\s*xlir_xvalue_type_rebind_owned\(',text)
            else: assert 'xl6ListCreateTakeV1(' not in text and 'xl6ListImportProviderV1(' not in text
            assert ('xl6CollectionImportChainSourceTakeV1(' in text)==(index in (0,3))
            if index in (0,3): assert 'XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1' in text
            for level in (0,2):
                lane=tag+'.O'+str(level); impl=out/(lane+'.runtime.c'); selected=[]
                impl.write_text(''.join('#define '+s+'\n' for s in plan['NativeModules'])+
                    '#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
                for i,source in enumerate([impl,*[root/s for s in plan['Sources']],memory]):
                    obj=out/(lane+'.'+str(i)+'.o')
                    run(lane+'.provider.'+str(i),[cc,*full,*san,'-O'+str(level),'-c',source,'-o',obj]); selected.append(obj)
                binary=out/(lane+suffix)
                run(lane+'.build',[cc,*full,*san,'-O'+str(level),'-include',generated,host,*selected,*systems,'-o',binary])
                log=run(lane+'.prefixes',[binary,'--all'])
                match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',log); assert match
                runs,first=map(int,match.groups()); assert runs==first+1
                sweeps.append(dict(Fixture=tag,Optimization=level,Runs=runs,FirstNoHit=first))
                report['PrunedExecutions'].append(dict(Fixture=tag,Optimization=level,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit.result')])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); assert report['InputsUnchanged']
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
