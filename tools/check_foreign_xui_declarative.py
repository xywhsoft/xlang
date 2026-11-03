"""Offline actual XUI declarations using a pinned host-XRT engine and source core.

Headless native input/layout/IME callbacks only; no desktop input, GPU or
network testing. This gate does not accept the full public XGE/XUI package.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','xui-root','xui-build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,public,native,engine,out=[x.resolve() for x in (a.root,a.public_directory,a.xui_root,a.xui_build,a.out)]
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver']);cc=shutil.which('gcc');assert cc
    metadata=json.loads((engine/'build-report.json').read_text());assert metadata['xrt_mode']=='host' and metadata['linked']
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    assert metadata['xrt_host_header_sha256']==hashlib.sha256((root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    source=root/'test/cases/foreign_xui_declarative.xl';provider=root/'test/cases/foreign_xui_declarative_native.c'
    library=engine/('xge.lib' if os.name=='nt' else 'libxge.so');dll=engine/('xge.dll' if os.name=='nt' else 'libxge.so')
    pinned.update([Path(__file__).resolve(),Path(cc),source,provider,root/'test/cases/foreign_xui_declarative.h',root/'test/cases/foreign_decl_module_host.c',library,dll,engine/'build-report.json',engine/'xge_build_config.h'])
    pinned.update(x for x in (engine/'host-xrt').iterdir() if x.is_file());pinned.update((native/'include').glob('*.h'));pinned.add(native/'extlibs/xrt/xrt_config.h')
    inputs={str(p):sha(p) for p in pinned};out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    runtime=runtime_directory(root)/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    sdk=runtime_directory(root) if os.name=='nt' else Path(build['SdkDirectory'])
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root),TEMP=str(out/'tmp'),TMP=str(out/'tmp'))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(dll,out/dll.name)
    else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,sourceBuild=str(public/'build.json'),engineBuild=str(engine/'build-report.json'),tccIncluded=False,networkTests=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert not r.returncode,log
    generated=out/'actual-xui.c';run('cgen',[driver,source,'cgen-default',generated]);run('ir',[driver,source,'ir-lowered',out/'ir.json'])
    text=generated.read_text(encoding='utf-8-sig');assert 'declared C function ABI' in text and 'xl6ForeignScopeViewMakeV1' in text
    for symbol in ('xuiWidgetSetDefaultActionOwned','xuiWidgetSetCancelActionOwned','xuiWidgetSetImeCandidateRectOwned','xuiWidgetSetCursorQueryCallbackOwned','xuiWidgetSetContentMeasureCallbackOwned','xuiWidgetSetLayoutPrepareCallbackOwned','xuiWidgetSetLayoutChildrenCallbackOwned','xuiWidgetSetLayoutCompleteCallbackOwned','xuiWidgetSetEventCallbackOwned'):
        assert symbol+'(' in text and 'extern int '+symbol+'(' not in text,symbol
    includes=['-I'+str(x) for x in (root,public/'include',sdk,sdk/'include',native,native/'include',engine/'host-xrt')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXL_STATIC_EMBED','-DXGE_XRT_HOST=1','-include',str(engine/'xge_build_config.h'),*includes]
    if os.name=='nt':common+=['-DXGE_DLL','-DXUI_DLL']
    run('emit-header',[driver,source,'emit',out/'public-header',provider]);run('header-ABI',[cc,*common,'-x','c','-fsyntax-only',out/'public-header.h'])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
    warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable')]
    for opt in ('O0','O2'):
        exe=out/('actual-'+opt+('.exe' if os.name=='nt' else ''));run(opt+'-build',[cc,*common,*warnings,'-'+opt,generated,provider,root/'test/cases/foreign_decl_module_host.c',*objects,runtime,library,*systems,'-o',exe]);run(opt+'-run',[exe])
    report['inputsUnchanged']=all(sha(p)==v for p,v in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
