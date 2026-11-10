"""Verify actual native objects, links, archive members and required IDE inputs.

Consumes the qualified dependencyAudit/linkRecipes receipt. Required C files
must be actual compiled translation units, not merely uncompiled working-tree
copies. Headers/includes must occur in a verified dependency closure. A frozen
copy may differ only in line endings; its actual compiled bytes stay recorded.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess

def sha(path):
    with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()

def verify(receipt,root,required,out):
    rows=receipt['dependencyAudit'];objects={};inputs={};sourceRows=[]
    for row in rows:
        obj=Path(row['actualObject']).resolve();assert sha(obj)==row['actualObjectSha256'],obj
        assert obj not in objects,('Duplicate actual object',obj)
        objects[obj]=row
        for name,h in row['dependencies'].items():
            path=Path(name).resolve();assert sha(path)==h,path;inputs[path]=h
        command=row['command'];source=Path(command[command.index('-c')+1]).resolve()
        assert source in {Path(p).resolve()for p in row['dependencies']},('Actual source missing from closure',source)
        sourceRows.append((source,obj))
    bindings=[]
    for name in required:
        expected=(root/name).resolve();assert expected.is_file(),expected
        candidates=sourceRows if expected.suffix=='.c'else[(p,None)for p in inputs]
        matches=[];body=expected.read_bytes().replace(b'\r\n',b'\n')
        for path,obj in candidates:
            if path.name==expected.name and path.read_bytes().replace(b'\r\n',b'\n')==body:
                matches.append(dict(actualSource=str(path),sha256=sha(path),object=str(obj)if obj else None))
        assert matches,('Required current input was not compiled',str(expected))
        bindings.append(dict(required=str(expected),sha256=sha(expected),consumers=matches))
    linked={};archives=set()
    for program,recipe in receipt['linkRecipes'].items():
        logical=Path(program).stem.replace('xlang-ls','xlang_ls')
        primary=[obj for source,obj in sourceRows if source.stem==logical]
        actual={Path(p).resolve()for p in recipe if p.endswith('.o')}
        assert actual<=set(objects),('Linked object outside actual rows',program,actual-set(objects))
        assert len(primary)==1 and primary[0]in actual,('Wrong primary source object',program,primary)
        for value in recipe:
            if value.endswith(('.o','.a')):
                path=Path(value).resolve();assert sha(path)==receipt['linkedObjects'][value],path;linked[str(path)]=sha(path)
                if path.name=='xlang-analysis-core.a':archives.add(path)
    assert len(archives)==1,archives
    compiler=Path(rows[0]['command'][0]);ar=compiler.with_name('ar.exe'if compiler.suffix=='.exe'else'ar')
    archive=next(iter(archives));assert sha(archive)==receipt['coreSha256']
    members=out/'archive-members';members.mkdir()
    subprocess.run([str(ar),'x','--output='+str(members),str(archive)],check=True,capture_output=True)
    hashes={row['actualObjectSha256']for row in rows};memberHashes={p.name:sha(p)for p in members.iterdir()}
    assert memberHashes and all(h in hashes for h in memberHashes.values()),'Unbound archive member'
    return dict(status='passed',consumers=len(rows),programs=len(receipt['linkRecipes']),required=bindings,
                linkedObjects=linked,archive=str(archive),archiveSha256=sha(archive),archiveMembers=memberHashes)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt',type=Path,required=True)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--require',action='append',default=[])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    receipt=json.loads(args.receipt.read_text(encoding='utf-8-sig'))
    try:
        assert receipt['status']=='passed'
        result=verify(receipt,args.root.resolve(),args.require,out)
    except BaseException as error:
        (out/'result.json').write_text(json.dumps(dict(status='failed',error=repr(error)),indent=2)+'\n')
        raise
    result['receiptSha256']=sha(args.receipt)
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS native current-source bindings',result['consumers'],result['programs'],flush=True)

if __name__=='__main__':main()
