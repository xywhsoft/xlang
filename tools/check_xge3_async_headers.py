"""Check generated async public headers and real inline policy/domain linkage.

Both matched SDK variants must compile as strict C11/C++11/C++17. The dynamic
variant also links and runs its actual native ownership metadata in C++11.
No device/window/network operation runs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','header','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    root,compiler,cohort,dynamic,static,header,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.header,a.out)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    metadata=json.loads(cohort.read_text(encoding='utf8'))
    assert metadata.get('passed') and metadata.get('fullTccXrtAndVfs')
    sdk=compiler.parent/'include'
    cc=Path(shutil.which('gcc')).resolve();cpp=Path(shutil.which('g++')).resolve()
    library=compiler.parent/('xlc.dll' if os.name=='nt' else 'libxlc.so')
    private=dynamic/'_xge_native'
    manifest=json.loads((private/'xlang.package.json').read_text(encoding='utf8'))
    assert manifest['native']['linkage']=='dynamic'
    image=private/manifest['links'][0];assert sha(image)==manifest['native']['sha256']
    # GNU ld accepts the delivered PE DLL's export table directly too; use
    # the actual package input rather than an unstaged development .lib.
    files=[Path(__file__).resolve(),cohort,header,cc,cpp,library]
    for folder in (sdk,dynamic,static):files.extend(v for v in folder.rglob('*') if v.is_file())
    inputs={str(v):sha(v) for v in files}
    out.mkdir(parents=True,exist_ok=False)
    probe=out/'probe.c'
    probe.write_text('#include "'+header.as_posix()+'"\n#include <xge3_async_load.h>\nint main(void){return 0;}\n',encoding='utf8')
    linked=out/'linked.cpp'
    linked.write_text('#include "'+header.as_posix()+'"\n#include <xge3_async_load.h>\n'
        'int main(){auto* image=xl6Xge3ImageLoadPolicyGet();auto* texture=xl6Xge3TextureLoadPolicyGet();'
        'auto* domain=xl6Xge3AsyncLoadDomainV1Get();return !image->Destroy || !texture->Destroy || !domain;}\n',encoding='utf8')
    inputs.update({str(v):sha(v) for v in (probe,linked)})
    env=dict(os.environ)
    if os.name=='nt':
        for v in (library,image):shutil.copy2(v,out/v.name)
    else:env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+str(image.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));r=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=90)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
    def include(packages):
        return ['-I'+str(v) for v in (root,sdk,compiler.parent,packages/'_xge_native/include',packages/'xge/include')]
    flags=['-Wall','-Wextra','-Werror','-Wno-unused-function']
    for name,packages in [('dynamic',dynamic),('static',static)]:
        mode=['-DXL6_NATIVE_LINK_SCHEMA=1'] if name=='static' else []
        for language,standard,compilerPath in [('c','c11',cc),('c++','c++11',cpp),('c++','c++17',cpp)]:
            run(name+'-'+standard,[compilerPath,'-x',language,'-std='+standard,*flags,*mode,'-fsyntax-only',*include(packages),probe])
    binary=out/('linked.exe' if os.name=='nt' else 'linked')
    systems=['-DXGE_DLL','-DXUI_DLL','-DXRT_USE_SHARED'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    run('cpp-linked-build',[cpp,'-std=c++11',*flags,*include(dynamic),*systems,linked,image,library,'-o',binary])
    run('cpp-linked-run',[binary])
    report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
    assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
