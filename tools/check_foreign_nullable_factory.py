"""Qualify nullable owning status/output factories without a method adapter.

The caller supplies and independently qualifies the compiler cohort. This
gate checks public CLI behavior, generated C, JIT, standalone AOT, rejected
declarations and allocation/error rollback; it does not install a release.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','sdk','out','compiler-report'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,sdk,out,cohort=[v.resolve() for v in
        (a.root,a.compiler,a.sdk,a.out,a.compiler_report)]
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    metadata=json.loads(cohort.read_text())
    assert metadata.get('passed'), 'Qualify the compiler inputs first'
    case=root/'test/cases/foreign_nullable_factory.xl'
    negatives=[root/'test/cases'/('bad_foreign_nullable_factory_'+name+'.xl') for name in ('caller','inplace')]
    pins=[Path(__file__).resolve(),compiler,cohort,case,case.with_suffix('.h'),*negatives]
    pins.extend(path for path in compiler.parent.iterdir() if path.is_file())
    pins.extend(path for path in sdk.rglob('*') if path.is_file())
    inputs={str(path):sha(path) for path in pins}
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env={k:v for k,v in os.environ.items() if k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    report=dict(passed=False,scope=__doc__,shippingDelivery=False,inputs=inputs,
        fullCompilerBuild=bool(metadata.get('fullTccXrtAndVfs')),rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,expected=0):
        command=list(map(str,command));r=subprocess.run(command,cwd=root/'release'/
            ('win_x64' if os.name=='nt' else 'linux_x64'),env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)))
        save();assert r.returncode==expected,log
        return r.stdout+r.stderr
    flags=['--inc-path',root,'--inc-path',sdk]
    run('emit',[compiler,'cgen',case,'--emit',out/'factory.c','--emit-header',out/'factory.h'])
    for name in ('factory.c','factory.h'):
        text=(out/name).read_text(encoding='utf8');assert 'nullableFactoryCreate' in text
    run('jit',[compiler,'run',case,*flags])
    binary=out/('factory.exe' if os.name=='nt' else 'factory')
    run('aot-build',[compiler,'build',case,'-o',binary,*flags])
    run('aot-run',[binary])
    for case in negatives:
        text=run(case.stem,[compiler,'check',case,*flags],expected=1)
        assert b'nullable factories require an owned_view' in text
    report.update(passed=True,inputsUnchanged=all(sha(path)==h for path,h in inputs.items()))
    assert report['inputsUnchanged'];save();print(json.dumps(dict(passed=True,checks=len(report['rows']))))


if __name__=='__main__':main()
