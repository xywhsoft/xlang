"""Qualify declaration-driven byte prefixes without per-function adapters."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from full_compiler_qualification import verify_full_compiler


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    cohort=parser.add_mutually_exclusive_group(required=True)
    cohort.add_argument('--compiler-report',type=Path)
    cohort.add_argument('--diagnostic-report',type=Path)
    a=parser.parse_args();root,compiler,out=[p.resolve()for p in(a.root,a.compiler,a.out)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    build=(a.compiler_report or a.diagnostic_report).resolve();full=a.compiler_report is not None
    if full:
        verify_full_compiler(build,compiler);sdk=compiler.parent/'include';extra=[]
    else:
        m=json.loads(build.read_text());assert m['passed']and m['inputsUnchanged']and not m['fullCompilerBuild']
        assert compiler.parent==build.parent/'runtime'
        assert all(sha(p)==h for p,h in m['inputs'].items())
        parent=Path(m['parent']);assert sha(parent)==m['parentSha256']
        snapshot=Path(json.loads(parent.read_text())['source'])
        sdk=snapshot/'release'/('win_x64'if os.name=='nt'else'linux_x64')/'include'
        extra=[parent,*map(Path,m['inputs'])]
        for name,digest in m['overrides'].items():assert sha(root/name)==digest,name
        extra.extend(root/name for name in m['overrides'])
    fixture=root/'test/cases/foreign_span_prefix.xl';provider=fixture.with_name('foreign_span_prefix_native.c')
    nested=fixture.with_name('foreign_span_prefix_nested.xl')
    cc,cpp=[Path(shutil.which(v)).resolve()for v in('gcc','g++')]
    files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),build,compiler,cc,cpp,
        fixture,provider,fixture.with_suffix('.h'),nested,*extra]
    for name in ('foreign_nullable_span','foreign_span_variants'):
        files.extend(root/'test/cases'/(name+suffix)for suffix in('.xl','.h','_native.c'))
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if os.name=='nt'else('libxl.so','libxlc.so')))
    files.extend(p for p in sdk.rglob('*')if p.is_file());inputs={str(p):sha(p)for p in files}
    out.mkdir(parents=True,exist_ok=False);env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')}
    report=dict(passed=False,shippingDelivery=False,fullCompilerBuild=full,embeddedCompilerTested=full,scope=__doc__,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,reject=False):
        command=list(map(str,command));z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,expectedFailure=reject,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert bool(z.returncode)==reject,log
    include=['--inc-path',root,'--inc-path',sdk];generated=out/'generated.c';header=out/'public.h'
    run('emit',[compiler,'cgen',fixture,'--emit',generated,'--emit-header',header,*include])
    text=generated.read_text();assert 'int64_t limit ='in text and 'limit < -1'in text
    assert re.search(r'prefix_sum\([^;\n]*native_span_data_\d+[^;\n]*native_span_count_\d+\)',text)
    run('jit',[compiler,'run',fixture,provider,*include])
    binary=out/('standalone.exe'if os.name=='nt'else'standalone')
    run('static-build',[compiler,'build',fixture,provider,'-o',binary,*include]);run('static-run',[binary])
    client=out/'header.c';client.write_text('#include "public.h"\nint main(void){return 0;}\n')
    for language,standard,tool in [('c','c11',cc),('c++','c++11',cpp),('c++','c++17',cpp)]:
        run('header-'+standard,[tool,'-x',language,'-std='+standard,'-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(sdk),'-I'+str(out),'-c',client,'-o',out/(standard+'.o')])
    shared=out/'library.xl';shared.write_text(fixture.read_text().replace('func main(...) as int','func prefixBody() as int',1))
    library=out/('prefix.dll'if os.name=='nt'else'libprefix.so');sharedHeader=out/'shared.h'
    run('shared-build',[compiler,'build',shared,provider,'-shared','-o',library,'--emit-header',sharedHeader,*include])
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
 auto image=LoadLibraryA(argv[1]);assert(image);auto address=GetProcAddress(image,"xl6_export_prefixSelfTest");
#else
 auto image=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(image);auto address=dlsym(image,"xl6_export_prefixSelfTest");
#endif
 decltype(&xl6_export_prefixSelfTest) function=nullptr;
 static_assert(sizeof(function)==sizeof(address),"procedure width");std::memcpy(&function,&address,sizeof(function));
 assert(function && function()==195 && function()==195);
#if defined(_WIN32)
 int(*retire)(void)=nullptr;auto retirement=GetProcAddress(image,"xl_script_runtime_retire_checked");
 static_assert(sizeof(retire)==sizeof(retirement),"retirement width");std::memcpy(&retire,&retirement,sizeof(retire));
 assert(retire && retire());assert(FreeLibrary(image));
#else
 assert(dlclose(image)==0);
#endif
 return 0;}
''')
    executable=out/('shared-host.exe'if os.name=='nt'else'shared-host')
    run('shared-host-build',[cpp,'-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(sdk),host,*([]if os.name=='nt'else['-ldl']),'-o',executable])
    run('shared-host-run',[executable,library])
    inputs.update((str(p),sha(p))for p in(shared,host))
    prefix='extern "prefix_sum" @header("test/cases/foreign_span_prefix.h") @no_raise '
    bad={
        'backward':('@c_span(0,"const void*","size_t",0)','bytes data,int size=-1'),
        'nonadjacent':('@c_span(0,"const void*","size_t",2)','bytes data,int unused,int size=-1'),
        'missing':('@c_span(0,"const void*","size_t",1)','bytes data'),
        'unsigned':('@c_span(0,"const void*","size_t",1)','bytes data,uint64 size=0'),
        'c-int':('@c_span(0,"const void*","size_t",1)','bytes data,c_int size=0'),
        'nullable-int':('@c_span(0,"const void*","size_t",1)','bytes data,int? size=null'),
        'writable':('@c_span(0,"void*","size_t",1)','Buffer data,int size=-1'),
        'word-elements':('@c_span(0,"const uint32_t*","size_t",1)','bytes data,int size=-1'),
        'overlap':('@c_span(0,"const void*","size_t",1) @c_text_view(1)','bytes data,int size=-1'),
        'output-split':('@c_span(0,"const void*","size_t",1) @c_output(1,0)','bytes data,int size=-1'),
        'bad-index':('@c_span(0,"const void*","size_t",64)','bytes data,int size=-1'),
    }
    for name,(attrs,params)in bad.items():
        source=out/(name+'.xl');source.write_text(prefix+attrs+' func bad('+params+') as c_int\nfunc main(...) as int\n return 0\nend\n')
        run('reject-'+name,[compiler,'cgen',source,'--emit',out/(name+'.c'),*include],True)
    for name in ('foreign_nullable_span','foreign_span_variants'):
        case=root/'test/cases'/(name+'.xl');native=case.with_name(name+'_native.c')
        run('regression-'+name,[compiler,'run',case,native,*include])
    if full:
        run('nested-jit',[compiler,'run',nested,*include])
        host=out/('nested-host.exe'if os.name=='nt'else'nested-host')
        run('nested-build',[compiler,'build',nested,'-o',host,*include])
        run('nested-run',[host])
    report.update(passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
