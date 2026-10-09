"""Fresh shared-library sync recovery and embedded-VFS checks, not full53 acceptance.
Products are tested in a source/header/TCC-distribution-free directory. Both
runtime and compiler libraries supply their own TCC and checked recovery exports.
Linux libc headers, CRT objects and the dynamic linker remain target-system inputs.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists(); windows=os.name=='nt'
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); objdump=shutil.which('objdump')
    assert cc and objdump
    host=project/'test/cases/stdlib_sync_language_jit_host.c'
    memory=project/'test/support/module_product_memory.c'
    cases=[(project/'test/cases'/('stdlib_sync_held_'+name+'.xl'),mode,global_case)
        for name,mode,global_case in [('mutex',1,0),('read',2,0),('write',3,0),('primary',1,0),('global',1,1)]]
    products=[root/'release'/name for name in (('xl.dll','xlc.dll') if windows else ('libxl.so','libxlc.so'))]
    assert all(path.is_file() for path in products)
    files={Path(__file__).resolve(),Path(cc),Path(objdump),host,memory,
        project/'test/support/module_product_memory.h',*products,*[row[0] for row in cases]}
    for folder in ('src','lib','release/include','res/tcc'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file())
    build=root/('build/tmp' if windows else 'build/linux')
    files.update(build/name for name in ('tcc_builtin_resources.c',
        'tcc_builtin_resources_runtime.c','tcc_bootstrap.exe' if windows else 'tcc','xrt.o'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); isolated=out/'isolated'; (isolated/'release').mkdir(parents=True)
    tmp=out/'tmp'; tmp.mkdir()
    env={key:value for key,value in os.environ.items() if not key.startswith('XL6_')
        and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    for path in products: shutil.copy2(path,isolated/'release'/path.name)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    rows=[]; failures=[]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'Platform':os.name,
        'Failures':failures,'SyncShippingApiVfsAcceptance':False,'ShippingLibraryAcceptance':False,
        'Full53Acceptance':False,'ExternalXlangHeadersUsedForJit':False,'ExternalTccDistributionUsed':False,
        'TargetSystemLibcHeadersCrtAndLinkerRequired':not windows}
    def save(): (out/'sync.shipping.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=isolated,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        if result.returncode: failures.append({'Name':name,'Tail':data[-6000:].decode(errors='replace')})
        save(); return result.returncode==0,data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED','-DSYNC_JIT_EMBEDDED_VFS',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(project)]
    if not windows: flags+=['-D_GNU_SOURCE']
    try:
        for product in products:
            run(product.stem+'.exports',[objdump,'-p',product])
            for level in ('O0','O2'):
                name=product.stem+'.'+level; binary=isolated/(name+('.exe' if windows else ''))
                libraries=([isolated/'release'/product.name] if windows else
                    ['-L'+str(isolated/'release'),'-l'+product.stem[3:],
                        '-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm'])
                good,_=run(name+'.build',[cc,*flags,'-'+level,
                    '-D'+('SYNC_JIT_TCC_DLL_NAME' if windows else 'SYNC_JIT_TCC_SO_NAME')+'="'+product.name+'"',
                    host,memory,*libraries,'-o',binary])
                if not good: continue
                # The loader resolves the import beside the host, but the
                # explicit TCC path stays in isolated/release. Both are same bytes.
                if windows: shutil.copy2(product,isolated/product.name)
                for fixture,mode,global_case in cases:
                    phase=name+'.'+fixture.stem
                    good,text=run(phase,[binary,isolated,'-',out/(phase+'.temps'),fixture,mode,global_case])
                    if good and 'lifetimes balanced' not in text:
                        failures.append({'Name':phase,'Tail':'Missing actual balanced-lifetime proof'})
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; assert not failures and len(rows)==26
        report['Complete']=True; report['SyncShippingApiVfsAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Sync shipping API/VFS: 26 phases, isolated real shared-library recovery and lifetimes balanced',flush=True)
if __name__=='__main__': main()
