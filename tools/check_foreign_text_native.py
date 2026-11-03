"""Compile existing text-view generated C; no compiler frontend rebuild.

Sanitizers cover generated C and native fixture, not the reused host DLL/SO.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','generated','runtime','out'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--include',type=Path,action='append',default=[])
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,generated,runtime,out=[v.resolve() for v in (a.root,a.generated,a.runtime,a.out)]
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir();cc=shutil.which('gcc');assert cc
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    provider=root/'test/cases/foreign_text_view_native.c';host=root/'test/cases/foreign_decl_module_host.c'
    inputs={str(path):sha(path) for path in (Path(__file__).resolve(),generated,runtime,provider,host,root/'test/cases/foreign_text_view.h',Path(cc))}
    for directory in a.include:
        inputs.update((str(path),sha(path)) for path in directory.resolve().rglob('*.h'))
    report=dict(complete=False,passed=False,scope=__doc__,networkTests=False,sanitizers=a.sanitize,reusedCoreInstrumented=False,inputs=inputs,rows=[])
    env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    if a.sanitize:
        assert os.name!='nt';env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
        flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.sanitize else []
        command=[cc,'-std=c11','-'+opt,*flags,'-I'+str(root),*['-I'+str(path.resolve()) for path in a.include],generated,provider,host,runtime,
            *(['-Wl,-rpath,'+str(runtime.parent)] if os.name!='nt' else []),'-o',exe]
        for name,cmd in ((opt+'-build',command),(opt+'-run',[exe])):
            started=time.monotonic();r=subprocess.run(list(map(str,command_with_response(cmd,cc,out/(name+'.rsp')))),cwd=root,env=env,capture_output=True,timeout=180)
            log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
            report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
            (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(name,r.returncode,flush=True)
            assert r.returncode==0,log
    report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report['complete']=True;report['passed']=report['inputsUnchanged'];(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');assert report['passed']
if __name__=='__main__':main()
