"""Exact Array entry arguments through both fresh products and embedded VFS.
NULL, boxed NUL/bytes/null, canonical Array mutation and invalid input retain
the entry contract. O0/O2 hosts check complete balanced language lifecycles.
OS argv is a separate contract, tested by the relocated AOT inventory.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    cc=shutil.which('gcc');assert cc;windows=os.name=='nt';task=Path(__file__).resolve().parents[1]
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/name for name in (('xl.dll','xlc.dll') if windows else ('libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded.get(product)==sha(product) for product in products)
    host=task/'test/cases/stdlib_array_entry_product_host.c';fixture=root/'test/cases/stdlib_array_entry_values.xl'
    headers=sorted((root/'src/runtime').glob('*.h'))
    pinned={Path(__file__).resolve(),Path(sys.executable),Path(cc),args.build_report.resolve(),host,fixture,*headers}
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];pinned.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir();isolated=out/'isolated';isolated.mkdir();sdk=out/'headers/__xl_runtime';sdk.mkdir(parents=True);temporary=out/'tmp';temporary.mkdir()
    for header in headers:shutil.copy2(header,sdk/header.name)
    for product in products:shutil.copy2(product,isolated/product.name)
    staged=isolated/fixture.name;shutil.copy2(fixture,staged)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary),TMP=str(temporary),TEMP=str(temporary))
    rows=[];proofs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Failures=failures,Scope=__doc__,
        EmbeddedVfsWithoutFallback=True,ProviderOverrides=False,Full53Acceptance=False,WorkerWideOomAcceptance=False)
    def run(name,command):
        start=time.monotonic()
        try:
            process=subprocess.run(list(map(str,command)),cwd=isolated,env=env,capture_output=True,timeout=240)
            code,data=process.returncode,process.stdout+process.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=code,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)))
        assert code==0,(name,data[-5000:]);return data
    try:
        for product in products:
            for opt in ('O0','O2'):
                name=product.stem+'-'+opt;binary=isolated/(name+('.exe' if windows else ''))
                links=[isolated/product.name] if windows else ['-L'+str(isolated),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN','-pthread','-ldl','-lm']
                try:
                    run(name+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,'-DXL_STATIC_EMBED',
                        '-I'+str(root/'release/include'),'-I'+str(out/'headers'),host,*links,'-o',binary])
                    data=run(name+'-run',[binary,isolated/product.name,staged,temporary/name])
                    assert b'8 balanced lifecycles' in data
                    proofs.append(dict(Product=product.name,Optimization=opt,BalancedLifecycles=8,ExactArguments=True))
                except Exception as error:failures.append(dict(Name=name,Error=repr(error)))
        assert not any((isolated/name).exists() for name in ('src','lib','include','res'))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Complete']=report['InputsUnchanged'] and len(proofs)==4 and not failures
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
