"""Exact owned Future iteration for SlotMap/Array/List/Dict/Set, nullable
projection, await/yield, source clearing, escaping references and all exits.
Current-source IR/Cgen, independent pruned C O0/O2, caller-thread failure
prefixes, real compiler mutation controls, JIT/module/AOT and optional selected
closure ASan/UBSan. Not WaitQueue or full53/shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists() and (not a.sanitize or os.name!='nt')
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'));assert build['SourceXrt'] and not build['RuntimeOnly']
    driver=Path(build['Driver']);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    sources=[root/('test/cases/'+n+'.xl') for n in ('stdlib_slot_map_loop_futures','stdlib_collection_loop_futures','stdlib_future_iteration_sync')]
    host=root/'test/cases/stdlib_future_iteration_host.c';contract=root/'test/cases/xlir_future_iteration_contract.c'
    artifact=root/'test/cases/xlir_artifact_runtime_contract.c';memory=root/'test/support/module_product_memory.c'
    manifest,sdk,skipped=stage_private_sdk(root)
    files={Path(__file__).resolve(),root/'tools/check_stdlib_slot_map.py',public/'build.json',Path(cc),Path(nm),runtime,driver,host,contract,artifact,memory,manifest,*sources}
    files.update(Path(r[k]) for r in sdk for k in ('Source','Path'))
    for r in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(r['Path'])==r['Sha256'].lower(),r['Path'];files.add(Path(r['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];products=[];prefixes=[];plans=[];controls=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,Plans=plans,NegativeControls=controls,
        LanguageFutureIteration=False,Sanitized=a.sanitize,SelectedNativeClosureInstrumented=a.sanitize,
        FaultPrefixScope='caller-thread only; sync main waits before iteration, no executor-thread injection',
        Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),'-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def product(f):products.append(dict(Path=str(f),Sha256=sha(f)))
    def run(name,cmd,bad=False):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Exit=r.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if bad else r.returncode==0),(name,(r.stdout+r.stderr)[-6000:])
        if bad:assert b'assert' in (r.stdout+r.stderr).lower(),(name,r.returncode)
        return (r.stdout+r.stderr).decode(errors='replace')
    try:
        proof=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-execute',[proof,*sources])
        # Compile two actual incorrect compiler implementations, not a regex
        # oracle. The retained-reference and exact payload contracts reject them.
        for tag,provider in (('trivial-future-binding','xl6_ir.c'),('wrong-projection-payload','xl6_ir_lower.c')):
            body=(root/('src/ir/'+provider)).read_text(encoding='utf-8-sig')
            if tag=='trivial-future-binding':
                start=body.index('static XL6_IrLoopTargetLifetimeAbi xl6IrLoopItemLifetimeAbi(');end=body.index('\n}',start)+2
                region=body[start:end];cases='    case XL6_IR_LOOP_ITEM_FUTURE_COPY:\n    case XL6_IR_LOOP_ITEM_NULLABLE_FUTURE_COPY:\n'
                assert region.count(cases)==1;region=region.replace(cases,'')
                marker='        return XL6_IR_LOOP_TARGET_LIFETIME_TRIVIAL_REPLACE;';assert region.count(marker)==1
                region=region.replace(marker,cases+marker);body=body[:start]+region+body[end:]
            else:
                start=body.index('static XL6_IrValueId xl6IrLowerProjectBorrowedXValueAsFuture(');end=body.index('\n}',start)+2
                region=body[start:end];old='].payloadType = payload;';assert region.count(old)==1
                body=body[:start]+region.replace(old,'].payloadType = futureTypeId;')+body[end:]
            bad=out/(tag+'.c');bad.write_text(body,encoding='utf-8');obj=out/(tag+'.o');binary=out/(tag+ext)
            run(tag+'-compile',[cc,*flags,'-Isrc/ir','-O0','-c',bad,'-o',obj]);replaced=[obj if Path(f).name==provider.removesuffix('.c')+'.o' else f for f in objects]
            run(tag+'-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);run(tag+'-refused',[binary,*sources],True)
            for f in (bad,obj,binary):product(f)
            controls.append(tag)
        planner=out/('artifact-plan'+ext);run('planner-build',[cc,*flags,'-O0',artifact,*objects,runtime,*links,'-o',planner]);product(planner)
        fixtures=[]
        for i,source in enumerate(sources):fixtures.append((str(i),source,i==2))
        for tag,loop in {'keys':'for key in slots\n assert(key == key)\nend\n','count':'for _, _ in slots\n count += 1\nend\nassert(count == 1)\n'}.items():
            source=out/(tag+'.xl');source.write_text('func value() as int\n return 42\nend\nfunc main(...) as int\n auto ready=task.spawn(value)\n assert(ready.wait())\n auto slots=SlotMap::<future<int>>()\n slots.insert(ready)\n int count=0\n'+loop+' return 0\nend\n',encoding='utf-8')
            product(source);fixtures.append((tag,source,True))
        for tag,source,has_main in fixtures:
            generated=out/(tag+'-program.c');repeat=out/(tag+'-repeat.c');typed=out/(tag+'-typed.json')
            run(tag+'-generate',[driver,source,'cgen-default',generated]);run(tag+'-repeat',[driver,source,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            run(tag+'-typed',[driver,source,'ir-typed',typed]);state=json.loads(typed.read_text(encoding='utf-8-sig'))
            future_loops=[n for n in state['nodes'] if n['loopItemAbiName'] in ('future-copy','nullable-future-copy')]
            assert future_loops or tag in ('keys','count')
            for n in future_loops:assert n['loopItemLifetimeAbiName']=='owned-iteration-release' and n['loopErrorAbiName']=='may-raise'
            for f in (generated,repeat,typed):product(f)
            implementation=out/(tag+'-native.c');text=run(tag+'-plan',[planner,'--emit-native',source.as_posix(),implementation])
            plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
            suppliers=plan[0]['Sources'];plans.append(dict(Fixture=tag,Sources=suppliers,FutureLoops=len(future_loops)))
            raw=implementation.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
            implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
            options=['-DXL6_FUTURE_ITERATION_HAS_MAIN'] if has_main else []
            for opt in ('O0','O2'):
                providers=[]
                for i,s in enumerate([implementation,*[root/f for f in suppliers],memory]):
                    obj=out/(tag+'-'+opt+'-'+str(i)+'.o');run(tag+'-'+opt+'-provider-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',obj]);providers.append(obj);product(obj)
                binary=out/(tag+'-'+opt+ext);run(tag+'-'+opt+'-link',[cc,*flags,'-'+opt,*options,'-include',generated,host,*providers,*systems,'-o',binary]);run(tag+'-'+opt+'-lifetimes',[binary]);product(binary)
                symbols=run(tag+'-'+opt+'-symbols',[nm,'--defined-only',binary]);assert not re.search(r'\bxlcCreateContext\b',symbols)
                if tag in ('keys','count'):
                    # ValuesV1 is a tiny wrapper in the required SlotMap core,
                    # not a separately selectable supplier. Prove the exact
                    # optional cursor providers, not fictional symbol pruning.
                    forbidden=['xl6SlotMapItemIteratorCreateV1','xl6SlotMapPairIteratorCreateV1']
                    if tag=='count':forbidden+=['xl6SlotMapKeyIteratorCreateV1','xl6SlotMapKeysV1']
                    for name in forbidden:assert not re.search(r'\b'+name+r'\b',symbols),(tag,name)
                if tag=='2':
                    fault=out/(opt+'-fault'+ext);wrappers=('xl6SlotMapPairIteratorCreateV1','xl6SlotMapKeyIteratorCreateV1','xl6SlotMapItemIteratorCreateV1','xl6SlotMapCountIteratorCreateV1','xl6ValueIteratorRuntimeV2Get','xl6ValueIteratorKeyCopyV3','xrtTypeCopyValue')
                    run(opt+'-fault-link',[cc,*flags,'-'+opt,*options,'-DXL6_FUTURE_ITERATION_FAULTS','-include',generated,host,*providers,*systems,*['-Wl,--wrap='+f for f in wrappers],'-o',fault]);product(fault)
                    for lane in ('pairs','keys','values','count','advance','key-copy','typed-copy','oom'):
                        for at in range(2048):
                            text=run(opt+'-'+lane+'-'+str(at),[fault,lane,at])
                            if 'hit=0' in text:break
                        else:raise AssertionError('unfinished fault prefix '+lane)
                        prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
            run(tag+'-jit',[driver,source,'run',out/(tag+'-jit')]);run(tag+'-module',[driver,source,'module',out/(tag+'-module')])
            exe=out/(tag+'-aot.exe');run(tag+'-aot-build',[driver,source,'exe',out/(tag+'-aot') if os.name=='nt' else exe]);run(tag+'-aot-execute',[exe]);product(exe)
            if a.sanitize and tag in ('0','1','2'):
                sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie'];env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                providers=[]
                for i,s in enumerate([implementation,*[root/f for f in suppliers],memory]):
                    obj=out/(tag+'-san-'+str(i)+'.o');run(tag+'-san-provider-'+str(i),[cc,*sf,'-O1','-c',s,'-o',obj]);providers.append(obj);product(obj)
                binary=out/(tag+'-san');run(tag+'-san-link',[cc,*sf,'-O1',*options,'-include',generated,host,*providers,*systems,*sl,'-o',binary]);run(tag+'-san-execute',[binary]);product(binary)
        report.update(LanguageFutureIteration=True,LifetimesPerFixturePerOptimization=200)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Owned Future iteration, caller-thread failures and independent products: verified',flush=True)
if __name__=='__main__':main()
