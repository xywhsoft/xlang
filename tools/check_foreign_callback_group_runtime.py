"""Development proof of owned callback-group runtime over a full source cohort.

Only the native-callback and runtime-symbol objects are replaced, explicitly
and immutably. Compiler pack lowering, full VFS products and shipping SDK/IDE
installation are separate work. CPU-only lifecycle/trace/rollback/OOM tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import load_build
from compiler_response import command_with_response


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();base=load_build(build=a.build)
    cohort=base['cohort'];runtime=base['runtime'];win=os.name=='nt'
    cc=Path(shutil.which('gcc')).resolve();cpp=Path(shutil.which('g++')).resolve()
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    overrides=('src/runtime/xl6_native_callback.c','src/runtime/xl6_native_callback.h',
               'src/runtime/xl6_native_callback_group.inc','src/api/xl_api_symbols.c')
    files=[Path(__file__),Path(__file__).with_name('foreign_validation_build.py'),
           Path(__file__).with_name('compiler_response.py'),cc,cpp,root/'test/cases/foreign_callback_group_runtime.c',
           root/'src/compile/xl6_artifact_runtime_symbols.inc']
    assert (root/'src/compile/xl6_artifact_runtime_symbols.inc').read_text().count(
        'XL6_ARTIFACT_SYMBOL("xl6NativeCallbackGroupRuntimeV1Get", NATIVE_CALLBACK)')==1
    inputs=dict(base['inputs']);inputs.update((str(v),sha(v)) for v in files)
    out.mkdir(parents=True,exist_ok=False);overlay=out/'source'
    for name in overrides:
        path=overlay/name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,path)
        inputs[str(root/name)]=sha(root/name);inputs[str(path)]=sha(path)
    env=dict(os.environ,TMP=str(out),TEMP=str(out),TMPDIR=str(out))
    key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=str(runtime.parent)+os.pathsep+env.get(key,'')
    if win:shutil.copy2(runtime,out/runtime.name)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],fullCompilerBuild=False,
                compilerPackSupport=False,shippingDelivery=False,objectOverrides=[],networkTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));compiler=str(cpp) if command[0]==str(cpp) else str(cc)
        actual=command_with_response(command,compiler,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert r.returncode==0,log
    try:
        sdk=Path(base['base'].get('RuntimeDirectory',base['base'].get('SdkDirectory','')))
        inc=['-I'+str(v) for v in (overlay,overlay/'src/runtime',cohort,cohort/'src/runtime',cohort/'src/api',base['basePath'].parent/'include',sdk,sdk/'include')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DXL_STATIC_EMBED',*inc]
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        header=out/'header.h';header.write_text('#include "src/common/xl6_xrt.h"\n#include "src/runtime/xl6_native_callback.h"\n')
        for std,tool,lang in (('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')):
            run('header-'+std,[tool,'-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*inc,'-x',lang,'-fsyntax-only',header])
        records=base['base']['Objects']
        for opt in ('O0','O2'):
            replacement={}
            for name in ('xl6_native_callback','xl_api_symbols'):
                row=[v for v in records if Path(v['Source']).stem==name];assert len(row)==1
                source=overlay/('src/runtime/' if name=='xl6_native_callback' else 'src/api/')/(name+'.c')
                obj=out/(name+'-'+opt+'.o');run(name+'-'+opt,[cc,*flags,'-'+opt,'-c',source,'-o',obj])
                replacement[row[0]['Path']]=str(obj);inputs[str(obj)]=sha(obj)
                report['objectOverrides'].append(dict(parent=row[0]['Path'],path=str(obj),sha256=sha(obj),optimization=opt))
            objects=[replacement.get(v,v) for v in base['objects']]
            exe=out/('group-'+opt+('.exe' if win else ''))
            run(opt+'-build',[cc,*flags,'-'+opt,root/'test/cases/foreign_callback_group_runtime.c',*objects,runtime,*systems,'-o',exe])
            run(opt+'-run',[exe]);inputs[str(exe)]=sha(exe)
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
