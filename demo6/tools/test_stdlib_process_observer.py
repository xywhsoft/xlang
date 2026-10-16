"""Reject mismatched, duplicated, non-full and trimmed private observer inputs."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,sys
from stage_stdlib_process_observer import stage

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    cc=shutil.which('gcc');assert cc
    names=('lib/xrt/xrt.h','lib/xruntime/xruntime.h','src/api/xl_xrt_dll.c')
    originals={name:(root/name).read_text(encoding='utf-8-sig') for name in names}
    files={Path(__file__).resolve(),Path(__file__).with_name('stage_stdlib_process_observer.py'),Path(sys.executable),Path(cc),*[root/n for n in names]}
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)];out.mkdir(parents=True);rows=[]
    body=re.findall(r'^struct xprocess \{\n.*?^\};',originals[names[0]],re.M|re.S);assert len(body)==1
    for label,name,changed in (
        ('mismatch',names[0],originals[names[0]].replace(body[0],body[0].replace('xthread* Waiter;','xthread* OtherWaiter;'))),
        ('duplicate',names[1],originals[names[1]]+'\n'+body[0]+'\n'),
        ('non-full',names[2],originals[names[2]].replace('#define XRT_MODULE_ALL','#define XRT_MODULE_PROCESS_TERMINAL'))):
        candidate=out/label
        for path,text in originals.items():
            target=candidate/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(changed if path==name else text,encoding='utf-8')
        rejected=False
        try:stage(candidate,out/(label+'-observer'))
        except AssertionError:rejected=True
        assert rejected and not (out/(label+'-observer')).exists(),label
        rows.append(dict(Name=label,Rejected=True))
    good=stage(root,out/'full-observer')
    source='#define XRT_MODULE_PROCESS_TERMINAL\n#include "lib/xruntime/xruntime.h"\n#include "process_observer_layout.h"\n'
    command=[cc,'-E','-P','-x','c','-I'+str(root),'-I'+str(out/'full-observer'),'-']
    result=subprocess.run(command,input=source.encode(),capture_output=True,timeout=60)
    log=out/'trimmed.log';log.write_bytes(result.stdout+result.stderr)
    assert result.returncode!=0 and b'Observer requires the ordinary full product' in result.stderr
    rows.append(dict(Name='trimmed',Rejected=True,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
    unchanged=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert unchanged
    report=dict(Complete=True,InputsUnchanged=unchanged,Inputs=inputs,Rows=rows,FullObserver=good,Full53Acceptance=False)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('four incompatible observer controls rejected')

if __name__=='__main__':main()
