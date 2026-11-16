"""Reject malformed callable-receiver IR before any C/header output.

Uses a hash-verified full public-source compiler build with its real XRT and
runtime. This supplements full O2 CLI tests; it is not a shipping rebuild.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import load_build
from compiler_response import command_with_response


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(build=a.build)
    cohort=b['cohort'];runtime=b['runtime'];win=os.name=='nt';cc=Path(shutil.which('gcc')).resolve()
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    files=[Path(__file__),Path(__file__).with_name('foreign_validation_build.py'),Path(__file__).with_name('compiler_response.py'),cc]
    for stem in ('foreign_field_receiver','foreign_callable_field'):
        files += [root/'test/cases'/(stem+suffix) for suffix in ('.xl','_ir.c')]
    inputs=dict(b['inputs']);inputs.update((str(v),sha(v)) for v in files)
    out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort),TMP=str(out),TEMP=str(out),TMPDIR=str(out))
    key='PATH' if win else 'LD_LIBRARY_PATH';env[key]=str(runtime.parent)+os.pathsep+env.get(key,'')
    if win:shutil.copy2(runtime,out/runtime.name)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],fullCompilerBuild=False,
                publicSourceOptimization=b['base'].get('Optimization'),networkTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));actual=command_with_response(command,str(cc),out/(name+'.rsp'))
        start=time.monotonic();r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert r.returncode==0,log
    try:
        sdk=Path(b['base'].get('RuntimeDirectory',b['base'].get('SdkDirectory','')))
        inc=['-I'+str(v) for v in (cohort,root,b['basePath'].parent/'include',sdk,sdk/'include')]
        # The frozen compiler preceding the formatting-only source cleanup has
        # one misleading-indentation warning; the public build records this
        # suppression explicitly. No compiler/source object is replaced here.
        flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DXL_STATIC_EMBED',*inc]
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for stem in ('foreign_field_receiver','foreign_callable_field'):
            for opt in ('O0','O2'):
                name=stem+'-'+opt;exe=out/(name+('.exe' if win else ''))
                run(name+'-build',[cc,*flags,'-'+opt,root/'test/cases'/(stem+'_ir.c'),*b['objects'],runtime,*systems,'-o',exe])
                run(name+'-run',[exe,root/'test/cases'/(stem+'.xl')])
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
