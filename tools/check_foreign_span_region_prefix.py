"""Direct byte-prefix input and strided Buffer output in one native call."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'):parser.add_argument('--'+name,type=Path,required=True)
    cohort=parser.add_mutually_exclusive_group(required=True)
    cohort.add_argument('--compiler-report',type=Path);cohort.add_argument('--diagnostic-report',type=Path)
    a=parser.parse_args();root,compiler,out=[p.resolve()for p in(a.root,a.compiler,a.out)]
    partial=a.diagnostic_report is not None;build=(a.diagnostic_report or a.compiler_report).resolve()
    sdk,extra=load_compiler_cohort(root,compiler,build,partial)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();tools=Path(__file__).parent
    fixture=root/'test/cases/foreign_span_region_prefix.xl';provider=fixture.with_name('foreign_span_region_prefix_native.c')
    nested=fixture.with_name('foreign_span_region_prefix_nested.xl')
    cc,cpp=[Path(shutil.which(v)).resolve()for v in('gcc','g++')]
    files=[Path(__file__).resolve(),tools/'foreign_compiler_cohort.py',tools/'full_compiler_qualification.py',build,
        compiler,cc,cpp,fixture,provider,fixture.with_suffix('.h'),nested,*extra]
    for name in ('foreign_region','foreign_span_prefix'):
        files.extend(root/'test/cases'/(name+suffix)for suffix in('.xl','.h','_native.c'))
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if os.name=='nt'else('libxl.so','libxlc.so')))
    files.extend(p for p in sdk.rglob('*')if p.is_file());inputs={str(p):sha(p)for p in files}
    out.mkdir(parents=True,exist_ok=False);env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')}
    env['XLANG_SPAN_REGION_ROOT']=str(root)
    report=dict(passed=False,shippingDelivery=False,fullCompilerBuild=not partial,embeddedCompilerTested=not partial,
        scope=__doc__,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,reject=False):
        command=list(map(str,command));z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedFailure=reject,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert bool(z.returncode)==reject,log
    include=['--inc-path',root,'--inc-path',sdk];header=out/'public.h';generated=out/'generated.c'
    run('emit',[compiler,'cgen',fixture,'--emit',generated,'--emit-header',header,*include])
    text=generated.read_text();assert 'int64_t limit ='in text and 'native_region_'in text
    run('jit',[compiler,'run',fixture,provider,*include]);binary=out/('standalone.exe'if os.name=='nt'else'standalone')
    run('static-build',[compiler,'build',fixture,provider,'-o',binary,*include]);run('static-run',[binary])
    for standard,tool,language in [('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')]:
        run('header-'+standard,[tool,'-x',language,'-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
            '-I'+str(root),'-I'+str(sdk),'-fsyntax-only',header])
    librarySource=out/'library.xl';librarySource.write_text(fixture.read_text().replace('func main(...) as int','func regionBody() as int',1))
    library=out/('region.dll'if os.name=='nt'else'libregion.so');public=out/'shared.h'
    run('shared-build',[compiler,'build',librarySource,provider,'-shared','-o',library,'--emit-header',public,*include])
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
 auto image=LoadLibraryA(argv[1]);assert(image);auto address=GetProcAddress(image,"xl6_export_prefixRegionSelfTest");
#else
 auto image=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(image);auto address=dlsym(image,"xl6_export_prefixRegionSelfTest");
#endif
 decltype(&xl6_export_prefixRegionSelfTest) function=nullptr;
 static_assert(sizeof(function)==sizeof(address),"procedure width");std::memcpy(&function,&address,sizeof(function));
 assert(function && function()==195 && function()==195);
#if defined(_WIN32)
 int(*retire)(void)=nullptr;auto address2=GetProcAddress(image,"xl_script_runtime_retire_checked");
 static_assert(sizeof(retire)==sizeof(address2),"retirement width");std::memcpy(&retire,&address2,sizeof(retire));
 assert(retire && retire());assert(FreeLibrary(image));
#else
 assert(dlclose(image)==0);
#endif
 return 0;}
''')
    executable=out/('shared-host.exe'if os.name=='nt'else'shared-host')
    run('shared-host-build',[cpp,'-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(sdk),host,*([]if os.name=='nt'else['-ldl']),'-o',executable])
    run('shared-host-run',[executable,library]);inputs.update((str(p),sha(p))for p in(librarySource,host))
    negatives={'count-selector':'@c_region(4,"write","1","3","5",1)',
        'pointer-selector':'@c_region(4,"write","0","3","5",1)',
        'count-region':'@c_region(1,"write","2","3","5",1)',
        'span-overlap':'@c_region(0,"write","2","3","5",1)',
        'scalar-field':'@c_region(4,"write","2.width","3","5",1)'}
    for name,region in negatives.items():
        source=out/(name+'.xl');source.write_text('extern "prefix_region_render" @header("test/cases/foreign_span_region_prefix.h") @no_raise @c_span(0,"const uint8_t*","int",1) '+region+' func bad(bytes data,int size,c_int width,c_int height,Buffer output,c_int stride) as c_int\nfunc main(...) as int\n return 0\nend\n')
        inputs[str(source)]=sha(source);run('reject-'+name,[compiler,'cgen',source,'--emit',out/(name+'.c'),*include],True)
    for name in ('foreign_region','foreign_span_prefix'):
        case=root/'test/cases'/(name+'.xl');run('regression-'+name,[compiler,'run',case,case.with_name(name+'_native.c'),*include])
    if not partial:
        run('nested-jit',[compiler,'run',nested,*include]);host=out/('nested-host.exe'if os.name=='nt'else'nested-host')
        run('nested-build',[compiler,'build',nested,'-o',host,*include]);run('nested-run',[host])
    report.update(passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
