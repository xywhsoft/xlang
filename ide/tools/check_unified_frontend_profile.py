"""Exercise real current/previous frontend guards and mixed-source refusals.

Private copies are required for deliberate source corruption. This verifies
build admission,not native semantics,performance,GUI or release acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,shutil
from native_current_semantic_profile import profile
from native_current_ir_overlay import derive
from native_current_unified_profile import COHORT


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--frontend-root',type=Path,required=True)
    p.add_argument('--previous-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();current=a.frontend_root.resolve(strict=True);previous=a.previous_root.resolve(strict=True)
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    protected={str(path):sha(path) for path in [*current.rglob('*'),*previous.rglob('*')] if path.is_file()}
    protected[str(Path(__file__).resolve())]=sha(Path(__file__).resolve())
    report=dict(status='running',scope=__doc__,protected=protected,cases=[])
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    def check(root,target):
        audited=profile(root);target.mkdir()
        _,_,proof=derive(root,target,phases=True)
        return dict(inputs=len(audited),cohort=proof['irDependencyCohort'])
    try:
        for name,root in [('current',current),('previous',previous)]:
            observed=check(root,out/name)
            assert observed['cohort']==(COHORT if name=='current' else 'current-ed78373d'),observed
            report['cases'].append(dict(name=name,passed=True,**observed));save()
        faults=[('old-checker','src/sem/xl6_sem.c','shared index/type cohort'),
                ('old-c-owned','src/sem/xl6_sem_c_owned.c','shared index/type cohort'),
                ('old-runtime','lib/xrt/xrt.h','shared index/type cohort'),
                ('tampered-checker','src/sem/xl6_sem.c','shared index/type cohort'),
                ('tampered-ir-header','src/ir/xl6_ir.h','current semantic include/declaration cohort')]
        for name,rel,expected in faults:
            folder=out/name;root=folder/'frontend';shutil.copytree(current,root)
            path=root/rel
            path.write_bytes((previous/rel).read_bytes() if name.startswith('old-') else path.read_bytes()+b'\n')
            refusals={}
            for kind in ('semantic','ir'):
                try:
                    if kind=='semantic':profile(root)
                    else:derive(root,folder/'derived',phases=True)
                except ValueError as error:
                    message='current IR asset changed' if kind=='ir' and name=='tampered-ir-header' else expected
                    assert message in str(error),(name,kind,str(error));refusals[kind]=str(error)
                else:raise AssertionError('mixed/unreviewed frontend admitted: '+name+'/'+kind)
            report['cases'].append(dict(name=name,passed=True,source=rel,sourceSha256=sha(path),refusals=refusals));save()
        for path,digest in protected.items():assert sha(Path(path))==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),report=str(out/'result.json'))))


if __name__=='__main__':main()
