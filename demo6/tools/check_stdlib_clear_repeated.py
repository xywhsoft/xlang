"""Repeat the exact relocated clear AOT images from a successful product gate.
No rebuilding, generated-C editing, provider overrides or retry-to-pass rule.
Every trial is retained; any nonzero exit keeps this acceptance false.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,time
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('clear-report','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--trials',type=int,default=50);a=p.parse_args();assert 1<=a.trials<=1000
    report_path=a.clear_report.resolve();out=a.out.resolve();assert not out.exists()
    original=json.loads(report_path.read_text(encoding='utf-8-sig'));assert original['Complete'] and original['InputsUnchanged']
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in original['Products']}
    binaries=[]
    for stem in ('stdlib_stack_clear_error','stdlib_stack_clear_error_continuation'):
        row=next(row for row in original['Rows'] if row['Name']==stem+'.aot-relocated');assert row['Exit']==0
        binary=Path(row['Command'][0]).resolve();assert sha(binary)==recorded[binary];binaries.append(binary)
    files={Path(__file__).resolve(),Path(sys.executable),report_path,*binaries}
    for row in original['Inputs']+original['Products']:assert sha(row['Path'])==row['Sha256'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)];out.mkdir();rows=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Scope=__doc__,TrialsPerImage=a.trials,
        Rebuilt=False,ProviderOverrides=False,Full53Acceptance=False,ClearRepeatedAotAcceptance=False)
    for binary in binaries:
        for trial in range(a.trials):
            start=time.monotonic()
            try:
                run=subprocess.run([str(binary)],cwd=binary.parent,capture_output=True,timeout=30)
                code,data=run.returncode,run.stdout+run.stderr
            except subprocess.TimeoutExpired as error:code,data='timeout',(error.stdout or b'')+(error.stderr or b'')
            log=out/(binary.stem+'.'+str(trial)+'.log');log.write_bytes(data)
            rows.append(dict(Image=str(binary),Trial=trial,Exit=code,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['Complete']=report['InputsUnchanged'] and all(row['Exit']==0 for row in rows)
    report['ClearRepeatedAotAcceptance']=report['Complete']
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Clear immutable AOT trials='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
