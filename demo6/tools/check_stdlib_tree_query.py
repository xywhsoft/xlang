"""Fresh native OrderedMap results, exact absence, ownership and checked failures.
Actual fresh source compiler; O0/O2 provider bodies and demand-linked closures.
The language still uses its previous Into API. Not language query cutover,
Future/owning-record domains, shipping DLL/VFS or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true',help='Instrument the independently linked selected native closure')
    a=parser.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    cc=shutil.which('gcc'); nm=shutil.which('nm'); driver=Path(build['Driver'])
    assert cc and nm and not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    host=root/'test/cases/stdlib_tree_query_native.c'; provider=root/'src/runtime/xl6_tree.c'
    memory=root/'test/support/module_product_memory.c'; artifact=root/'test/cases/xlir_artifact_runtime_contract.c'
    contract=root/'test/cases/xlir_tree_native_contract.c'; oldHost=root/'test/cases/stdlib_tree_native_host.c'
    selection=root/'test/cases/stdlib_tree_query_selection.xl'
    cases=[root/'test/cases'/name for name in ('xlir_avl_resource_abi.xl','stdlib_tree_native_reference.xl',
        'stdlib_tree_native_projection.xl','stdlib_nullable_resource_transport.xl',selection.name)]
    files={Path(__file__).resolve(),public/'build.json',Path(cc),Path(nm),Path(os.sys.executable),driver,runtime,
        host,provider,memory,artifact,contract,oldHost,*cases}
    for folder in ('src','lib','release/include','test/support'):
        files.update(p for p in (root/folder).rglob('*') if p.suffix in ('.c','.h','.inc'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(); rows=[]; products=[]; prefixes=[]; controls=[]; contracts=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Prefixes=prefixes,NegativeControls=controls,Contracts=contracts,NativeFreshTreeQuery=True,
        LanguageOwnedTreeQueries=False,Full53Acceptance=False,PrunedShippingArtifact=False,
        PrunedSourceNativeQuery=False,SelectedClosureSanitizers=a.sanitize,WholeRuntimeInstrumented=False,Scope=__doc__)
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,cmd,refused=False):
        command=list(map(str,cmd)); p=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600)
        data=p.stdout+p.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        if refused: assert p.returncode!=0 and (b'Assertion' in data or b'assertion' in data),(name,data[-6000:])
        else: assert p.returncode==0,(name,data[-6000:])
        return data.decode(errors='replace')
    def product(p): products.append(dict(Path=str(p),Sha256=sha(p)))
    ext='.exe' if os.name=='nt' else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Isrc/runtime','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[runtime,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    nativeObjects=[p for p in objects if Path(p).name!='xl6_tree.o']
    def prefix(binary,lane,operation,limit=4097):
        for point in range(limit):
            text=run(lane+'-'+operation+'-'+str(point),[binary,operation,point])
            if 'hit=0 complete-prefix balanced' in text:
                prefixes.append(dict(Lane=lane,Operation=operation,FirstNoHit=point,Runs=point+1)); return
        raise AssertionError('allocation prefix did not close: '+operation)
    try:
        for opt in ('O0','O2'):
            binary=out/('query-'+opt+ext)
            run(opt+'-build',[cc,*flags,'-'+opt,host,provider,memory,*nativeObjects,*links,'-o',binary]); product(binary)
            run(opt+'-vectors',[binary]); run(opt+'-lifetimes',[binary,'repeat'])
            for operation in ('pair','get','take'): prefix(binary,opt,operation)
            probe=out/('probe-'+opt+'.o'); probeBinary=out/('probe-'+opt+ext)
            run(opt+'-probe-body',[cc,*flags,'-'+opt,'-DxrtTypeCopyValue=xl6TreeQueryProbeCopy',
                '-DxrtTypeDropValue=xl6TreeQueryProbeDrop','-c',provider,'-o',probe]); product(probe)
            run(opt+'-probe-build',[cc,*flags,'-'+opt,host,memory,probe,*nativeObjects,*links,'-o',probeBinary]); product(probeBinary)
            run(opt+'-copy-faults',[probeBinary,'copy-faults'])
            oldBinary=out/('legacy-'+opt+ext)
            run(opt+'-legacy-build',[cc,*flags,'-'+opt,oldHost,provider,memory,*nativeObjects,*links,'-o',oldBinary]); product(oldBinary)
            run(opt+'-legacy-vectors',[oldBinary])
            for operation in ('create','clone','endpoint','projection'): prefix(oldBinary,opt+'-legacy',operation)
        report['LifetimesPerOptimization']=1000; report['LogicalCopyFailuresPerOptimization']=8
        body=provider.read_text(encoding='utf-8'); start=body.index('static bool querySlotShape('); end=body.index('static bool copyPair(',start)
        query=body[start:end]
        mutations={
            'drop-leak':('xrtTypeDropValue(keyType,keyOutput.data);','(void)keyType;'),
            'false-presence':('if (success) *found=present;','if (success) *found=present || true;'),
            'copy-not-take':('xrtTypedTreeTake(owner->storage,search,valueOutput.data)',
                'xrtTypeCopyValue(valueType,valueOutput.data,value)'),
            'overwrite-owned':('return querySlotEmpty(&layout->value,value.data) && (!pair || querySlotEmpty(&layout->key,key.data));',
                'return querySlotEmpty(&layout->value,value.data) || true;'),
        }
        for tag,(old,new) in mutations.items():
            assert query.count(old)==1,(tag,old)
            bad=out/(tag+'.c'); bad.write_text(body[:start]+query.replace(old,new)+body[end:],encoding='utf-8'); product(bad)
            binary=out/(tag+ext)
            run(tag+'-build',[cc,*flags,'-O0',host,bad,memory,*nativeObjects,*links,'-o',binary]); product(binary)
            args=[binary,'pair',1] if tag=='drop-leak' else [binary]
            run(tag+'-reject',args,refused=True); controls.append(dict(Name=tag,CompiledProvider=str(bad),ActualExecution=True))
        proof=out/('contract'+ext); planner=out/('planner'+ext)
        run('contract-build',[cc,*flags,'-Ilib/tcc','-O0',contract,*objects,*links,'-o',proof]); product(proof)
        run('planner-build',[cc,*flags,'-Ilib/tcc','-O0',artifact,*objects,*links,'-o',planner]); product(planner)
        for case in cases:
            run(case.stem+'-jit',[driver,case,'run',out/(case.stem+'-jit')])
            text=run(case.stem+'-contract',[proof,case])
            match=re.search(r'(\d+) actual factories \((\d+) nominal\), (\d+) operations, (\d+) rejected mutations',text); assert match
            contracts.append(dict(Source=str(case),**dict(zip(('Factories','Nominal','Operations','RejectedMutations'),map(int,match.groups())))))
        implementation=out/'selected-runtime.c'
        text=run('selected-plan',[planner,'--emit-native',selection.as_posix(),implementation])
        plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')]; assert len(plans)==1
        plan=plans[0]; assert 'src/runtime/xl6_tree.c' in plan['Sources']
        raw=implementation.read_text(); assert '#define XRT_MODULE_ALL' not in raw and '#define XRUNTIME_MODULE_ALL' not in raw
        implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw,encoding='utf-8'); product(implementation)
        report['PrunedPlan']=plan
        generatedFlags=flags+['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
            '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
            '-Wno-missing-field-initializers','-Wno-pointer-sign']
        instrumentation=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
        assert not a.sanitize or os.name!='nt'
        for opt in ('O0','O2'):
            providers=[]
            for at,source in enumerate([implementation,*[root/s for s in plan['Sources']],memory]):
                obj=out/('selected-'+opt+'-'+str(at)+'.o')
                run('selected-'+opt+'-body-'+str(at),[cc,*generatedFlags,*instrumentation,'-'+opt,'-c',source,'-o',obj]); product(obj); providers.append(obj)
            binary=out/('selected-'+opt+ext)
            run('selected-'+opt+'-link',[cc,*flags,*instrumentation,'-'+opt,host,*providers,*systems,'-o',binary]); product(binary)
            run('selected-'+opt+'-vectors',[binary]); run('selected-'+opt+'-lifetimes',[binary,'repeat'])
            symbols=run('selected-'+opt+'-symbols',[nm,'--defined-only',binary])
            assert re.search(r'\bxl6TreeQueryV2\b',symbols)
            assert not re.search(r'\bxl6(?:SpscQueue|MpscQueue|MpmcQueue|SlotMap|Regex)\w*\b',symbols)
            for operation in ('pair','get','take'): prefix(binary,'selected-'+opt,operation)
        report['PrunedSourceNativeQuery']=True; report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('Fresh native tree queries and checked ownership verified',flush=True)

if __name__=='__main__': main()
