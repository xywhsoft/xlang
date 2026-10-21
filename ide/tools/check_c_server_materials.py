"""Exercise actual selected provider material and pre-copy integrity rejections."""
import argparse, copy, json, shutil
from pathlib import Path
from package_portable import require, sha, load_json, plain, safe_name, PackageError
from c_server_materials import collect_c_server_materials

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args(); record=load_json(a.manifest); original=Path(record['directory'])
    root=a.output.absolute(); root.mkdir(parents=True,exist_ok=False)
    shutil.copy2(__file__,root/Path(__file__).name)
    expected={name.removeprefix('runtime/'):digest for name,digest in record['files'].items()}
    protected={original/name:digest for name,digest in record['files'].items()}
    for row in record['providerMaterials']['files']:protected[original/record['providerMaterials']['root']/row['path']]=row['sha256']
    protected[a.manifest.absolute()]=sha(a.manifest)
    def run(value,base,selection):
        added=[]
        try:
            result=collect_c_server_materials(value,base,selection,add=lambda *row: added.append(row),
                load_json=load_json,plain=plain,safe_name=safe_name,sha=sha,require=require)
            return result,added,None
        except (PackageError,OSError,ValueError,TypeError) as error:
            assert not added,'Invalid material was copied before validation'
            return None,added,str(error)
    result,added,error=run(record,original,expected)
    assert not error and len(added)==len(record['providerMaterials']['files']) and result['inputTraceProtocol']==1, error
    ordinary,added,error=run({'files':{}},original,expected)
    assert not error and ordinary['inputTraceProtocol']==0 and not added
    checks=[]
    def reject(label,value,base=original,selection=expected):
        _,_,error=run(value,base,selection);assert error,label
        checks.append({'case':label,'rejectedBeforeCopy':True,'reason':error})
    changed=copy.deepcopy(expected);changed['bin/clangd.exe']='0'*64
    reject('binary-does-not-match-material',record,selection=changed)
    for label,mutate in (
        ('boolean-protocol-version',lambda m:m.update(inputTraceProtocol=True)),
        ('formal-release-claim',lambda m:m.update(formalRelease=True)),
        ('missing-notice',lambda m:m.update(files=[r for r in m['files'] if r['target']!='notices/clangd/COPYING.RUNTIME'])),
        ('missing-adaptation-source',lambda m:m.update(files=[r for r in m['files'] if not r['target'].endswith('/support/NativeInput.c')])),
        ('duplicate-material',lambda m:m['files'].append(copy.deepcopy(m['files'][0]))),
        ('runtime-overwrite-target',lambda m:m['files'][0].update(target='toolchains/clangd/bin/clangd.exe')),
        ('traversal-path',lambda m:m['files'][0].update(path='../runtime/bin/clangd.exe')),
        ('missing-original-build-command',lambda m:m.update(files=[r for r in m['files'] if not r['target'].endswith('/build-command.json')])),
    ):
        value=copy.deepcopy(record);mutate(value['providerMaterials']);reject(label,value)
    for label,target,alter,rebind in (
        ('changed-source','sources/clangd-adaptation/clang-tools-extra/clangd/support/NativeInput.c',lambda b:b+b'\n/* changed */\n',False),
        ('source-build-nonzero','provenance/clangd-adaptation/source-build.json',lambda b:json.dumps({**json.loads(b),'actualBuildExitCode':1}).encode(),True),
        ('source-build-boolean-exit','provenance/clangd-adaptation/source-build.json',lambda b:json.dumps({**json.loads(b),'actualBuildExitCode':False}).encode(),True),
        ('rebound-source-different-from-prepared','sources/clangd-adaptation/clang-tools-extra/clangd/support/NativeInput.c',lambda b:b+b'\n/* rebound */\n',True),
    ):
        base=root/label;base.mkdir();shutil.copytree(original/'materials',base/'materials')
        value=copy.deepcopy(record); row=next(r for r in value['providerMaterials']['files'] if r['target']==target)
        file=base/'materials'/row['path']; file.write_bytes(alter(file.read_bytes()))
        if rebind: row.update(sha256=sha(file),size=file.stat().st_size)
        reject(label,value,base)
    for file,digest in protected.items():assert sha(file)==digest,str(file)
    evidence={'status':'selected-c-server-materials-and-rejections-passed','validMaterialFiles':result['copiedFiles'],
        'manifest':str(a.manifest.absolute()),'manifestSha256':sha(a.manifest),'actualSelectedBinarySha256':expected['bin/clangd.exe'],
        'ordinaryClangdCompatible':True,'rejections':checks,'providerInputsUnchanged':True,
        'formalRelease':False,'distributionReviewComplete':False}
    (root/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'status':evidence['status'],'validMaterialFiles':result['copiedFiles'],'rejections':len(checks),'evidence':str(root/'evidence.json')}))
if __name__=='__main__':main()
