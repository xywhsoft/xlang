"""Direct owned SVG/cache/raster declarations against a pinned compiler and SDK.

CPU ownership/callback/thread/OOM cases run by default. --graphics additionally
requires one bounded OpenGL frame or headless software EGL, with actual pixels.
An explicit diagnostic cohort is never reported as a complete compiler build.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    group=p.add_mutually_exclusive_group(required=True)
    group.add_argument('--compiler-report',type=Path)
    group.add_argument('--diagnostic-report',type=Path)
    p.add_argument('--graphics',action='store_true')
    p.add_argument('--shared-client',action='store_true',help='Require package bootstrap support in a real shared-library C++ client')
    a=p.parse_args();root,compiler,dynamic,static,out=[v.resolve() for v in
        (a.root,a.compiler,a.dynamic_packages,a.static_packages,a.out)]
    partial=a.diagnostic_report is not None;cohort=(a.diagnostic_report or a.compiler_report).resolve()
    sdk,extra=load_compiler_cohort(root,compiler,cohort,partial)
    windows=os.name=='nt';sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    cc,cpp,objdump=[Path(shutil.which(v)).resolve() for v in ('gcc','g++','objdump')]
    cases=[root/'test/cases'/(name+'.xl') for name in
        ('foreign_xge3_svg','foreign_xge3_svg_invalid','foreign_xge3_svg_oom','foreign_xge3_svg_callbacks')]
    example=root/'lib/bindings/xge/examples/svg_documents.xl';cases.append(example)
    if a.graphics:cases.append(root/'test/cases/foreign_xge3_svg_gpu.xl')
    files=[Path(__file__).resolve(),Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),compiler,cohort,cc,cpp,objdump,*extra,*cases,
        example.parent/'data/vector.svg']
    for case in cases:
        files.extend(v for v in (case.with_suffix('.c'),case.with_suffix('.h')) if v.is_file())
    # OOM's helper includes this base implementation literally.
    files.append(root/'test/cases/foreign_xge3_svg.c')
    files.extend(compiler.parent/name for name in
        (('xl.dll','xlc.dll') if windows else ('libxl.so','libxlc.so')))
    for folder in (sdk,dynamic,static):files.extend(v for v in folder.rglob('*') if v.is_file())
    inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=not partial,
        actualGraphics=a.graphics,hardwarePerformance=False,networkTests=False,
        sharedLibraryAcceptance=a.shared_client,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,args,packages=None,cwd=None):
        env={k:v for k,v in os.environ.items() if not k.startswith('XL6_')}
        env.pop('XLANG_PACKAGE_PATH',None)
        if packages:env['XLANG_PACKAGE_PATH']=str(packages)
        if not windows:
            env.update(EGL_PLATFORM='surfaceless',LIBGL_ALWAYS_SOFTWARE='1')
            env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None)
        args=list(map(str,args));working=cwd or compiler.parent
        try:
            z=subprocess.run(args,cwd=working,env=env,capture_output=True,timeout=180)
            code=z.returncode;data=z.stdout+z.stderr
        except subprocess.TimeoutExpired as error:
            code='timeout';data=(error.stdout or b'')+(error.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=args,workingDirectory=str(working),
            packages=str(packages) if packages else None,exitCode=code,logSha256=sha(log)))
        save();print(name,code,flush=True);assert code==0,log
        return data
    include=['--inc-path',root,'--inc-path',sdk]
    publicIncludes=['-I'+str(v)for v in sorted(dynamic.glob('*/include')) if v.is_dir()]
    for case in cases:
        name=case.stem;working=example.parent if case==example else compiler.parent
        cfile,header=out/(name+'.c'),out/(name+'.h')
        run(name+'-emit',[compiler,'cgen',case,'--emit',cfile,'--emit-header',header,*include],dynamic,working)
        for kind,packages in (('jit',dynamic),('static',static)):
            if kind=='jit':command=[compiler,'run',case,*include]
            else:
                binary=out/(name+('.exe' if windows else ''))
                run(name+'-static-build',[compiler,'build',case,'-o',binary,*include],packages,working)
                command=[binary]
            data=run(name+'-'+kind,command,packages if kind=='jit' else None,working)
            if name=='foreign_xge3_svg':assert b'SVG scalar/tuple hot calls: zero XRT heap operations' in data
            if name=='foreign_xge3_svg_oom':
                counts=re.findall(rb'SVG language prefixes: (\d+) (\d+) (\d+)',data)
                assert len(counts)==2 and {int(k) for k,_,_ in counts}=={0,1}
                assert all(int(f)>0 and int(s)>0 and int(f)+int(s)==96 for _,f,s in counts)
                report.setdefault('oomPrefixes',{})[kind]=[dict(kind=int(k),failed=int(f),passed=int(s))for k,f,s in counts]
            if name=='foreign_xge3_svg_gpu':assert b'SVG RGBA/stride/direct draw and owned Texture close passed' in data
        if windows:
            data=run(name+'-imports',[objdump,'-p',binary]);imports=re.findall(rb'DLL Name:\s*(\S+)',data)
            assert imports and not {b'xge3_host.dll',b'xge.dll',b'xl.dll',b'xlc.dll',b'libtcc.dll'}.intersection(v.lower()for v in imports)
            report.setdefault('binaries',[]).append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,
                imports=[v.decode('ascii')for v in imports]))
        if name=='foreign_xge3_svg':
            generated=cfile.read_text();assert all(re.search(r'\b'+symbol+r'\(',generated) for symbol in
                ('xgeSvgCreate','xgeSvgGetSize','xgeSvgCacheCreate','xgeSvgCacheLoad','xgeSvgClone'))
            for standard,tool,language in [('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')]:
                run('header-'+standard,[tool,'-x',language,'-std='+standard,'-Wall','-Wextra','-Werror',
                    '-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(sdk),*publicIncludes,'-fsyntax-only',header])
    if not a.shared_client:
        report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()),
            remainingSharedLibraryGate='Explicit --shared-client; existing package bootstrap top-level call is rejected by shared builds')
        assert report['inputsUnchanged'];save();return
    # Use the exported root operation through a genuine foreign C++ client.
    # The library variant keeps main's test body uncalled; no shared-library
    # main entry or automatic fixture-provider discovery is assumed.
    librarySource=out/'svg_library.xl'
    librarySource.write_text(cases[0].read_text().replace('func main(...) as int','func unusedTestBody() as int',1))
    library=out/('svg.dll'if windows else'libsvg.so');header=out/'shared.h'
    run('shared-build',[compiler,'build',librarySource,cases[0].with_suffix('.c'),'-shared','-o',library,
        '--emit-header',header,*include],dynamic)
    for image in (dynamic/'_xge_native/native').glob('*.dll'if windows else'*.so'):
        shutil.copy2(image,out/image.name)
    host=out/'host.cpp';host.write_text('''#include "shared.h"
#include <cassert>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
int main(int argc,char** argv){assert(argc==2);
#if defined(_WIN32)
 auto image=LoadLibraryA(argv[1]);assert(image);auto address=GetProcAddress(image,"xl6_export_svgRootSelfTest");
#else
 auto image=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(image);auto address=dlsym(image,"xl6_export_svgRootSelfTest");
#endif
 decltype(&xl6_export_svgRootSelfTest) function=nullptr;
 static_assert(sizeof(function)==sizeof(address),"procedure width");std::memcpy(&function,&address,sizeof(function));
 assert(function && function()==16 && function()==16);
#if defined(_WIN32)
 int(*retire)(void)=nullptr;auto tail=GetProcAddress(image,"xl_script_runtime_retire_checked");
 static_assert(sizeof(retire)==sizeof(tail),"retire width");std::memcpy(&retire,&tail,sizeof(retire));
 assert(retire && retire());assert(FreeLibrary(image));
#else
 assert(dlclose(image)==0);
#endif
 return 0;}
''')
    executable=out/('shared-host.exe'if windows else'shared-host')
    run('shared-host-build',[cpp,'-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(sdk),*publicIncludes,host,*([]if windows else['-ldl']),'-o',executable])
    run('shared-host-run',[executable,library],cwd=out)
    inputs.update((str(v),sha(v))for v in (librarySource,host))
    report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
    assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
