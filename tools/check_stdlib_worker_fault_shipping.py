"""Normative language-worker fault gate on actual shared products and AOT.
Enumerate complete one-shot allocation prefixes in the worker body, not its
startup/tail or scheduler-wide allocation. Caught MemoryError must not poison
the thread exit. Keep finally, retained XRT completion, caller isolation and full
ledgers. Logical completion is not OS join; shared products stay loaded.
Independent failures are collected; known failures are never accepted/skipped.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,time

CASES=('text','regex','array','list','dict','set','stack','tree','slot-map','queues')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists() and 1<=args.jobs<=4
    windows=os.name=='nt';cc=shutil.which('gcc');assert cc;task=Path(__file__).resolve().parents[1]
    sys.path.insert(0,str(root/'tools'))
    from gen_tcc_vfs_resources import load_resources
    source=task/'test/cases/stdlib_containers_worker_oom.xl';host=task/'test/cases/stdlib_worker_oom_product_host.c'
    probe=task/'test/support/worker_oom_probe.c'
    returnControl=task/'test/cases/stdlib_checked_return_scope_host.c'
    scopeControl=task/'test/cases/xlir_cleanup_error_scope_runtime.c'
    scopePeer=task/'test/cases/xlir_foreign_error_peer.c'
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded.get(p)==sha(p) for p in products)
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),args.build_report.resolve(),source,host,probe,
        returnControl,scopeControl,scopePeer}
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(parents=True);isolated=out/'compiler';release=isolated/'release';release.mkdir(parents=True)
    tmp=out/'tmp';tmp.mkdir();sdk=out/'sdk';sdk.mkdir()
    for product in products:
        shutil.copy2(product,release/product.name)
        if windows:shutil.copy2(product,isolated/product.name)
    fixture=isolated/source.name;shutil.copy2(source,fixture)
    helper=isolated/'worker_oom_probe.c';helper.write_bytes(b'#define XL6_WORKER_PROBE_EMBEDDED_SDK 1\n'+probe.read_bytes())
    for p in (root/'release/include').glob('*.h'):shutil.copy2(p,sdk/p.name)
    build_dir=root/('build/tmp' if windows else 'build/linux')
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    dist=root/'res/tcc/win64/x86_64' if windows else build_dir/'tcc-dist'
    for resource in load_resources(manifest,root,dist,build_dir/'xrt.o',build_dir):
        if resource.virtual.startswith('include/__xl_runtime/'):
            target=sdk/Path(resource.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(resource.source,target)
    # Only explicit native observer compilation sees these SDK declarations.
    # The real compiler/JIT/AOT resolve production dependencies through VFS.
    environment={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    environment.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[];cases=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Cases=cases,Failures=failures,Scope=__doc__,
        IndependentFailuresCollected=True,ExplicitSubset=False,WorkerBodyPrefixAcceptance=False,SchedulerWideOomAcceptance=False,
        RuntimeInstrumented=False,ProviderOverrides=False,Full53Acceptance=False)
    report['OsThreadJoinAcceptance']=False
    report['CheckedReturnIsolationAcceptance']=False
    report['CallbackFirstErrorAcceptance']=False
    def command(name,argv,env=environment):
        argv=list(map(str,argv));started=time.monotonic()
        try:r=subprocess.run(argv,cwd=isolated,env=env,capture_output=True,timeout=180);code,data=r.returncode,r.stdout+r.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        return dict(Name=name,Command=argv,Exit=code,Seconds=time.monotonic()-started,Log=str(log),LogSha256=sha(log)),data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk)]
    if not windows:flags+=['-D_GNU_SOURCE']
    cli=release/products[0].name
    row,data=command('cgen',[cli,'cgen',fixture,'--emit',out/'worker.c']);rows.append(row)
    if row['Exit']!=0:failures.append(dict(Name='cgen',Diagnostic=data.decode(errors='replace')[-5000:]))
    else:
        compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        for level in ('O0','O2'):
            row,data=command('strict.'+level,[cc,*flags,*compat,'-'+level,'-c',out/'worker.c','-o',out/('worker.'+level+'.o')]);rows.append(row)
            if row['Exit']!=0:failures.append(dict(Name=row['Name'],Diagnostic=data.decode(errors='replace')[-5000:]))
    jobs=[];controls=[]
    for product in products[1:]:
        libs=[release/product.name] if windows else ['-L'+str(release),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        for level in ('O0','O2'):
            name=product.stem+'.'+level;binary=isolated/(name+('.exe' if windows else ''))
            # Private declaration headers are explicit observer inputs only.
            # Do not compile their implementation units or give this include
            # path to the isolated language compiler/JIT/AOT.
            for kind,sources in (('return',[returnControl]),('scope',[scopeControl,scopePeer])):
                tag=name+'.'+kind;control=isolated/(tag+('.exe' if windows else ''))
                row,data=command(tag+'.build',[cc,*flags,'-'+level,'-I'+str(root),'-I'+str(root/'release'),*sources,*libs,'-o',control]);rows.append(row)
                if row['Exit']==0:
                    row,data=command(tag,[control]);rows.append(row)
                    passed=row['Exit']==0
                else:passed=False
                controls.append(dict(Name=tag,Kind=kind,Passed=passed))
                if not passed:failures.append(dict(Name=tag,Diagnostic=data.decode(errors='replace')[-5000:]))
            row,data=command(name+'.build',[cc,*flags,'-'+level,host,probe,*libs,'-o',binary]);rows.append(row)
            if row['Exit']!=0:failures.append(dict(Name=name+'.build',Diagnostic=data.decode(errors='replace')[-5000:]));continue
            jobs.extend((name,product.name,level,binary,kind) for kind in range(len(CASES)))
    def run_case(job):
        name,product,level,binary,kind=job;tag=name+'.'+CASES[kind]
        row,data=command(tag,[binary,release/product,fixture,tmp/tag,kind])
        prefix=re.findall(rb'worker-complete-prefix case=(\d+) failures=(\d+) first-no-hit=(\d+) confirmed=(\d+) balanced=1',data)
        passed=row['Exit']==0 and len(prefix)==1
        if passed:
            actual,count,first,confirmed=map(int,prefix[0]);passed=actual==kind and count==first and count>0 and confirmed==first+1
        handled=re.search(rb'worker-observed started=1 finally=1 hit=1 caught=1 native-exit=1',data) is not None
        return dict(Name=tag,Product=product,Optimization=level,Kind=kind,Passed=passed,Row=row,
            CompletePrefixes=[[int(v) for v in item] for item in prefix],CaughtErrorPoisonedThread=handled,
            Diagnostic='' if passed else data.decode(errors='replace')[-3500:])
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for case in pool.map(run_case,jobs):
            cases.append(case);rows.append(case['Row']);print(case['Name']+': '+str(case['Passed']),flush=True)
            if not case['Passed']:failures.append(dict(Name=case['Name'],CaughtErrorPoisonedThread=case['CaughtErrorPoisonedThread'],Diagnostic=case['Diagnostic']))
    # Same language program and explicit test-only callbacks as a real AOT.
    # Boundary controls here do not replace the exhaustive shared-product ledger.
    image=isolated/('worker-aot'+('.exe' if windows else ''))
    row,data=command('aot.build',[cli,'build',fixture,helper,'-o',image]);rows.append(row)
    if row['Exit']==0:
        for kind in range(len(CASES)):
            for budget in (0,18446744073709551615):
                env=dict(environment,XL6_WORKER_CASE=str(kind),XL6_WORKER_BUDGET=str(budget))
                row,data=command('aot.'+CASES[kind]+'.'+str(budget),[image],env);rows.append(row)
                if row['Exit']!=0:failures.append(dict(Name=row['Name'],Diagnostic=data.decode(errors='replace')[-3500:]))
    else:failures.append(dict(Name='aot.build',Diagnostic=data.decode(errors='replace')[-5000:]))
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['NativeBoundaryControls']=controls
    report['CheckedReturnIsolationAcceptance']=len([c for c in controls if c['Kind']=='return' and c['Passed']])==4
    report['CallbackFirstErrorAcceptance']=len([c for c in controls if c['Kind']=='scope' and c['Passed']])==4
    report['Complete']=report['InputsUnchanged'] and len(cases)==4*len(CASES) and not failures
    report['DiscoveryComplete']=report['InputsUnchanged'] and len(cases)==4*len(CASES)
    report['WorkerBodyPrefixAcceptance']=report['Complete']
    report['PhaseCount']=len(rows)
    report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],DiscoveryComplete=report['DiscoveryComplete'],Failed=len(failures),
        PoisonedThreads=sum(case['CaughtErrorPoisonedThread'] for case in cases))),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
