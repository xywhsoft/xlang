"""Actual TCC native and selected-XRT object tables for synchronization fixtures.
A real full-runtime fallback is the negative control for omitted kernel families.
Windows also executes standalone DLLs with runtime/TLS retirement before unload.
Linux proves kernel/native selection only, not standalone ELF execution. This
external source proof does not certify released embedded VFS/shipping libraries.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,compiler,out=[getattr(args,name).resolve() for name in ('root','compiler','out')]
    assert not out.exists(); windows=os.name=='nt'
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); nm=shutil.which('nm'); objdump=shutil.which('objdump')
    assert cc and nm and objdump
    manifest=compiler/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly']
    assert sha(build['Driver'])==build['DriverSha256'].lower()
    native=Path(build['NativeRoot']) if not windows else None
    tcc_lib=root/'res/tcc/win64/x86_64' if windows else native/'build/linux/tcc-dist'
    fixtures=[project/'test/cases'/('stdlib_sync_artifact_'+name+'.xl') for name in ('mutex','cond','rwlock','policy')]
    contract=project/'test/cases/xlir_sync_artifact_contract.c'
    host=project/'test/cases/stdlib_sync_artifact_host.c'
    files={Path(__file__).resolve(),Path(cc),Path(nm),Path(objdump),manifest,Path(build['Driver']),contract,host,*fixtures}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (compiler/'include').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); work=out/'work'; work.mkdir(); isolated=out/'isolated'; isolated.mkdir()
    if windows:
        # Real development layout for the same bytes, not a generated VFS and
        # not an older provider object. Discovery follows the driver's path.
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
    rows=[]; kernel_proofs=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Staged':staged,'Rows':rows,
        'KernelProofs':kernel_proofs,
        'Platform':os.name,
        'ActualTccNativeFamilySelectionAcceptance':False,'StandaloneDllExecutionAcceptance':False,
        'StandaloneStaticXrtPruningAcceptance':False,'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    report['StandaloneElfExecutionAcceptance']=False
    def save(): (out/'sync.artifact.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=root):
        command=list(map(str,command)); result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace')); return data.decode(errors='replace')
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        {'xlir_public_source_driver.o','xlir_public_source_linux_driver.o'}]
    libraries=([*objects,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
        if windows else [*objects,'-L'+str(native/'release'),'-lxl','-Wl,-rpath,'+str(native/'release'),
            '-Wl,--export-dynamic','-pthread','-ldl','-lm'])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc')]
    if windows: shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    else: flags+=['-D_GNU_SOURCE']
    try:
        for level in ('O0','O2'):
            binary=out/('contract.'+level+('.exe' if windows else ''))
            run('contract.'+level+'.build',[cc,*flags,'-'+level,contract,*libraries,'-o',binary])
            if windows:
                dll_host=isolated/('dll-host.'+level+'.exe')
                run('dll-host.'+level+'.build',[cc,*flags,'-Wshadow','-'+level,host,'-o',dll_host])
            full=out/('full-kernels.'+level+'.o')
            run('full-kernels.'+level+'.tcc-compile',[binary,fixtures[3],root,
                tcc_lib,compiler/'include',full,'full'])
            full_symbols=set(re.findall(r'^\S+\s+[A-Za-z]\s+(\S+)\s*$',
                run('full-kernels.'+level+'.symbols',[nm,'--defined-only',full]),re.M))
            anchors={'xrtMutexInit','xrtCondInit','xrtRWLockInit','xrtSemInit','xrtFileOpen','xrtProcessRun'}
            assert anchors.issubset(full_symbols),'The real full-runtime negative control must contain all anchors'
            for index,fixture in enumerate(fixtures):
                name=fixture.stem+'.'+level; obj=out/(name+'.o')
                run(name+'.tcc-compile',[binary,fixture,root,tcc_lib,compiler/'include',obj])
                symbols=run(name+'.symbols',[nm,'--defined-only',obj])
                for family,bit in (('Mutex',1),('Cond',2),('RWLock',4)):
                    assert ('xl6'+family+'CreateV1' in symbols)==(index<3 and bit==(1<<index)),(name,family)
                kernels=out/(name+'.kernels.o')
                run(name+'.kernels.tcc-compile',[binary,fixture,root,tcc_lib,
                    compiler/'include',kernels,'selected'])
                defined=set(re.findall(r'^\S+\s+[A-Za-z]\s+(\S+)\s*$',
                    run(name+'.kernels.symbols',[nm,'--defined-only',kernels]),re.M))
                expected=[{'xrtMutexInit'},{'xrtMutexInit','xrtCondInit'},{'xrtRWLockInit'},set()][index]
                assert defined.intersection(anchors)==expected,(name,defined.intersection(anchors),expected)
                kernel_proofs.append({'Fixture':str(fixture),'HostOptimization':level,
                    'PresentAnchors':sorted(expected),'AbsentAnchors':sorted(anchors-expected),
                    'SelectedObjectSha256':sha(kernels),'FullControlObjectSha256':sha(full),
                    'SelectedObjectBytes':kernels.stat().st_size,'FullControlObjectBytes':full.stat().st_size})
                if windows:
                    prefix=work/name
                    run(name+'.dll',[driver,fixture,'dll',prefix],work)
                    dll=isolated/(name+'.dll'); shutil.copy2(prefix.with_suffix(prefix.suffix+'.dll'),dll)
                    imports=run(name+'.imports',[objdump,'-p',dll])
                    assert 'DLL Name: xl.dll' not in imports and 'DLL Name: xlc.dll' not in imports
                    assert 'actual exported program and checked cleanup passed' in run(name+'.run',[dll_host,dll],isolated)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs+staged)
        assert report['InputsUnchanged']; report['Complete']=True
        report['ActualTccNativeFamilySelectionAcceptance']=True; report['StandaloneDllExecutionAcceptance']=windows
        report['StandaloneStaticXrtPruningAcceptance']=len(kernel_proofs)==8
        assert report['StandaloneStaticXrtPruningAcceptance']
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Sync artifacts: '+str(len(rows))+' phases; actual selected TCC providers/XRT kernels'+
        (' and standalone DLL execution' if windows else '; standalone ELF execution not tested'),flush=True)
if __name__=='__main__': main()
