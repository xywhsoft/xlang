"""Stack clear error injection through real rebuilt DLL/SO and embedded VFS.
Two required fixtures retain RangeError/finally/async/generator assertions.
O0/O2 hosts check 25 complete lifecycles; CLI JIT and independent AOT use only
an explicit test callback sidecar. Not worker OOM or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('root','build-report','out'):parser.add_argument('--'+n,type=Path,required=True)
    a=parser.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists()
    cc=shutil.which('gcc');assert cc;windows=os.name=='nt';project=Path(__file__).resolve().parents[1]
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged']
    products=[root/'release'/n for n in (('xl.dll','xlc.dll','xl.exe') if windows else ('libxl.so','libxlc.so','xl'))]
    cases=[project/'test/cases'/n for n in ('stdlib_stack_clear_error.xl','stdlib_stack_clear_error_continuation.xl')]
    host=project/'test/cases/stdlib_sequence_clear_product_host.c'
    probe=project/'test/support/sequence_clear_probe.c';sidecar=project/'test/cases/stdlib_sequence_clear_embedded_probe.c'
    files={Path(__file__).resolve(),Path(cc),a.build_report.resolve(),host,probe,sidecar,*cases}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'],row['Path'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir();isolated=out/'isolated';isolated.mkdir();relocated=out/'relocated';relocated.mkdir();tmp=out/'tmp';tmp.mkdir()
    headers=out/'headers/__xl_runtime';headers.mkdir(parents=True)
    for p in list((root/'release/include').glob('*.h'))+list((root/'src/runtime').glob('*.h')):shutil.copy2(p,headers/p.name)
    for p in products:shutil.copy2(p,isolated/p.name);assert sha(p)==sha(isolated/p.name)
    for p in cases:shutil.copy2(p,isolated/p.name)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp));rows=[];proofs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Failures=failures,
        Scope=__doc__,EmbeddedVfsWithoutFallback=True,RuntimeInstrumented=False,Full53Acceptance=False)
    def run(name,command,cwd=isolated):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=240)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=r.returncode,Seconds=time.monotonic()-start))
        assert r.returncode==0,(name,data[-6000:]);return data.decode(errors='replace')
    def batch(name,action):
        try:action()
        except Exception as e:failures.append(dict(Name=name,Error=repr(e)));print(name+': FAILED '+repr(e),flush=True)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out/'headers')]
    if not windows:flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    def links(p):return [isolated/p.name] if windows else ['-L'+str(isolated),'-l'+p.stem[3:],'-Wl,-rpath,$ORIGIN','-pthread','-ldl','-lm']
    cli=isolated/products[2].name
    def check(case):
        fixture=isolated/case.name;generated=out/(case.stem+'.c');ir=out/(case.stem+'.json')
        run(case.stem+'.cgen',[cli,'cgen',fixture,'--emit',generated])
        run(case.stem+'.ir',[cli,'ir',fixture,'--lowered','--format','json','--emit',ir])
        data=json.loads(ir.read_text());operations=[r for r in data['instructions'] if r.get('runtimeOpName')=='stack-clear']
        assert operations
        for p in products[:2]:
            for opt in ('O0','O2'):
                name=case.stem+'.'+p.stem+'.'+opt;binary=isolated/(name+('.exe' if windows else ''))
                run(name+'.build',[cc,*flags,*compat,'-'+opt,host,probe,*links(p),'-o',binary])
                text=run(name+'.run',[binary,isolated/p.name,fixture,tmp/(name+'.temps')])
                assert '25 complete language lifecycles' in text and 'memory balanced' in text
                proofs.append(dict(Case=case.stem,Product=p.name,Optimization=opt,Lifecycles=25))
        for opt in ('O0','O2'):run(case.stem+'.'+opt+'.strict-C',[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',out/(case.stem+'.'+opt+'.o')])
        run(case.stem+'.jit',[cli,'run',fixture,sidecar])
        binary=isolated/(case.stem+'.exe');run(case.stem+'.aot-build',[cli,'build',fixture,sidecar,'-o',binary])
        independent=relocated/binary.name;shutil.copy2(binary,independent)
        run(case.stem+'.aot-relocated',[independent],relocated)
        proofs.append(dict(Case=case.stem,IndependentAot=True))
    for case in cases:batch(case.stem,lambda c=case:check(c))
    assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
    report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
    report['Complete']=not failures and report['InputsUnchanged'] and len(proofs)==10
    report['PhaseCount']=len(rows)
    report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.rglob('*') if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Clear shipping: '+str(len(rows))+' phases, '+str(len(proofs))+' proofs, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
