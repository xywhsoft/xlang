"""CPU acceptance of exact leased-domain semantic continuation queries.

Rebuilds the two changed resident translation units at O0/O2 over an honestly
verified full source parent. This is a partial runtime gate, not a new compiler
or shipping-provider qualification. Checks old V1 plans/views and NativeSequence
beside the new no-Load native-domain failure/retry/identity scenarios.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(parser);args=parser.parse_args()
    selected=load_build(args.build,args.overlay)
    root=args.root.resolve();source=selected['cohort'];base=selected['base']
    sdk=Path(base.get('RuntimeDirectory') or base.get('SdkDirectory') or source/'release/win_x64')
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    cc=shutil.which('gcc');cxx=shutil.which('g++');assert cc and cxx
    changed=('xl6_ownership_catalog','xl6_foreign_view')
    cases=('xlir_ownership_leased_finalization_runtime','xlir_ownership_finalization_runtime',
           'xlir_ownership_finalization_view_runtime','native_sequence_runtime')
    pins=[Path(__file__).resolve(),Path(cc),Path(cxx),selected['runtime']]
    pins.extend(root/'src/runtime'/(name+'.c') for name in changed)
    pins.extend((root/'src/runtime').glob('xl6_ownership*.h'))
    pins.extend(root/'src/runtime'/name for name in ('xl6_foreign_view.h','xl6_native_sequence.h'))
    pins.extend(root/'test/cases'/(name+'.c') for name in cases)
    inputs=dict(selected['inputs']);inputs.update((str(p),sha(p)) for p in pins)
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerRebuild=False,
        parentBuild=selected['buildManifest'],parentOverlay=selected['overlay'],inputs=inputs,rows=[],shippingDelivery=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=command_with_response(list(map(str,command)),cc if str(command[0])==cc else cxx,out/(name+'.rsp'))
        start=time.monotonic();r=subprocess.run(command,cwd=source,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(source),'-I'+str(sdk)]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(selected['runtime'].parent)]
    if os.name=='nt':shutil.copy2(selected['runtime'],out/selected['runtime'].name)
    old=[obj for obj in selected['objects'] if Path(obj).stem not in changed]
    try:
        for opt in ('O0','O2'):
            objects=[]
            for name in changed:
                obj=out/(name+'-'+opt+'.o');objects.append(obj)
                run(name+'-'+opt,[cc,*flags,'-'+opt,'-c',root/'src/runtime'/(name+'.c'),'-o',obj])
            for name in cases:
                target=out/(name+'-'+opt+('.exe' if os.name=='nt' else ''))
                run(name+'-'+opt+'-build',[cc,*flags,'-'+opt,root/'test/cases'/(name+'.c'),*objects,*old,selected['runtime'],*systems,'-o',target])
                run(name+'-'+opt+'-run',[target])
        header=out/'domain.cpp';header.write_text('#include "src/runtime/xl6_ownership_catalog.h"\n#include <cstddef>\nstatic_assert(sizeof(XL6_OwnershipDomainV1)==4*sizeof(void*),"V1 ABI");\nstatic_assert(offsetof(XL6_OwnershipDomainV2,V1)==0,"V1 prefix");\nstatic_assert(sizeof(XL6_OwnershipDomainV2)==6*sizeof(void*),"V2 ABI");\nint main(){ XL6_OwnershipDomainV1 d={};return xl6OwnershipDomainValidV1(&d); }\n',encoding='utf-8')
        for standard in ('c++11','c++17'):
            run(standard+'-header',[cxx,*[v for v in flags if not v.startswith('-std=')],'-std='+standard,'-fsyntax-only',header])
        report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged']
        report.update(complete=True,passed=True)
    finally:save()

if __name__=='__main__':main()
