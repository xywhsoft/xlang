"""All eight existing signal language suites: complete caller-thread OOM prefixes.

Unmodified C emitted by the actual compiler, linked to the actual DLL/SO at
O0/O2. Preserve the original language assertions and heap/error invariants; no
Channel error waiver or provider implementation objects. A transparent call-site
observer forwards the real callback diagnostic entry and verifies exact original
exception identity on optional diagnostic OOM. Consumer heap observation forwards
real debug resets. Other allocation failures must propagate MemoryError.
Not unobserved JIT OOM, trimmed AOT OOM, worker-wide injection, signal white-box
overflow/races, OS join/unload, Windows DST or full53 completion.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, threading, time
import check_stdlib_system_suites_shipping as native

FIXTURES=('stdlib_signal','stdlib_signal_core','stdlib_signal_core_only',
          'stdlib_signal_subscription_only','stdlib_signal_class_surface',
          'stdlib_signal_dispatch','stdlib_signal_dispatch_lifetime','stdlib_signal_dispatch_routes')
WITNESS=re.compile(rb'language-signal point=(\d+) hit=([01]) balanced=1 outcome=(memory|success|preserved-callback) callback-diagnostic-failures=(\d+)')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def witness(data,budget):
    found=WITNESS.findall(data);assert len(found)==1
    point,hit,outcome,count=found[0];point,hit,count=map(int,(point,hit,count))
    assert point==(2**64-1 if budget is None else budget)
    assert budget!=0 or hit,'Allocation zero must actually trigger'
    assert (outcome==b'preserved-callback')==(count==1) and count<=1
    assert outcome in (b'memory',b'preserved-callback') if hit else outcome==b'success' and not count
    found=native.LEDGER.findall(data);assert len(found)==1
    resets,allocated,freed=map(int,found[0]);assert resets==0 and allocated==freed
    # Budget zero can reject the very first allocation: no successful resource
    # acquisition is then expected. Successful executions must do real work.
    assert hit or allocated>0
    return bool(hit),bool(count)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists() and 1<=args.jobs<=4
    task=Path(__file__).resolve().parents[1];win=os.name=='nt'
    cc,nm,inspect=[shutil.which(n) for n in ('gcc','nm','objdump' if win else 'readelf')];assert cc and nm and inspect
    sys.path.insert(0,str(root/'tools'))
    from gen_tcc_vfs_resources import load_resources
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p)==sha(p) for p in products)
    sources={n:task/'test/cases'/(n+'.xl') for n in FIXTURES}
    host=task/'test/cases/stdlib_signal_language_product_oom_host.c';observer=task/'test/support/stdlib_native_suite_observer.c'
    probe=task/'test/support/signal_language_product_probe.c'
    build_dir=root/('build/tmp' if win else 'build/linux')
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
    dist=root/'res/tcc/win64/x86_64' if win else build_dir/'tcc-dist'
    resources=[r for r in load_resources(manifest,root,dist,build_dir/'xrt.o',build_dir) if r.virtual.startswith('include/__xl_runtime/')]
    files={Path(__file__).resolve(),Path(native.__file__).resolve(),Path(sys.executable),Path(cc),Path(nm),Path(inspect),
           args.build_report.resolve(),host,observer,probe,manifest,root/'tools/gen_tcc_vfs_resources.py',*sources.values(),*(r.source for r in resources)}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path'];files.add(Path(row['Path']))
    # Keep all original language assertions, not copies edited to pass OOM.
    for p in sources.values():assert p.read_bytes().replace(b'\r\n',b'\n')==(root/p.relative_to(task)).read_bytes().replace(b'\r\n',b'\n'),p
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir();isolated=out/'consumer';release=isolated/'release';release.mkdir(parents=True)
    temporary=out/'tmp';temporary.mkdir();sdk=out/'sdk';sdk.mkdir()
    for resource in resources:
        target=sdk/Path(resource.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,target)
    for p in products:
        shutil.copy2(p,release/p.name)
        if win:shutil.copy2(p,isolated/p.name)
    fixtures={}
    for name,p in sources.items():fixtures[name]=isolated/p.name;shutil.copy2(p,fixtures[name])
    assert not any((isolated/p).exists() for p in ('src','lib','res','include'))
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary),TEMP=str(temporary),TMP=str(temporary))
    if not win:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(sdk),'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if not win:flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    rows,proofs,controls,failures,generated=[],[],[],[],{};lock=threading.Lock()
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Controls=controls,Failures=failures,
        Scope=__doc__,FixtureCount=8,ExplicitSubset=False,CompilerSourceObjects=False,RuntimeImplementationObjects=False,
        ProviderOverrides=False,TransparentCallSiteObservation=['xl6ForeignCallbackFail','xrtValueInvoke'],ChannelErrorWaiver=False,ConsumerResetObservation=True,
        RuntimeInstrumented=False,UnobservedJitOomAcceptance=False,TrimmedAotOomAcceptance=False,WorkerWideOomAcceptance=False,
        SignalWhiteBoxAcceptance=False,OsThreadJoinAcceptance=False,ImageUnloadAcceptance=False,WindowsDstAcceptance=False,
        Full53Acceptance=False,EmittedLanguageSignalPrefixAcceptance=False)
    def run(name,command,directory=isolated,expected=0):
        command=list(map(str,command));start=time.monotonic()
        try:r=subprocess.run(command,cwd=directory,env=env,capture_output=True,timeout=180);code,data=r.returncode,r.stdout+r.stderr
        except subprocess.TimeoutExpired as e:code,data='timeout',(e.stdout or b'')+(e.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        with lock:rows.append(dict(Name=name,Command=command,Exit=code,ExpectedExit=expected,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert code==expected,(name,code,data.decode(errors='replace')[-6000:])
        return data
    def definitions(name,obj):
        data=run(name,[nm,'--defined-only','--extern-only',obj]);assert not native.providers(data),(name,native.providers(data))
    def link(name,obj,obs,probe_obj,product,unarmed=False):
        binary=isolated/(name+('.exe' if win else ''))
        libs=[release/product.name] if win else ['-L'+str(release),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        wraps=['-Wl,--wrap=xrtMemDebugReset','-Wl,--wrap=xl6ForeignCallbackFail','-Wl,--wrap=xrtValueInvoke']
        if unarmed:wraps+=['-Wl,--wrap=xrtMemDebugFailAfter']
        run(name+'.link',[cc,obj,obs,probe_obj,*libs,*wraps,'-o',binary])
        data=run(name+'.imports',[inspect,'-p' if win else '-d',binary])
        names=re.findall(rb'DLL Name:\s*([^\s]+)',data) if win else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
        assert {n.decode().lower() for n in names}&{p.name.lower() for p in products[1:]}=={product.name.lower()}
        return binary
    try:
        objects,observers,probes={},{},{}
        def expected(name):return 1 if name.endswith('dispatch_lifetime') else 2 if name.endswith('dispatch_routes') else 0
        for name in FIXTURES:
            try:
                source=out/(name+'.c');run(name+'.cgen',[release/products[0].name,'cgen',fixtures[name],'--emit',source])
                generated[name]=dict(Path=str(source),Sha256=sha(source),SourceSha256=sha(sources[name]))
                for opt in ('O0','O2'):
                    obj=out/(name+'.'+opt+'.o')
                    run(obj.stem+'.compile',[cc,*flags,*compat,'-'+opt,'-Dmain=stdlibNativeSuiteEntry','-include',source,'-c',host,'-o',obj])
                    definitions(obj.stem+'.definitions',obj);objects[name,opt]=obj
            except Exception as e:failures.append(dict(Name=name+'.build',Error=repr(e)))
        for opt in ('O0','O2'):
            obj=out/('observer.'+opt+'.o');run(obj.stem+'.compile',[cc,*flags,'-'+opt,'-c',observer,'-o',obj])
            definitions(obj.stem+'.definitions',obj);observers[opt]=obj
            for kind in range(3):
                obj=out/('callback.'+str(kind)+'.'+opt+'.o')
                run(obj.stem+'.compile',[cc,*flags,'-'+opt,'-DXL6_SIGNAL_CALLBACK_EXPECTED='+str(kind),'-c',probe,'-o',obj])
                definitions(obj.stem+'.definitions',obj);probes[kind,opt]=obj
        def exercise(spec):
            p,opt,name=spec;tag=p.stem+'.'+opt+'.'+name
            try:
                binary=link(tag,objects[name,opt],observers[opt],probes[expected(name),opt],p)
                assert witness(run(tag+'.baseline',[binary]),None)==(False,False)
                fallbacks=[]
                for point in range(512):
                    hit,fallback=witness(run(tag+'.point.'+str(point),[binary,point]),point)
                    if fallback:fallbacks.append(point)
                    if not hit:break
                else:raise AssertionError('No complete allocation prefix')
                assert point>0 and witness(run(tag+'.confirmed',[binary,point+1]),point+1)==(False,False)
                assert len(fallbacks)==(2 if expected(name)>0 else 0),fallbacks
                proof=dict(Name=tag,Product=p.name,Optimization=opt,Fixture=name,FailedAllocationPoints=point,FirstNoHit=point,
                    Confirmation=point+1,PreservedCallbackDiagnosticPoints=fallbacks,Generated=generated[name],Image=dict(Path=str(binary),Sha256=sha(binary)))
                print(tag+': complete '+str(point),flush=True);return proof
            except Exception as e:
                with lock:failures.append(dict(Name=tag,Error=repr(e)))
                print(tag+': failed '+repr(e),flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for proof in pool.map(exercise,[(p,opt,n) for p in products[1:] for opt in ('O0','O2') for n in FIXTURES if (n,opt) in objects]):
                if proof:proofs.append(proof)
        name='stdlib_signal_subscription_only'
        for p in products[1:]:
            for macro,marker in (('LEAK_AT_END',b'!after.LiveCount'),('DISABLE_DEBUG',b'xrtMemDebugEnabled'),('SKIP_LOGICAL_INJECTION',None)):
                tag=p.stem+'.control.'+macro;obj=out/(tag+'.o')
                run(tag+'.compile',[cc,*flags,'-O2','-DSTDLIB_SUITE_'+macro,'-c',observer,'-o',obj])
                binary=link(tag,objects[name,'O2'],obj,probes[0,'O2'],p,unarmed=marker is None)
                if marker:
                    data=run(tag+'.reject',[binary],expected=3 if win else -6);assert b'assert' in data.lower() and marker in data
                    controls.append(dict(Name=tag,Compiled=True,AssertionRejected=True))
                else:
                    data=run(tag+'.unarmed',[binary,0],expected=3 if win else -6)
                    assert b'valid && (point!=0 || hit)' in data
                    try:witness(data,0)
                    except AssertionError:controls.append(dict(Name=tag,Compiled=True,AssertionRejected=True,MissingInjectionRejected=True))
                    else:raise AssertionError('Missing injection accepted')
            tag=p.stem+'.control.MISSING_CALLBACK_WITNESS';obj=out/(tag+'.o')
            run(tag+'.compile',[cc,*flags,'-O2','-DXL6_SIGNAL_CALLBACK_EXPECTED=1','-DXL6_SIGNAL_SKIP_DIAGNOSTIC_WITNESS','-c',probe,'-o',obj])
            fixture='stdlib_signal_dispatch_lifetime'
            positive=next(r for r in proofs if r['Product']==p.name and r['Optimization']=='O2' and r['Fixture']==fixture)
            binary=link(tag,objects[fixture,'O2'],observers['O2'],obj,p)
            data=run(tag+'.reject',[binary,positive['PreservedCallbackDiagnosticPoints'][0]],expected=3 if win else -6)
            assert b'valid && (point!=0 || hit)' in data
            controls.append(dict(Name=tag,Compiled=True,AssertionRejected=True,MissingCallbackWitnessRejected=True))
        fake=out/'fake-provider.c';fake.write_text('void xl6SignalFakeStrong(void) {}\n__attribute__((weak)) void xl6SignalFakeWeak(void) {}\nconst unsigned xrtSignalFakeData=1;\n',encoding='ascii')
        obj=out/'fake-provider.o';run('fake-provider.compile',[cc,*flags,'-c',fake,'-o',obj])
        data=run('fake-provider.definitions',[nm,'--defined-only','--extern-only',obj]);expected={b'xl6SignalFakeStrong',b'xl6SignalFakeWeak',b'xrtSignalFakeData'}
        assert set(native.providers(data))==expected and set(native.providers(data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')))==expected
        controls.append(dict(Name='fake-provider',Compiled=True,StrongWeakAndDataRejected=True,CrLfChecked=True))
        report['Generated']=list(generated.values());assert all(sha(p['Path'])==p['Sha256'] for p in generated.values())
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['EmittedLanguageSignalPrefixAcceptance']=len(proofs)==32 and len(controls)==9 and not failures
        report['Complete']=report['InputsUnchanged'] and report['EmittedLanguageSignalPrefixAcceptance']
    except Exception as e:failures.append(dict(Name='gate',Error=repr(e)))
    report['PhaseCount']=len(rows);report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Controls=len(controls),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
