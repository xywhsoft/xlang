#!/usr/bin/env python3
"""Nine actual construction-only native closures: no Array/Stack facade or Stack algorithms."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True); parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=Path(__file__).resolve().parents[1]; out=args.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm')
    if out.exists() or not cc or not nm or (args.sanitize and os.name=='nt'):
        parser.error('Require gcc/nm, fresh evidence and Linux for sanitizers')
    profiles=[('reference',['src/runtime/xl6_value_admission.c'],'xl6SequenceReferenceCreateV1'),
        ('nominal',['src/runtime/xl6_nominal_admission.c'],'xl6SequenceNominalCreateV1'),
        ('primitive',['src/runtime/xl6_value_admission.c'],'xl6SequencePrimitiveCreateV1'),
        ('c_scalar',[],'xl6SequenceCScalarCreateV1'),
        ('sequence',['src/runtime/xl6_array_primitive_admission.c','src/runtime/xl6_value_admission.c'],'xl6SequenceNestedCreateTakeV2'),
        ('nullable_c_scalar',['src/runtime/xl6_nullable_native_u16.c'],'xl6SequenceNullableCScalarCreateV1'),
        ('numeric_enum',[],'xl6SequenceNumericEnumCreateV1'),('callable',[],'xl6SequenceCallableCreateV1'),
        ('future',[],'xl6SequenceFutureCreateV1')]
    common=['test/cases/stdlib_sequence_factory_only_host.c','test/support/sequence_factory_native.c','src/runtime/xl6_sequence.c']
    files={Path(__file__),Path(cc),Path(nm),Path(sys.executable),root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h'}
    files.update(root/name for name in common)
    for name,extras,_ in profiles: files.update(root/path for path in [f'src/runtime/xl6_array_{name}_admission.c',*extras])
    for directory in ['src/runtime','release/include']: files.update((root/directory).rglob('*.h'))
    inputs=[{'Path':str(path.resolve()),'Sha256':digest(path)} for path in sorted(files)]
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,'Profiles':9,'Cycles':100,
        'Sanitized':args.sanitize,'RuntimeInstrumented':args.sanitize,'Scope':__doc__}
    out.mkdir(parents=True); env=dict(os.environ)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    links=[] if os.name=='nt' else ['-pthread','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        assert result.returncode==0,name+': '+data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')
    try:
        for index,(profile,extras,factory) in enumerate(profiles):
            for level in [0,2]:
                lane=f'{profile}.O{level}'; binary=out/(lane+('.exe' if os.name=='nt' else ''))
                sources=[*common,f'src/runtime/xl6_array_{profile}_admission.c',*extras]
                run(lane+'.build',[cc,*flags,f'-O{level}',f'-DXL6_FACTORY_PROFILE={index}',*sources,*links,'-o',str(binary)])
                names={line.split()[-1].lstrip('_') for line in run(lane+'.symbols',[nm,'--defined-only',str(binary)]).splitlines() if line.split()}
                assert {factory,'xl6SequenceCreateAdmittedV1','xl6SequenceCloneRoleV1'}<=names
                required={factory,'xl6SequencePrimitiveCreateV1'} if profile=='sequence' else {factory}
                assert required<=names
                assert not ({item[2] for item in profiles}-required) & names
                assert 'xl6SequenceNestedCreateV1' not in names
                assert not any(name.startswith(('xl6Stack','xrtTypedStack')) for name in names)
                assert not {'xl6ArrayCreateV1','xl6ArrayCloneV1','xl6ArrayPushV1','xl6ArrayFromValueV1','xl6ArrayToValueV1'}&names
                assert f'factory {index}: 100 ARRAY/STACK direct publication/clone cycles balanced' in run(lane+'.vectors',[str(binary)])
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Sequence factories: {len(rows)} checks, 9 actual closures, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
