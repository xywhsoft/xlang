"""Ordinary offline Executor native ABI/ownership prerequisite, not language API acceptance."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',default='gcc');a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    if out.exists():p.error('Preserve previous evidence; use a fresh output directory')
    out.mkdir(parents=True)
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    paths=[root/name for name in ('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'test/cases/xrt_executor_ownership_native.c','tools/check_stdlib_executor_native.py')]
    inputs={str(path):sha(path) for path in paths};rows=[];failures=[];logs=[];products=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Failures=failures,
        Products=products,NetworkTests=False,NetworkSecurityTests=False,
        Scope='native prerequisite only; no public language Executor/TaskPool completion claim')
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    save()
    try:
        for product in ('xrt','xruntime'):
            for opt in ('O0','O2'):
                name=product+'.'+opt;binary=out/(name+('.exe' if os.name=='nt' else ''))
                command=[a.compiler,'-Wall','-Werror','-'+opt,'-I'+str(root)]
                if Path(a.compiler).stem.lower()!='tcc':command+=['-std=c11','-Wextra']
                if product=='xruntime':command+=['-DXL6_EXECUTOR_TEST_RUNTIME']
                command += [str(root/'test/cases/xrt_executor_ownership_native.c'),'-o',str(binary)]
                if os.name!='nt':command+=['-pthread','-lm']
                for kind,args in (('build',command),('run',[str(binary)])):
                    log=out/(name+'.'+kind+'.log');start=time.monotonic()
                    with log.open('wb') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=120 if kind=='build' else 30)
                    digest=sha(log);logs.append((log,digest));rows.append(dict(Name=name+'.'+kind,Exit=result.returncode,Sha256=digest,Seconds=time.monotonic()-start));save()
                    if result.returncode:raise RuntimeError('Failed '+name+'.'+kind)
                    if kind=='run' and 'seven APIs, five physical nodes, six edges' not in log.read_text():raise RuntimeError('Incomplete proof '+name)
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
        report['InputsUnchanged']=all(sha(Path(path))==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(path)==digest for path,digest in logs)
        if not report['InputsUnchanged'] or not report['LogsUnchanged']:raise RuntimeError('Input/log drift')
        report['Complete']=True;save();print('Executor native prerequisite complete: actual XRT and xruntime, O0/O2; no network tests')
    except BaseException as error:
        failures.append(repr(error));report['InputsUnchanged']=all(sha(Path(path))==digest for path,digest in inputs.items());save();raise

if __name__=='__main__':main()
