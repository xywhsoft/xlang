"""Build an explicit immutable-schema compiler override for semaphore migration.
This is NOT a coherent source build or shipping-library acceptance. Existing
compiler inputs/objects are hash checked, then all affected IR/Cgen/API and
native owner providers are rebuilt. New native C has no warning exclusions.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','schema-root','compiler','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,schema,compiler,out=[getattr(args,name).resolve() for name in ('root','schema_root','compiler','out')]
    assert not out.exists(); windows=os.name=='nt'; cc=shutil.which('gcc'); assert cc
    manifest=compiler/('overlay.json' if windows else 'overlay.linux.json')
    prior=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert prior['Complete'] and prior['InputsUnchanged']
    sources=['src/ir/xl6_ir.c','src/cgen/xl6_ir_cgen.c','src/compile/xl6_compile_unit.c',
        'src/api/xl_api.c','src/api/xl_api_symbols.c','src/sem/xl6_sem.c','src/runtime/xl6_semaphore.c',
        'src/runtime/xl6_array_semaphore_admission.c','src/runtime/xl6_tree.c',
        'src/runtime/xl6_tree_admission.c','src/runtime/xl6_array_sequence_admission.c',
        *['src/runtime/xl6_queue_'+family+suffix+'.c' for family in ('spsc','mpsc','mpmc') for suffix in ('','_admission')]]
    files={Path(__file__).resolve(),Path(cc),manifest}
    for row in prior['Inputs']+prior['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (schema/'src').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    shutil.copytree(compiler/'include',out/'include')
    rows=[]; objects=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,
        'Inputs':inputs,'Objects':objects,'Rows':rows}
    target=out/('overlay.json' if windows else 'overlay.linux.json')
    def save(): target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert not result.returncode,(name,data[-6000:].decode(errors='replace'))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-O0','-DXL_STATIC_EMBED',
        '-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
        '-I'+str(schema/'src/api'),'-I'+str(schema),'-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc')]
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation']
    # The Windows compiler imports the real configured runtime-DLL TCC API.
    # Its original source build is NOT a statically embedded vendor libtcc.
    if not windows: flags+=['-D_GNU_SOURCE','-DXL_STATIC_TCC']
    try:
        for source in sources:
            obj=out/(Path(source).stem+'.o')
            run(obj.stem,[cc,*flags,*([] if source.startswith('src/runtime/') else compat),
                '-c',schema/source,'-o',obj])
            objects.append({'Path':str(obj),'Sha256':sha(obj),'Source':source})
            print('compiled '+source,flush=True)
        names={Path(row['Path']).name for row in objects}
        objects.extend(row for row in prior['Objects'] if Path(row['Path']).name not in names)
        if windows:
            runtime=root/'release/xl.dll'; shutil.copy2(runtime,out/'xl.dll')
            libraries=[runtime,'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
        else:
            libraries=['-L'+str(root/'release'),'-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),
                '-Wl,--export-dynamic','-pthread','-ldl','-lm']
        driver=out/('public_source.exe' if windows else 'xl')
        run('link',[cc,*[row['Path'] for row in objects],*libraries,'-o',driver])
        report.update(Driver=str(driver),DriverSha256=sha(driver))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Semaphore diagnostic compiler linked: '+str(driver),flush=True)

if __name__=='__main__': main()
