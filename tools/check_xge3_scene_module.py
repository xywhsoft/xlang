"""Qualify a real Scene stack/callback/module owning graph and native code tails.

Both an empty scene and a self-capturing callback must keep the native image
alive after host Close. After native publishers stop, checked drain must
collect the cycle and unload it. No window/device or network operation runs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','packages','stage-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--diagnostic',action='store_true',help='Explicitly allow a partial source compiler; never report it as a full build')
    a=parser.parse_args();root,compiler,cohort,packages,stage,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.packages,a.stage_report,a.out)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    metadata=json.loads(cohort.read_text(encoding='utf8'))
    full=bool(metadata.get('fullTccXrtAndVfs'))
    assert metadata.get('passed') and (full or a.diagnostic)
    delivery=json.loads(stage.read_text(encoding='utf8'))
    assert delivery.get('complete') and delivery.get('inputsUnchanged')
    assert all(sha(packages/name)==digest for name,digest in delivery['outputs'].items())
    private=packages/'_xge_native';manifest=json.loads((private/'xlang.package.json').read_text(encoding='utf8'))
    assert manifest['native']['linkage']=='dynamic'
    image=private/manifest['links'][0];assert sha(image)==manifest['native']['sha256']
    sdk=compiler.parent/'include';assert sdk.is_dir()
    host=root/'test/cases/foreign_xge3_scene_module_host.c'
    case=root/'test/cases/foreign_xge3_scene_module.xl';sidecar=case.with_suffix('.h')
    cc=Path(shutil.which('gcc'));assert cc.is_file()
    library=compiler.parent/('xlc.dll' if os.name=='nt' else 'libxlc.so')
    files=[Path(__file__).resolve(),cohort,stage,cc,host,case,sidecar,library]
    files.extend(p for p in sdk.rglob('*') if p.is_file())
    files.extend(p for p in packages.rglob('*') if p.is_file())
    inputs={str(p):sha(p) for p in files}
    out.mkdir(parents=True,exist_ok=False)
    if os.name=='nt':shutil.copy2(library,out/library.name)
    env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages))
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(passed=False,scope=__doc__,fullCompilerBuild=full,shippingDelivery=False,
        networkTests=False,graphicsDevice=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));r=subprocess.run(command,cwd=compiler.parent,env=env,capture_output=True,timeout=90)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert not r.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type',
        '-I'+str(root),'-I'+str(compiler.parent),'-I'+str(sdk),'-I'+str(root/'test/cases')]
    systems=['-DXRT_USE_SHARED'] if os.name=='nt' else ['-Wl,-rpath,'+str(compiler.parent),'-pthread','-ldl','-lm']
    for optimization in ('O0','O2'):
        binary=out/('host-'+optimization+('.exe' if os.name=='nt' else ''))
        run(optimization+'-build',[cc,*flags,'-'+optimization,host,library,*systems,'-o',binary])
        for callbacks in ('0','1'):
            temp=out/(optimization+'-tmp-'+callbacks);temp.mkdir()
            run(optimization+'-callbacks-'+callbacks,[binary,case,compiler.parent,sdk,temp,image,callbacks,root])
    report.update(passed=True,inputsUnchanged=all(sha(p)==digest for p,digest in inputs.items()))
    assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
