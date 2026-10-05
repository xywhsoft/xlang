#!/usr/bin/env python3
"""Exact nullable native C scalar/type-id policies and selected physical profiles; not source Array cutover."""
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
    host='test/cases/stdlib_array_nullable_c_scalar_native_host.c'
    sources=[host,'test/support/array_value_native.c','src/runtime/xl6_sequence.c','src/runtime/xl6_array.c',
        'src/runtime/xl6_array_iterator.c','src/runtime/xl6_array_value.c','src/runtime/xl6_array_reference_value.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_array_nullable_c_scalar_admission.c','src/runtime/xl6_ownership_plan.c']
    sources += ["src/runtime/xl6_nullable_bool.c","src/runtime/xl6_nullable_bool_value.c","src/runtime/xl6_nullable_i64.c","src/runtime/xl6_nullable_i64_value.c","src/runtime/xl6_nullable_u64.c","src/runtime/xl6_nullable_u64_value.c","src/runtime/xl6_nullable_f32.c","src/runtime/xl6_nullable_f32_value.c","src/runtime/xl6_nullable_f64.c","src/runtime/xl6_nullable_f64_value.c","src/runtime/xl6_nullable_native_i8.c","src/runtime/xl6_nullable_native_i8_value.c","src/runtime/xl6_nullable_native_u8.c","src/runtime/xl6_nullable_native_u8_value.c","src/runtime/xl6_nullable_native_i16.c","src/runtime/xl6_nullable_native_i16_value.c","src/runtime/xl6_nullable_native_u16.c","src/runtime/xl6_nullable_native_u16_value.c","src/runtime/xl6_nullable_native_i32.c","src/runtime/xl6_nullable_native_i32_value.c","src/runtime/xl6_nullable_native_u32.c","src/runtime/xl6_nullable_native_u32_value.c"]
    product=root/('release/xl.dll' if os.name=='nt' else 'release/libxl.so')
    files={Path(__file__),Path(cc),Path(nm),Path(sys.executable),root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'test/support/array_native.c',
        root/'test/cases/stdlib_array_nullable_c_scalar_storage_host.c'}
    files.update(root/name for name in sources)
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
            assert 'xl6SequenceNullableCScalarCreateV1' in names
            if not args.shared:
                assert 'xl6SequencePrimitiveCreateV1' not in names
                assert not {'xl6NullableTimeTypeV1','xl6NullableCharTypeV1','xl6NullableStringTypeV1'} & names
            if not args.shared:
                for base,profile in [('Bool','Bool'),('Int8','NativeI8'),('UInt8','NativeU8'),('Int16','NativeI16'),
                        ('UInt16','NativeU16'),('Int32','NativeI32'),('UInt32','NativeU32'),('Int64','I64'),
                        ('UInt64','U64'),('Float32','F32'),('Float64','F64')]:
                    lower=re.sub(r'^Native','native_',profile).lower()
                    probe=out/(lane+'.only-'+profile+('.exe' if os.name=='nt' else ''))
                    selected=['test/cases/stdlib_array_nullable_c_scalar_storage_host.c','test/support/array_native.c',
                        'src/runtime/xl6_sequence.c','src/runtime/xl6_array.c',
                        'src/runtime/xl6_array_nullable_c_scalar_admission.c',f'src/runtime/xl6_nullable_{lower}.c']
                    run(lane+'.only-'+profile+'.build',[cc,*flags,f'-O{level}',
                        '-DXL6_TEST_NATIVE_C_DESCRIPTOR=xl6Nullable'+profile+'TypeV1',
                        '-DXL6_TEST_NATIVE_C_BASE=xrtType'+base,*selected,*links,'-o',str(probe)],timeout=180)
                    text=run(lane+'.only-'+profile+'.symbols',[nm,'--defined-only',str(probe)])
                    found={line.split()[-1].lstrip('_') for line in text.splitlines() if line.split()}
                    nullable={n for n in found if re.match(r'xl6Nullable.*(?:Type|Converter)V1$',n)}
                    assert nullable=={'xl6Nullable'+profile+'TypeV1'},nullable
                    run(lane+'.only-'+profile+'.run',[str(probe)])
            vectors=run(lane+'.vectors',[str(binary)])
            assert '21 native nullable C/type mappings and 12 exact domains' in vectors and 'owner lifetimes balanced' in vectors
            for profile in range(12):
                for operation in ['create','clone','erased-clone','cursor','snapshot','read','take','push','set','insert','resize','reserve']:
                    for point in range(513):
                        text=run(f'{lane}.p{profile}.{operation}.{point}',[str(binary),str(profile),operation,str(point)])
                        match=re.search(r'hit=([01]) balanced',text); assert match,'Missing ownership outcome'
                        if match[1]=='0':
                            sweeps.append({'Lane':lane,'Profile':profile,'Operation':operation,'FirstNoHit':point}); break
                    else: raise AssertionError('Unclosed allocation failure prefix')
        assert len(sweeps)==288
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Nullable C scalar Array: {len(rows)} checks, {len(sweeps)} closed prefixes, {sum(row["FirstNoHit"]+1 for row in sweeps)} positions, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
