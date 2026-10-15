"""Original anchored-root fixture through relocated shipping-only AOT."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists()
    windows=os.name=='nt';build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged']
    products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    actual={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']};assert all(actual[f]==sha(f) for f in products)
    fixture=root/'test/cases/stdlib_fs_root.xl'
    files={Path(__file__).resolve(),Path(sys.executable),a.build_report.resolve(),fixture,*products}
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    (out/'compiler/release').mkdir(parents=True);(out/'relocated/build/test').mkdir(parents=True);(out/'tmp').mkdir()
    for f in products:
        shutil.copy2(f,out/'compiler/release'/f.name)
        if windows:shutil.copy2(f,out/'compiler'/f.name)
    local=out/'compiler'/fixture.name;shutil.copy2(fixture,local)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    env.update(TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    rows=[];report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Full53Acceptance=False)
    image=out/'compiler'/('root.exe' if windows else 'root.aot')
    try:
        for name,command,cwd in [('build',[out/'compiler/release'/products[0].name,'build',local,'-o',image],out/'compiler'),('run',[out/'relocated'/image.name],out/'relocated')]:
            if name=='run':shutil.copy2(image,out/'relocated'/image.name)
            result=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=300)
            log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
            rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
            assert result.returncode==0,(name,(result.stdout+result.stderr)[-5000:])
            if name=='run':assert b'fs root ok' in result.stdout
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,IndependentRootAotAcceptance=True)
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print('root AOT complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
