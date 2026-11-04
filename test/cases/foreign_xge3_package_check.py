"""Headless acceptance of the private declarative XGE/XUI package bootstrap.

Uses one verified full source compiler cohort, no shipping compiler fallback.
The accepted scope is package initialization, ABI policies and real native
DLL/.so residency; this does not claim full XGE/XUI API or GPU acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--fixture',type=Path,help='Public-import initialization fixture; default retains the private bootstrap gate')
    a=parser.parse_args()
    root,cohort,public,packages,out=[p.resolve() for p in
        (a.root,a.cohort_root,a.public_directory,a.packages,a.out)]
    sys.path.insert(0,str(root/'tools'))
    from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public)
    cc=shutil.which('gcc');assert cc
    driver=Path(build['Driver']);sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    native=packages/'_xge_native';engine=native/'native'/('xge.dll' if os.name=='nt' else 'libxge.so')
    fixtures=[root/'test/cases'/('foreign_xge3_package'+s) for s in
        ('.xl','_host.c','_native.c','_bind_failure.c')]
    if a.fixture:fixtures[0]=a.fixture.resolve()
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,*fixtures,
                   root/'test/cases/foreign_decl_module_host.c',
                   *[p for p in packages.rglob('*') if p.is_file()]])
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in pinned}
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort if os.name=='nt' else Path(build['NativeRoot'])),XLANG_PACKAGE_PATH=str(packages),
             TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(engine,out/engine.name)
    else:env['LD_LIBRARY_PATH']=str(engine.parent)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],networkTests=False,shippingDelivery=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,accepted=True,alternate_env=None):
        command=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        result=subprocess.run(list(map(str,command)),cwd=cohort,env=alternate_env or env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=result.returncode,
                                  seconds=round(time.monotonic()-start,3),accepted=accepted,logSha256=sha(log)))
        save();print(name,result.returncode,flush=True)
        assert (result.returncode==0)==accepted,log
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else [
        '-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine.parent)]
    includes=['-I'+str(p) for p in (cohort,sdk,sdk/'include',public/'include',native/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror',*includes]
    if os.name=='nt':common+=['-DXGE_DLL','-DXUI_DLL']
    provider=native/'src/runtime.c';suffix='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in
             ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    for opt in ('O0','O2'):
        exe=out/('native-'+opt+suffix)
        run('native-'+opt+'-build',[cc,*common,'-DXL_STATIC_EMBED','-'+opt,fixtures[2],provider,*objects,runtime,engine,*systems,'-o',exe])
        run('native-'+opt+'-run',[exe])
        exe=out/('bind-failure-'+opt+suffix)
        wrap=[] if os.name=='nt' else ['-Wl,--wrap=xgeHostXrtBind']
        run('bind-failure-'+opt+'-build',[cc,*common,'-DXL_STATIC_EMBED','-'+opt,fixtures[3],provider,*objects,runtime,engine,*wrap,*systems,'-o',exe])
        run('bind-failure-'+opt+'-run',[exe])
    source=fixtures[0];generated=out/'package.c'
    run('cgen',[driver,source,'cgen-default',generated])
    text=generated.read_text(encoding='utf-8-sig')
    if a.fixture:
        # The shared native singleton admits the same identity from each
        # public module import context. Repeating xge/xui adds no more calls.
        assert text.count('xl6Xge3RuntimeBindV1();')==2
        assert text.index('xl6Xge3RuntimeBindV1();')<text.index('xuiTableViewCellOutputCreate(&native_raw)')
    else:assert 'xuiWidgetCreate(' in text and 'xuiSetViewportSize(' in text
    assert 'xl6Xge3RuntimeBindV1(' in text and 'extern int xuiWidgetCreate(' not in text
    for mode in ('run','module','exe'):
        prefix=out/('package-'+mode)
        run('tcc-'+mode,[driver,source,mode,prefix])
        if mode=='exe':run('tcc-standalone',[prefix.with_suffix('.exe') if os.name=='nt' else prefix])
    warnings=['-Wno-'+s for s in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable')]
    for opt in ('O0','O2'):
        exe=out/('generated-'+opt+suffix)
        run('generated-'+opt+'-build',[cc,*common,*warnings,'-DXL_STATIC_EMBED','-'+opt,generated,provider,
            root/'test/cases/foreign_decl_module_host.c',*objects,runtime,engine,*systems,'-o',exe])
        run('generated-'+opt+'-run',[exe])
    host=out/('module-residency'+suffix)
    run('module-residency-build',[cc,*common,'-DXL_STATIC_EMBED','-O2',fixtures[1],*objects,runtime,*systems,'-o',host])
    tcc=Path(build['TccCore'])
    libraries=cohort/'res/tcc/win64/x86_64' if os.name=='nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
    run('module-residency-run',[host,source,cohort,public/'include',tcc,libraries,out/'residency-temp',sdk/'include',engine])
    # Corrupt only a fresh test SDK's host fingerprint. Native bind must fail
    # through the top-level language checked initializer, never assert/abort.
    rejected=out/'rejected-packages';shutil.copytree(packages,rejected)
    fingerprint=rejected/'_xge_native/include/xge_host_xrt_fingerprint.h'
    content=fingerprint.read_text(encoding='utf-8')
    marker='XGE_HOST_XRT_HEADER_SHA256 "';start=content.index(marker)+len(marker)
    content=content[:start]+('1' if content[start]=='0' else '0')+content[start+1:]
    fingerprint.write_text(content,encoding='utf-8')
    run('checked-init-rejection',[driver,source,'run',out/'rejected-init'],False,dict(env,XLANG_PACKAGE_PATH=str(rejected)))
    # The native engine fingerprint is independent from its XRT ABI. A
    # declaration SDK must reject an old/different same-name engine image.
    rejected_native=out/'rejected-native-packages';shutil.copytree(packages,rejected_native)
    config=rejected_native/'_xge_native/include/xge_build_config.h'
    content=config.read_text(encoding='utf-8');marker='XGE_BUILD_ABI_FINGERPRINT UINT64_C(0x';start=content.index(marker)+len(marker)
    config.write_text(content[:start]+('1' if content[start]=='0' else '0')+content[start+1:],encoding='utf-8')
    run('checked-native-init-rejection',[driver,source,'run',out/'rejected-native-init'],False,
        dict(env,XLANG_PACKAGE_PATH=str(rejected_native)))
    report['inputsUnchanged']=all(sha(p)==value for p,value in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()


if __name__=='__main__':main()
