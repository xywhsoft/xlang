"""Real native callback Drop failure delivery, with an explicit runtime source overlay.

The parent full source build is verified. If --runtime-overlay is supplied,
only xl6_native_callback.c is rebuilt, and that development qualification is
recorded rather than attributed to the unchanged parent source cohort.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime-overlay',action='store_true')
    a=p.parse_args();root,cohort,public,out=[v.resolve() for v in (a.root,a.cohort_root,a.public_directory,a.out)]
    sys.path.insert(0,str(root/'tools'));from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    source=root/'src/runtime/xl6_native_callback.c';fixture=root/'test/cases/foreign_xge3_package_callback_drop.c'
    pins=[Path(__file__).resolve(),fixture,Path(cc),runtime]
    if a.runtime_overlay:pins+=[source,root/'src/api/xl_foreign_error.h',root/'src/runtime/xl6_native_callback.h']
    pinned.update(pins);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in pinned};out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,runtimeOverlay=a.runtime_overlay,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        actual=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(list(map(str,actual)),capture_output=True,env=env,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    objects=[];callback=None
    for row in build['Objects']:
        name=Path(row['Source']).name
        if name in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):continue
        if a.runtime_overlay and name=='xl6_native_callback.c':callback=row;continue
        objects.append(row['Path'])
    if a.runtime_overlay:
        assert callback;overlay=out/'xl6_native_callback.o'
        flags=callback.get('Flags',build.get('CompileFlags'));assert flags
        flags=[x for x in flags if not x.startswith('-I')]+['-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include')]
        run('runtime-overlay-build',[cc,*flags,'-c',source,'-o',overlay]);objects.append(overlay)
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for level in ('O0','O2'):
        program=out/(level+('.exe' if os.name=='nt' else ''))
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+level,
            '-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include'),fixture,*objects,runtime,*systems,'-o',program])
        run(level+'-run',[program])
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()));save();assert report['passed']
if __name__=='__main__':main()
