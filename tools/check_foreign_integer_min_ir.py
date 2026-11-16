"""Reject signed-integer bound IR damage before C or header output.

Tests the supplied recorded source-provider objects, independently of final
compiler/VFS/SDK delivery. O0/O2 below are client optimization levels.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve()
    build=load_build(a.build,a.overlay);cohort,runtime=build['cohort'],build['runtime']
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    out.mkdir(parents=True,exist_ok=False);cc=Path(shutil.which('gcc')).resolve()
    case=root/'test/cases/foreign_integer_min.xl';client=case.with_name('foreign_integer_min_ir.c')
    inputs=dict(build['inputs']);inputs.update((str(v),sha(v))for v in(Path(__file__),cc,case,client,case.with_suffix('.h')))
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort),TMP=str(out),TEMP=str(out),TMPDIR=str(out))
    win=os.name=='nt'
    if win:shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    sdk=Path(build['base'].get('SdkDirectory')or runtime.parent)
    includes=['-I'+str(v)for v in(cohort,root,build['basePath'].parent/'include',sdk,sdk/'include')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    report=dict(complete=False,passed=False,shippingDelivery=False,fullCompilerQualification=False,scope=__doc__,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));actual=command_with_response(command,str(cc),out/(name+'.rsp'))
        start=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=360)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log
    for opt in ('O0','O2'):
        binary=out/(opt+('.exe'if win else''))
        run(opt+'-build',[cc,*flags,'-'+opt,client,*build['objects'],runtime,*systems,'-o',binary])
        run(opt+'-mutations',[binary,case])
    report.update(complete=True,inputsUnchanged=all(sha(path)==value for path,value in inputs.items()))
    report['passed']=report['inputsUnchanged'];save();assert report['passed']


if __name__=='__main__':main()
