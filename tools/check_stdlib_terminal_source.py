"""Exact-source terminal resize delivery, deliberate input-before-resize order,
strict C/JIT/module/AOT, repeated sessions and rejecting real counterexamples.
No arbitrary sleeps, terminal provider replacement or OS handle-zero claim.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--trials',type=int,default=128);a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc=shutil.which('gcc');assert cc and not out.exists() and 4<=a.trials<=512 and a.trials%4==0
    build,pinned,_=verify_public_build(root,public);windows=os.name=='nt';driver=Path(build['Driver'])
    fixtures=[root/'test/cases'/(name+'.xl') for name in ('stdlib_process_terminal_session','stdlib_process_terminal_event_order')]
    childSource=root/'test/cases/xlir_process_child.c'
    native=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(cc),childSource,native,*fixtures));inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(pinned)]
    out.mkdir();env=dict(os.environ);rows=[];lock=None
    if windows:shutil.copy2(native,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if not windows:flags+=['-D_GNU_SOURCE']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Trials=a.trials,Scope=__doc__,
        TerminalDeliveryAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,OsHandleZeroGrowthAcceptance=False)
    def command(name,argv,environment=env,reject=False):
        argv=list(map(str,argv));start=time.monotonic()
        try:r=subprocess.run(argv,cwd=root,env=environment,capture_output=True,timeout=120);data,code=r.stdout+r.stderr,r.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        row=dict(Name=name,Command=argv,Exit=code,ExpectedRejection=reject,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log))
        return row,data
    def run(name,argv,reject=False):
        row,data=command(name,argv,reject=reject);rows.append(row);print(name+': '+str(row['Exit']),flush=True)
        assert row['Exit'] not in (0,'timeout') if reject else row['Exit']==0,(name,data[-5000:].decode(errors='replace'))
        return data
    try:
        child=out/('process-child'+('.exe' if windows else ''))
        run('child.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',*(['-municode'] if windows else ['-D_GNU_SOURCE']),childSource,'-o',child])
        env['XL6_PROCESS_CHILD']=str(child)
        for source in fixtures:
            name=source.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[driver,source,'cgen-default',generated]);run(name+'.repeat',[driver,source,'cgen-default',repeat]);assert generated.read_bytes()==repeat.read_bytes()
            for opt in ('O0','O2'):run(name+'.'+opt+'.strict',[cc,*flags,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            for mode in ('run','module'):
                data=run(name+'.'+mode,[driver,source,mode,out/(name+'.'+mode)])
                assert (('public source '+mode+' complete') if windows else 'public Linux source complete').encode() in data
            binary=out/(name+'.aot');run(name+'.aot.build',[driver,source,'exe',binary])
            run(name+'.aot.run',[Path(str(binary)+'.exe') if windows else binary])
        for label,replacement in (('missing-resize','true'),('wrong-resize','handle.resize(95, 31)')):
            original=fixtures[1].read_text();changed,count=re.subn(r'handle\.resize\(96, 32\)',replacement,original);assert count==1
            mutant=out/(label+'.xl');mutant.write_text(changed)
            for mode in ('run','module'):
                data=run(label+'.'+mode,[driver,mutant,mode,out/(label+'.'+mode)],True)
                assert b'AssertionError' in data
        def trial(i):
            source=fixtures[i%2];mode=('run','module')[(i//2)%2]
            row,data=command('trial-'+str(i),[driver,source,mode,out/('trial-'+str(i))])
            marker=(('public source '+mode+' complete') if windows else 'public Linux source complete').encode() in data
            row.update(Fixture=source.stem,Mode=mode,CompletionMarker=marker)
            return row
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            trials=list(pool.map(trial,range(a.trials)))
        rows.extend(trials);report['TrialRows']=trials
        assert all(r['Exit']==0 and r['CompletionMarker'] for r in trials)
        assert all(sum(r['Fixture']==s.stem and r['Mode']==m for r in trials)==a.trials//4 for s in fixtures for m in ('run','module'))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);assert report['InputsUnchanged']
        report['TerminalDeliveryAcceptance']=True;report['Complete']=True
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in out.rglob('*') if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
