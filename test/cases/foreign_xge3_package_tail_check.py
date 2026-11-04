"""Actual native publisher DLL return tail versus explicit host quiescence.

A joined native thread, rather than an arbitrary safe boundary, establishes
that the DLL has physically returned before checked retirement may unload it.
No networking, desktop input or graphics device is used.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--tcc-provider',type=Path,help='Explicit independently verified provider overlay; does not alter the parent cohort.')
    a=p.parse_args();root,cohort,public,out=[x.resolve() for x in (a.root,a.cohort_root,a.public_directory,a.out)]
    sys.path.insert(0,str(root/'tools'))
    from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    fixtures=[root/'test/cases'/('foreign_xge3_package_tail'+suffix) for suffix in ('.h','.xl','_host.c','_native.c')]
    provider=a.tcc_provider.resolve() if a.tcc_provider else Path(build.get('TccCore',runtime))
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,provider,*fixtures]);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in pinned};out.mkdir(parents=True,exist_ok=False)
    package=out/'packages/foreign_tail';(package/'include').mkdir(parents=True);(package/'native').mkdir()
    shutil.copy2(fixtures[0],package/'include'/fixtures[0].name)
    (package/'empty.xl').write_text('// Native publisher fixture metadata.\n',encoding='utf-8')
    binary=package/'native'/('tail_native.dll' if os.name=='nt' else 'libtail_native.so')
    manifest=dict(format='xlang-package-1',name='foreign_tail',version='1.0.0',abiVersion='2',kind='xlang-extlib',
                  artifact=dict(kind='source',cSources=[],xlSources=['empty.xl'],includeDirs=['include'],xrtModules=[]),
                  links=['native/'+binary.name],loadlibs=['native/'+binary.name],dependencies=[])
    (package/'xlang.package.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort),XLANG_PACKAGE_PATH=str(package.parent))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(binary.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],networkTests=False,
                providerOverlay=str(provider) if a.tcc_provider else None,parentBuild=str(public/'build.json'))
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(list(map(str,command)),cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    include=['-I'+str(x) for x in (cohort,public/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror',*include]
    run('publisher-build',[cc,*common,'-O2','-shared',*(['-fPIC'] if os.name!='nt' else []),fixtures[3],'-o',binary])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    host=out/('tail-host.exe' if os.name=='nt' else 'tail-host')
    run('host-build',[cc,*common,'-DXL_STATIC_EMBED','-O2',fixtures[2],*objects,runtime,*systems,'-o',host])
    tcc=provider
    libraries=cohort/('res/tcc/win64/x86_64' if os.name=='nt' else 'res/tcc/linux/x86_64')
    run('host-run',[host,fixtures[1],cohort,public/'include',tcc,libraries,out/'jit-temp',sdk/'include',binary])
    report['inputsUnchanged']=all(sha(p)==value for p,value in inputs.items());assert report['inputsUnchanged']
    report['publisherSha256']=sha(binary);report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
