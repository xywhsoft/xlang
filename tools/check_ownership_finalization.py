"""Focused semantic FinalizationV1 resident-plan acceptance, not a complete compiler rebuild.

Rebuilds the lifecycle components against the exact current declaration SDK;
reuses the one verified parent's XRT object (including its checked-error core).
The new XRT protocol is additive types only, with no implementation changes.
Runs CPU-only programs and earlier V1 lifecycle regressions at O0 and O2.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

RUNTIME=(
    'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_ownership_catalog.c',
    'src/runtime/xl6_code_lease.c','src/runtime/xl6_foreign_context.c',
    'src/runtime/xl6_foreign_callback_scope.c','src/runtime/xl6_callback_failure.c')
CASES=('xlir_ownership_finalization_runtime','xlir_ownership_checked_drop_runtime','xlir_ownership_native_finish_runtime',
       'xlir_ownership_finish_runtime','xlir_ownership_preparation_runtime',
       'xlir_ownership_plan_runtime','xlir_ownership_value_plan_runtime',
       'xlir_code_lease_retirement_tail')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','parent-build','sdk','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();sdk=a.sdk.resolve();out=a.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    parent=json.loads(a.parent_build.read_text(encoding='utf-8-sig'))
    assert parent['Complete'] and parent['InputsUnchanged'] and parent['SourceXrt']
    native=[row for row in parent['Objects'] if Path(row['Source']).name=='xrt_single_header_impl.c']
    assert len(native)==1 and sha(native[0]['Path'])==native[0]['Sha256'].lower()
    # XRT's TU embeds this resident checked-error implementation; do not link
    # it twice or silently reuse an older checked-error implementation.
    oldroot=Path(native[0]['Source']).parent.parent
    assert sha(oldroot/'src/api/xl_foreign_error.c')==sha(root/'src/api/xl_foreign_error.c')
    pins=[a.parent_build.resolve(),Path(native[0]['Path']),Path(__file__).resolve(),
          root/'src/api/xl_foreign_error.c',root/'src/common/xl6_xrt.h',
          sdk/'include/xrt_decl.h',sdk/'include/xruntime_decl.h',sdk/'include/xl_clock.h']
    pins.extend(root/'test/cases'/(name+'.c') for name in CASES)
    pins.extend(root/path for path in RUNTIME)
    pins.extend((root/'src/runtime').glob('xl6_ownership*.h'))
    pins.extend(root/'src/runtime'/name for name in ('xl6_code_lease.h','xl6_foreign_context.h',
        'xl6_foreign_callback_scope.h','xl6_callback_failure.h','xl6_deferred_cleanup.h'))
    inputs={str(v):sha(v) for v in pins}
    report=dict(scope=__doc__,complete=False,passed=False,fullCompilerRebuild=False,
                reusedXrtObject=native[0],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=root,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
                                   seconds=round(time.monotonic()-start,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert r.returncode==0,log
    cc=shutil.which('gcc');assert cc;inputs[cc]=sha(cc)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(sdk)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    try:
        for opt in ('O0','O2'):
            objects=[]
            for source in RUNTIME:
                target=out/(Path(source).stem+'.'+opt+'.o');objects.append(target)
                run(target.stem,[cc,*flags,'-'+opt,'-c',root/source,'-o',target])
            for case in CASES:
                target=out/(case+'.'+opt+('.exe' if os.name=='nt' else '.out'))
                run(target.stem+'.build',[cc,*flags,'-'+opt,root/'test/cases'/(case+'.c'),
                    *objects,native[0]['Path'],*systems,'-o',target])
                run(target.stem+'.run',[target])
        report['inputsUnchanged']=all(sha(v)==h for v,h in inputs.items());assert report['inputsUnchanged']
        report.update(complete=True,passed=True)
    finally:save()
if __name__=='__main__':main()
