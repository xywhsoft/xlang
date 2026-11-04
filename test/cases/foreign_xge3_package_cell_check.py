"""CPU-only Cell bootstrap policy and exact native domain integration.

The complete source cohort is verified. An optional explicit runtime overlay
supports development kernels and is recorded separately from formal reuse.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','packages','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime-overlay',action='store_true',help='Development only: rebuild five runtime translation units')
    p.add_argument('--case',choices=('cell','leaf'),default='cell')
    a=p.parse_args();root,cohort,public,packages,out=[v.resolve() for v in (a.root,a.cohort_root,a.public_directory,a.packages,a.out)]
    sys.path.insert(0,str(root/'tools'));from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    names=('xl6_code_lease','xl6_foreign_context','xl6_ownership_plan','xl6_ownership_catalog','xl6_foreign_view')
    source=root/'test/cases'/('foreign_xge3_package_'+a.case+'.c');native=packages/'_xge_native'
    engine=native/'native'/('xge.dll' if os.name=='nt' else 'libxge.so')
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,source,*[path for path in native.rglob('*') if path.is_file()]])
    if a.runtime_overlay:pinned.update([*[root/'src/runtime'/(n+'.c') for n in names],root/'src/api/xl_foreign_error.h',
        *[path for path in (root/'src/runtime').glob('*.h')]])
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs={str(p):sha(p) for p in pinned}
    out.mkdir(parents=True,exist_ok=False)
    if a.runtime_overlay:
        headers=out/'sdk/__xl_runtime';headers.mkdir(parents=True)
        for path in (root/'src/runtime').glob('*.h'):shutil.copy2(path,headers/path.name)
        shutil.copy2(root/'src/api/xl_foreign_error.h',headers/'xl_foreign_error.h')
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(engine,out/engine.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[],runtimeOverlay=list(names) if a.runtime_overlay else [],
        sourceCohort=str(public/'build.json'),testCase=a.case)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        actual=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(list(map(str,actual)),capture_output=True,env=env,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    objects=[];overlays={}
    for row in build['Objects']:
        name=Path(row['Source']).stem
        if name in ('xlir_public_source_driver','xlir_public_source_linux_driver'):continue
        if a.runtime_overlay and name in names:overlays[name]=row;continue
        objects.append(row['Path'])
    include=['-I'+str(root if a.runtime_overlay else cohort),'-I'+str(sdk),'-I'+str(sdk/'include'),
        *(['-I'+str(out/'sdk')] if a.runtime_overlay else []),'-I'+str(public/'include'),'-I'+str(native/'include')]
    for name in names if a.runtime_overlay else ():
        row=overlays[name];flags=row.get('Flags',build.get('CompileFlags'));assert flags
        flags=[x for x in flags if not x.startswith('-I')]+include
        dest=out/(name+'.o');run(name+'-build',[cc,*flags,'-c',root/'src/runtime'/(name+'.c'),'-o',dest]);objects.append(dest)
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine.parent)]
    for level in ('O0','O2'):
        program=out/(level+('.exe' if os.name=='nt' else ''))
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+level,*include,source,native/'src/runtime.c',*objects,runtime,engine,*systems,'-o',program])
        run(level+'-run',[program])
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()));save();assert report['passed']
if __name__=='__main__':main()
