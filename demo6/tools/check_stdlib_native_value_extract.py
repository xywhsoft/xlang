"""Strict native/wire owning extraction closure; not full language/shipping acceptance."""
from pathlib import Path
import argparse,ast,hashlib,json,os,shutil,subprocess,sys
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','out'): p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args()
    root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc')
    assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    population=root/'tools/check_stdlib_dict_native.py'
    tree=ast.parse(population.read_text(encoding='utf-8'))
    sources=ast.literal_eval(next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign) and
        any(isinstance(t,ast.Name) and t.id=='sources' for t in n.targets)))
    sources[0]='test/cases/stdlib_native_value_extract.c'; sources[1]='test/support/native_value_native.c'
    sources+=['src/runtime/xl6_slot_map.c','src/runtime/xl6_slot_map_key.c','src/runtime/xl6_slot_map_key_value.c',
        'src/runtime/xl6_array_native_value_admission.c','src/runtime/xl6_array.c',
        'src/runtime/xl6_native_leaf_value.c','src/runtime/xl6_array_extract_native_value.c']
    files={Path(__file__).resolve(),population,Path(cc),Path(sys.executable),*[root/s for s in sources],
        root/'test/support/slot_map_key_value_native.c'}
    for folder in ('src/runtime','src/api','src/type','release/include','lib/xrt','lib/xruntime','test/support'):
        files.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(parents=True); rows=[]; objects=[]; products=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Objects=objects,Products=products,
        SelectedNativeClosureSanitized=a.sanitize,Full53Acceptance=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']; links=[]; env=dict(os.environ)
    if os.name!='nt': flags+=['-D_GNU_SOURCE']; links+=['-pthread','-lm']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        links+=['-fsanitize=address,undefined','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log'); log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert not r.returncode,(name,(r.stdout+r.stderr)[-6000:])
    try:
        for opt in ('O0','O2'):
            current=[]
            for at,source in enumerate(sources):
                obj=out/(opt+'-'+str(at)+'.o'); run(opt+'-'+str(at),[cc,*flags,'-'+opt,'-c',source,'-o',obj])
                objects.append(dict(Path=str(obj),Sha256=sha(obj))); current.append(obj)
            product=out/(opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-link',[cc,*current,*links,'-o',product]); run(opt+'-boundary',[product])
            products.append(dict(Path=str(product),Sha256=sha(product)))
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    assert report['Complete'] and report['InputsUnchanged']; print('Owning native extraction O0/O2 closure passed')
if __name__=='__main__': main()
