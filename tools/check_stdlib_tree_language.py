"""Actual Tree language C/AOT, closed foreign-error ABI and deterministic direct provider calls.
JIT is tested separately against the packaged current-source public compiler.
"""
from __future__ import annotations
import argparse, hashlib, json, os, shutil, subprocess, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--public',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists() and (os.name=='nt')==bool(args.public)
    files={Path(__file__).resolve(),Path(cc)}
    cases=['xlir_avl_resource_abi','stdlib_tree_native_projection','stdlib_tree_native_reference']
    sources=['test/cases/stdlib_tree_native_language_host.c','test/support/module_product_memory.c']
    files.update(root/f for f in sources+['test/support/module_product_memory.h'])
    files.update(root/('test/cases/'+case+'.xl') for case in cases)
    for folder in ('src','lib','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    if args.public:
        public=args.public.resolve(); build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        cli=Path(build['Driver']); assert sha(cli)==build['DriverSha256'].lower()
        links=[*[row['Path'] for row in build['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o'],
               str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
        include=public/'include'; suffix='.exe'
    else:
        cli=root/'release/xl'; include=out/'include'; suffix=''
        links=['-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
        files.update(root/('release/'+name) for name in ('libxl.so','libxlc.so'))
    files.add(cli)
    inputs=[{'Path':str(path.resolve()),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    if args.public: shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    else:
        private=include/'__xl_runtime'; private.mkdir(parents=True)
        for folder in (root/'src/runtime',root/'release/include'):
            for header in folder.glob('*.h'): shutil.copy2(header,private/header.name)
    rows=[]; proofs=[]
    report={'Complete':False,'InputsUnchanged':False,'Full53Acceptance':False,
            'LanguageGraphAcceptance':False,'Platform':os.name,'Inputs':inputs,'Rows':rows,'Proofs':proofs,'Scope':__doc__}
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start})
        if result.returncode: raise RuntimeError(name+': '+data[-5000:].decode(errors='replace'))
        return data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease/include','-I'+str(include),
           '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
           '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    if not args.public: flags+=['-D_GNU_SOURCE']
    try:
        for case in cases:
            fixture=root/('test/cases/'+case+'.xl'); generated=out/(case+'.c'); repeat=out/(case+'.repeat.c')
            for name,target in [('emit',generated),('repeat',repeat)]:
                run(case+'.'+name,[cli,fixture,'cgen-default',target] if args.public else [cli,'cgen',fixture,'--emit',target])
            assert generated.read_bytes()==repeat.read_bytes()
            text=generated.read_text(encoding='utf-8')
            assert 'xlir_avl_' not in text and 'xl6TreeCreateTakeV1(' in text
            assert 'const XL6_TreeLayoutV2 tree_layout' in text
            ir=out/(case+'.ir.json')
            run(case+'.ir',[cli,fixture,'ir-lowered',ir] if args.public else [cli,'ir',fixture,'--lowered','--format','json','--emit',ir])
            data=json.loads(ir.read_text(encoding='utf-8')); types={row['id']:row for row in data['types']}
            for tree in (row for row in data['types'] if row.get('runtimeResourceAbiName')=='avl-tree' and len(row.get('params',[]))==2):
                for field,index in [('treeKeySchemaType',0),('treeValueSchemaType',1)]:
                    schema=types[tree[field]]; assert schema['params']==[tree['params'][index]]
            for level in (0,2):
                binary=out/(case+'.O'+str(level)+suffix)
                run(case+'.O'+str(level)+'.build',[cc,*flags,'-O'+str(level),
                    '-DXLIR_GENERATED_MODULE="'+str(generated).replace('\\','/')+'"',*sources,*links,'-o',binary])
                run(case+'.O'+str(level)+'.run',[binary])
            proofs.append({'Source':case,'CDeterministic':True,'LegacyTreeImplementationAbsent':True,
                'ClosedKeyValueSchemas':True,'GeneratedSha256':sha(generated),'AotLevels':[0,2]})
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); raise
    finally:
        (out/'tree.language.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('Native Tree actual C/AOT language: '+str(len(rows))+' phases balanced',flush=True)
if __name__=='__main__': main()
