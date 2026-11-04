"""Actual CPU-only Font/Texture/Image in-place declarations against shared-XRT XGE.

GCC O0/O2 clients use one pinned O2 engine and full source compiler. No GPU,
desktop, network, shipping-package, sanitizer or actual-XGE TCC acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','public-directory','xge-root','xge-build','out'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--fixture-root',type=Path)
    a=p.parse_args()
    root,public,native,engine,out=[x.resolve() for x in (a.root,a.public_directory,a.xge_root,a.xge_build,a.out)]
    fixture=a.fixture_root.resolve() if a.fixture_root else root
    build,pinned,_=verify_public_build(root,public)
    driver=Path(build['Driver']);cc=shutil.which('gcc');assert cc
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    metadata=json.loads((engine/'build-report.json').read_text(encoding='utf-8'))
    assert metadata['linked'] and metadata['xrt_mode']=='host'
    assert metadata['xrt_host_header_sha256']==hashlib.sha256((root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    source=fixture/'test/cases/foreign_xge_resources.xl'
    providers=[fixture/'test/cases/foreign_xge_resources_native.c',fixture/'test/cases/foreign_xui_declarative_native.c']
    font=native/'tests/xge/data/xge_opentype_fixture.ttf'
    library=engine/('xge.lib' if os.name=='nt' else 'libxge.so')
    dll=engine/('xge.dll' if os.name=='nt' else 'libxge.so')
    pinned.update([Path(__file__).resolve(),Path(cc),source,*providers,font,library,dll,
        fixture/'test/cases/foreign_xge_resources.h',fixture/'test/cases/foreign_xui_declarative.h',
        root/'test/cases/foreign_decl_module_host.c',engine/'build-report.json',engine/'xge_build_config.h'])
    pinned.update((native/'include').glob('*.h'))
    pinned.update(x for x in (engine/'host-xrt').iterdir() if x.is_file())
    inputs={str(path):sha(path) for path in pinned}
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    runtime=runtime_directory(root)/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    sdk=runtime_directory(root) if os.name=='nt' else Path(build['SdkDirectory'])
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root),XL6_TEST_FONT=str(font),
        TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(dll,out/dll.name)
    else:env['LD_LIBRARY_PATH']=str(engine)+':'+str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,sourceBuild=str(public/'build.json'),
        engineBuild=str(engine/'build-report.json'),networkTests=False,tccIncluded=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=command_with_response(command,cc,out/(name+'.rsp'))
        start=time.monotonic();result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,exitCode=result.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)))
        save();print(name,result.returncode,flush=True);assert not result.returncode,log
    generated=out/'actual-xge.c'
    run('cgen',[driver,source,'cgen-default',generated]);run('ir',[driver,source,'ir-lowered',out/'ir.json'])
    text=generated.read_text(encoding='utf-8-sig')
    for name in ('xgeFontLoad','xgeFontLoadMemory','xgeFontCreateSized','xgeTextureCreateRGBA',
                 'xgeTextureLoadMemory','xgeImageLoadMemory','xgeFontAdoptStorage','xgeTextureAdoptStorage'):
        assert name+'(' in text and 'extern int '+name+'(' not in text,name
    assert 'sizeof(xge_font_t)' in text and 'sizeof(xge_texture_t)' in text
    assert 'xgeFontCreateSized(native_raw, (xge_font)native_view_0.raw' in text
    includes=['-I'+str(path) for path in (fixture,root,public/'include',sdk,sdk/'include',native,native/'include',engine/'host-xrt')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-DXL_STATIC_EMBED',
        '-DXGE_XRT_HOST=1','-include',str(engine/'xge_build_config.h'),*includes]
    if os.name=='nt':common+=['-DXGE_DLL','-DXUI_DLL']
    run('emit-header',[driver,source,'emit',out/'public-header',*providers])
    run('header-ABI',[cc,*common,'-x','c','-fsyntax-only',out/'public-header.h'])
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
        ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else \
        ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine)]
    warnings=['-Wno-'+key for key in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable')]
    for opt in ('O0','O2'):
        executable=out/('actual-'+opt+('.exe' if os.name=='nt' else ''))
        run(opt+'-build',[cc,*common,*warnings,'-'+opt,generated,*providers,
            root/'test/cases/foreign_decl_module_host.c',*objects,runtime,library,*systems,'-o',executable])
        run(opt+'-run',[executable])
    report['inputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()

if __name__=='__main__':main()
