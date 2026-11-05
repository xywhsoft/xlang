"""CPU-only real XUI Model roots, leased graph and checked cleanup acceptance.

Rebuilds the two lifecycle runtime units and private package policy source over
a verified compiler parent. No complete compiler/provider or GUI qualification
is implied. Native SDK/binary inputs are taken from the matched staged package.
"""
from pathlib import Path
import argparse,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from foreign_validation_build import add_build_arguments,load_build,sha

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','packages','out'):p.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(p);a=p.parse_args();selected=load_build(a.build,a.overlay)
    root=a.root.resolve();source=selected['cohort'];base=selected['base']
    sdk=Path(base.get('RuntimeDirectory') or base.get('SdkDirectory') or source/'release/win_x64')
    package=a.packages.resolve()/'_xge_native';manifest=package/'xlang.package.json'
    description=json.loads(manifest.read_text(encoding='utf-8-sig'))
    native=package/description['loadlibs'][0];assert sha(native)==description['native']['sha256']
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    headers=out/'include/__xl_runtime';headers.mkdir(parents=True)
    originals=list((root/'src/runtime').glob('*.h'))+[root/'src/api/xl_foreign_error.h']
    for item in originals:shutil.copy2(item,headers/item.name)
    private=root/'lib/bindings/_xge_native/src/runtime.c'
    public=root/'lib/bindings/_xge_native/include/xge3_runtime.h'
    # Override only the two root-owned private package sources; all SDK-owned
    # headers and the native image remain the exact staged dependency.
    shutil.copy2(public,out/public.name)
    case=root/'test/cases/foreign_xui_document_model_policy.c'
    changed=('xl6_ownership_catalog','xl6_foreign_view')
    cc=shutil.which('gcc');cxx=shutil.which('g++');assert cc and cxx
    pins=[Path(__file__).resolve(),Path(cc),Path(cxx),private,public,case,manifest,native,selected['runtime'],*originals]
    pins.extend(root/'src/runtime'/(name+'.c') for name in changed)
    pins.extend((package/'include').rglob('*.h'))
    inputs=dict(selected['inputs']);inputs.update((str(v),sha(v)) for v in pins)
    report=dict(complete=False,passed=False,fullCompilerRebuild=False,shippingDelivery=False,
        scope=__doc__,parentBuild=selected['buildManifest'],parentOverlay=selected['overlay'],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=command_with_response(list(map(str,command)),cc if str(command[0])==cc else cxx,out/(name+'.rsp'))
        started=time.monotonic();r=subprocess.run(command,cwd=source,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,
            seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(out),
        '-I'+str(out/'include'),'-I'+str(package/'include'),'-I'+str(root),'-I'+str(source),
        '-I'+str(sdk),'-I'+str(sdk/'include')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else [
        '-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent),
        '-Wl,-rpath,'+str(selected['runtime'].parent)]
    if os.name=='nt':
        shutil.copy2(selected['runtime'],out/selected['runtime'].name);shutil.copy2(native,out/native.name)
    old=[obj for obj in selected['objects'] if Path(obj).stem not in changed]
    try:
        for opt in ('O0','O2'):
            fresh=[]
            for name,path in [(name,root/'src/runtime'/(name+'.c')) for name in changed]+[('private',private),('kernel',case)]:
                obj=out/(name+'-'+opt+'.o');fresh.append(obj)
                run(name+'-'+opt,[cc,*flags,'-'+opt,'-c',path,'-o',obj])
            target=out/('kernel-'+opt+('.exe' if os.name=='nt' else ''))
            run('link-'+opt,[cc,*fresh,*old,selected['runtime'],native,*systems,'-o',target])
            run('run-'+opt,[target])
        header=out/'header.cpp';header.write_text('#include <xge3_runtime.h>\nint main(){return xl6Xui3DocumentPolicyGet()==nullptr;}\n',encoding='utf-8')
        for standard in ('c++11','c++17'):
            run(standard+'-header',[cxx,*[v for v in flags if not v.startswith('-std=')],'-std='+standard,'-fsyntax-only',header])
        report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items());assert report['inputsUnchanged']
        report.update(complete=True,passed=True)
    finally:save()

if __name__=='__main__':main()
