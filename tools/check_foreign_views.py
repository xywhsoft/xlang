"""Batch private native-view lifetime/graph checks without enabling language syntax."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--cc',default='gcc')
    p.add_argument('--sanitize',action='store_true')
    p.add_argument('--captures',action='store_true',help='test graph capture schema and typed closures')
    a=p.parse_args(); root,runtime,out=[x.resolve() for x in (a.root,a.runtime,a.out)]
    cc=shutil.which(a.cc); assert cc
    out.mkdir(parents=True,exist_ok=False); (out/'tmp').mkdir()
    sources=[root/('test/support/foreign_view_capture.c' if a.captures else 'test/support/foreign_view.c'),root/'src/runtime/xl6_foreign_view.c',
        root/'src/runtime/xl6_foreign_callback_scope.c']
    if a.captures:
        sources += [root/('src/runtime/'+name+'.c') for name in ('xl6_native_capture_graph','xl6_foreign_view_capture','xl6_capture_v3','xl6_closure_capture_v4')]
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in [Path(__file__).resolve(),runtime,*sources,root/'test/support/foreign_view_native_fixture.h',
        *sorted((root/'src/runtime').glob('*.h')),*sorted((root/'src/common').glob('*.h')),
        *sorted((root/'src/api').glob('*.h')),*sorted((root/'release/include').glob('*.h'))]}
    env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    env['PATH']=str(runtime.parent)+os.pathsep+env.get('PATH','')
    if a.sanitize:
        assert os.name!='nt'
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    report=dict(complete=False,passed=False,scope=__doc__,captureSchema=a.captures,networkTests=False,
        sourceSanitizers=a.sanitize,sharedRuntimeInstrumented=False,compiler=cc,compilerSha256=sha(Path(cc)),
        inputs=inputs,rows=[])
    def save(): (out/'results.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def run(name,command):
        started=time.monotonic()
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=result.returncode,
            seconds=round(time.monotonic()-started,3),logSha256=sha(log))); save()
        print(name,result.returncode,flush=True)
        return result.returncode==0
    flags=['-std=c11','-g','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'src/runtime')]
    if os.name!='nt': flags+=['-Wl,-rpath,'+str(runtime.parent)]
    if a.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
        if run(opt+'-build',[cc,*flags,'-'+opt,*sources,runtime,'-o',exe]):
            cases=('layout','sparse','metadata','cleanup','invoke','cycle','allocation','all') if a.captures else ('views','residency','affinity','retry','graph','allocation','exclusive','admission','all')
            for case in cases:
                run(opt+'-'+case,[exe,*([] if case=='all' else [case])])
    report['inputsUnchanged']=all(sha(Path(path))==value for path,value in inputs.items())
    report['complete']=True
    report['passed']=report['inputsUnchanged'] and all(row['exitCode']==0 for row in report['rows'])
    save(); assert report['passed'],out/'results.json'


if __name__=='__main__': main()
