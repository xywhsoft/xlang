"""Clone the pinned private compiler and apply byte-verified native path fixes."""
import argparse,base64,hashlib,json,os,shutil,stat
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def prepare(seed_root,source_manifest,output):
    seed=Path(seed_root).resolve();manifest=Path(source_manifest).resolve();output=Path(output).absolute()
    patches=Path(__file__).resolve().parent.parent/'third_party/compiler_patches/native-file-paths-v6'
    record=json.loads((patches/'changes.json').read_text(encoding='utf8'))
    assert record['format']=='xlang-ide-private-compiler-patch-1'
    assert sha(manifest)==record['seedSourceManifestSha256'],'Compiler source manifest is not the pinned seed'
    rows=json.loads(manifest.read_text(encoding='utf-8-sig'))['files']
    assert len(rows)==record['seedSourceFiles'] and not output.exists()
    assert not output.resolve().is_relative_to(seed) and not seed.is_relative_to(output.resolve())
    for row in rows:
        p=seed/row['path'];assert p.resolve().is_relative_to(seed)
        assert sha(p)==row['sha256'],row['path']
    for parent,dirs,files in os.walk(seed,followlinks=False):
        for name in dirs+files:
            p=Path(parent)/name;s=p.lstat()
            assert not stat.S_ISLNK(s.st_mode) and not getattr(s,'st_file_attributes',0)&0x400,p
    replacements={}
    for row in record['changedFiles']:
        name=row['path'];p=seed/name;original=p.read_bytes()
        assert hashlib.sha256(original).hexdigest()==row['beforeSha256'],name
        cursor=0;parts=[]
        for edit in row['edits']:
            at=edit['offset'];before=base64.b64decode(edit['removeBase64'],validate=True);after=base64.b64decode(edit['insertBase64'],validate=True)
            assert cursor<=at<=len(original) and original[at:at+len(before)]==before,name
            parts.extend((original[cursor:at],after));cursor=at+len(before)
        parts.append(original[cursor:]);new=b''.join(parts)
        assert hashlib.sha256(new).hexdigest()==row['afterSha256'],name;replacements[name]=new
    # All validation precedes creation. The seed is read-only throughout; a
    # failed fresh clone is retained for inspection and never accepted as built.
    shutil.copytree(seed,output)
    for name,data in replacements.items():(output/name).write_bytes(data)
    expected={row['path']:row['sha256'] for row in rows}
    expected.update({row['path']:row['afterSha256'] for row in record['changedFiles']})
    for name,digest in expected.items():assert sha(output/name)==digest,name
    for row in rows:assert sha(seed/row['path'])==row['sha256'],row['path']
    result={'format':'xlang-ide-prepared-compiler-1','seed':str(seed),'sourceManifest':str(manifest),
        'seedSourceManifestSha256':sha(manifest),'adaptationSha256':sha(patches/'changes.json'),
        'files':[{'path':name,'sha256':digest} for name,digest in expected.items()],
        'changedSources':list(replacements),'prepared':True,'built':False,'formalRelease':False}
    (output/'prepared-native-path-source.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print('Prepared',len(expected),'verified source files in',output,flush=True)
    print('Build with build.bat in this fresh compiler directory; no root demo6 changes are required.',flush=True)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--seed-root',required=True)
    p.add_argument('--source-manifest',required=True);p.add_argument('--output',required=True)
    a=p.parse_args();prepare(a.seed_root,a.source_manifest,a.output)
