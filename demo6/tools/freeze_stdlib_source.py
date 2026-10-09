"""Export a fresh selected Git index snapshot; do not consume unrelated edits.
Build products are copied only from an explicit candidate for vendor TCC use.
This creates evidence, not a commit or any source/shipping acceptance claim.
"""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--runtime-root',type=Path,required=True)
    args=parser.parse_args(); project=Path(__file__).resolve().parents[2]
    out,runtime=args.out.resolve(),args.runtime_root.resolve()
    assert not out.exists() and (runtime/'release/xl.dll').is_file()
    paths=['demo6/'+name for name in ('src','lib','tools','docs','test/cases','test/support',
        'res','release/include','include','build_dll.bat','build_linux.sh','.gitattributes','.gitignore')]
    files=subprocess.check_output(['git','ls-files','-z','--',*paths],cwd=project)
    names=[path.decode('utf-8') for path in files.split(b'\0') if path]; assert names
    index=subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()
    base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=project,text=True).strip()
    out.mkdir(parents=True); frozen=out/'frozen'; frozen.mkdir()
    subprocess.run(['git','-c','core.autocrlf=false','checkout-index','--stdin','-z',
        '--prefix='+frozen.as_posix()+'/'],cwd=project,input=files,check=True)
    # One process, exact batch contents: avoid thousands of slow per-file Git
    # launches while preserving the same index-vs-export byte verification.
    objects=subprocess.check_output(['git','cat-file','--batch'],cwd=project,
        input=b''.join(b':'+name.encode('utf-8')+b'\n' for name in names))
    rows=[]; offset=0
    for name in names:
        boundary=objects.index(b'\n',offset); header=objects[offset:boundary].split()
        assert len(header)==3 and header[1]==b'blob',name
        count=int(header[2]); offset=boundary+1; blob=objects[offset:offset+count]; offset+=count
        assert len(blob)==count and objects[offset:offset+1]==b'\n',name
        offset+=1
        actual=(frozen/name).read_bytes()
        # An explicit eol=crlf attribute may still control exported batch files.
        assert actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),name
        rows.append({'Path':name,'IndexSha256':hashlib.sha256(blob).hexdigest(),
            'FrozenSha256':sha(frozen/name)})
    assert offset==len(objects)
    products=[]
    for name in ('xl.dll','xlc.dll'):
        source=runtime/'release'/name; assert source.is_file()
        target=frozen/'demo6/release'/name; assert not target.exists()
        shutil.copy2(source,target); assert sha(source)==sha(target)
        products.append({'Path':str(target),'Source':str(source),'Sha256':sha(target)})
    assert subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()==index
    (out/'freeze.json').write_text(json.dumps({'Scope':__doc__,'Complete':True,
        'IndexTree':index,'Base':base,'Files':rows,'CopiedRuntimeCandidates':products,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False},indent=2)+'\n',encoding='utf-8')
    print('Exported '+str(len(rows))+' isolated index inputs to '+str(frozen),flush=True)
if __name__=='__main__': main()
