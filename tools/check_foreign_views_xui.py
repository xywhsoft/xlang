"""Exercise private view controls with the real hosted XUI and host XRT."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--engine-root',type=Path,required=True)
    p.add_argument('--engine-build',type=Path,required=True)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args(); root,upstream,build,runtime,out=[v.resolve() for v in (a.root,a.engine_root,a.engine_build,a.runtime,a.out)]
    assert os.name=='nt'
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    metadata=json.loads((build/'build-report.json').read_text())
    assert metadata['xrt_mode']=='host'
    assert metadata['xrt_host_header_sha256']==hashlib.sha256((root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    out.mkdir(parents=True,exist_ok=False); (out/'tmp').mkdir()
    engine=build/'xge.dll'; cc=shutil.which('gcc'); assert cc
    sources=[root/'test/support/foreign_view_xui.c',root/'src/runtime/xl6_foreign_view.c',root/'src/runtime/xl6_foreign_callback_scope.c']
    protected=[Path(__file__).resolve(),runtime,engine,*sources,root/'src/runtime/xl6_foreign_view.h',
        root/'src/runtime/xl6_foreign_callback_scope.h',upstream/'include/xui.h',upstream/'include/xge_host_xrt.h',
        *sorted((build/'host-xrt').glob('*.h')),build/'host-xrt/xge_host_xrt_slots.inc',build/'xge_build_config.h']
    report=dict(complete=False,passed=False,networkTests=False,scope=__doc__,
        inputs={str(path):sha(path) for path in protected},rows=[])
    env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'))
    env['PATH']=os.pathsep.join([str(build),str(runtime.parent),env.get('PATH','')])
    def save(): (out/'results.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def run(name,command):
        start=time.monotonic(); result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=120)
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,exitCode=result.returncode,seconds=round(time.monotonic()-start,3),
            command=list(map(str,command)),logSha256=sha(log))); save(); print(name,result.returncode,flush=True)
        return result.returncode==0
    flags=['-std=c11','-g','-Wall','-Wextra','-Werror','-DXGE_DLL','-DXUI_DLL','-DXGE_XRT_HOST=1',
        '-DXRT_USE_SHARED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'src/runtime'),
        '-I'+str(upstream),'-I'+str(upstream/'include'),'-I'+str(build/'host-xrt'),
        '-include',str(build/'xge_build_config.h')]
    for opt in ('O0','O2'):
        exe=out/(opt+'.exe')
        if run(opt+'-build',[cc,*flags,'-'+opt,*sources,runtime,engine,'-o',exe]): run(opt+'-run',[exe])
    report['inputsUnchanged']=all(sha(Path(path))==digest for path,digest in report['inputs'].items())
    report['complete']=True; report['passed']=report['inputsUnchanged'] and all(r['exitCode']==0 for r in report['rows'])
    save(); assert report['passed'],out/'results.json'


if __name__=='__main__': main()
