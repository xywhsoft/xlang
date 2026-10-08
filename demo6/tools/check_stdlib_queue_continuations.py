#!/usr/bin/env python3
"""Actual Queue string/bytes/Array/closure ownership across three resident awaits.

O0/O2: success/reject/cancel/close, ready/pending Watch/pending Commit. This is
generated-language AOT/shared-runtime acceptance, not full-source sanitizers,
public JIT module retirement, scheduler-wide OOM or the full 53-module gate.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public-directory',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args()
    root,out=Path(__file__).resolve().parents[1],args.out.resolve()
    cc=shutil.which('gcc')
    if not cc or out.exists() or (os.name=='nt')!=bool(args.public_directory) or (args.sanitize and os.name=='nt'):
        parser.error('Require GCC, fresh output, Windows current source objects / Linux source products')
    fixture=root/'test/cases/stdlib_concurrent_queue_continuation.xl'
    host=root/'test/cases/stdlib_concurrent_queue_continuation_host.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),fixture,host,
           root/'test/cases/xlir_multi_await_host_prelude.h',root/'test/cases/xlir_multi_await_host_support.h',
           root/'lib/xrt/xrt.snapshot.json',root/'lib/xruntime/xruntime.snapshot.json'}
    for directory in ('src','lib','release/include'):
        files.update(path for path in (root/directory).rglob('*') if path.suffix in ('.c','.h','.inc'))
    if args.public_directory:
        public=args.public_directory.resolve()
        build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(),'Stale resident source '+row['Path']
            files.add(Path(row['Path']))
        files.add(public/'build.json')
        cli=Path(build['Driver']); assert sha(cli)==build['DriverSha256'].lower()
        dll=root/'release/xl.dll'; files.add(dll)
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']
        links=[*objects,str(dll),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
        include=public/'include'
    else:
        cli=root/'release/xl'; include=out/'include'
        files.update(root/('release/'+name) for name in ('libxl.so','libxlc.so'))
        links=['-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    files.add(cli)
    inputs=[{'Path':str(path.resolve()),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    if args.public_directory:
        shutil.copy2(dll,out/'xl.dll')
    else:
        private=include/'__xl_runtime'; private.mkdir(parents=True)
        for directory in (root/'src/runtime',root/'release/include'):
            for header in directory.glob('*.h'): shutil.copy2(header,private/header.name)
    rows,proofs,failures=[],[],[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Platform':sys.platform,
            'Inputs':inputs,'Rows':rows,'Proofs':proofs,'Failures':failures,'RuntimeInstrumented':False,
            'GeneratedCodeSanitized':args.sanitize,'Full53Acceptance':False,'SchedulerWideOomAcceptance':False}
    def save():
        (out/'continuations.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    env=dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        try:
            result=subprocess.run(command,cwd=root,capture_output=True,env=env,timeout=180)
            output,code=result.stdout+result.stderr,result.returncode
        except subprocess.TimeoutExpired as error:
            output,code=(error.stdout or b'')+(error.stderr or b''),'timeout-180s'
        (out/(name+'.log')).write_bytes(output)
        rows.append({'Name':name,'Command':command,'Exit':code,'Seconds':time.monotonic()-start}); save()
        assert code==0,name+': '+output[-4000:].decode(errors='replace')
        return output.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease/include','-I'+str(include),
           '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
           '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    if not args.public_directory: flags+=['-D_GNU_SOURCE']
    if args.sanitize: flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    try:
        generated=out/'queue.c'; repeated=out/'queue.repeat.c'; ir=out/'queue.ir.json'
        for label,target in [('emit',generated),('emit.repeat',repeated)]:
            command=[cli,fixture,'cgen-default',target] if args.public_directory else [cli,'cgen',fixture,'--emit',target]
            run(label,command)
        assert generated.read_bytes()==repeated.read_bytes(),'Nondeterministic actual language C'
        run('ir',[cli,fixture,'ir-lowered',ir] if args.public_directory else [cli,'ir',fixture,'--lowered','--format','json','--emit',ir])
        data=json.loads(ir.read_text(encoding='utf-8'))
        assert len(data['asyncMachines'])==1 and len(data['asyncSuspensions'])==3 and len(data['asyncMachineEntries'])==13
        assert all(row['futureOutcomeAbi']==1 for row in data['instructions'] if row['kindName']=='await')
        code=generated.read_text(encoding='utf-8')
        assert 'xlir_special_queue_' not in code
        for family in ('Spsc','Mpsc','Mpmc'): assert 'xl6'+family+'QueueCreateTakeV1(' in code
        for level in (0,2):
            for terminal,macro in [('success',None),('reject','XLIR_CHAIN_THIRD_REJECT'),('cancel','XLIR_CHAIN_THIRD_CANCEL'),('close','XLIR_CHAIN_THIRD_CLOSE')]:
                for timing in ('ready','pending-watch','pending-commit'):
                    name=f'O{level}.{terminal}.{timing}'
                    binary=out/(name+('.exe' if os.name=='nt' else ''))
                    defines=['-DXLIR_MULTI_AWAIT_GENERATED_SOURCE="'+generated.as_posix()+'"']
                    if macro: defines+=['-D'+macro]
                    if timing!='ready': defines+=['-DXLIR_MULTI_AWAIT_PENDING']
                    if timing=='pending-commit': defines+=['-DXLIR_MULTI_AWAIT_AFTER_COMMIT']
                    try:
                        run(name+'.strict-C',[cc,*flags,f'-O{level}',*defines,host,*links,'-o',binary])
                        text=run(name+'.run',[binary])
                        assert 'two complete language lifecycles and memory balance' in text and 'terminal cleanup balanced' in text
                        if timing!='ready': assert 'every input completed after actual pending Watch registration' in text
                        if timing=='pending-commit': assert 'deferred completion after actual activation Commit' in text
                        proofs.append({'Optimization':level,'Terminal':terminal,'Timing':timing,'Activations':2,'FinallyCallsPerActivation':1})
                        print(name+': two actual activations, once-only finally, owned values and memory balanced',flush=True)
                    except Exception as error:
                        failures.append({'Name':name,'Failure':str(error)}); print(failures[-1],file=sys.stderr,flush=True)
                    save()
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'],'Inputs changed during batch'
        report['Complete']=not failures and len(proofs)==24
    except Exception as error:
        report['Error']=str(error); print(error,file=sys.stderr)
    finally:
        save()
    print(f'Queue continuations: {len(rows)} phases, {len(proofs)} modes, complete={report["Complete"]}',flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':
    raise SystemExit(main())
