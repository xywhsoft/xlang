"""Qualify owning EGL context/native code after real module Close.

Explicit optional EGL SDK only; headless Mesa is allowed. Both a local view
and a module global must retire after the external native owner releases.
No network or hardware-performance test runs.
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
    a=parser.parse_args();root,compiler,cohort,packages,stage,out=[v.resolve() for v in
        (a.root,a.compiler,a.compiler_report,a.packages,a.stage_report,a.out)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    metadata=json.loads(cohort.read_text(encoding='utf8'))
    assert metadata.get('passed') and metadata.get('fullTccXrtAndVfs')
    delivery=json.loads(stage.read_text(encoding='utf8'))
    assert delivery.get('complete') and delivery.get('inputsUnchanged')
    assert all(sha(packages/name)==digest for name,digest in delivery['outputs'].items())
    private=packages/'_xge_native';manifest=json.loads((private/'xlang.package.json').read_text(encoding='utf8'))
    assert manifest['native']['linkage']=='dynamic'
    assert '#define XGE_ENABLE_EGL 1' in (private/'include/xge_build_config.h').read_text(encoding='utf8'), 'Use a matched EGL-enabled SDK'
    image=private/manifest['links'][0];assert sha(image)==manifest['native']['sha256']
    sdk=compiler.parent/'include';assert sdk.is_dir()
    host=root/'test/cases/foreign_xge3_graphics_context_module_host.c'
    case=root/'test/cases/foreign_xge3_graphics_context_module.xl';sidecar=case.with_suffix('.h')
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
    report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,
        networkTests=False,graphicsDevice=True, hardwarePerformance=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,discover=False):
        command=list(map(str,command));r=subprocess.run(command,cwd=compiler.parent,env=env,capture_output=True,timeout=90)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)))
        save();print(name,r.returncode,flush=True)
        if not discover:assert not r.returncode,log
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-cast-function-type',
        '-I'+str(root),'-I'+str(compiler.parent),'-I'+str(sdk),'-I'+str(root/'test/cases'),
        '-I'+str(private/'include'),'-include',private/'include/xge_build_config.h']
    systems=['-DXRT_USE_SHARED'] if os.name=='nt' else ['-Wl,-rpath,'+str(compiler.parent),'-pthread','-ldl','-lm']
    for optimization in ('O0','O2'):
        binary=out/('host-'+optimization+('.exe' if os.name=='nt' else ''))
        run(optimization+'-build',[cc,*flags,'-'+optimization,host,library,*systems,'-o',binary])
        for mode in ('0','1'):
            temp=out/(optimization+'-tmp-'+mode);temp.mkdir()
            run(optimization+'-mode-'+mode,[binary,case,compiler.parent,sdk,temp,image,mode,root],discover=True)
    report.update(passed=all(not row['exitCode'] for row in report['rows']),inputsUnchanged=all(sha(p)==digest for p,digest in inputs.items()))
    assert report['inputsUnchanged'];save()
    assert report['passed'],out/'results.json'


if __name__=='__main__':main()
