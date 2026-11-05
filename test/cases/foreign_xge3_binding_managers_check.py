"""Real public four-manager declarations, safe descriptor factories and checked OOM.

The emitted C uses native constructors/methods directly. GCC O0/O2 link the
full immutable source compiler objects and matched native shared-XRT engine.
The source budget helpers are test-only fault injection. No drawing/window,
actual clipboard, GPU, network or shipping AOT acceptance is claimed.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,cohort,public,packages,out=[v.resolve() for v in (a.root,a.cohort_root,a.public_directory,a.packages,a.out)]
    sys.path.insert(0,str(root/'tools'))
    from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    native=packages/'_xge_native';engine=native/'native'/('xge.dll' if os.name=='nt' else 'libxge.so')
    fixtures=[root/'test/cases'/name for name in ('foreign_xge3_binding_managers.xl','foreign_xge3_binding_managers.h','foreign_xge3_binding_managers_host.c')]
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,*fixtures,*[v for v in packages.rglob('*') if v.is_file()]])
    inputs={str(v):sha(v) for v in pinned};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],networkTests=False,gpuTests=False)
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort if os.name=='nt' else Path(build['NativeRoot'])),XLANG_PACKAGE_PATH=str(packages))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(engine,out/engine.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine.parent)+':'+env.get('LD_LIBRARY_PATH','')
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(list(map(str,command)),cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    includes=['-I'+str(v) for v in (cohort,root,root/'test/cases',public/'include',sdk,sdk/'include',native/'include')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    if os.name=='nt':flags+=['-DXGE_DLL','-DXUI_DLL']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    driver=Path(build['Driver']);generated=out/'managers.c'
    run('cgen',[driver,fixtures[0],'cgen-default',generated])
    run('ir',[driver,fixtures[0],'ir-lowered',out/'ir.json'])
    text=generated.read_text(encoding='utf-8-sig')
    for name in ('xuiMsgBoxCreate','xuiToastCreate','xuiMsgTipCreate','xuiToastShowOwned','xuiMsgTipSetCloseOwned','xuiMsgBoxGetBackdropRect','xuiToastGetIconRect','xuiToastGetCloseRect','xuiOpenFileDialog','xuiSaveFileDialog','xuiSelectFolderDialog'):
        assert name+'(' in text and 'extern int '+name+'(' not in text,name
    provider=native/'src/runtime.c'
    warnings=['-Wno-'+key for key in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable')]
    for opt in ('O0','O2'):
        host=out/('host-'+opt+('.exe' if os.name=='nt' else ''))
        run(opt+'-build',[cc,*flags,*warnings,'-'+opt,generated,provider,fixtures[2],*objects,runtime,engine,*systems,'-o',host])
        run(opt+'-run',[host])
    report['inputsUnchanged']=all(sha(v)==value for v,value in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
