"""Versioned native owning capture foundation: sparse/whole payloads, typed
detach, first physical failure, real independent key owners, metadata refusal,
no allocation on healthy cleanup, O0/O2 and independent pruned native execution.
Optional ASan/UBSan covers all selected native providers. Not source suspension
or native generator-result acceptance; no shipping/full53 claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true'); a=parser.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm'); assert cc and nm and not out.exists() and (not a.sanitize or os.name!='nt')
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig')); assert build['SourceXrt'] and not build['RuntimeOnly']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    host=root/'test/cases/stdlib_native_capture_host.c'; planner=root/'test/cases/xlir_native_capture_plan.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(cc),Path(nm),public/'build.json',runtime,host,planner,memory}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    for directory in ('src/runtime','src/api','src/type','lib/xrt','lib/xruntime','release/include'):
        files.update(p for p in (root/directory).rglob('*') if p.is_file() and p.suffix in ('.h','.c','.inc'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; products=[]; controls=[]; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,NegativeControls=controls,
        NativeCaptureV2=False,SourceSuspensionAcceptance=False,Full53Acceptance=False,ShippingArtifact=False,
        Sanitized=a.sanitize,SelectedNativeClosureInstrumented=a.sanitize,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt',
        '-I'+str(public/'include'),'-DXL_STATIC_EMBED','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    ext='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def run(name,cmd,refusal=False):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,ExpectedRejection=refusal,Log=str(log),LogSha256=sha(log)))
        assert (p.returncode!=0 if refusal else p.returncode==0),(name,(p.stdout+p.stderr)[-5500:])
        if refusal:
            # This mutant illegally observes a deliberately poisoned inactive
            # native key. Its deterministic access violation is the oracle,
            # not a generic nonzero loader/build failure.
            poisoned=name=='inactive-native-drop-refused'
            expected_fault=p.returncode in (-11,3221225477,-1073741819)
            assert b'Assertion' in p.stdout+p.stderr or b'assertion' in p.stdout+p.stderr or (poisoned and expected_fault)
        return (p.stdout+p.stderr).decode(errors='replace')
    try:
        plan=out/('planner'+ext)
        run('planner-build',[cc,*flags,'-O0',planner,*objects,runtime,*links,'-o',plan]); products.append(dict(Path=str(plan),Sha256=sha(plan)))
        native=out/'native.c'; sources=run('native-plan',[plan,native]).splitlines()
        assert sources and 'XRT_MODULE_ALL' not in native.read_text() and 'XRUNTIME_MODULE_ALL' not in native.read_text()
        assert all('slot_map_key_value' not in s and 'native_leaf_value' not in s and s!='src/runtime/xl6_slot_map.c' for s in sources)
        report['Sources']=sources; report['NativeModules']=re.findall(r'^#define ((?:XRT|XRUNTIME)_MODULE_\w+)$',native.read_text(),re.M)
        products.append(dict(Path=str(native),Sha256=sha(native)))
        selected={}; counts=[]
        for opt in ('O0','O2'):
            current=[]
            for index,source in enumerate([native,*[root/s for s in sources],memory]):
                obj=out/(opt+'-'+str(index)+'.o'); run(opt+'-provider-'+str(index),[cc,*flags,'-'+opt,'-c',source,'-o',obj])
                current.append(obj); products.append(dict(Path=str(obj),Sha256=sha(obj)))
            selected[opt]=current; binary=out/(opt+ext)
            run(opt+'-link',[cc,*flags,'-'+opt,host,*current,*systems,'-o',binary])
            text=run(opt+'-vectors',[binary]); match=re.search(r'vectors=(\d+) refusals=(\d+) drops=(\d+) outside-freeze=(\d+); balanced',text); assert match
            counts.append(list(map(int,match.groups())))
            symbols=run(opt+'-symbols',[nm,'--defined-only',binary])
            for name in ('xl6SlotMapKeyBoxV1','xl6SlotMapKeyUnboxV1','xl6SlotMapKeyConverterV1','xl6NativeLeafValueBoxV1','xl6SlotMapCreateTakeV1'):
                assert not re.search(r'\b'+name+r'\b',symbols),name
            products.append(dict(Path=str(binary),Sha256=sha(binary)))
        assert counts[0]==counts[1]; report['Vectors'],report['Refusals'],report['Drops'],report['OutsideFreeze']=counts[0]
        mutations=[
            ('no-native-drop','src/runtime/xl6_capture_v2.c','s->native()->DetachCleanup((char*)data+s->value.offset,live); return;','return;'),
            ('inactive-native-drop','src/runtime/xl6_capture_v2.c','if(live && !*live) return;','if(live && !*live && (unsigned)s->value.kind!=XL6_CAPTURE_NATIVE_LEAF_V2) return;'),
            ('wrong-order','src/runtime/xl6_capture_v2.c','const XL6_ActivationCaptureSlotV2* slot=&l->slots[l->cleanup[i]];','const XL6_ActivationCaptureSlotV2* slot=&l->slots[l->cleanup[l->cleanupCount-1-i]];'),
            ('forget-key-empty','src/runtime/xl6_slot_map_key_capture.c','owned=*slot; *slot=(XL6_SlotMapKeyV1){0}; if(live) *live=0;','owned=*slot; if(live) *live=0;')]
        for label,source,old,new in mutations:
            body=(root/source).read_text(encoding='utf-8-sig'); assert body.count(old)==1,label
            bad=out/(label+'.c'); bad.write_text(body.replace(old,new),encoding='utf-8')
            obj=out/(label+'.o'); binary=out/(label+ext)
            run(label+'-compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
            index=1+sources.index(source); current=selected['O0'].copy(); current[index]=obj
            run(label+'-link',[cc,*flags,'-O0',host,*current,*systems,'-o',binary]); run(label+'-refused',[binary],True)
            products.extend(dict(Path=str(p),Sha256=sha(p)) for p in (bad,obj,binary)); controls.append(label)
        if a.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']; sl=['-fsanitize=address,undefined','-no-pie']
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            current=[]
            for index,source in enumerate([native,*[root/s for s in sources],memory]):
                obj=out/('san-'+str(index)+'.o'); run('san-provider-'+str(index),[cc,*sf,'-O1','-c',source,'-o',obj]); current.append(obj)
                products.append(dict(Path=str(obj),Sha256=sha(obj)))
            binary=out/'san'; run('san-link',[cc,*sf,'-O1',host,*current,*systems,*sl,'-o',binary]); run('san-vectors',[binary])
            products.append(dict(Path=str(binary),Sha256=sha(binary)))
        report['NativeCaptureV2']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs); report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows); (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete']; print('Native capture V2: independent O0/O2, legacy semantics, real owned keys, checked cleanup and four bad implementations verified',flush=True)
if __name__=='__main__': main()
