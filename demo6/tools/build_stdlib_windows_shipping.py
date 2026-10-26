"""Rebuild a fresh Windows CLI/DLL candidate from the ordinary build.bat.
No old products, XRT objects, compiler/provider overrides or VFS images are
reused. Records genuine source/bootstrap/CLI/DLL/XRT/VFS provenance; this is
build evidence, not runtime, pruning, release promotion or full53 acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess
from stdlib_manifest_inputs import local_manifest_inputs

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def normalized_sha(path): return hashlib.sha256(Path(path).read_bytes().replace(b'\r\n',b'\n')).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    assert os.name=='nt' and not out.exists()
    assert (root/'build.bat').is_file() and not (root/'build/tmp').exists()
    products=[root/'release'/name for name in ('xl.dll','xlc.dll','xl.exe')]
    assert not any(path.exists() for path in products),'Do not overwrite previous products'
    files={Path(__file__).resolve(),root/'build.bat',root/'build_dll.bat'}
    for name in ('gcc','powershell','python'):
        selected=shutil.which(name); assert selected; files.add(Path(selected).resolve())
    for folder in ('src','lib','tools','res','docs','release/include','include','release/examples','test/cases'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file())
    files.update(local_manifest_inputs(root))
    generated={root/'src/api/xl_xrt_symbols.inc',root/'src/stdlib/xl6_stdlib_func_index.inc'}
    inputs=[{'Path':str(path),'Sha256':sha(path),**({'NormalizedSha256':normalized_sha(path)} if path in generated else {})}
        for path in sorted(files)]
    out.mkdir(parents=True); tmp=out/'tmp'; tmp.mkdir()
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_')}
    env.update(TEMP=str(tmp),TMP=str(tmp))
    command=[os.environ['COMSPEC'],'/d','/c','build.bat']
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,
        'Command':command,'BuildDirectory':str(root/'build/tmp'),'Products':[],
        'GeneratedLineEndingChanges':[],'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    def save(): (out/'build.shipping.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    save()
    try:
        with (out/'build.log').open('wb') as log:
            process=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1800)
        report['Exit']=process.returncode; assert process.returncode==0
        assert all(path.is_file() for path in products)
        outputs=[root/'build/tmp'/name for name in ('tcc_bootstrap.exe','xrt.o',
            'tcc_builtin_resources.c','tcc_builtin_resources_runtime.c')]
        assert all(path.is_file() for path in outputs)
        # The two standard generators run before compiling. Windows may
        # rewrite their LF export to CRLF. Permit only that exact normalization
        # and pin the actual bytes consumed; semantic generator drift fails.
        for row in inputs:
            actual=sha(row['Path'])
            if actual!=row['Sha256']:
                assert 'NormalizedSha256' in row and normalized_sha(row['Path'])==row['NormalizedSha256'],row['Path']
                report['GeneratedLineEndingChanges'].append({'Path':row['Path'],'BeforeSha256':row['Sha256'],'AfterSha256':actual})
                row['Sha256']=actual
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Products']=[{'Path':str(path),'Sha256':sha(path)} for path in products+outputs]
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Fresh Windows shipping candidate rebuilt with source TCC/XRT, CLI and both VFS images',flush=True)
if __name__=='__main__': main()
