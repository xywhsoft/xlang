"""Deterministic native pool startup admission in both pinned consumer headers.
Checks exact canonical function provenance, real frozen-startup graphs, active
callback refusal and lifecycle balance. Optional old-header negative control.
This is not shipping-library, full scheduler, or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def function(path):
    text=Path(path).read_text(encoding='utf-8-sig');start=text.index('static bool __xrtTaskPoolOwnershipCount(')
    end=text.index('static bool __xrtTaskPoolOwnershipTrace(',start)
    return text[start:end].strip()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','canonical','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--baseline-root',type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,canonical,out=a.root.resolve(),a.canonical.resolve(),a.out.resolve();assert not out.exists()
    task=Path(__file__).resolve().parents[1];source=task/'test/support/task_pool_startup_native.c'
    receipt=task/'lib/xrt/backports/task_pool_idle_workers.json';provenance=json.loads(receipt.read_text())
    canonical_source=canonical/'src/concurrency/task_pool.c';expected=function(canonical_source)
    assert sha(canonical_source)==provenance['CanonicalSourceSha256']
    headers=[root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h']
    assert all(function(path)==expected for path in headers)
    assert hashlib.sha256(expected.encode()).hexdigest()==provenance['FunctionSha256']
    assert all(sha(root/row['Path'])==row['AfterSha256'] for row in provenance['Consumers'])
    cc=shutil.which('gcc');assert cc
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),source,receipt,canonical_source,
        root/'lib/xrt/xrt.snapshot.json',root/'lib/xruntime/xruntime.snapshot.json',*headers}
    if a.baseline_root:
        for row in provenance['Consumers']:
            original=a.baseline_root.resolve()/row['Path'];files.add(original)
            assert sha(original)==row['BeforeSha256']
            text=original.read_text(encoding='utf-8-sig');prior=function(original)
            assert text.count(prior)==1
            assert text.replace(prior,expected)==(root/row['Path']).read_text(encoding='utf-8-sig')
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    rows=[];report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Scope=__doc__,
        RuntimeInstrumented=a.sanitize,CanonicalFunctionExact=True,Full53Acceptance=False,ShippingLibraryAcceptance=False)
    report['ExactBackportAgainstBaseline']=a.baseline_root is not None
    out.mkdir();env=dict(os.environ)
    if a.sanitize:assert os.name!='nt';env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,expected_exit=0):
        start=time.monotonic();command=list(map(str,command))
        result=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=180);data=result.stdout+result.stderr
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert result.returncode==0 if expected_exit==0 else result.returncode!=0,(name,data[-5000:])
        return data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-g','-fno-omit-frame-pointer']
    if a.sanitize:flags+=['-fsanitize=address,undefined']
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    suffix='.exe' if os.name=='nt' else ''
    try:
        for core in (True,False):
            for opt in ('O0','O2'):
                name=('core' if core else 'runtime')+'.'+opt;binary=out/(name+suffix)
                run(name+'.build',[cc,*flags,'-'+opt,'-I'+str(root),*(['-DXL6_TEST_CORE_HEADER'] if core else []),source,*libs,'-o',binary])
                assert b'32 exact frozen graphs, active callback refused, balanced' in run(name,[binary])
        if a.baseline_root:
            binary=out/('baseline'+suffix)
            run('baseline.build',[cc,*flags,'-O0','-I'+str(a.baseline_root.resolve()),'-DXL6_TEST_CORE_HEADER',source,*libs,'-o',binary])
            data=run('baseline',[binary],1);assert b'ref.Ops->Count(ref.Data,&refs)' in data
            report['OriginalHeaderRejectedDeterministically']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report['Complete']=True;report['TaskPoolStartupAcceptance']=True
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('task-pool startup phases='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
