"""Declaration-only private VFS closure in actual generated shipping images.
Reads pinned product build inputs, checks all runtime header sibling references,
four automatic header manifests and both real VFS declaration tables.
Does not infer provider implementation/linkage from embedded declarations.
"""
from pathlib import Path
import argparse, hashlib, json, re

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve(); assert not out.exists()
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    headers=sorted((root/'src/runtime').glob('*.h')); assert headers
    for header in headers:
        text=header.read_text(encoding='utf-8')
        assert not re.search(r'^#\s*include "src/runtime/',text,re.M),header
        for sibling in re.findall(r'^#\s*include "(xl6_[^"/]+\.h)"',text,re.M):
            assert (header.parent/sibling).is_file(),(header,sibling)
    for platform in ('linux','win64'):
        for kind in ('runtime','xlc'):
            manifest=root/'tools'/('tcc_vfs_manifest_'+platform+'_'+kind+'.txt')
            text=manifest.read_text(encoding='utf-8').replace('\\','/')
            assert text.count('dir|include/__xl_runtime|%XL6_ROOT%/src/runtime|*.h')==1,manifest
            assert not re.search(r'^file\|include/__xl_runtime/[^|]+\|%XL6_ROOT%/src/runtime/',text,re.M),manifest
    images=[Path(row['Path']) for row in build['Products'] if Path(row['Path']).name in
        ('tcc_builtin_resources.c','tcc_builtin_resources_runtime.c')]; assert len(images)==2
    rows=[]
    for image in images:
        text=image.read_text(encoding='utf-8')
        paths=re.findall(r'\{ "([^"\n]+)", tcc_builtin_res_',text)
        assert paths and len(paths)==len(set(paths)),image
        expected={'include/__xl_runtime/'+header.name.lower() for header in headers}
        assert expected.issubset(paths),(image,sorted(expected-set(paths)))
        private=[path for path in paths if path.startswith('include/__xl_runtime/')]
        assert all(path.endswith('.h') for path in private),private
        if image.name=='tcc_builtin_resources_runtime.c':
            assert not any(path.startswith('__xl_compiler/') for path in paths),image
        rows.append({'Path':str(image),'Sha256':sha(image),'RuntimeHeaders':len(expected),
            'PrivateDeclarationPaths':len(private),'AllPaths':len(paths)})
    for row in build['Inputs']+build['Products']: assert sha(row['Path'])==row['Sha256'],row['Path']
    inputs=build['Inputs']+build['Products']+[{'Path':str(Path(__file__).resolve()),'Sha256':sha(__file__)}]
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps({'Scope':__doc__,'Complete':True,'InputsUnchanged':True,'Inputs':inputs,
        'Rows':rows,'PrivateHeaderClosureAcceptance':True,'ProviderLinkPruningAcceptance':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False},indent=2)+'\n',encoding='utf-8')
    print('Actual private header VFS: '+str(len(headers))+' sibling-closed headers in both generated images')
if __name__=='__main__': main()
