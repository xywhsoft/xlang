"""Actual public xui.Proxy.shapeText factory, with a permanent CPU host provider.

Generated-language C is executed against an explicit source-compiler overlay
and a matched Native SDK. This does not certify a shipping VFS/TCC provider,
Context/requested/cache APIs, a dynamically unloadable custom backend, or GPU.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
from compiler_response import command_with_response


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler-overlay','packages','engine-build','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,overlay_path,packages,engine,out=[value.resolve() for value in
        (args.root,args.compiler_overlay,args.packages,args.engine_build,args.out)]
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    overlay=json.loads(overlay_path.read_text(encoding='utf-8'))
    assert overlay['complete'] and overlay['passed']
    parent_path=Path(overlay['parentBuild']);assert sha(parent_path)==overlay['parentBuildSha256']
    parent=json.loads(parent_path.read_text(encoding='utf-8'))
    source=Path(overlay['sourceRoot']);driver=Path(overlay['Driver']);runtime=Path(overlay['RuntimeLibrary'])
    assert sha(driver)==overlay['DriverSha256']
    replacements={Path(row['Source']).name:row for row in overlay['Objects']}
    assert len(replacements)==len(overlay['Objects'])
    objects=[];used=set()
    for row in parent['Objects']:
        name=Path(row['Source']).name
        if name in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):continue
        if name in replacements:row=replacements[name];used.add(name)
        path=Path(row['Path']);assert sha(path)==row['Sha256'].lower();objects.append(path)
    assert used==replacements.keys()
    cc=shutil.which('gcc');assert cc
    windows=os.name=='nt';binary=engine/('xge.dll' if windows else 'libxge.so')
    library=engine/('xge.lib' if windows else 'libxge.so')
    native=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    assert native['linked'] and native['xrt_mode']=='host'
    sdk=Path(parent.get('SdkDirectory',source/'release/win_x64'))
    fixture=root/'test/cases/foreign_xge3_package_shape_factory.xl'
    sidecar=root/'test/cases/foreign_xge3_package_shape_factory_native.c'
    host=root/'test/cases/foreign_xge3_package_shape_factory_host.c'
    private=packages/'_xge_native';policy=packages/'shape-only-policy-qualification.json'
    assert policy.is_file(), 'This focused gate needs its explicit Shape-only policy candidate.'
    pinned={Path(__file__).resolve(),Path(cc),overlay_path,parent_path,driver,runtime,binary,library,
            engine/'build-report.json',engine/'xge_build_config.h',fixture,sidecar,host,
            fixture.with_suffix('.h'),policy,*objects}
    pinned.update(path for path in packages.rglob('*') if path.is_file())
    pinned.update(path for path in (source/'__xl_runtime').rglob('*') if path.is_file())
    inputs={str(path):sha(path) for path in pinned}
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages),XL6_SOURCE_NATIVE_ROOT=str(source),
             TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    if windows:
        shutil.copy2(runtime,out/runtime.name);shutil.copy2(binary,out/binary.name)
    else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,tccIncluded=False,
                compilerOverlay=str(overlay_path),nativeBuild=str(engine/'build-report.json'),
                inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=command_with_response(command,cc,out/(name+'.rsp'))
        start=time.monotonic();result=subprocess.run(list(map(str,command)),cwd=source,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)))
        save();print(name,result.returncode,flush=True);assert not result.returncode,log
    generated=out/'shape.c'
    run('cgen',[driver,fixture,'cgen-default',generated])
    run('ir',[driver,fixture,'ir-lowered',out/'ir.json'])
    emitted=generated.read_text(encoding='utf-8-sig')
    for name in ('xuiTextShapeCreateOwned','xl6ForeignResultProbeReserveRootV1','xl6ForeignResultProbeMaterializeRootV1'):
        assert name+'(' in emitted
    assert 'xl6ForeignViewCreateRootProbedV1(' not in emitted
    includes=['-I'+str(path) for path in (source,root,private/'include',sdk,sdk/'include')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1',*includes]
    flags+=['-Wno-'+name for name in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else \
        ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
    if windows:flags+=['-DXGE_DLL','-DXUI_DLL']
    for optimization in ('O0','O2'):
        executable=out/(optimization+('.exe' if windows else '.out'))
        run(optimization+'-build',[cc,*flags,'-'+optimization,generated,sidecar,private/'src/runtime.c',host,
                                  *objects,runtime,library,*systems,'-o',executable])
        run(optimization+'-run',[executable])
    report['inputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()


if __name__=='__main__':main()
