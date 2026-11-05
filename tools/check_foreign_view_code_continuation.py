"""Cold ForeignView code capabilities over a verified compiler parent.

Only the changed resident ForeignView TU is rebuilt. This focused overlay is
not a complete compiler/provider rebuild, COM acceptance or shipping delivery.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(parser);args=parser.parse_args();root=args.root.resolve()
    selected=load_build(args.build,args.overlay);cohort=selected['cohort'];sdk=Path(selected['base'].get('RuntimeDirectory') or selected['base']['SdkDirectory'])
    runtime=selected['runtime'];cc=shutil.which('gcc');assert cc
    out=args.out.resolve();out.mkdir(exist_ok=False)
    inputs=dict(selected['inputs'])
    paths=[Path(__file__),root/'test/cases/foreign_view_code_continuation.c',root/'src/runtime/xl6_foreign_view.c',root/'src/runtime/xl6_foreign_view.h',Path(cc),runtime]
    paths+=list((cohort/'src/runtime').glob('*.h'))
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs.update((str(path),sha(path)) for path in paths)
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    env=dict(os.environ)
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerRebuild=False,compilerBuild=selected['buildManifest'],compilerOverlay=selected['overlay'],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=command_with_response(list(map(str,command)),cc,out/(name+'.rsp'));start=time.monotonic()
        result=subprocess.run(command,cwd=cohort,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,logSha256=sha(log),seconds=round(time.monotonic()-start,3)));save()
        print(name,result.returncode,flush=True);assert not result.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*['-I'+str(path) for path in (root,root/'src/runtime',cohort,cohort/'src/runtime',sdk,sdk/'include')]]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    objects=[path for path in selected['objects'] if Path(path).stem!='xl6_foreign_view']
    try:
        for opt in ('O0','O2'):
            obj=out/('foreign-view-'+opt+'.o');run(opt+'-resident',[cc,*flags,'-'+opt,'-c',root/'src/runtime/xl6_foreign_view.c','-o',obj])
            exe=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-build',[cc,*flags,'-'+opt,root/'test/cases/foreign_view_code_continuation.c',obj,*objects,runtime,*systems,'-o',exe]);run(opt+'-run',[exe])
        header=out/'header.cpp';header.write_text('#include "src/runtime/xl6_foreign_view.h"\nint main(){return 0;}\n')
        cxx=shutil.which('g++');assert cxx
        for standard in ('c++11','c++17'):run(standard+'-header',[cxx,*[f for f in flags if not f.startswith('-std=')],'-std='+standard,'-fsyntax-only',header])
        report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True)
    finally:save()
if __name__=='__main__':main()
