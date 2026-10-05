#!/usr/bin/env python3
"""Actual Array loop / multi-await language ownership; shared runtime, not full-source ASan or JIT."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    family=parser.add_mutually_exclusive_group()
    family.add_argument('--array-only',action='store_true')
    family.add_argument('--multi-only',action='store_true')
    args=parser.parse_args(); root=Path(__file__).resolve().parents[1]; out=args.out.resolve()
    cc=shutil.which('gcc'); cli=root/('release/xl.exe' if os.name=='nt' else 'release/xl')
    if out.exists() or not cc or not cli.is_file() or (args.sanitize and os.name=='nt'):
        parser.error('Require fresh output, gcc/current CLI and Linux for sanitizer')
    # Eleven terminal modes, including managed text with embedded NUL. Every
    # binary checks two full activations, actual retirement and MemDebug balance.
    cases=[]
    if not args.multi_only:
        cases.append(('xlir_array_i64_for_abi','test/cases/xlir_array_i64_for_snapshot_host.c',
            'XLIR_ARRAY_FOR_GENERATED_SOURCE',[None],0))
    if not args.array_only:
        for name,modes,suspensions in [
            ('xlir_async_future_two_awaits',[None,'XLIR_CHAIN_SECOND_REJECT'],2),
            ('xlir_async_future_three_awaits',[None,'XLIR_CHAIN_THIRD_REJECT','XLIR_CHAIN_THIRD_CANCEL','XLIR_CHAIN_THIRD_CLOSE'],3),
            ('xlir_async_future_three_awaits_mixed',[None],3),
            ('xlir_async_future_three_strings',[None,'XLIR_STRING_THIRD_REJECT','XLIR_STRING_THIRD_CANCEL','XLIR_STRING_THIRD_CLOSE'],3)]:
            cases.append((name,f'test/cases/{name}_host.c','XLIR_MULTI_AWAIT_GENERATED_SOURCE',modes,suspensions))
    files={Path(__file__),Path(cc),Path(sys.executable),cli,
        root/'test/cases/xlir_multi_await_host_support.h',
        root/'test/cases/xlir_multi_await_host_prelude.h',
        root/'tools/check_xlir_array_i64_for_abi.ps1',root/'tools/check_xlir_async_machine_multi_await.ps1',
        root/'lib/xrt/xrt.snapshot.json',root/'lib/xruntime/xruntime.snapshot.json'}
    for directory in ['src/runtime','src/ir','src/cgen','src/compile','src/stdlib','release/include']:
        files.update(p for p in (root/directory).rglob('*') if p.suffix in ['.h','.c','.inc'])
    for name,host,_,_,_ in cases: files.update([root/f'test/cases/{name}.xl',root/host])
    files.update(root/name for name in (['release/xl.dll','release/xlc.dll'] if os.name=='nt' else ['release/libxl.so','release/libxlc.so']))
    inputs=[{'Path':str(p.resolve()),'Sha256':digest(p)} for p in sorted(files)]
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Rows':rows,
        'Fixtures':len(cases),'TerminalModes':sum(len(row[3]) for row in cases),
        'PendingModes':sum(2*len(row[3]) for row in cases if row[4]),
        'Modes':sum(len(row[3])*(3 if row[4] else 1) for row in cases),
        'Sanitized':args.sanitize,'RuntimeInstrumented':False}
    out.mkdir(parents=True); private=out/'include/__xl_runtime'; private.mkdir(parents=True)
    for directory in [root/'src/runtime',root/'release/include']:
        for path in directory.glob('*.h'): shutil.copy2(path,private/path.name)
    if os.name=='nt': shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    env=dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,timeout=90):
        try:
            result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out/(name+'.log')).write_bytes((error.stdout or b'')+(error.stderr or b''))
            rows.append({'Name':name,'Exit':None,'Command':command,'Complete':False,'TimedOut':True}); raise
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Exit':result.returncode,'Command':command,'Complete':result.returncode==0})
        assert result.returncode==0,name+': '+data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I.','-Irelease/include','-I'+str(out/'include')]
    links=['release/xl.dll','-lws2_32','-liphlpapi','-lshell32'] if os.name=='nt' else [
        '-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize: flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    try:
        for name,host,macro,modes,suspensions in cases:
            generated=out/(name+'.c'); ir=out/(name+'.json')
            for repeat in [False,True]:
                target=out/(name+'.repeat.c') if repeat else generated
                run(name+f'.emit.{repeat}',[str(cli),'cgen',f'test/cases/{name}.xl','--backend','xlir',
                    '--inc-path',str(out/'include'),'--emit',str(target)])
            assert generated.read_bytes()==(out/(name+'.repeat.c')).read_bytes()
            run(name+'.ir',[str(cli),'ir',f'test/cases/{name}.xl','--lowered','--format','json','--emit',str(ir)])
            data=json.loads(ir.read_text())
            if suspensions:
                assert len(data['asyncMachines'])==1 and len(data['asyncSuspensions'])==suspensions
                assert len(data['asyncMachineEntries'])==1+4*suspensions
                assert all(row['futureOutcomeAbi']==1 for row in data['instructions'] if row['kindName']=='await')
            else:
                loops=[row for row in data['nodes'] if row['loopAbiName'].startswith('xvalue-array-i64-')]
                assert len(loops)==11,'Expected eleven precisely typed Array loops'
                assert sum(row['loopResourceAbiName']=='xvalue-shallow-snapshot-release' for row in loops)==9
                assert sum(row['loopResourceAbiName']=='xvalue-owned-iterator-release' for row in loops)==2
                instructions=data['instructions']
                for loop in loops:
                    owned=[row for row in instructions if row['origin']==loop['id'] and row['kindName']=='store'
                        and row['type']==loop['loopResourceType'] and row['targetSymbol']>=0xf0000000]
                    assert len(owned)==1,f"Loop {loop['id']}: expected exactly one Array owner"
                    assert any(row['kindName']=='release' and row['type']==loop['loopResourceType']
                        and row['targetSymbol']==owned[0]['targetSymbol'] for row in instructions)
            for level in [0,2]:
                for mode in modes:
                    for timing in (['ready','pending','pending-after-commit'] if suspensions else ['snapshot']):
                        label=f'{name}.O{level}.'+(mode or 'success')+'.'+timing
                        binary=out/(label+('.exe' if os.name=='nt' else ''))
                        defines=[f'-D{macro}="{generated.as_posix()}"']+([f'-D{mode}'] if mode else [])
                        if timing.startswith('pending'): defines+=['-DXLIR_MULTI_AWAIT_PENDING']
                        if timing=='pending-after-commit': defines+=['-DXLIR_MULTI_AWAIT_AFTER_COMMIT']
                        run(label+'.build',[cc,*flags,f'-O{level}',*defines,host,*links,'-o',str(binary)],timeout=180)
                        text=run(label+'.run',[str(binary)])
                        assert 'two ' in text and 'language lifecycles and memory balance' in text
                        if timing.startswith('pending'): assert 'every input completed after actual pending Watch registration' in text
                        if timing=='pending-after-commit': assert 'deferred completion after actual activation Commit' in text
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Collection continuations: {len(rows)} checks, {report["Modes"]} modes at O0/O2, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
