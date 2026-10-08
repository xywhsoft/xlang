"""Actual TCC compilation of verified resident-source selection; Windows DLLs.
Native object tables (not DLL export visibility) prove exact private provider
selection. DLL execution uses only its own embedded runtime and OS/CRT imports,
including terminal runtime/TLS retirement before real unload. This external
source proof does not rebuild or certify the released embedded VFS/shipping DLL.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,compiler,out=[getattr(args,name).resolve() for name in ('root','compiler','out')]
    assert os.name=='nt' and not out.exists()
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); nm=shutil.which('nm'); objdump=shutil.which('objdump')
    assert cc and nm and objdump
    manifest=compiler/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly']
    fixtures=[project/'test/cases'/('stdlib_sync_artifact_'+name+'.xl') for name in ('mutex','cond','rwlock','policy')]
    contract=project/'test/cases/xlir_sync_artifact_contract.c'
    host=project/'test/cases/stdlib_sync_artifact_host.c'
    files={Path(__file__).resolve(),Path(cc),Path(nm),Path(objdump),manifest,Path(build['Driver']),contract,host,*fixtures}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (compiler/'include').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); work=out/'work'; work.mkdir(); isolated=out/'isolated'; isolated.mkdir()
    # Real development layout for the same bytes, not a generated VFS and not
    # an older provider object. Source discovery is based on the driver's path.
    for folder in ('src','res','release/include'):
        shutil.copytree(root/folder,work/folder)
    for folder in ('lib/xrt','lib/xruntime'): (work/folder).mkdir(parents=True)
    shutil.copy2(root/'release/include/xrt_decl.h',work/'lib/xrt/xrt.h')
    shutil.copy2(root/'lib/xruntime/xruntime.h',work/'lib/xruntime/xruntime.h')
    shutil.copy2(root/'release/xl.dll',work/'release/xl.dll')
    shutil.copy2(root/'release/xl.dll',work/'xl.dll')
    driver=work/'public_source.exe'; shutil.copy2(build['Driver'],driver)
    shutil.copytree(compiler/'include',work/'include')
    staged=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(work.rglob('*')) if path.is_file()]
    tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Staged':staged,'Rows':rows,
        'ActualTccNativeFamilySelectionAcceptance':False,'StandaloneDllExecutionAcceptance':False,
        'StandaloneStaticXrtPruningAcceptance':False,'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    def save(): (out/'sync.artifact.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=root):
        command=list(map(str,command)); result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace')); return data.decode(errors='replace')
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']
    libraries=[*objects,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc')]
    shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    try:
        for level in ('O0','O2'):
            binary=out/('contract.'+level+'.exe')
            run('contract.'+level+'.build',[cc,*flags,'-'+level,contract,*libraries,'-o',binary])
            dll_host=isolated/('dll-host.'+level+'.exe')
            run('dll-host.'+level+'.build',[cc,*flags,'-Wshadow','-'+level,host,'-o',dll_host])
            for index,fixture in enumerate(fixtures):
                name=fixture.stem+'.'+level; obj=out/(name+'.o')
                run(name+'.tcc-compile',[binary,fixture,root,root/'res/tcc/win64/x86_64',compiler/'include',obj])
                symbols=run(name+'.symbols',[nm,'--defined-only',obj])
                for family,bit in (('Mutex',1),('Cond',2),('RWLock',4)):
                    assert ('xl6'+family+'CreateV1' in symbols)==(index<3 and bit==(1<<index)),(name,family)
                prefix=work/name
                run(name+'.dll',[driver,fixture,'dll',prefix],work)
                dll=isolated/(name+'.dll'); shutil.copy2(prefix.with_suffix(prefix.suffix+'.dll'),dll)
                imports=run(name+'.imports',[objdump,'-p',dll])
                assert 'DLL Name: xl.dll' not in imports and 'DLL Name: xlc.dll' not in imports
                assert 'actual exported program and checked cleanup passed' in run(name+'.run',[dll_host,dll],isolated)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs+staged)
        assert report['InputsUnchanged']; report['Complete']=True
        report['ActualTccNativeFamilySelectionAcceptance']=True; report['StandaloneDllExecutionAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Sync artifacts: '+str(len(rows))+' phases; actual selected TCC providers and standalone DLLs',flush=True)
if __name__=='__main__': main()
