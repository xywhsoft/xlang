#!/usr/bin/env python3
"""Exact native C scalar/type-id Array policies; not language Array carrier cutover."""
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
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--shared',action='store_true',help='Real product ABI; sanitizer instruments host only')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]; out=args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm')
    objdump=shutil.which('objdump') if args.shared and os.name=='nt' else None
    if out.exists() or not cc or not nm or (args.sanitize and os.name=='nt') or (args.shared and os.name=='nt' and not objdump):
        parser.error('Require gcc/nm, fresh output and Linux for sanitizers')
    host='test/cases/stdlib_array_c_scalar_native_host.c'
    sources=[host,'test/support/array_value_native.c','src/runtime/xl6_sequence.c','src/runtime/xl6_array.c',
        'src/runtime/xl6_array_iterator.c','src/runtime/xl6_array_value.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_ownership_plan.c']
    sources.append('src/api/xl_foreign_error.c')
    product=root/('release/xl.dll' if os.name=='nt' else 'release/libxl.so')
    files={Path(__file__),Path(cc),Path(nm),Path(sys.executable),root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h'}
    files.update(root/name for name in sources)
    files.add(root/'src/api/xl_foreign_error.h')
    for directory in ['src/runtime','release/include']: files.update((root/directory).rglob('*.h'))
    if args.shared:
        files.add(product)
        if objdump: files.add(Path(objdump))
    inputs=[{'Path':str(path.resolve()),'Sha256':digest(path)} for path in sorted(files)]
    rows=[]; sweeps=[]
    report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,'Sweeps':sweeps,
        'Profiles':12,'NativeMappings':21,'SharedRuntime':args.shared,'Sanitized':args.sanitize,
        'RuntimeInstrumented':args.sanitize and not args.shared,'Scope':__doc__}
    out.mkdir(parents=True)
    if args.shared and os.name=='nt': shutil.copy2(product,out/'xl.dll')
    env=dict(os.environ)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=[] if os.name=='nt' else ['-pthread','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,timeout=60):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=timeout)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        assert result.returncode==0,name+': '+data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')
    try:
        for level in [0,2]:
            lane=f'O{level}'; binary=out/(lane+('.exe' if os.name=='nt' else ''))
            selected=sources
            linked=links
            if args.shared:
                selected=[host]
                linked=[str(product)] if os.name=='nt' else ['-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),*links]
            run(lane+'.build',[cc,*flags,f'-O{level}',*selected,*linked,'-o',str(binary)],timeout=180)
            symbols=run(lane+'.symbols',([objdump,'-p',str(product)] if objdump else
                [nm,*(['-D'] if args.shared else []),'--defined-only',str(product if args.shared else binary)]))
            names={line.split()[-1].lstrip('_') for line in symbols.splitlines() if line.split()}
            assert 'xl6SequenceCScalarCreateV1' in names
            if not args.shared:
                assert 'xl6SequencePrimitiveCreateV1' not in names
                assert not any(re.match(r'xl6(?:Nullable.*|String|Char)(?:Type|Converter)V1$',name) for name in names)
            vectors=run(lane+'.vectors',[str(binary)])
            assert '21 native C ABI/type-id mappings' in vectors and '12 exact native C scalar/type-id policies and identities balanced' in vectors
            for profile in range(12):
                for operation in ['create','clone','erased-clone','cursor','snapshot','read','take','push','set','insert','resize']:
                    for point in range(513):
                        text=run(f'{lane}.p{profile}.{operation}.{point}',[str(binary),str(profile),operation,str(point)])
                        match=re.search(r'hit=([01]) balanced',text); assert match,'Missing ownership outcome'
                        if match[1]=='0':
                            sweeps.append({'Lane':lane,'Profile':profile,'Operation':operation,'FirstNoHit':point}); break
                    else: raise AssertionError('Unclosed allocation failure prefix')
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'C scalar Array: {len(rows)} checks, {len(sweeps)} closed prefixes, {sum(row["FirstNoHit"]+1 for row in sweeps)} positions, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
