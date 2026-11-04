"""Offline GCC native publisher / source compiler rootless callback acceptance.

The publisher owns process-global callback registrations until explicit clear;
this test does not pretend that unmodelled native globals are cycle-collected.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,cohort,public,out=[x.resolve() for x in (a.root,a.cohort_root,a.public_directory,a.out)]
    sys.path.insert(0,str(root/'tools'))
    from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    driver=Path(build['Driver']);sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    source=root/'test/cases/foreign_global_callback.xl';provider=root/'test/cases/foreign_global_callback_native.c'
    pinned.update([Path(__file__).resolve(),Path(cc),source,provider,root/'test/cases/foreign_global_callback.h',runtime,
                   root/'test/cases/foreign_decl_module_host.c',root/'src/runtime/xl6_callback_failure.h'])
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in pinned};out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],networkTests=False,tccPublisherAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(list(map(str,command)),cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    generated=out/'global.c';run('cgen',[driver,source,'cgen-default',generated])
    text=generated.read_text(encoding='utf-8-sig')
    assert 'global_callback_set(' in text and 'xl6NativeCallbackReportFailureV1' in text
    assert 'xl6ForeignScopeViewMakeV2' in text
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    includes=['-I'+str(x) for x in (root,cohort,public/'include',sdk,sdk/'include')]
    warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe' if os.name=='nt' else ''))
        run(opt+'-build',[cc,'-std=c11','-'+opt,'-Wall','-Wextra','-Werror',*warnings,'-DXL_STATIC_EMBED',*includes,
            generated,provider,root/'test/cases/foreign_decl_module_host.c',*objects,runtime,*systems,'-o',exe])
        run(opt+'-run',[exe])
    report['inputsUnchanged']=all(sha(p)==value for p,value in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
