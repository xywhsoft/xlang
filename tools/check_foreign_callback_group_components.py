"""Verify actual callback/group component and ownership-domain selection.

Supplemental source dependency-solver proof only. A full compiler, emitted
native artifacts, linker pruning and pack syntax are separate gates.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import load_build


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(build=a.build)
    cohort=b['cohort'];runtime=b['runtime'];win=os.name=='nt';cc=Path(shutil.which('gcc')).resolve()
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    out.mkdir(parents=True,exist_ok=False);overlay=out/'source'
    inputs=dict(b['inputs'])
    for name in ('src/compile/xl6_compile_unit.c','src/compile/xl6_artifact_runtime.h',
                 'src/compile/xl6_artifact_runtime_symbols.inc','src/runtime/xl6_ownership_domains.inc'):
        path=overlay/name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,path)
        inputs[str(root/name)]=sha(root/name);inputs[str(path)]=sha(path)
    fixture=root/'test/cases/foreign_callback_group_components.c'
    for path in (fixture,cc,Path(__file__),Path(__file__).with_name('foreign_validation_build.py'),Path(__file__).with_name('compiler_response.py')):inputs[str(path)]=sha(path)
    env=dict(os.environ,TMP=str(out),TEMP=str(out),TMPDIR=str(out))
    key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=str(runtime.parent)+os.pathsep+env.get(key,'')
    if win:shutil.copy2(runtime,out/runtime.name)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],fullCompilerBuild=False,emittedArtifactPruning=False,networkTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));actual=command_with_response(command,str(cc),out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
    try:
        sdk=Path(b['base'].get('RuntimeDirectory',b['base'].get('SdkDirectory','')))
        flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-parameter','-Wno-pointer-sign','-DXL_STATIC_EMBED']
        flags+=['-Wno-'+v for v in ('missing-field-initializers','unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','format-truncation')]
        flags+=['-I'+str(v) for v in (overlay,cohort,cohort/'src/compile',cohort/'src/runtime',b['basePath'].parent/'include',sdk,sdk/'include')]
        parent=[v for v in b['base']['Objects'] if Path(v['Source']).name=='xl6_compile_unit.c'];assert len(parent)==1
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0','O2'):
            obj=out/('unit-'+opt+'.o');run(opt+'-unit',[cc,*flags,'-'+opt,'-c',overlay/'src/compile/xl6_compile_unit.c','-o',obj])
            objects=[str(obj) if v==parent[0]['Path'] else v for v in b['objects']]
            exe=out/('components-'+opt+('.exe' if win else ''))
            run(opt+'-build',[cc,*flags,'-'+opt,fixture,*objects,runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
            inputs[str(obj)]=sha(obj);inputs[str(exe)]=sha(exe)
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
