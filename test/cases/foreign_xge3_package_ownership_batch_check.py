"""Actual descriptor DLL lifetime through the entire ownership-plan Drop batch.

Uses a verified parent source build and an explicit four-source runtime
development overlay; it does not claim a new complete compiler cohort.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,cohort,public,out=[v.resolve() for v in (a.root,a.cohort_root,a.public_directory,a.out)]
    sys.path.insert(0,str(root/'tools'));from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    names=('xl6_code_lease','xl6_foreign_context','xl6_ownership_plan','xl6_ownership_catalog')
    sources=[root/'src/runtime'/(name+'.c') for name in names]
    fixtures=[root/'test/cases'/('foreign_xge3_package_ownership_batch'+suffix) for suffix in ('.h','_native.c','_host.c')]
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,*sources,*fixtures,
        *[root/'src/runtime'/(name+'.h') for name in names]])
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs={str(p):sha(p) for p in pinned}
    out.mkdir(parents=True,exist_ok=False);env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[],runtimeOverlay=list(names))
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
        if name in names:overlays[name]=row;continue
        objects.append(row['Path'])
    include=['-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include')]
    for name in names:
        row=overlays[name];flags=row.get('Flags',build.get('CompileFlags'));assert flags
        flags=[x for x in flags if not x.startswith('-I')]+include
        dest=out/(name+'.o');run(name+'-build',[cc,*flags,'-c',root/'src/runtime'/(name+'.c'),'-o',dest]);objects.append(dest)
    publisher=out/('batch-native.dll' if os.name=='nt' else 'libbatch-native.so')
    run('publisher-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-shared',*(['-fPIC'] if os.name!='nt' else []),*include,fixtures[1],'-o',publisher])
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for level in ('O0','O2'):
        program=out/(level+('.exe' if os.name=='nt' else ''))
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+level,*include,fixtures[2],*objects,runtime,*systems,'-o',program])
        run(level+'-run',[program,publisher])
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()));save();assert report['passed']
if __name__=='__main__':main()
