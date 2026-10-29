"""Inventory reachable native handle transfers from verified lowered IR.

Complete three-group fixture discovery plus the existing nested SDK fixture.
Unresolved policies are reported as audit work, NOT accepted as leaf resources.
This compiles/dumps language IR only; it does not execute fixtures or prove AOT.
"""
from pathlib import Path
import argparse,concurrent.futures,json,os,subprocess,time
from discover_stdlib_three_groups import PREFIX,NEGATIVE
from xlir_public_build_inputs import sha,verify_public_build

def reachable(data):
    lookup={target['symbol']:target for target in data['callTargets']};result=set()
    for number,owner in enumerate(data['functions']):
        if owner['reachability']!='unreachable':
            result.update(range(owner['firstInstruction'],owner['firstInstruction']+owner['instructionCount']));continue
        target=lookup.get(owner['symbol'])
        if not target or target['reachability']=='unreachable':continue
        for machine in data['asyncMachines']:
            if machine['function']!=number:continue
            blocks=data['asyncMachineBlocks'][machine['firstBlock']:machine['firstBlock']+machine['blockCount']]
            assert len(blocks)==machine['blockCount']
            for block in blocks:result.update(range(block['firstInstruction'],block['firstInstruction']+block['instructionCount']))
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--jobs',type=int,default=4);a=p.parse_args()
    root,out=a.root.resolve(),a.out.resolve();assert not out.exists() and 1<=a.jobs<=8
    build,pinned,compared=verify_public_build(root,a.public_directory)
    fixtures=sorted(path for path in (root/'test/cases').glob('stdlib_*.xl') if PREFIX.match(path.name))
    fixtures.append(root/'test/cases/stdlib_nested_sdk.xl');assert len(set(fixtures))==len(fixtures)
    pinned.update((Path(__file__).resolve(),*fixtures));inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def scan(source):
        target=out/(source.stem+'.json');started=time.monotonic()
        r=subprocess.run([build['Driver'],str(source),'ir-lowered',str(target)],cwd=root,env=env,capture_output=True,timeout=120)
        data=r.stdout+r.stderr;log=out/(source.stem+'.log');log.write_bytes(data)
        row=dict(Name=source.stem,Exit=r.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-started,
            Passed=False,ExpectedRejection=source.stem in NEGATIVE,Transfers=[])
        if source.stem in NEGATIVE:
            row['Passed']=r.returncode!=0 and NEGATIVE[source.stem].encode() in data;return row
        if r.returncode:return row
        data=json.loads(target.read_text());lookup={item['symbol']:item for item in data['callTargets']};live=reachable(data)
        for i,inst in enumerate(data['instructions']):
            if i not in live or inst['runtimeOpName']!='xvalue-native-handle-take':continue
            item=lookup[inst['targetSymbol']]
            entry=dict(Symbol=item['symbol'],Name=item['cName'],Bridge=item['runtimeBridgeName'],
                PolicyAbi=item['nativeHandlePolicyAbi'],Policy=item['nativeHandlePolicyCName'])
            if entry not in row['Transfers']:row['Transfers'].append(entry)
        row.update(Passed=True,Ir=str(target),IrSha256=sha(target));return row
    rows=list(concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs).map(scan,fixtures));missing={}
    for row in rows:
        for item in row['Transfers']:
            if item['PolicyAbi']==0:missing.setdefault(item['Name'],set()).add(row['Name'])
    unresolved=[dict(Name=name,Fixtures=sorted(names),Disposition='requires actual native storage and lifecycle audit') for name,names in sorted(missing.items())]
    unchanged=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report=dict(Complete=unchanged and all(row['Passed'] for row in rows),InputsUnchanged=unchanged,Inputs=inputs,ComparedSources=compared,
        Scope=__doc__,Cases=rows,CandidateCount=len(fixtures),Unresolved=unresolved,AllPoliciesResolved=not unresolved,
        Full53Acceptance=False,ShippingLibraryAcceptance=False)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Cases=len(rows),Unresolved=len(unresolved),Names=sorted(missing))),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
