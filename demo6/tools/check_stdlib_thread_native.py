"""Strict actual Thread owner/closure diagnostics against a real runtime library.
Only the new provider and test host are built/instrumented. This is not yet
language Cgen integration, coherent shipping products or full53 acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    instrumentation=parser.add_mutually_exclusive_group()
    instrumentation.add_argument('--sanitize',action='store_true')
    instrumentation.add_argument('--race',action='store_true')
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists(); windows=os.name=='nt'; assert not (args.sanitize or args.race) or not windows
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); assert cc
    product=root/'release'/('xl.dll' if windows else 'libxl.so'); assert product.is_file()
    provider=project/'src/runtime/xl6_thread.c'; host=project/'test/cases/stdlib_thread_native_host.c'
    memory=project/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(cc),product,provider,host,memory,
        project/'test/support/module_product_memory.h'}
    for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime'):
        files.update((project/folder).glob('*.h'))
    files.update((root/'release/include').glob('*.h'))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); rows=[]; failures=[]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'Failures':failures,'LanguageCgenAcceptance':False,'SourceProductAcceptance':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False,'WholeRuntimeInstrumented':False,
        'ProviderHostInstrumentation':'thread' if args.race else 'address,undefined' if args.sanitize else None,
        'RaceStressOnly':args.race,
        'OsThreadJoinOrImageUnloadAcceptance':False}
    def save(): (out/'thread.native.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=out,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        if result.returncode: failures.append({'Name':name,'Tail':data[-6000:].decode(errors='replace')})
        save(); return result.returncode==0,data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(project),'-I'+str(root),'-I'+str(root/'release/include')]
    if not windows: flags+=['-D_GNU_SOURCE']
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    if args.race: flags+=['-g','-fsanitize=thread','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    if windows:
        shutil.copy2(product,out/product.name); links=[product]
    else: links=['-L'+str(root/'release'),'-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    links.append('-Wl,--wrap=xrtThreadWait')
    try:
        for level in ('O0','O2'):
            obj=out/('thread.'+level+'.o'); binary=out/('host.'+level+('.exe' if windows else ''))
            good,_=run(level+'.provider',[cc,*flags,'-'+level,'-c',provider,'-o',obj])
            if not good: continue
            good,_=run(level+'.host',[cc,*flags,'-'+level,host,memory,obj,*links,'-o',binary])
            if not good: continue
            modes=('methods','stress') if args.race else ('vectors','graph','stop','drop-freeze',
                'active-release','lease','self-drop','capture-retirement','errors','oom','stress','methods')
            for mode in modes:
                good,text=run(level+'.'+mode,[binary,mode])
                if good and 'lifetimes balanced' not in text: failures.append({'Name':mode,'Tail':'Missing lifetime proof'})
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'] and not failures and len(rows)==(8 if args.race else 28)
        report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Thread native: '+str(len(rows))+' strict O0/O2 phases, actual owner/worker/closure lifetimes balanced',flush=True)
if __name__=='__main__': main()
