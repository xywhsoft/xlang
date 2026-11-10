"""Check resident code-owner RC, native return-tail marking and warmed OOM.

Requires a separately qualified full compiler/runtime candidate. No engine,
window, audio device, network access or installed release modification.
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
    for name in ('root','compiler','compiler-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args()
    root,compiler,cohort,out=[p.resolve() for p in (a.root,a.compiler,a.compiler_report,a.out)]
    metadata=json.loads(cohort.read_text(encoding='utf8'))
    assert metadata.get('passed') and metadata.get('fullTccXrtAndVfs')
    library=compiler.parent/('xlc.dll' if os.name=='nt' else 'libxlc.so')
    source=root/'test/cases/code_lease_native_owner.c'
    cc=Path(shutil.which('gcc')).resolve()
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),compiler,cohort,library,source,cc]
    files.extend(p for p in (compiler.parent/'include').rglob('*') if p.is_file())
    files.extend(root/p for p in ('src/runtime/xl6_code_lease.h',
        'src/runtime/xl6_foreign_callback_scope.h','src/common/xl6_xrt.h'))
    inputs={str(p):sha(p) for p in files}
    out.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy()
    if os.name=='nt':shutil.copy2(library,out/library.name)
    else:env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(passed=False,fullCompilerBuild=True,shippingDelivery=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command))
        result=subprocess.run(command,cwd=compiler.parent,env=env,capture_output=True,timeout=90)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,logSha256=sha(log)))
        save();assert not result.returncode,log
    for optimization in ('O0','O2'):
        binary=out/(optimization+('.exe' if os.name=='nt' else ''))
        flags=['-DXRT_USE_SHARED'] if os.name=='nt' else ['-Wl,-rpath,'+str(compiler.parent),'-pthread','-ldl','-lm']
        run(optimization+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+optimization,
            '-I'+str(root),'-I'+str(compiler.parent),'-I'+str(compiler.parent/'include'),
            *flags,source,library,'-o',binary])
        run(optimization+'-run',[binary])
    report.update(passed=True,inputsUnchanged=all(sha(p)==digest for p,digest in inputs.items()))
    assert report['inputsUnchanged'];save()
    print(json.dumps(dict(passed=True,checks=len(report['rows']))))


if __name__=='__main__':main()
