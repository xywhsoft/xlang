"""Current-source six-fixture hosted execution and complete mode matrices.
Uses every verified full-source provider object and genuine TCC, original graph
hosts and real Promise completion. No provider override, shipping/VFS claim,
runtime-wide sanitizer, worker-wide OOM, unmap or full53 acceptance.
"""
from pathlib import Path
import argparse,copy,hashlib,itertools,json,os,re,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

GRAPH={'stack':('stdlib_stack_ownership',1,25),
    'tree':('stdlib_tree_ownership',5,12),'queue':('stdlib_concurrent_queue_ownership',6,25)}
CONTINUATION='stdlib_concurrent_queue_continuation'
CLEAR=('stdlib_stack_clear_error','stdlib_stack_clear_error_continuation')
STEMS={row[0] for row in GRAPH.values()}|{CONTINUATION,*CLEAR}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def validate_hosted_source(report,fixtures,driver):
    assert report['Complete'] and report['InputsUnchanged'] and not report['Failures']
    assert report['DriverSha256']==driver and report['FixtureHashes']==fixtures
    assert not report['ProviderOverrides'] and not report['ExplicitSubset']
    assert not report['ShippingLibraryAcceptance'] and not report['Full53Acceptance']
    assert report['Rows'] and all(r['Exit']==0 for r in report['Rows'])
    covered={};total=0
    for label,(stem,families,lifetimes) in GRAPH.items():
        proofs=[p for p in report['Proofs'] if p['Fixture']==stem]
        expected={(opt,mode,family) for opt in ('O0','O2')
            for mode,family in [(0,0)]+list(itertools.product((1,2,3),range(families)))}
        actual=[(p['Optimization'],p['Mode'],p['Family']) for p in proofs]
        assert len(actual)==len(set(actual)) and set(actual)==expected
        assert all(p['MeasuredLifecycles']==lifetimes for p in proofs)
        total+=lifetimes*len(proofs);covered[stem]=dict(Modes=len(proofs),MeasuredLifecycles=lifetimes*len(proofs))
    proofs=[p for p in report['Proofs'] if p['Fixture']==CONTINUATION]
    expected=set(itertools.product(('O0','O2'),('success','reject','cancel','close'),('ready','pending-watch','pending-commit')))
    actual=[(p['Optimization'],p['Terminal'],p['Timing']) for p in proofs]
    assert len(actual)==len(set(actual)) and set(actual)==expected
    assert all(p['MeasuredLifecycles']==2 and p['FinallyCallsPerActivation']==1 for p in proofs)
    total+=2*len(proofs);covered[CONTINUATION]=dict(Modes=len(proofs),MeasuredLifecycles=2*len(proofs))
    for stem in CLEAR:
        proofs=[p for p in report['Proofs'] if p['Fixture']==stem]
        assert len(proofs)==2 and {p['Optimization'] for p in proofs}=={'O0','O2'}
        assert all(p['MeasuredLifecycles']==25 for p in proofs)
        total+=50;covered[stem]=dict(Modes=2,MeasuredLifecycles=50)
    assert len(report['Proofs'])==106 and total==1682 and set(covered)==set(fixtures)==STEMS
    return covered,total

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--certify-report',type=Path,help='Independently certify a completed saved execution; do not replay or overwrite it.')
    a=p.parse_args();root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc=shutil.which('gcc');assert cc and not out.exists();build,pinned,compared=verify_public_build(root,public)
    windows=os.name=='nt';cases=root/'test/cases';driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    libraries=root/'res/tcc/win64/x86_64' if windows else Path(build['NativeRoot'])/'build/linux/tcc-dist'
    links=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    links+=[str(runtime),*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    fixtures={stem:sha(cases/(stem+'.xl')) for stem in STEMS}
    pinned.update((Path(__file__).resolve(),Path(cc),runtime,cases/'stdlib_sequence_clear_source_host.c',
        root/'test/support/sequence_clear_probe.c',cases/'stdlib_concurrent_queue_continuation_host.c',
        cases/'xlir_multi_await_host_prelude.h',cases/'xlir_multi_await_host_support.h'))
    pinned.update(cases/(stem+'.xl') for stem in STEMS)
    pinned.update(cases/(stem+'_public.c') for stem,_,_ in GRAPH.values())
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    if a.certify_report:
        path=a.certify_report.resolve();existing=json.loads(path.read_text(encoding='utf-8-sig'))
        validate_hosted_source(existing,fixtures,sha(driver))
        assert existing['SourceFingerprint']==compared
        assert set(existing['NegativeControls'])=={'missing-pending-commit','wrong-fixture','short-lifecycle','wrong-driver','failed-phase'}
        assert all(sha(r['Path'])==r['Sha256'] for r in existing['Inputs'])
        # Old runners may have included the report before their final save.
        # A report cannot hash itself. Every other product remains immutable.
        self_rows=[r for r in existing['Products'] if Path(r['Path']).resolve()==path]
        assert len(self_rows)<=1
        products=[r for r in existing['Products'] if Path(r['Path']).resolve()!=path]
        assert all(sha(r['Path'])==r['Sha256'] for r in products)
        assert all(sha(r['Log'])==r['LogSha256'] for r in existing['Rows'])
        existing.update(Products=products,RawExecutionReport=dict(Path=str(path),Sha256=sha(path)),
            IndependentlyCertified=True,ExecutionReplayed=False,SelfReferentialCollectorCorrected=bool(self_rows))
        existing['Inputs']+=inputs+[dict(Path=str(path),Sha256=sha(path))]
        out.mkdir();(out/'results.json').write_text(json.dumps(existing,indent=2)+'\n')
        print('Certified saved hosted execution without replay: 106 modes, 1682 measured lifecycles',flush=True)
        return 0
    out.mkdir();tmp=out/'tmp';tmp.mkdir();env=dict(os.environ)
    if windows:shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include')]
    if not windows:flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    rows=[];proofs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Rows=rows,Proofs=proofs,Failures=failures,Inputs=inputs,
        DriverSha256=sha(driver),FixtureHashes=fixtures,SourceFingerprint=compared,ExplicitSubset=False,
        ProviderOverrides=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,EmbeddedVfsWithoutFallback=False,
        RuntimeInstrumented=False,WorkerWideOomAcceptance=False,ModuleUnmapAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        start=time.monotonic();command=list(map(str,command))
        try:r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=600);data,code=r.stdout+r.stderr,r.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=code,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)));save()
        assert code==0,(name,data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def batch(name,action):
        try:action()
        except Exception as error:failures.append(dict(Name=name,Error=repr(error)));print(name+': FAILED '+repr(error),flush=True);save()
    def generate(stem):
        generated=out/(stem+'.c');repeat=out/(stem+'.repeat.c')
        for tag,target in (('cgen',generated),('repeat',repeat)):run(stem+'.'+tag,[driver,cases/(stem+'.xl'),'cgen-default',target])
        assert generated.read_bytes()==repeat.read_bytes();return generated
    def graph(label,stem,families,lifetimes):
        generated=generate(stem)
        for opt in ('O0','O2'):
            run(stem+'.'+opt+'.strict',[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',out/(stem+'.'+opt+'.o')])
            binary=out/(label+'.'+opt+('.exe' if windows else ''))
            run(stem+'.'+opt+'.host',[cc,*flags,'-'+opt,cases/(stem+'_public.c'),*links,'-o',binary])
            for mode,family in [(0,0)]+list(itertools.product((1,2,3),range(families))):
                name=stem+'.'+opt+'.mode'+str(mode)+'.family'+str(family)
                args=[mode,*([family] if label!='stack' else []),public/'include',tmp/name,runtime,libraries,cases/(stem+'.xl')]
                text=run(name,[binary,*args]);assert 'balanced' in text and str(lifetimes)+' ' in text
                proofs.append(dict(Fixture=stem,Optimization=opt,Mode=mode,Family=family,MeasuredLifecycles=lifetimes));save()
                print(name+': balanced',flush=True)
    def continuation():
        generated=generate(CONTINUATION);ir=out/(CONTINUATION+'.ir.json')
        run(CONTINUATION+'.ir',[driver,cases/(CONTINUATION+'.xl'),'ir-lowered',ir])
        document=json.loads(ir.read_text());assert len(document['asyncMachines'])==1 and len(document['asyncSuspensions'])==3
        for opt,terminal,timing in itertools.product(('O0','O2'),('success','reject','cancel','close'),('ready','pending-watch','pending-commit')):
            name=CONTINUATION+'.'+opt+'.'+terminal+'.'+timing;binary=out/(name+('.exe' if windows else ''))
            defines=['-DXLIR_MULTI_AWAIT_GENERATED_SOURCE="'+generated.as_posix()+'"']
            if terminal!='success':defines+=['-DXLIR_CHAIN_THIRD_'+terminal.upper()]
            if timing!='ready':defines+=['-DXLIR_MULTI_AWAIT_PENDING']
            if timing=='pending-commit':defines+=['-DXLIR_MULTI_AWAIT_AFTER_COMMIT']
            run(name+'.build',[cc,*flags,*compat,'-'+opt,*defines,cases/'stdlib_concurrent_queue_continuation_host.c',*links,'-o',binary])
            text=run(name,[binary]);assert 'two complete language lifecycles and memory balance' in text and 'terminal cleanup balanced' in text
            if timing!='ready':assert 'every input completed after actual pending Watch registration' in text
            if timing=='pending-commit':assert 'deferred completion after actual activation Commit' in text
            proofs.append(dict(Fixture=CONTINUATION,Optimization=opt,Terminal=terminal,Timing=timing,MeasuredLifecycles=2,FinallyCallsPerActivation=1));save()
            print(name+': balanced',flush=True)
    def clear(stem):
        generated=generate(stem);ir=out/(stem+'.ir.json');run(stem+'.ir',[driver,cases/(stem+'.xl'),'ir-lowered',ir])
        document=json.loads(ir.read_text());assert any(t['name']=='var' and int(str(t['typeValueId']),0)==0x3594F07E9DD4DB58 for t in document['types'])
        for opt in ('O0','O2'):
            run(stem+'.'+opt+'.strict',[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',out/(stem+'.'+opt+'.o')])
            binary=out/(stem+'.'+opt+('.exe' if windows else ''))
            run(stem+'.'+opt+'.host',[cc,*flags,'-'+opt,cases/'stdlib_sequence_clear_source_host.c',root/'test/support/sequence_clear_probe.c',*links,'-o',binary])
            text=run(stem+'.'+opt+'.run',[binary,public/'include',runtime,libraries,cases/(stem+'.xl'),tmp/(stem+'.'+opt)])
            assert '25 complete language lifecycles' in text and 'memory balanced' in text
            proofs.append(dict(Fixture=stem,Optimization=opt,MeasuredLifecycles=25));save()
    for label,(stem,count,lifetimes) in GRAPH.items():batch(label,lambda l=label,s=stem,c=count,n=lifetimes:graph(l,s,c,n))
    batch(CONTINUATION,continuation)
    for stem in CLEAR:batch(stem,lambda s=stem:clear(s))
    report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
    report['Complete']=not failures and report['InputsUnchanged'] and len(proofs)==106
    if report['Complete']:
        report['CoveredFixtures'],report['MeasuredLifecycles']=validate_hosted_source(report,fixtures,sha(driver))
        controls=[]
        for name in ('missing-pending-commit','wrong-fixture','short-lifecycle','wrong-driver','failed-phase'):
            bad=copy.deepcopy(report)
            if name=='missing-pending-commit':bad['Proofs'].remove(next(p for p in bad['Proofs'] if p.get('Timing')=='pending-commit'))
            elif name=='wrong-fixture':bad['FixtureHashes']['stdlib_stack_ownership']='0'*64
            elif name=='short-lifecycle':bad['Proofs'][0]['MeasuredLifecycles']-=1
            elif name=='wrong-driver':bad['DriverSha256']='0'*64
            else:bad['Rows'][0]['Exit']=1
            try:validate_hosted_source(bad,fixtures,sha(driver))
            except AssertionError:controls.append(name)
            else:raise AssertionError('Damaged hosted report accepted: '+name)
        report['NegativeControls']=controls
    report['PhaseCount']=len(rows)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in out.rglob('*') if path.is_file() and path!=out/'results.json']
    save();print('Hosted source: '+str(len(proofs))+' modes, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
