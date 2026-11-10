"""Qualify distinct owned policies sharing an exact borrowed C input ABI.

Nominal values, producer policies, domains, nullability and effects remain
independent contracts. A diagnostic compiler must be explicitly requested.
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
    for name in ('root','compiler','sdk','compiler-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--diagnostic',action='store_true')
    parser.add_argument('--skip-aot',action='store_true',help='Diagnostic-only CLI/JIT/header subset; never qualifies standalone AOT')
    a=parser.parse_args()
    root,compiler,sdk,cohort,out=[p.resolve() for p in (a.root,a.compiler,a.sdk,a.compiler_report,a.out)]
    metadata=json.loads(cohort.read_text(encoding='utf8'))
    full=bool(metadata.get('fullTccXrtAndVfs'))
    assert metadata.get('passed') and (full or a.diagnostic)
    assert not a.skip_aot or (a.diagnostic and not full)
    source=root/'test/cases/foreign_policy_inputs.xl'
    legacy=root/'test/cases/foreign_policy_same_result.xl'
    negatives=sorted((root/'test/cases').glob('bad_foreign_policy_input_*.xl'))
    assert len(negatives)==5
    cc=Path(shutil.which('gcc')).resolve();cpp=Path(shutil.which('g++')).resolve()
    files=[Path(__file__).resolve(),compiler,cohort,source,source.with_suffix('.h'),legacy,legacy.with_suffix('.h'),cc,cpp,*negatives]
    files.extend(p for p in sdk.rglob('*') if p.is_file())
    files.extend(p for p in compiler.parent.iterdir() if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p) for p in files}
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env={k:v for k,v in os.environ.items() if k!='XLANG_PACKAGE_PATH'}
    env.update(TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    report=dict(passed=False,fullCompilerBuild=full,aotQualification=not a.skip_aot,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,expected=0):
        command=list(map(str,command))
        r=subprocess.run(command,cwd=compiler.parent,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)))
        save();assert r.returncode==expected,log
        return r.stdout+r.stderr
    flags=['--inc-path',root,'--inc-path',sdk]
    headers=[]
    for name,case in [('policy',source),('same-result',legacy)]:
        header=out/(name+'.h');headers.append((name,header))
        run(name+'-emit',[compiler,'cgen',case,'--emit',out/(name+'.c'),'--emit-header',header])
        run(name+'-jit',[compiler,'run',case,*flags])
        if not a.skip_aot:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'-aot-build',[compiler,'build',case,'-o',binary,*flags]);run(name+'-aot-run',[binary])
    for source in negatives:
        text=run(source.stem,[compiler,'check',source,*flags],expected=1)
        assert b'[sem]' in text
        if not source.stem.endswith('nominal'):assert b'extern C symbol declared with incompatible ABI' in text
    include=['-I'+str(p) for p in (root,sdk,compiler.parent)]
    for name,header in headers:
        for language,standard,compilerPath in [('c','c11',cc),('c++','c++11',cpp),('c++','c++17',cpp)]:
            run(name+'-header-'+standard,[compilerPath,'-x',language,'-std='+standard,'-Wall','-Wextra','-Werror',
                '-Wno-unused-function','-fsyntax-only',*include,header])
    report.update(passed=True,inputsUnchanged=all(sha(p)==digest for p,digest in inputs.items()))
    assert report['inputsUnchanged'];save()
    print(json.dumps(dict(passed=True,checks=len(report['rows']),fullCompilerBuild=full)))


if __name__=='__main__':main()
