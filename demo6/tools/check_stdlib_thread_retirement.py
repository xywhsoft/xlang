"""Source-built Thread capture retirement: deterministic handoff and bad code.
Complete resident/XRT closure O0/O2, no released library or compiler. Linux
ASan/UBSan instruments the entire selected native closure. This is logical
worker/capture completion, not OS-thread join or image-unload acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
MODES=('vectors','graph','stop','drop-freeze','active-release','lease','self-drop',
    'capture-retirement','errors','oom','stress','methods')
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','out'): p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root,out=a.root.resolve(),a.out.resolve(); cc=shutil.which('gcc')
    assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    sources=['test/cases/stdlib_thread_native_host.c','test/support/thread_retirement_native.c',
        'test/support/module_product_memory.c','src/runtime/xl6_thread.c','src/runtime/xl6_closure.c',
        'src/runtime/xl6_code_lease.c','src/runtime/xl6_cell.c','src/api/xl_foreign_error.c']
    files={Path(__file__),Path(sys.executable),Path(cc),*[root/s for s in sources]}
    for folder in ('src/runtime','src/api','lib/xruntime','lib/xrt','release/include','test/support'):
        files.update(p for p in (root/folder).rglob('*.h'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(parents=True); rows=[]; products=[]; controls=[]; objects=[]
    report=dict(Complete=False,InputsUnchanged=False,Rows=rows,Inputs=inputs,Products=products,
        Objects=objects,NegativeControls=controls,Sanitized=a.sanitize,SelectedClosureInstrumented=a.sanitize,
        Full53Acceptance=False,ShippingArtifact=False,OsThreadJoin=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include')]
    links=['-Wl,--wrap=xrtThreadWait']; env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-ldl','-lm']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,cmd,negative=False):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=120)
        data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
        rows.append(dict(Name=label,Command=list(map(str,cmd)),Exit=r.returncode,ExpectedRejection=negative,
            Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if negative else r.returncode==0),(label,data[-6000:].decode(errors='replace'))
        if negative: assert b'releaseReturned' in data,(label,data[-2000:])
        return data.decode(errors='replace')
    try:
        cached={}
        for opt in (0,2):
            compiled=[]
            for i,source in enumerate(sources):
                obj=out/(str(opt)+'-'+str(i)+'.o')
                run('O'+str(opt)+'.compile'+str(i),[cc,*flags,'-O'+str(opt),'-c',root/source,'-o',obj])
                compiled.append(obj); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
            cached[opt]=compiled; binary=out/('O'+str(opt)+('.exe' if os.name=='nt' else '.host'))
            run('O'+str(opt)+'.link',[cc,*compiled,*links,'-o',binary]); products.append(dict(Path=str(binary),Sha256=sha(binary)))
            for mode in MODES: assert 'lifetimes balanced' in run('O'+str(opt)+'.'+mode,[binary,mode])
        # Reintroduce precisely the pre-cleanup publication race. Compile it
        # against the same real XRT, then require the handshake oracle to fail.
        body=(root/'src/runtime/xl6_thread.c').read_text(encoding='utf-8')
        before='owner->callable=NULL; unlock(owner); mutationEnd(&mutation);'
        assert body.count(before)==1
        guard='if (!owner->workerActive || owner->callable) abort();'
        assert body.count(guard)==1
        bad=out/'early-completion.c'; bad.write_text(body.replace(before,
            'owner->callable=NULL; owner->workerActive=false; unlock(owner); mutationEnd(&mutation);').replace(
                guard,'if (owner->callable) abort();'),encoding='utf-8')
        obj=out/'bad.o'; run('bad.compile',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj])
        linked=list(cached[0]); linked[sources.index('src/runtime/xl6_thread.c')]=obj
        binary=out/('bad'+('.exe' if os.name=='nt' else '.host'))
        run('bad.link',[cc,*linked,*links,'-o',binary]); run('bad.reject',[binary,'capture-retirement'],True)
        controls.append(dict(Name='early-worker-completion',Compiled=True,Rejected=True,Source=str(bad),Sha256=sha(bad)))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs+objects+products)
        report['Complete']=report['InputsUnchanged'] and len(rows)==45 and len(controls)==1
        assert report['Complete']
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Actual Thread retirement: '+str(len(rows))+' phases, deterministic race rejected; complete='+str(report['Complete']),flush=True)
if __name__=='__main__': main()
