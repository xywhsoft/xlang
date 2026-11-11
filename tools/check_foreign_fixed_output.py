"""Fixed native array outputs on a verified complete O2/TCC/XRT/VFS compiler.

CPU-only value, tuple, bound/protocol/field calls, failures and heap counters.
Generated headers and actual shared-library C++ clients are separate checks.
No window, GPU, network or security tests are run.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from full_compiler_qualification import verify_full_compiler


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out','objdump'):
        parser.add_argument('--'+name,type=Path,required=True)
    build=parser.add_mutually_exclusive_group(required=True)
    build.add_argument('--compiler-report',type=Path)
    build.add_argument('--diagnostic-report',type=Path,help='Explicit frontend-only smoke tests, never final compiler qualification')
    args=parser.parse_args();root,compiler,cohort,out,objdump=[p.resolve()for p in
        (args.root,args.compiler,args.compiler_report or args.diagnostic_report,args.out,args.objdump)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    full=args.compiler_report is not None
    sdk=compiler.parent/'include'
    if full:verify_full_compiler(cohort,compiler)
    else:
        diagnostic=json.loads(cohort.read_text());assert diagnostic['passed']and diagnostic['inputsUnchanged']and not diagnostic['fullCompilerBuild']
        assert all(sha(p)==h for p,h in diagnostic['inputs'].items())
        assert compiler.parent==cohort.parent/'runtime'
        parent=json.loads(Path(diagnostic['parent']).read_text())
        sdk=Path(parent['source'])/'release'/('win_x64'if os.name=='nt'else'linux_x64')/'include'
    source=root/'test/cases/foreign_fixed_output.xl';native=root/'test/cases/foreign_fixed_output.h'
    nested=root/'test/cases/foreign_fixed_output_nested.xl'
    regressions={
        'foreign_output_values':'foreign_output_values_native.c',
        'foreign_output_record':'foreign_output_record_native.c',
        'foreign_fixed_ref':'foreign_region_native.c'}
    cc=Path(shutil.which('gcc')).resolve();cpp=Path(shutil.which('g++')).resolve()
    files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),compiler,cohort,source,native,nested,objdump,cc,cpp]
    files.extend(compiler.parent/p for p in (('xl.dll','xlc.dll')if os.name=='nt'else('libxl.so','libxlc.so')))
    files.extend(p for p in sdk.rglob('*')if p.is_file())
    files.extend(root/'test/cases'/(name+'.xl')for name in regressions)
    providers=(*regressions.values(),'foreign_status_native.c','foreign_native_fields_native.c',
        'foreign_scoped_view_native.c','foreign_managed_view_native.c')
    files.extend(root/'test/cases'/name for name in providers)
    files.extend(root/'test/cases'/name for name in ('foreign_output_values.h','foreign_output_record.h',
        'foreign_region.h','foreign_status.h','foreign_native_fields.h','foreign_scoped_view.h',
        'foreign_managed_view.h','foreign_expanded_calls.h'))
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,XLANG_FIXED_OUTPUT_ROOT=str(root));env.pop('XLANG_PACKAGE_PATH',None)
    if os.name=='nt':env['PATH']=str(compiler.parent)+';'+env.get('PATH','')
    else:env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(passed=False,fullCompilerBuild=full,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,reject=False,working=None):
        command=list(map(str,command));working=working or compiler.parent
        r=subprocess.run(command,cwd=working,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,cwd=str(working),exitCode=r.returncode,expectedFailure=reject,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert bool(r.returncode)==reject,log
        return r.stdout+r.stderr
    include=['--inc-path',root,'--inc-path',sdk]
    c=out/'generated.c';h=out/'public.h'
    run('emit',[compiler,'cgen',source,'--emit',c,'--emit-header',h,*include])
    text=c.read_text();assert 'native_raw.Elements' in text and 'native_raw.e0.Elements' in text and 'native_raw.e2.Elements'in text
    assert 'FixedVertex*'in text and 'fixed_vertices('in text
    run('jit',[compiler,'run',source,*include])
    binary=out/('standalone.exe'if os.name=='nt'else'standalone')
    run('static-build',[compiler,'build',source,'-o',binary,*include]);run('static-run',[binary])
    imports=run('static-imports',[objdump,'-p',binary]).decode(errors='replace').lower()
    for forbidden in ('xl.dll','xlc.dll','libxl.so','libxlc.so','libtcc.dll','libtcc.so'):assert forbidden not in imports,forbidden
    system=['-ldl']if os.name!='nt'else[]
    for standard in ('c11','c++11','c++17'):
        compilerC=cpp if standard.startswith('c++')else cc;language='c++'if standard.startswith('c++')else'c'
        run('header-'+standard,[compilerC,'-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(sdk),'-x',language,'-fsyntax-only',h])
    library=out/('values.dll'if os.name=='nt'else'libvalues.so');header=out/'shared.h'
    librarySource=out/'library.xl'
    libraryCode=source.read_text().replace('func main(...) as int','func fixedOutputBody() as int',1).replace('assert main()==0','assert fixedOutputBody()==0',1)
    librarySource.write_text(libraryCode);inputs[str(librarySource)]=sha(librarySource)
    run('shared-build',[compiler,'build',librarySource,'-shared','-o',library,'--emit-header',header,*include])
    host=out/'host.cpp';host.write_text('''#define XL6_TEST_FIXED_OUTPUT_DECL_ONLY
#include "shared.h"
#include <cassert>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
int main(int argc,char** argv){assert(argc==2);
#if defined(_WIN32)
 auto image=LoadLibraryA(argv[1]);assert(image);auto fn=(decltype(&xl6_export_fixedOutputSelfTest))GetProcAddress(image,"xl6_export_fixedOutputSelfTest");
#else
 auto image=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(image);auto fn=(decltype(&xl6_export_fixedOutputSelfTest))dlsym(image,"xl6_export_fixedOutputSelfTest");
#endif
 assert(fn && fn()==73 && fn()==73);
#if defined(_WIN32)
 int(*retire)(void)=nullptr;auto retireAddress=GetProcAddress(image,"xl_script_runtime_retire_checked");
 static_assert(sizeof(retire)==sizeof(retireAddress),"Windows procedure width");
 std::memcpy(&retire,&retireAddress,sizeof(retire));
 assert(retire && retire());
 assert(FreeLibrary(image));
#else
 assert(dlclose(image)==0);
#endif
 return 0;}
''')
    executable=out/('shared-host.exe'if os.name=='nt'else'shared-host')
    run('shared-host-build',[cpp,'-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(sdk),host,*system,'-o',executable])
    raw=run('shared-host-run',[executable,library]);assert raw.count(b'Fixed outputs: exact arrays')==2
    original=source.read_text();old='func vertices(c_int code=0) as Quad';assert old in original
    negatives={name:original.replace(old,'func vertices(c_int code=0) as '+value,1)for name,value in
        [('dynamic','array<Vertex>'),('pointer','point[4]'),('managed','string[4]'),('nullable','Quad?'),('nested','Vertex[4][4]'),('zero','Vertex[0]'),('overflow','Vertex[4294967296]')]}
    negatives['projected']=original.replace('struct Vertex @header("test/cases/foreign_fixed_output.h")','struct Vertex @header("test/cases/foreign_fixed_output.h") @c_projection()',1)
    negatives['raw-array-return']=original.replace('@no_raise @c_output(1,0) func vertices(c_int code=0)','@no_raise func vertices(c_int code=0)',1)
    for name,code in negatives.items():
        path=out/(name+'.xl');path.write_text(code);target=out/(name+'.c')
        raw=run('reject-'+name,[compiler,'cgen',path,'--emit',target,*include],True)
        assert (b'[sem]'in raw or b'[parse]'in raw) and not target.exists()
    prefix=native.read_text().split('#if defined(XL6_TEST_FIXED_OUTPUT_DECL_ONLY)')[0]
    declarations='''int fixed_vertices(int,POINTEE);
void fixed_ints(int*);int fixed_mixed(FixedVertex*,int*,float*);
int fixed_span(const uint8_t*,int,FixedVertex*,int);int fixed_method(FixedRoot*,FixedVertex*);
int fixed_root(FixedRoot**);void fixed_root_free(FixedRoot*);
unsigned fixed_calls(void);void fixed_counters_begin(void);void fixed_counters_end(void);
#ifdef __cplusplus
}
#endif
#endif
'''
    # Keep the owning descriptor declaration, while eliminating implementation
    # bodies so a failure must come from emitted native prototype proofs.
    declarations='extern const XL6_ForeignViewDescriptorV1 fixedRootPolicy;\n'+declarations
    for name,pointee in [('const','const FixedVertex*'),('pointer-to-array','FixedVertex (*)[4]'),('element','int*')]:
        folder=out/('wrong-'+name);path=folder/'test/cases/foreign_fixed_output.h';path.parent.mkdir(parents=True)
        path.write_text(prefix+declarations.replace('POINTEE',pointee))
        raw=run('reject-header-'+name,[cc,'-std=c11','-I'+str(folder),'-I'+str(root),'-I'+str(sdk),'-fsyntax-only',c],True)
        assert b'declared C function ABI'in raw
    run('nested-jit',[compiler,'run',nested,*include],working=root)
    nestedBinary=out/('nested.exe'if os.name=='nt'else'nested')
    run('nested-build',[compiler,'build',nested,'-o',nestedBinary,*include])
    raw=run('nested-run',[nestedBinary],working=root);assert b'Nested fixed array outputs passed'in raw
    imports=run('nested-imports',[objdump,'-p',nestedBinary]).decode(errors='replace').lower()
    for forbidden in ('xl.dll','xlc.dll','libxl.so','libxlc.so','libtcc.dll','libtcc.so'):assert forbidden not in imports,forbidden
    for name,provider in regressions.items():
        fixture=root/'test/cases'/(name+'.xl');nativeSource=root/'test/cases'/provider
        run(name+'-jit',[compiler,'run',fixture,nativeSource,*include])
        binary=out/(name+('.exe'if os.name=='nt'else''))
        run(name+'-build',[compiler,'build',fixture,nativeSource,'-o',binary,*include])
        run(name+'-run',[binary])
    report.update(passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
