"""Recheck actual B142 inputs and staged index against its isolated source build.
Read-only source audit; writes one fresh evidence file. Documentation and the
additional nullable negative in the test runner may differ from the compiler
snapshot, but no compiled source/header may differ. Not shipping acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def normalized(data): return data.replace(b'\r\n',b'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('stage','report','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); stage,report,out=[getattr(args,name).resolve() for name in ('stage','report','out')]
    project=Path(__file__).resolve().parents[2]; assert not out.exists()
    audit=json.loads(report.read_text(encoding='utf-8-sig'))
    assert audit['Complete'] and audit['InputsUnchanged'] and audit['LanguageCgenAcceptance']
    assert not audit['Errors'] and len(audit['Rows'])==140
    assert not audit['ShippingLibraryAcceptance'] and not audit['Full53Acceptance']
    assert not audit['StandaloneLinkPruningAcceptance']
    for row in audit['Inputs']: assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    snapshot=json.loads((stage/'freeze.json').read_text(encoding='utf-8-sig')); assert snapshot['Complete']
    names=[row['Path'] for row in snapshot['Files']]
    index=subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()
    base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=project,text=True).strip()
    assert base==snapshot['Base']
    objects=subprocess.check_output(['git','cat-file','--batch'],cwd=project,
        input=b''.join(b':'+name.encode('utf-8')+b'\n' for name in names))
    blobs={}; offset=0
    for name in names:
        boundary=objects.index(b'\n',offset); header=objects[offset:boundary].split()
        assert len(header)==3 and header[1]==b'blob',name
        count=int(header[2]); offset=boundary+1; blob=objects[offset:offset+count]; offset+=count
        assert len(blob)==count and objects[offset:offset+1]==b'\n'; offset+=1; blobs[name]=blob
    assert offset==len(objects)
    allowed={'docs/三组标准库接入长期任务.md','docs/标准库决策清单.md',
        'tools/check_stdlib_thread_language.py'}
    changed=[]
    for row in snapshot['Files']:
        name=row['Path']; path=stage/'frozen'/name
        assert sha(path)==row['FrozenSha256'],name
        if normalized(blobs[name])!=normalized(path.read_bytes()):
            assert name in allowed,name; changed.append(name)
    # A current test host/fixture/runner must be the exact staged source. Other
    # ignored build objects and genuine TCC candidates are checked by raw hash.
    current=0
    for row in audit['Inputs']:
        path=Path(row['Path'])
        if not path.is_relative_to(project): continue
        name=path.relative_to(project).as_posix()
        if name not in blobs: continue
        assert normalized(path.read_bytes())==normalized(blobs[name]),name; current+=1
    assert current>=25
    assert subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()==index
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps({'Scope':__doc__,'Complete':True,'IndexTree':index,'Base':base,
        'SnapshotIndexTree':snapshot['IndexTree'],'ComparedIndexInputs':len(names),'CurrentTestInputs':current,
        'AllowedNonCompilerDifferences':changed,'Report':str(report),'ReportSha256':sha(report),
        'RecheckedInputs':len(audit['Inputs']),'ShippingLibraryAcceptance':False,'Full53Acceptance':False},
        indent=2)+'\n',encoding='utf-8')
    print('B142 final freeze: '+str(len(names))+' index inputs and '+str(len(audit['Inputs']))+' actual input hashes verified')
if __name__=='__main__': main()
