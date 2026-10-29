"""Export a fresh selected Git index snapshot; do not consume unrelated edits.
Build products, when requested, come only from an explicit vendor-TCC candidate.
This creates evidence, not a commit or any source/shipping acceptance claim.
"""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess
from stdlib_manifest_inputs import index_manifest_inputs

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def blob_hashes(stream,count=None):
    """Hash raw and CRLF-normalized bytes with bounded memory.

    A CR at a block boundary must be normalized together with the following
    LF; a lone/trailing CR is preserved. None means read through file EOF.
    """
    raw=hashlib.sha256();normalized=hashlib.sha256();tail=b''
    while count is None or count:
        block=stream.read(65536 if count is None else min(count,65536))
        if not block:
            assert count is None or count==0,'Truncated Git blob'
            break
        if count is not None:count-=len(block)
        raw.update(block);block=tail+block
        tail=b'\r' if block.endswith(b'\r') else b''
        if tail:block=block[:-1]
        normalized.update(block.replace(b'\r\n',b'\n'))
    normalized.update(tail)
    return raw.hexdigest(),normalized.digest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--runtime-root',type=Path)
    args=parser.parse_args(); project=Path(__file__).resolve().parents[2]
    out=args.out.resolve(); runtime=args.runtime_root.resolve() if args.runtime_root else None
    assert not out.exists() and not out.is_relative_to(project)
    assert runtime is None or (runtime/'release/xl.dll').is_file()
    paths=[''+name for name in ('src','lib','tools','docs','test',
        'res','release/include','include','build.bat','build_dll.bat','build_linux.sh','.gitattributes','.gitignore')]
    # The ordinary Windows build compiles its committed host examples too.
    # Export source fixtures, never old binaries or extension packages.
    paths.append('release/examples/*.c')
    paths.append('release/examples/*.xl')
    index=subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()
    base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=project,text=True).strip()
    files=subprocess.check_output(['git','ls-files','-z','--',*paths],cwd=project)
    names={path.decode('utf-8') for path in files.split(b'\0') if path};assert names
    all_files=subprocess.check_output(['git','ls-files','-z','--','demo6'],cwd=project)
    relative=[path.decode('utf-8')[len(''):] for path in all_files.split(b'\0') if path]
    def indexed_text(name):
        return subprocess.check_output(['git','show',':'+name],cwd=project).decode('utf-8-sig')
    manifest_sources,manifest_closure=index_manifest_inputs(relative,indexed_text)
    names=sorted(names|{''+name for name in manifest_sources})
    files=b''.join(name.encode('utf8')+b'\0' for name in names)
    stages=subprocess.check_output(['git','ls-files','--stage','-z','--','demo6'],cwd=project)
    modes={row.split(b'\t',1)[1].decode('utf8'):row.split(b' ',1)[0]
        for row in stages.split(b'\0') if row}
    assert all(modes[name] in (b'100644',b'100755') for name in names),'Only regular source files may be exported'
    assert subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()==index
    out.mkdir(parents=True); frozen=out/'frozen'; frozen.mkdir()
    subprocess.run(['git','-c','core.autocrlf=false','checkout-index','--stdin','-z',
        '--prefix='+frozen.as_posix()+'/'],cwd=project,input=files,check=True)
    # Keep a single Git process, but request and validate one object at a time.
    # Neither all source blobs nor even one large blob are buffered in RAM.
    rows=[]; objects=subprocess.Popen(['git','cat-file','--batch'],cwd=project,
        stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    try:
        for name in names:
            objects.stdin.write(b':'+name.encode('utf-8')+b'\n');objects.stdin.flush()
            header=objects.stdout.readline().split()
            assert len(header)==3 and header[1]==b'blob',name
            index_sha,index_normalized=blob_hashes(objects.stdout,int(header[2]))
            assert objects.stdout.read(1)==b'\n',name
            with (frozen/name).open('rb') as exported:
                frozen_sha,frozen_normalized=blob_hashes(exported)
            # An explicit eol=crlf attribute may control exported batch files.
            assert index_normalized==frozen_normalized,name
            rows.append({'Path':name,'IndexSha256':index_sha,'FrozenSha256':frozen_sha})
        objects.stdin.close();assert objects.stdout.read(1)==b''
        assert objects.wait()==0
    finally:
        if objects.poll() is None:objects.terminate();objects.wait()
        objects.stdout.close()
        if not objects.stdin.closed:objects.stdin.close()
    products=[]
    for name in (('xl.dll','xlc.dll') if runtime else ()):
        source=runtime/'release'/name; assert source.is_file()
        target=frozen/'release'/name; assert not target.exists()
        shutil.copy2(source,target); assert sha(source)==sha(target)
        products.append({'Path':str(target),'Source':str(source),'Sha256':sha(target)})
    assert subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()==index
    (out/'freeze.json').write_text(json.dumps({'Scope':__doc__,'Complete':True,
        'IndexTree':index,'Base':base,'Files':rows,'CopiedRuntimeCandidates':products,
        'ManifestSourceClosure':manifest_closure,'CompleteIndexedTestControls':True,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False},indent=2)+'\n',encoding='utf-8')
    print('Exported '+str(len(rows))+' isolated index inputs to '+str(frozen),flush=True)
if __name__=='__main__': main()
