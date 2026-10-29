"""Strict native BUSY schema admission with actual Queue operation pins.
Only the three selected provider bodies and host are rebuilt/instrumented;
this diagnostic override is not whole-runtime or shipping-product acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('root','schema-root','compiler','out'): parser.add_argument('--'+n,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root,schema,compiler,out=[getattr(args,n).resolve() for n in ('root','schema_root','compiler','out')]
    assert not out.exists() and (not args.sanitize or os.name!='nt'); cc=shutil.which('gcc'); assert cc
    manifest=compiler/('overlay.json' if os.name=='nt' else 'overlay.linux.json')
    build=json.loads(manifest.read_text(encoding='utf-8-sig')); assert build['Complete'] and build['InputsUnchanged']
    host=Path(__file__).resolve().parents[1]/'test/cases/stdlib_queue_reference_busy_native_host.c'
    providers=['xl6_queue_spsc','xl6_queue_mpsc','xl6_queue_mpmc']
    files={Path(__file__).resolve(),Path(cc),manifest,host,root/'test/support/module_product_memory.c',root/'test/support/module_product_memory.h'}
    for row in build['Inputs']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(p for p in (schema/'src').rglob('*') if p.suffix in ('.h','.c','.inc'))
    inputs=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(); rows=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,'Full53Acceptance':False,
        'ProvidersInstrumented':args.sanitize,'WholeRuntimeInstrumented':False,'Inputs':inputs,'Rows':rows}
    def save(): (out/'queue.reference.busy.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert not result.returncode,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(schema),'-I.','-Irelease','-Irelease/include']
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    omitted={n+'.o' for n in providers}|{'xl6_xlc_shell.o','xlir_public_source_driver.o'}
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in omitted]
    if os.name=='nt':
        shutil.copy2(root/'release/xl.dll',out/'xl.dll'); suffix='.exe'
        links=[*objects,str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        suffix=''; links=[*objects,'-L'+str(root/'release'),'-lxlc','-lxl',
            '-Wl,-rpath,'+str(root/'release'),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
    try:
        for optimization in ('O0','O2'):
            selected=[]
            for name in providers:
                obj=out/(name+'.'+optimization+'.o')
                run(name+'.'+optimization+'.build',[cc,*flags,'-'+optimization,'-c',schema/'src/runtime'/(name+'.c'),'-o',obj]); selected.append(obj)
            binary=out/('busy.'+optimization+suffix)
            run(optimization+'.build',[cc,*flags,'-'+optimization,host,'test/support/module_product_memory.c',*selected,*links,'-o',binary])
            assert 'last outside shell release balanced' in run(optimization+'.run',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Queue BUSY schema: {len(rows)} phases; actual active pins and three-family last-shell release balanced',flush=True)
if __name__=='__main__': main()
