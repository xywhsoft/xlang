"""CPU acceptance of real production xl/xlc profiles in an isolated directory.

Hosts link only one new production image, never SourceDriver implementation
objects. Compiler cases use header SDK copies, including required XRT implementation products and real embedded
catalogs, including double nesting and relocated standalone programs. The
runtime host checks static-provider selection, scripts, modules and recovery.
This does not install release files or qualify graphical/device execution.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','production-directory','runtime-headers','tcc-dist','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,production,headers,dist,out=[v.resolve() for v in
        (a.root,a.production_directory,a.runtime_headers,a.tcc_dist,a.out)]
    windows=os.name=='nt';cc=shutil.which('gcc');assert cc
    assert not out.exists() and not out.is_relative_to(root/'release')
    out.mkdir(parents=True);images=out/'release'/('win_x64' if windows else 'linux_x64');images.mkdir(parents=True)
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    names=('xl.dll','xlc.dll') if windows else ('libxl.so','libxlc.so')
    pins=[Path(__file__).resolve(),Path(cc)]
    for name in names:
        original=production/name;assert original.is_file();pins.append(original)
        shutil.copy2(original,images/name)
        if windows:shutil.copy2(original,out/name)
    for original in headers.rglob('*'):
        if original.is_file() and original.suffix in ('.h','.inc','.def'):
            target=out/'include'/original.relative_to(headers);target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(original,target);pins.append(original)
    sdk=root/'release'/('win_x64' if windows else 'linux_x64')
    for original in (sdk/'include').glob('*.h'):
        target=out/'include'/original.name;shutil.copy2(original,target);pins.append(original)
    assert not any(v.suffix in ('.c','.o') for v in (out/'include').rglob('*') if v.is_file())
    work=out/'empty-work';work.mkdir();(work/'build/test').mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();(relocated/'build/test').mkdir(parents=True)
    target_dist=work/'res/tcc/win64/x86_64' if windows else out/'build/linux/tcc-dist'
    shutil.copytree(dist,target_dist);pins.extend(v for v in dist.rglob('*') if v.is_file())
    # The diagnostic entry has public API calls only; implementation is solely
    # the selected production DLL. Linux only derives the current provider path.
    original=root/'test/cases'/('xlir_public_source_driver.c' if windows else 'xlir_public_source_linux_driver.c')
    text=original.read_text();pins.append(original)
    if not windows:
        assert '%s/release/libxlc.so' in text
        text=text.replace('%s/release/libxlc.so','%s/release/linux_x64/libxlc.so')
    entry=out/'public_driver.c';entry.write_text(text);pins.append(entry)
    runtime_source=out/'runtime_host.c'
    runtime_source.write_text(r'''#include "include/xl.h"
#include <stdio.h>
int main(int argc,char**argv){
 if(argc!=4)return 2;
 XL_ContextOptions options=xlDefaultContextOptions();options.workDir=argv[1];options.tempDir=argv[1];
 options.tccLibPath=argv[2];options.tccDllPath="__invalid_dynamic_provider_static_profile__";
 XL_Context* context=xlCreateContext(&options);if(!context)return 3;
 if(!xlAddIncludePath(context,argv[3]))return 4;
 if(!xlRunScriptString(context,"auto s=NativeSequence::<c_int>()\nassert s.length==0\nvar v=s\nassert (v as NativeSequence<c_int>).count()==0\n"))goto failed;
 XL_Module* module=xlCompileScriptStringToModule(context,"assert 6*7==42\n");
 if(!module||!xlRunModuleMain(context,module,NULL))goto failed;
 xlDestroyModule(module);
 if(xlRunScriptString(context,"assert missingProductionName==0\n"))return 5;
 if(!xlRunScriptString(context,"assert 20+22==42\n"))goto failed;
 xlDestroyContext(context);
 XL_RuntimeDrainResult result=xlRuntimeDrainRetiringChecked(0);
 if(!result.ready||!result.ok)return 6;
 puts("production runtime: PASS");return 0;
failed:fprintf(stderr,"%s\n",xlGetLastError(context));xlDestroyContext(context);return 7;
}
''');pins.append(runtime_source)
    inputs={str(v):sha(v) for v in pins}
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],
        profile='XL_BUILD_DLL + XL_STATIC_TCC; runtime also XL_RUNTIME_ONLY_DLL',
        linkedImplementationObjects=0,sourceFreeSdk=True,shippingDelivery=False,
        runtimeImages={name:sha(images/name) for name in names})
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    env.update(PYTHONDONTWRITEBYTECODE='1',TEMP=str(out),TMP=str(out),TMPDIR=str(out))
    if not windows:env['LD_LIBRARY_PATH']=str(images)
    def run(name,command,cwd=work,custom_env=None):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=custom_env or env,
            capture_output=True,timeout=360);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,log
        return r.stdout.decode(errors='replace')
    includes=['-I'+str(root),'-I'+str(sdk),'-I'+str(sdk/'include'),'-I'+str(out/'include')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(images)]
    suffix='.exe' if windows else ''
    platform_flags=[] if windows else ['-D_GNU_SOURCE']
    for level in ('O0','O2'):
        program=out/('runtime-'+level+suffix)
        run('runtime-'+level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+level,*platform_flags,*includes,
            runtime_source,images/names[0],*systems,'-o',program])
        run('runtime-'+level+'-run',[program,work,target_dist,out/'include'])
    driver=out/('public_source'+suffix)
    run('compiler-host-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',*platform_flags,*includes,
        entry,images/names[1],*systems,'-o',driver])
    if windows:
        imported=run('compiler-host-imports',['objdump','-p',driver])
        assert 'DLL Name: xlc.dll' in imported and 'DLL Name: xl.dll' not in imported
    compiler_env=dict(env,XL6_SOURCE_NATIVE_ROOT=str(work if windows else out),XL6_NESTED_SDK_WORK=str(work))
    cases=[]
    for name in ('stdlib_nested_sdk','stdlib_nested_aot','stdlib_string_bridge_boundaries','stdlib_tcc_embed','native_sequence'):
        source=root/'test/cases'/(name+'.xl');local=out/source.name;shutil.copy2(source,local)
        inputs[str(source)]=sha(source);cases.append((local,[]))
    inner='auto values=NativeSequence::<c_int>()\nassert values.length==0\nfor value in values\n throw "unexpected"\nend\n'
    middle='import xlang\nauto context=xlang.Context()\nassert context.run('+json.dumps(inner)+'), context.lastError\n'
    for name,text in (('nested-native-sequence',inner),('double-nested-compiler',middle)):
        source=out/(name+'.xl');source.write_text('import xlang\nauto context=xlang.Context()\nassert context.run('+json.dumps(text)+'), context.lastError\n')
        inputs[str(source)]=sha(source);cases.append((source,[]))
    for name in ('foreign_output_record_size',):
        source=root/'test/cases'/(name+'.xl');native=root/'test/cases'/(name+'_native.c');header=root/'test/cases'/(name+'.h')
        local=out/source.name;sidecar=out/native.name;target=out/'include/test/cases'/header.name;target.parent.mkdir(parents=True,exist_ok=True)
        for original,destination in ((source,local),(native,sidecar),(header,target)):
            shutil.copy2(original,destination);inputs[str(original)]=sha(original)
        cases.append((local,[sidecar]))
    for source,sidecars in cases:
        for mode in ('run','module','exe'):
            output=out/(source.stem+'-'+mode)
            run(source.stem+'-'+mode,[driver,source,mode,output,*sidecars],custom_env=compiler_env)
            if mode=='exe':
                program=Path(str(output)+suffix)
                dependencies=run(source.stem+'-dependencies',['objdump','-p',program] if windows else ['readelf','-d',program])
                assert not any(name in dependencies for name in names)
                local=relocated/program.name;shutil.copy2(program,local)
                standalone_env={k:v for k,v in env.items() if not k.startswith('XL6_') and k!='LD_LIBRARY_PATH'}
                run(source.stem+'-relocated',[local],relocated,standalone_env)
    report.update(complete=True,inputsUnchanged=all(sha(path)==digest for path,digest in inputs.items()))
    report['passed']=report['inputsUnchanged'];save();assert report['passed']

if __name__=='__main__':main()
