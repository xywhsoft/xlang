#!/usr/bin/env python3
"""Actual private Cgen factory renderer, nine recipe families; source Array remains Value-v1."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    args=parser.parse_args(); root=Path(__file__).resolve().parents[1]; out=args.out.resolve()
    cc=shutil.which('gcc'); fixtures=['stdlib_array_layouts','stdlib_array_future_reference','stdlib_stack_nullable_c_scalar']
    if not cc or out.exists() or (os.name=='nt' and not args.public_directory):
        parser.error('Require gcc, fresh evidence and Windows current-source driver')
    public=None
    if args.public_directory:
        args.public_directory=args.public_directory.resolve()
        public=json.loads((args.public_directory/'build.json').read_text(encoding='utf-8-sig'))
        for row in [*public['Inputs'],*public['Headers'],*public['Objects']]:
            if digest(Path(row['Path'])).lower()!=row['Sha256'].lower(): parser.error('Stale public build: '+row['Path'])
    product=root/('release/xl.dll' if os.name=='nt' else 'release/libxl.so')
    compiler=root/('release/xlc.dll' if os.name=='nt' else 'release/libxlc.so')
    files={Path(__file__),Path(cc),Path(sys.executable),product,compiler,
        root/'test/cases/xlir_sequence_recipe_emit_contract.c',root/'test/cases/stdlib_sequence_recipe_emit_host.c'}
    files.update(root/f'test/cases/{name}.xl' for name in fixtures)
    for folder in ('src/cgen','src/ir','src/runtime','src/compile','release/include'):
        files.update(path for path in (root/folder).rglob('*') if path.suffix in ('.c','.h','.inc'))
    if public:
        files.add(args.public_directory/'build.json')
        files.update(Path(row['Path']) for row in [*public['Inputs'],*public['Headers'],*public['Objects']])
    inputs=[{'Path':str(path.resolve()),'Sha256':digest(path)} for path in sorted(files)]
    out.mkdir(parents=True); private=out/'include/__xl_runtime'; private.mkdir(parents=True)
    for folder in (root/'src/runtime',root/'release/include'):
        for header in folder.glob('*.h'): shutil.copy2(header,private/header.name)
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    rows=[]; proofs=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Rows':rows,'Proofs':proofs}
    strict=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-const-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-missing-field-initializers','-I.','-Irelease','-Irelease/include','-I'+str(out/'include')]
    links=[str(product)] if os.name=='nt' else ['-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    if os.name!='nt': strict+=['-D_GNU_SOURCE']
    def run(name,command):
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        if result.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    try:
        contract=out/('contract.exe' if os.name=='nt' else 'contract')
        compiler_links=([row['Path'] for row in public['Objects'] if Path(row['Path']).name not in
            ('xlir_public_source_driver.o','xl6_ir_cgen.o')]+[str(product),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']) if public else ['-lxlc',*links]
        run('contract.build',[cc,*strict,'-O0','-DXL_STATIC_EMBED','-Ilib/tcc',
            'test/cases/xlir_sequence_recipe_emit_contract.c',*compiler_links,'-o',str(contract)])
        mask=0
        for fixture in fixtures:
            generated=out/(fixture+'.c')
            log=run(fixture+'.contract',[str(contract),f'test/cases/{fixture}.xl',str(generated)])
            proof=re.search(r'Sequence recipe emitter: (\d+) actual recipes; (\d+) refused mutations; layout-mask=(\d+); source carrier unchanged',log)
            assert proof and int(proof[2])==int(proof[1])*15
            mask|=int(proof[3]); proofs.append({'Fixture':fixture,'Recipes':int(proof[1]),'Mutations':int(proof[2]),'LayoutMask':int(proof[3])})
            nested=re.search(r'Nested schema rejection: (\d+) descendant mutations refused before output',log)
            assert nested,'Missing recursive schema rejection proof'
            if fixture=='stdlib_array_layouts': assert int(nested[1])>0,'Missing actual nested recipes'
            proofs[-1]['DescendantMutations']=int(nested[1])
            text=generated.read_text(encoding='utf-8')
            probe=text.split('int xl6_recipe_probe(void)',1)[1]
            assert 'xrtValueArray(' not in probe and 'xl6ArrayFromValueV1' not in probe
            assert 'xl6SequenceNestedCreateV1' not in probe
            if int(nested[1])>0: assert 'xl6SequenceNestedCreateTakeV2' in probe
            for level in (0,2):
                lane=f'{fixture}.O{level}'; obj=out/(lane+'.o'); binary=out/(lane+('.exe' if os.name=='nt' else ''))
                run(lane+'.compile',[cc,*strict,f'-O{level}','-Dmain=xl6RecipeLanguageMain','-c',str(generated),'-o',str(obj)])
                run(lane+'.build',[cc,*strict,f'-O{level}','test/cases/stdlib_sequence_recipe_emit_host.c',str(obj),*links,'-o',str(binary)])
                run(lane+'.cycles',[str(binary)])
        # Enum values are fixed compiler ABI and checked against live headers.
        expected={2,3,4,7,8,9,10,11,12}
        assert expected <= {index for index in range(32) if mask & (1<<index)},mask
        report['LayoutMask']=mask
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Sequence recipe emitter: {len(rows)} checks, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
