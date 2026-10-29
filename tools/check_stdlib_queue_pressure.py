"""Fresh-source public queue overlap and actor-local checked failures.

Selected family-only native closures, exact NUL/UTF-8 ownership, strict O0/O2,
JIT/module/AOT, real negative providers and optional full-closure ASan/UBSan.
Not WaitQueue, all-scheduler-thread injection, shipping DLL or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from check_stdlib_slot_map import stage_private_sdk

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
    root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists()
    assert not args.sanitize or os.name!='nt'
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build.get('Complete',os.name=='nt')
    driver=Path(build['Driver']);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    artifact=root/'test/cases/xlir_artifact_runtime_contract.c';memory=root/'test/support/module_product_memory.c'
    host=root/'test/cases/stdlib_queue_pressure_host.c';actor=root/'test/cases/stdlib_queue_actor_fault_host.c'
    families=(('spsc','SpscQueue','SPSC'),('mpsc','MpscQueue','MPSC'),('mpmc','MpmcQueue','MPMC'))
    manifest,sdk,_=stage_private_sdk(root)
    files={Path(__file__).resolve(),root/'tools/check_stdlib_slot_map.py',public/'build.json',Path(cc),Path(nm),driver,runtime,manifest,artifact,memory,host,actor}
    files.update(root/'test/cases'/('stdlib_'+tag+'_pressure.xl') for tag,_,_ in families)
    files.update(Path(row[key]) for row in sdk for key in ('Source','Path'))
    reused=[]
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path'];files.add(Path(row['Path']))
        source=Path(row.get('Source',row['Path']))
        if 'frozen' in source.parts:
            index=source.parts.index('frozen');assert source.parts[index+1]=='demo6'
            relative=Path(*source.parts[index+2:]);current=root/relative
            assert source.read_bytes().replace(b'\r\n',b'\n')==current.read_bytes().replace(b'\r\n',b'\n'),str(source)
            files.add(current);reused.append(str(relative))
    assert sha(driver)==build['DriverSha256'].lower()
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir();rows=[];products=[];plans=[];prefixes=[];controls=[];errors=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Products=products,Plans=plans,
        Prefixes=prefixes,NegativeControls=controls,Errors=errors,ReusedIdenticalSourceInputs=reused,
        FaultScope='one actual actor thread; simultaneous separate-queue peer proves TLS/error isolation, not global scheduler coverage',
        PublicSharedQueueOverlap=True,ModuleLifetimesPerFamilyPerOptimization=8,SelectedNativeClosureSanitized=args.sanitize,
        Full53Acceptance=False,ShippingArtifact=False,WaitQueueComplete=False)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    extension='.exe' if os.name=='nt' else ''
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    def run(name,command,bad=False):
        started=time.monotonic()
        try:
            result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
            data,code=result.stdout+result.stderr,result.returncode
        except subprocess.TimeoutExpired as error:
            data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Exit=code,ExpectedRejection=bad,Seconds=time.monotonic()-started,Log=str(log),LogSha256=sha(log)))
        assert code!='timeout' and (code!=0 if bad else code==0),(name,code,data[-4000:])
        if bad:assert b'assert' in data.lower(),(name,code,data[-4000:])
        return data.decode(errors='replace')
    def prefix(binary,name,operation,lane,optimization):
        for at in range(128):
            text=run(name+'-'+str(at),[binary,operation,lane,at])
            if 'hit=0' in text:break
        else:raise AssertionError('unfinished actor prefix '+name)
        assert at>0,name+' did not exercise a failure'
        prefixes.append(dict(Family=name.split('-')[0],Optimization=optimization,Operation=operation,Lane=lane,Runs=at+1,Complete=True))
    try:
        planner=out/('artifact-plan'+extension)
        run('planner-build',[cc,*flags,'-O0',artifact,*objects,runtime,*links,'-o',planner]);product(planner)
        for tag,family,backend in families:
            try:
                fixture=root/'test/cases'/('stdlib_'+tag+'_pressure.xl')
                generated=out/(tag+'-program.c');repeat=out/(tag+'-repeat.c')
                run(tag+'-generate',[driver,fixture,'cgen-default',generated]);run(tag+'-repeat',[driver,fixture,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
                for path in (generated,repeat):product(path)
                implementation=out/(tag+'-native.c');text=run(tag+'-plan',[planner,'--emit-native',fixture.as_posix(),implementation])
                plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
                suppliers=plan[0]['Sources'];plans.append(dict(Family=tag,Sources=suppliers))
                for other,_,_ in families:
                    assert ('src/runtime/xl6_queue_'+other+'.c' in suppliers)==(other==tag)
                assert 'src/runtime/xl6_string_type.c' in suppliers
                assert not any('channel' in source or 'wait_queue' in source for source in suppliers)
                raw=implementation.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
                implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
                for optimization in ('O0','O2'):
                    try:
                        providers=[]
                        for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                            obj=out/(tag+'-'+optimization+'-'+str(index)+'.o')
                            run(obj.stem,[cc,*flags,'-'+optimization,'-c',source,'-o',obj]);providers.append(obj);product(obj)
                        binary=out/(tag+'-'+optimization+extension)
                        run(binary.stem+'-link',[cc,*flags,'-'+optimization,'-include',generated,host,*providers,*systems,'-o',binary]);product(binary)
                        run(binary.stem+'-overlap',[binary])
                        symbols=run(binary.stem+'-symbols',[nm,'--defined-only',binary])
                        assert re.search(r'\bxrtTyped'+backend+r'QueueTryPush\b',symbols)
                        assert not re.search(r'\bxlcCreateContext\b|\bxrtTQueue|\bxrtChannel',symbols)
                        for _,_,other in families:
                            if other!=backend:assert not re.search(r'\bxrtTyped'+other+'Queue',symbols)
                        fault=out/(tag+'-'+optimization+'-actor'+extension)
                        options=['-DXL6_TEST_QUEUE=xl6'+family,'-Wl,--wrap=xrtTypeCopyValue']
                        run(fault.stem+'-link',[cc,*flags,'-'+optimization,*options,actor,*providers,*systems,'-o',fault]);product(fault)
                        for operation in range(3):
                            for lane in (('oom','copy','partial','diagnostic') if operation<2 else ('oom',)):
                                prefix(fault,tag+'-'+optimization+'-'+str(operation)+'-'+lane,operation,lane,optimization)
                        if optimization=='O0':
                            bad=out/(tag+'-empty-string-copy.c');body=(root/'src/runtime/xl6_string_type.c').read_text(encoding='utf-8-sig')
                            needle='bool copied=present(source) && xrtTypeCopyValue(xrtTypeStringView(),target,source);';assert body.count(needle)==1
                            bad.write_text(body.replace(needle,'bool copied=true;'),encoding='utf-8');obj=out/(tag+'-bad.o');control=out/(tag+'-bad'+extension)
                            run(tag+'-bad-build',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
                            replaced=providers.copy();replaced[1+suppliers.index('src/runtime/xl6_string_type.c')]=obj
                            run(tag+'-bad-link',[cc,*flags,'-O0',*options,actor,*replaced,*systems,'-o',control]);run(tag+'-bad-refused',[control,0,'oom',127],True)
                            for path in (bad,obj,control):product(path)
                            controls.append(dict(Family=tag,Mutation='copy reports success without copying exact text'))
                            bad=out/(tag+'-unchecked-string-copy.c')
                            first=body.index('static bool copy(');last=body.index('static bool move(',first)
                            old='static bool copy(ptr target, const void* source, const xrttype* type)\n{ (void)type; return present(source) && xrtTypeCopyValue(xrtTypeStringView(),target,source); }\n'
                            bad.write_text(body[:first]+old+body[last:],encoding='utf-8')
                            obj=out/(tag+'-unchecked.o');control=out/(tag+'-unchecked'+extension)
                            run(tag+'-unchecked-build',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
                            replaced=providers.copy();replaced[1+suppliers.index('src/runtime/xl6_string_type.c')]=obj
                            run(tag+'-unchecked-link',[cc,*flags,'-O0',*options,actor,*replaced,*systems,'-o',control]);run(tag+'-unchecked-refused',[control,0,'diagnostic',0],True)
                            for path in (bad,obj,control):product(path)
                            controls.append(dict(Family=tag,Mutation='original unchecked string copier publishes on success plus diagnostic'))
                    except (AssertionError,RuntimeError) as error:errors.append(dict(Family=tag,Optimization=optimization,Error=str(error)))
                for mode in ('run','module'):
                    run(tag+'-'+mode,[driver,fixture,mode,out/(tag+'-'+mode)])
                exe=out/(tag+'-aot.exe');run(tag+'-aot-build',[driver,fixture,'exe',out/(tag+'-aot') if os.name=='nt' else exe]);run(tag+'-aot-run',[exe]);product(exe)
                if args.sanitize:
                    sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie']
                    env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                    providers=[]
                    for index,source in enumerate([implementation,*[root/name for name in suppliers],memory]):
                        obj=out/(tag+'-san-'+str(index)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);providers.append(obj);product(obj)
                    binary=out/(tag+'-san');run(tag+'-san-link',[cc,*sf,'-O1','-include',generated,host,*providers,*systems,*sl,'-o',binary]);run(tag+'-san-overlap',[binary]);product(binary)
                    fault=out/(tag+'-san-actor');run(tag+'-san-actor-link',[cc,*sf,'-O1','-DXL6_TEST_QUEUE=xl6'+family,'-Wl,--wrap=xrtTypeCopyValue',actor,*providers,*systems,*sl,'-o',fault]);product(fault)
                    for operation in range(3):
                        for lane in (('oom','copy','partial','diagnostic') if operation<2 else ('oom',)):
                            for at in (0,127):run(tag+'-san-'+str(operation)+'-'+lane+'-'+str(at),[fault,operation,lane,at])
            except (AssertionError,RuntimeError) as error:errors.append(dict(Family=tag,Error=str(error)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=not errors and report['InputsUnchanged'] and len(prefixes)==54 and len(controls)==6
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'],errors
    print('Public concurrent queues, actor-local checked prefixes and selected closures: verified',flush=True)
if __name__=='__main__':main()
