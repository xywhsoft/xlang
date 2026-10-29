"""Native owning generator frames: actual resident control/frame owners, sparse
cleanup, independent native item transfer, native-key source suspension and
checked copy outcomes. Strict O0/O2, complete fault prefixes, real bad runtime
implementations, exact native closure/pruning, public JIT/module/AOT and selected
closure ASan/UBSan. Not native source yield/await or full53/shipping acceptance.
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
    host=root/'test/cases/stdlib_native_generator_host.c';planner=root/'test/cases/xlir_native_generator_plan.c'
    source=root/'test/cases/stdlib_slot_map_generator_capture.xl';languageHost=root/'test/cases/stdlib_slot_map_generator_host.c'
    contract=root/'test/cases/xlir_native_generator_contract.c';artifact=root/'test/cases/xlir_artifact_runtime_contract.c';memory=root/'test/support/module_product_memory.c'
    manifest,sdk,skipped=stage_private_sdk(root)
    files={Path(__file__).resolve(),root/'tools/check_stdlib_slot_map.py',Path(cc),Path(nm),public/'build.json',runtime,driver,host,planner,source,languageHost,contract,artifact,memory,manifest}
    files.update(Path(r[k]) for r in sdk for k in ('Source','Path'))
    for r in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(r['Path'])==r['Sha256'].lower(),r['Path'];files.add(Path(r['Path']))
    for folder in ('src/runtime','src/api','src/type','lib/xrt','lib/xruntime','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.h','.c','.inc'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];products=[];prefixes=[];controls=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,NegativeControls=controls,
        NativeGeneratorV3=False,LanguageNativeKeySuspension=False,NativeSourceYield=False,AsyncNativeKeySuspension=False,
        Full53Acceptance=False,ShippingArtifact=False,Sanitized=a.sanitize,SelectedNativeClosureInstrumented=a.sanitize,Scope=__doc__)
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
        plan=out/('planner'+ext);run('planner-build',[cc,*flags,'-O0',planner,*objects,runtime,*links,'-o',plan]);product(plan)
        native=out/'native.c';suppliers=run('native-plan',[plan,native]).splitlines();assert suppliers and 'XRT_MODULE_ALL' not in native.read_text();product(native)
        assert 'src/runtime/xl6_slot_map_key_value.c' not in suppliers and 'src/runtime/xl6_slot_map.c' not in suppliers
        selected={}
        for opt in ('O0','O2'):
            providers=[]
            for i,s in enumerate([native,*[root/f for f in suppliers],memory]):
                obj=out/(opt+'-'+str(i)+'.o');run(opt+'-provider-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',obj]);providers.append(obj);product(obj)
            selected[opt]=providers;binary=out/(opt+ext);run(opt+'-link',[cc,*flags,'-'+opt,host,*providers,*systems,'-o',binary]);text=run(opt+'-vectors',[binary]);product(binary)
            assert 'vectors=7001' in text
            symbols=run(opt+'-symbols',[nm,'--defined-only',binary])
            for name in ('xl6SlotMapKeyBoxV1','xl6SlotMapKeyUnboxV1','xl6NativeLeafValueBoxV1','xl6SlotMapCreateTakeV1'):assert not re.search(r'\b'+name+r'\b',symbols)
            for at in range(64):
                text=run(opt+'-native-prefix-'+str(at),[binary,at])
                if 'hit=0' in text:break
            else:raise AssertionError('incomplete native allocation prefix')
            prefixes.append(dict(Optimization=opt,Lane='native-allocation',Runs=at+1,Complete=True))
        mutations=[('missing-frame-cleanup','outcome=frame->layoutV2 ? xl6ActivationCaptureRuntimeV2Get()->Cleanup(frame->layoutV2,data,frame->bytes,(xl6_error_result){true,NULL}) :','outcome=frame->layoutV2 ? (xl6_error_result){true,NULL} :'),
            ('missing-item-cleanup','if(v2){\n        xl6_error_result r=','if(v2)return;\n    if(v2){\n        xl6_error_result r='),
            ('wrong-native-item-owner','if(v2)return v2->slots[0].value.kind!=XL6_CAPTURE_TRIVIAL;','if(v2)return false;')]
        for name,old,new in mutations:
            body=(root/'src/runtime/xl6_generator.c').read_text(encoding='utf-8-sig');assert body.count(old)==1,name
            bad=out/(name+'.c');bad.write_text(body.replace(old,new),encoding='utf-8');obj=out/(name+'.o');binary=out/(name+ext)
            run(name+'-compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj]);providers=selected['O0'].copy();providers[1+suppliers.index('src/runtime/xl6_generator.c')]=obj
            run(name+'-link',[cc,*flags,'-O0',host,*providers,*systems,'-o',binary]);run(name+'-refused',[binary],True)
            for f in (bad,obj,binary):product(f)
            controls.append(name)
        body=(root/'src/runtime/xl6_slot_map_key.c').read_text(encoding='utf-8-sig')
        old=' || output->instance || output->slot)';assert body.count(old)==1
        bad=out/'copy-overwrites-owner.c';bad.write_text(body.replace(old,')'),encoding='utf-8')
        obj=out/'copy-overwrites-owner.o';binary=out/('copy-overwrites-owner'+ext)
        run('copy-overwrites-owner-compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj]);providers=selected['O0'].copy();providers[1+suppliers.index('src/runtime/xl6_slot_map_key.c')]=obj
        run('copy-overwrites-owner-link',[cc,*flags,'-O0',host,*providers,*systems,'-o',binary]);run('copy-overwrites-owner-refused',[binary],True)
        for f in (bad,obj,binary):product(f)
        controls.append('copy-overwrites-owner')
        generated=out/'language-program.c';repeat=out/'language-repeat.c'
        run('language-generate',[driver,source,'cgen-default',generated]);run('language-repeat',[driver,source,'cgen-default',repeat]);assert sha(generated)==sha(repeat);product(generated);product(repeat)
        body=generated.read_text(encoding='utf-8-sig');assert 'XL6_CAPTURE_NATIVE_LEAF_V2' in body and 'xl6GeneratorRuntimeV3Get' in body and 'xlir_acquired_ok' in body
        assert 'xl6SlotMapKeyBoxV1(' not in body and 'xl6SlotMapKeyUnboxV1(' not in body
        proof=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);report['IrContracts']=run('contract-execute',[proof,source]);product(proof)
        artifactPlan=out/('artifact-plan'+ext);run('artifact-build',[cc,*flags,'-O0',artifact,*objects,runtime,*links,'-o',artifactPlan]);product(artifactPlan)
        implementation=out/'language-native.c'
        text=run('language-plan',[artifactPlan,'--emit-native',source.relative_to(root).as_posix(),implementation])
        plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1
        languageSuppliers=plans[0]['Sources'];raw=implementation.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
        implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
        assert 'src/runtime/xl6_slot_map_key_capture.c' in languageSuppliers
        for opt in ('O0','O2'):
            providers=[]
            for i,s in enumerate([implementation,*[root/f for f in languageSuppliers],memory]):
                obj=out/('language-'+opt+'-'+str(i)+'.o');run('language-'+opt+'-provider-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',obj]);providers.append(obj);product(obj)
            binary=out/('language-'+opt+ext);run('language-'+opt+'-link',[cc,*flags,'-'+opt,'-include',generated,languageHost,*providers,*systems,'-o',binary]);run('language-'+opt+'-lifetimes',[binary]);product(binary)
            fault=out/('language-'+opt+'-fault'+ext);wrappers=('xl6SlotMapPairIteratorCreateV1','xl6SlotMapKeyIteratorCreateV1','xl6ValueIteratorKeyCopyV3','xl6SlotMapKeyCopyValueV1')
            run('language-'+opt+'-fault-link',[cc,*flags,'-'+opt,'-DXL6_SLOT_MAP_LOOP_FAULTS','-include',generated,languageHost,*providers,*systems,*['-Wl,--wrap='+f for f in wrappers],'-o',fault]);product(fault)
            for lane in ('pairs','keys','key-copy','native-copy','oom'):
                for at in range(2048):
                    text=run(opt+'-'+lane+'-'+str(at),[fault,lane,at])
                    if 'hit=0' in text:break
                else:raise AssertionError('unfinished language prefix '+lane)
                prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
        run('language-jit',[driver,source,'run',out/'jit']);run('language-module',[driver,source,'module',out/'module'])
        exe=out/'aot.exe';run('language-aot-build',[driver,source,'exe',out/'aot' if os.name=='nt' else exe]);run('language-aot-execute',[exe]);product(exe)
        if a.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie'];env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for tag,impl,sources,test in [('native',native,suppliers,host),('language',implementation,languageSuppliers,languageHost)]:
                providers=[]
                for i,s in enumerate([impl,*[root/f for f in sources],memory]):
                    obj=out/(tag+'-san-'+str(i)+'.o');run(tag+'-san-provider-'+str(i),[cc,*sf,'-O1','-c',s,'-o',obj]);providers.append(obj);product(obj)
                binary=out/(tag+'-san');run(tag+'-san-link',[cc,*sf,'-O1',*(['-include',generated] if tag=='language' else []),test,*providers,*systems,*sl,'-o',binary]);run(tag+'-san-vectors',[binary]);product(binary)
        report.update(NativeGeneratorV3=True,LanguageNativeKeySuspension=True,LifetimesPerScenarioPerOptimization=1000,NativeSources=suppliers,LanguageSources=languageSuppliers)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Native generator V3 and source native-key suspension: verified',flush=True)
if __name__=='__main__':main()
