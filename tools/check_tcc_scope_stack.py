"""Qualify TinyCC lexical stack reuse with the actual provider implementation.

CPU-only: scope lifetimes, spill/aggregate statement expressions, VLA, alloca,
cleanup/goto, repeated compiler states and a large sequential-block frame.
An optional frozen baseline inspects its oversized frame without executing it.
This gate does not install runtime products or replace full compiler testing.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--baseline-source',type=Path)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve()
    assert not out.is_relative_to(root);out.mkdir(parents=True,exist_ok=False)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    cc=Path(shutil.which('gcc')).resolve();fixture=root/'test/cases/tcc_scope_stack.c'
    inputs={str(f):sha(f) for f in (Path(__file__).resolve(),cc,fixture)}
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],programs=[],
        shippingDelivery=False,fullCompilerQualified=False)
    def save():
        tmp=out/'results.json.tmp';tmp.write_text(json.dumps(report,indent=2)+'\n','utf8');os.replace(tmp,out/'results.json')
    def run(name,command):
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=out,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log
        return z.stdout+z.stderr
    modes=[]
    if a.baseline_source:modes.append(('baseline',a.baseline_source.resolve()))
    modes.append(('fixed',root))
    try:
        for mode,origin in modes:
            source=out/mode;shutil.copytree(origin/'lib/tcc',source)
            for f in (origin/'lib/tcc').rglob('*'):
                if f.is_file():inputs[str(f)]=sha(f)
            body=source/'scope_stack_probe.c';shutil.copy2(fixture,body);inputs[str(body)]=sha(body)
            for opt in ('O0','O2'):
                binary=out/(mode+'-'+opt+('.exe' if os.name=='nt' else ''))
                command=[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source),body,'-o',binary]
                command+=([source/'tcc_utf8_io.c','-lws2_32'] if os.name=='nt' else ['-ldl','-lpthread','-lm'])
                run(mode+'-'+opt+'-build',command)
                data=run(mode+'-'+opt+'-run',[binary,source,'1' if mode=='fixed' else '0'])
                assert b'FUNCTIONAL_STATUS=0' in data and b'SCOPE_STACK_PROBE_OK' in data
                frame=int(re.search(rb'STACK_FRAME_BYTES=(\d+)',data)[1])
                assert (frame<8192 if mode=='fixed' else frame>2*1024*1024)
                report['programs'].append(dict(path=str(binary),sha256=sha(binary),frameBytes=frame,mode=mode,optimization=opt))
                save()
        report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
