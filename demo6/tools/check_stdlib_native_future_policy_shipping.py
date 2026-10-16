"""Native Future payload admission in both fresh DLL/SO products at O0/O2.
Exact policy only; missing-policy rejection, frozen no-allocation and full
balanced native lifecycles. Language AOT result/capture fixtures are separate.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    cc=shutil.which('gcc');assert cc;windows=os.name=='nt'
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/name for name in (('xl.dll','xlc.dll') if windows else ('libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']};assert all(recorded.get(p)==sha(p) for p in products)
    host=Path(__file__).resolve().parents[1]/'test/cases/stdlib_native_future_policy_product_host.c'
    headers=sorted((root/'src/runtime').glob('*.h'))
    pinned={Path(__file__).resolve(),Path(sys.executable),Path(cc),args.build_report.resolve(),host,*headers}
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];pinned.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir();sdk=out/'headers/__xl_runtime';sdk.mkdir(parents=True)
    for header in headers:shutil.copy2(header,sdk/header.name)
    for product in products:shutil.copy2(product,out/product.name)
    rows=[];proofs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,Failures=failures,Scope=__doc__,
        NativeFuturePolicyAcceptance=False,RuntimeInstrumented=False,Full53Acceptance=False)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    def run(name,command):
        try:
            result=subprocess.run(list(map(str,command)),cwd=out,env=env,capture_output=True,timeout=180)
            code,data=result.returncode,result.stdout+result.stderr
        except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=code,Log=str(log),LogSha256=sha(log)))
        assert code==0,(name,data[-5000:]);return data
    try:
        for product in products:
            for opt in ('O0','O2'):
                name=product.stem+'-'+opt;binary=out/(name+('.exe' if windows else ''))
                links=[out/product.name] if windows else ['-L'+str(out),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN','-pthread','-ldl','-lm']
                try:
                    run(name+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,'-DXL_STATIC_EMBED',
                        '-I'+str(root/'release/include'),'-I'+str(out/'headers'),host,*links,'-o',binary])
                    assert b'8 balanced lifecycles' in run(name+'-run',[binary]);proofs.append(dict(Product=product.name,Optimization=opt))
                except Exception as error:failures.append(dict(Name=name,Error=repr(error)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['NativeFuturePolicyAcceptance']=len(proofs)==4 and not failures
        report['Complete']=report['InputsUnchanged'] and report['NativeFuturePolicyAcceptance']
    finally:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(Complete=report['Complete'],Failures=failures)),flush=True);return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
