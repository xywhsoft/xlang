"""Fresh Windows DLL sync recovery and embedded-VFS checks, not full53 acceptance.
Products are tested in a source/header/TCC-distribution-free directory. Both
xl.dll and xlc.dll supply their own genuine TCC and checked recovery exports.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    assert os.name=='nt' and not out.exists()
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); objdump=shutil.which('objdump')
    assert cc and objdump
    host=project/'test/cases/stdlib_sync_language_jit_host.c'
    memory=project/'test/support/module_product_memory.c'
    cases=[(project/'test/cases'/('stdlib_sync_held_'+name+'.xl'),mode,global_case)
        for name,mode,global_case in [('mutex',1,0),('read',2,0),('write',3,0),('primary',1,0),('global',1,1)]]
    products=[root/'release'/name for name in ('xl.dll','xlc.dll')]
    assert all(path.is_file() for path in products)
    files={Path(__file__).resolve(),Path(cc),Path(objdump),host,memory,
        project/'test/support/module_product_memory.h',*products,*[row[0] for row in cases]}
    for folder in ('src','lib','release/include','res/tcc'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file())
    files.update(root/'build/tmp'/name for name in ('tcc_builtin_resources.c',
        'tcc_builtin_resources_runtime.c','tcc_bootstrap.exe','xrt.o'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); isolated=out/'isolated'; (isolated/'release').mkdir(parents=True)
    for path in products: shutil.copy2(path,isolated/'release'/path.name)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    rows=[]; failures=[]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'Failures':failures,'SyncShippingApiVfsAcceptance':False,'ShippingLibraryAcceptance':False,
        'Full53Acceptance':False,'ExternalHeadersUsedForJit':False,'ExternalTccDistributionUsed':False}
    def save(): (out/'sync.shipping.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=isolated,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        if result.returncode: failures.append({'Name':name,'Tail':data[-6000:].decode(errors='replace')})
        save(); return result.returncode==0,data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED','-DSYNC_JIT_EMBEDDED_VFS',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(project)]
    try:
        for product in products:
            run(product.stem+'.exports',[objdump,'-p',product])
            for level in ('O0','O2'):
                name=product.stem+'.'+level; binary=isolated/(name+'.exe')
                good,_=run(name+'.build',[cc,*flags,'-'+level,
                    '-DSYNC_JIT_TCC_DLL_NAME="'+product.name+'"',host,memory,
                    isolated/'release'/product.name,'-o',binary])
                if not good: continue
                # The loader resolves the import beside the host, but the
                # explicit TCC path stays in isolated/release. Both are same bytes.
                shutil.copy2(product,isolated/product.name)
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
    print('Sync shipping API/VFS: 26 phases, isolated real DLL recovery and lifetimes balanced',flush=True)
if __name__=='__main__': main()
