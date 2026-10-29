"""Time/environment and real signal resident APIs on actual DLL/SO products.

Four unchanged C suites plus one public-entry-only signal consumer at O0/O2.
No signal implementation inclusion, private hook, recorder injection, compiler
or runtime implementation objects. Old suites retain first-no-hit logical
prefixes; the signal consumer additionally confirms the next no-hit point.
Process-local POSIX TZ fixtures do not prove Windows system-zone DST behavior.
Not generated-language OOM, signal overflow/race white-box coverage, runtime
sanitizers, worker-wide OOM, OS join, image unload or full53 completion.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, threading, time

SUITES=('xlir_time_core_host','xlir_time_calendar_host','xlir_time_text_host','xlir_environment_host','stdlib_signal_product_host')
LEDGER=re.compile(rb'stdlib-suite-ledger resets=(\d+) allocations=(\d+) frees=(\d+) live=0 bytes=0 invalid=0 double=0 uaf=0 caller-error=0')
PREFIX=re.compile(rb'signal prefix (core|queue)=(\d+) failures=(\d+) first=(\d+) confirmed=(\d+) balanced')
RESOURCES=re.compile(rb'signal product resources: before=(\d+) after=(\d+) balanced')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def providers(data):
    result=[]
    for line in data.splitlines():
        fields=line.split()
        if len(fields)<3:continue
        name=fields[-1]
        # PE/COFF GNU weak definitions use a defined .weak.NAME.FALLBACK alias.
        if name.startswith(b'.weak.'):name=name[6:].split(b'.',1)[0]
        if name.startswith((b'xrt',b'xl6')):result.append(name)
    return result
def summaries(name,data,windows):
    descriptions={
        'xlir_time_core_host':((rb'time adapter errors: (\d+) actual allocation failures, (\d+) first no-hit completions, balanced',8),),
        'xlir_time_calendar_host':((rb'time calendar: (\d+) actual allocation failures, (\d+) first no-hit completions, balanced',14 if windows else 20),),
        'xlir_time_text_host':((rb'time text errors: (\d+) actual allocation failures, (\d+) first no-hit completions, balanced',11),),
        'xlir_environment_host':((rb'environment errors: (\d+) actual allocation failures, (\d+) first no-hit completions, balanced',7),),
        'stdlib_signal_product_host':(
            (rb'signal product core errors: (\d+) actual failures, (\d+) first no-hit completions, balanced',7),
            (rb'signal product queue errors: (\d+) actual failures, (\d+) first no-hit completions, balanced',6))}
    result=[]
    for pattern,count in descriptions[name]:
        found=re.findall(pattern,data);assert len(found)==1 and int(found[0][0])>0 and int(found[0][1])==count
        result.append(dict(Families=count,FailurePoints=int(found[0][0]),FirstNoHit=True,
                           NextNoHitConfirmed=name=='stdlib_signal_product_host'))
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('root','build-report','out'):parser.add_argument('--'+n,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists() and 1<=args.jobs<=4
    task=Path(__file__).resolve().parents[1];win=os.name=='nt'
    cc,nm,inspect=[shutil.which(n) for n in ('gcc','nm','objdump' if win else 'readelf')];assert cc and nm and inspect
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p)==sha(p) for p in products)
    hosts={name:task/'test/cases'/(name+'.c') for name in SUITES};observer=task/'test/support/stdlib_native_suite_observer.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(nm),Path(inspect),args.build_report.resolve(),observer,*hosts.values()}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path'];files.add(Path(row['Path']))
    for name in SUITES[:-1]:assert hosts[name].read_bytes().replace(b'\r\n',b'\n')==(root/'test/cases'/hosts[name].name).read_bytes().replace(b'\r\n',b'\n')
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir();rows,proofs,controls,failures=[],[],[],[];lock=threading.Lock()
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Controls=controls,Failures=failures,
        Scope=__doc__,SuiteCount=5,ExplicitSubset=False,CompilerSourceObjects=False,RuntimeImplementationObjects=False,
        ProviderOverrides=False,SignalPrivateHooks=False,RecorderInjection=False,ConsumerResetObservation=True,
        WindowsDstAcceptance=False,PosixDstZones=[] if win else ['America/New_York','Australia/Lord_Howe'],
        GeneratedLanguageOomAcceptance=False,RuntimeInstrumented=False,WorkerWideOomAcceptance=False,
        OsThreadJoinAcceptance=False,ImageUnloadAcceptance=False,Full53Acceptance=False,NativeSystemSuiteProductAcceptance=False)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    if not win:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if not win:flags+=['-D_GNU_SOURCE']
    def run(name,command,directory,expected=0):
        command=list(map(str,command));start=time.monotonic()
        try:
            r=subprocess.run(command,cwd=directory,env=env,capture_output=True,timeout=180);code,data=r.returncode,r.stdout+r.stderr
        except subprocess.TimeoutExpired as e:code,data='timeout',(e.stdout or b'')+(e.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        with lock:rows.append(dict(Name=name,Command=command,Exit=code,ExpectedExit=expected,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert code==expected,(name,code,data.decode(errors='replace')[-5000:])
        return data
    def definitions(name,obj):
        data=run(name,[nm,'--defined-only','--extern-only',obj],out);assert not providers(data),(name,providers(data))
    def observed(name,binary,directory):
        data=run(name,[binary],directory);found=LEDGER.findall(data);assert len(found)==1
        reset,allocated,freed=map(int,found[0]);assert allocated==freed>0
        return data,dict(Resets=reset,Allocations=allocated,Frees=freed)
    try:
        objects={};observers={};directories={}
        for opt in ('O0','O2'):
            obj=out/('observer.'+opt+'.o')
            run(obj.stem+'.compile',[cc,*flags,'-'+opt,'-DSTDLIB_SUITE_VOID_ENTRY','-c',observer,'-o',obj],out)
            definitions(obj.stem+'.definitions',obj);observers[opt]=obj
            for name in SUITES:
                obj=out/(name+'.'+opt+'.o')
                run(obj.stem+'.compile',[cc,*flags,'-'+opt,'-Dmain=stdlibNativeSuiteEntry','-c',hosts[name],'-o',obj],out)
                definitions(obj.stem+'.definitions',obj);objects[name,opt]=obj
        for p in products:
            directory=out/p.stem;(directory/'release').mkdir(parents=True)
            shutil.copy2(p,directory/'release'/p.name)
            if win:shutil.copy2(p,directory/p.name)
            directories[p.name]=directory
        def libs(p,directory):
            return [directory/'release'/p.name] if win else ['-L'+str(directory/'release'),'-l'+p.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        def exercise(spec):
            p,opt,name=spec;tag=p.stem+'.'+opt+'.'+name;directory=directories[p.name];binary=directory/(opt+'.'+name+('.exe' if win else ''))
            try:
                run(tag+'.link',[cc,objects[name,opt],observers[opt],*libs(p,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary],directory)
                data=run(tag+'.imports',[inspect,'-p' if win else '-d',binary],directory)
                imported=re.findall(rb'DLL Name:\s*([^\s]+)',data) if win else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
                assert {x.decode().lower() for x in imported}&{q.name.lower() for q in products}=={p.name.lower()}
                data,ledger=observed(tag+'.run',binary,directory);prefixes=summaries(name,data,win);signal=[];resources=None
                if name=='stdlib_signal_product_host':
                    found=PREFIX.findall(data);assert len(found)==13
                    assert {(family.decode(),int(index)) for family,index,_,_,_ in found}=={('core',i) for i in range(7)}|{('queue',i) for i in range(6)}
                    for family,index,fail,first,confirmed in found:
                        fail,first,confirmed=map(int,(fail,first,confirmed));assert fail==first and confirmed==first+1
                        signal.append(dict(Family=family.decode(),Case=int(index),FailurePoints=fail,FirstNoHit=first,Confirmation=confirmed))
                    assert sum(p['FailurePoints'] for p in signal)==sum(p['FailurePoints'] for p in prefixes)
                    found=RESOURCES.findall(data);assert len(found)==1 and found[0][0]==found[0][1]
                    resources=dict(Before=int(found[0][0]),After=int(found[0][1]),Kind='process-handles' if win else 'process-fds',WarmupBaseline=True)
                    assert b'nine owner guards, 100 foreign snapshot reads and detached lifetime passed' in data and b'32 real once deliveries and exhaustion passed' in data
                if name=='xlir_environment_host':assert b'6000 operations, whole snapshots, balanced' in data
                return dict(Name=tag,Suite=name,Product=p.name,Optimization=opt,Prefixes=prefixes,SignalPrefixes=signal,Resources=resources,
                            FullHeapLedger=ledger,Image=dict(Path=str(binary),Sha256=sha(binary)))
            except Exception as e:
                with lock:failures.append(dict(Name=tag,Error=repr(e)))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for proof in pool.map(exercise,[(p,opt,name) for p in products for opt in ('O0','O2') for name in SUITES]):
                if proof:proofs.append(proof);print(proof['Name']+': passed',flush=True)
        for p in products:
            directory=directories[p.name]
            for macro,marker in (('LEAK_AT_END',b'!after.LiveCount'),('DISABLE_DEBUG',b'xrtMemDebugEnabled')):
                tag=p.stem+'.control.'+macro;obj=out/(tag+'.o');binary=directory/(tag+('.exe' if win else ''))
                run(tag+'.compile',[cc,*flags,'-O0','-DSTDLIB_SUITE_VOID_ENTRY','-DSTDLIB_SUITE_'+macro,'-c',observer,'-o',obj],out)
                run(tag+'.link',[cc,objects['xlir_time_core_host','O0'],obj,*libs(p,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary],directory)
                data=run(tag+'.reject',[binary],directory,3 if win else -6);assert b'assert' in data.lower() and marker in data
                controls.append(dict(Name=tag,Compiled=True,AssertionRejected=True))
            tag=p.stem+'.control.NO_LOGICAL_INJECTION';obj=out/(tag+'.o');binary=directory/(tag+('.exe' if win else ''))
            run(tag+'.compile',[cc,*flags,'-O0','-DSTDLIB_SUITE_VOID_ENTRY','-DSTDLIB_SUITE_SKIP_LOGICAL_INJECTION','-c',observer,'-o',obj],out)
            run(tag+'.link',[cc,objects['xlir_time_core_host','O0'],obj,*libs(p,directory),'-Wl,--wrap=xrtMemDebugReset','-Wl,--wrap=xrtMemDebugFailAfter','-o',binary],directory)
            data,_=observed(tag+'.unarmed',binary,directory);assert b'time adapter errors: 0 actual allocation failures, 8 first no-hit completions' in data
            try:summaries('xlir_time_core_host',data,win)
            except AssertionError:controls.append(dict(Name=tag,Compiled=True,ExecutionSucceeded=True,MissingInjectionRejected=True))
            else:raise AssertionError('Missing logical injection accepted')
        fake=out/'fake-provider.c';fake.write_text('void xl6SystemProviderStrong(void) {}\n__attribute__((weak)) void xl6SystemProviderWeak(void) {}\nconst unsigned xrtSystemProviderData=1;\n',encoding='ascii')
        obj=out/'fake-provider.o';run('fake-provider.compile',[cc,*flags,'-c',fake,'-o',obj],out)
        data=run('fake-provider.definitions',[nm,'--defined-only','--extern-only',obj],out)
        expected={b'xl6SystemProviderStrong',b'xl6SystemProviderWeak',b'xrtSystemProviderData'}
        assert set(providers(data))==expected and set(providers(data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')))==expected
        controls.append(dict(Name='fake-provider',Compiled=True,StrongWeakAndDataRejected=True,CrLfChecked=True))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['NativeSystemSuiteProductAcceptance']=len(proofs)==20 and len(controls)==7 and not failures
        report['Complete']=report['InputsUnchanged'] and report['NativeSystemSuiteProductAcceptance']
    except Exception as e:failures.append(dict(Name='gate',Error=repr(e)))
    report['PhaseCount']=len(rows);report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Controls=len(controls),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
