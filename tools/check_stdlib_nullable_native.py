"""Optional owning native-record foundation: explicit canonical component root,
strict independently selected C O0/O2, checked false/partial/diagnostic copy,
actual OOM, 1000 lifecycle vectors and full selected Linux ASan/UBSan.
Does not claim nullable language registration, IR lowering or tryPop completion.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
    root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve();cc,nm=shutil.which('gcc'),shutil.which('nm')
    assert cc and nm and not out.exists() and (not args.sanitize or os.name!='nt')
    build,pinned,_=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    host=root/'test/cases/stdlib_nullable_native_host.c';planner=root/'test/cases/xlir_nullable_native_plan.c'
    memory=root/'test/support/module_product_memory.c';fixture=root/'test/cases/cgen_basic.xl'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),runtime,host,planner,memory,fixture))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir();rows=[];products=[];prefixes=[];controls=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,
        LanguageNullableComplete=False,Full53Acceptance=False,ShippingArtifact=False,SelectedNativeClosureSanitized=args.sanitize,NegativeControls=controls)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/xrt','-Ilib/xruntime','-Ilib/tcc',
        '-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-missing-field-initializers','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);extension='.exe' if os.name=='nt' else ''
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    def run(name,command,bad=False):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180);data=result.stdout+result.stderr
        log=out/(name+'.log');log.write_bytes(data);rows.append(dict(Name=name,Exit=result.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (result.returncode!=0 if bad else result.returncode==0),(name,data[-4000:])
        if bad:assert b'assert' in data.lower(),(name,data[-4000:])
        return data.decode(errors='replace')
    try:
        binary=out/('plan'+extension);run('planner-build',[cc,*flags,'-O0',planner,*objects,runtime,*links,'-o',binary]);product(binary)
        run('unused-supplier-prune',[binary,out/'unused.c',fixture]);report['UnusedLanguageSupplierPruned']=True
        implementation=out/'native.c';text=run('plan',[binary,implementation]);plan=json.loads(text.strip());suppliers=plan['Sources'];report['Sources']=suppliers
        raw=implementation.read_text();implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
        wrap=['-Wl,--wrap=xl6SlotMapKeyCopyValueV1','-Wl,--wrap=xl6SlotMapKeyUnboxV1']
        all_providers={}
        for optimization in ('O0','O2'):
            providers=[]
            for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                obj=out/(optimization+'-'+str(index)+'.o');run(obj.stem,[cc,*flags,'-'+optimization,'-c',source,'-o',obj]);providers.append(obj);product(obj)
            all_providers[optimization]=providers
            binary=out/(optimization+extension);run(optimization+'-link',[cc,*flags,'-'+optimization,host,*providers,*systems,*wrap,'-o',binary]);product(binary)
            run(optimization+'-vectors',[binary]);symbols=run(optimization+'-symbols',[nm,'--defined-only',binary]);assert 'xlcCreateContext' not in symbols
            assert 'xl6SlotMapCreateTake' not in symbols and 'xl6StackPush' not in symbols
            for operation in range(6):
                for lane in (('oom',) if operation==4 else ('copy','partial','diagnostic','oom')):
                    for at in range(128):
                        text=run(optimization+'-'+str(operation)+'-'+lane+'-'+str(at),[binary,lane,operation,at])
                        if 'hit=0' in text:break
                    else:raise AssertionError('unfinished prefix')
                    if lane!='oom':assert at>0
                    prefixes.append(dict(Optimization=optimization,Operation=operation,Lane=lane,Runs=at+1,Complete=True))
        body=(root/'src/runtime/xl6_nullable_slot_map_key.c').read_text(encoding='utf-8-sig')
        drop='xl6SlotMapKeyDropValueV1(value.value);'
        publish='if(ok && !xrtGetError() && !scope.failed)'
        assert body.count(drop)==1 and body.count(publish)==3
        start=body.index('bool xl6NullableSlotMapKeyProjectV1(');end=body.index('xvalue* xl6NullableSlotMapKeyBoxV1(',start)
        mutations=[('lost-partial-owner',body.replace(drop,'if(value.hasValue)'+drop),'partial',0),
            ('unchecked-record-publication',body.replace(publish,'if(ok)',1),'diagnostic',5),
            ('unchecked-projection-publication',body[:start]+body[start:end].replace(publish,'if(ok)')+body[end:],'diagnostic',2)]
        at=suppliers.index('src/runtime/xl6_nullable_slot_map_key.c')+1
        for name,changed,lane,operation in mutations:
            source=out/(name+'.c');source.write_text(changed,encoding='utf-8');product(source)
            obj=out/(name+'.o');binary=out/(name+extension)
            run(name+'-build',[cc,*flags,'-Isrc/runtime','-O0','-c',source,'-o',obj]);product(obj)
            providers=all_providers['O0'][:];providers[at]=obj
            run(name+'-link',[cc,*flags,'-O0',host,*providers,*systems,*wrap,'-o',binary]);product(binary)
            run(name+'-reject',[binary,lane,operation,0],bad=True);controls.append(dict(Name=name,Rejected=True))
        if args.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie']
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');providers=[]
            for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                obj=out/('san-'+str(index)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);providers.append(obj);product(obj)
            binary=out/'san';run('san-link',[cc,*sf,'-O1',host,*providers,*systems,*wrap,*sl,'-o',binary]);product(binary);run('san-vectors',[binary])
            for operation in range(6):
                for lane in (('oom',) if operation==4 else ('copy','partial','diagnostic','oom')):
                    run('san-'+str(operation)+'-'+lane,[binary,lane,operation,0])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Optional native-record foundation: verified',flush=True)
if __name__=='__main__':main()
