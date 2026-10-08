#!/usr/bin/env python3
"""Actual public xlang/JIT module retirement: six cyclic families and real external roots.

GCC O0/O2 hosts use one current resident island. This is not AOT graph retirement,
MEM_FREE/unmap proof, scheduler-wide OOM, or 53-module acceptance.
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
    args=parser.parse_args()
    root,out=Path(__file__).resolve().parents[1],args.out.resolve()
    cc=shutil.which('gcc')
    if not cc or out.exists() or (os.name=='nt')!=bool(args.public_directory):
        parser.error('Require gcc, fresh output and Windows source objects / Linux source product')
    fixture=root/'test/cases/stdlib_concurrent_queue_ownership.xl'
    host=root/'test/cases/stdlib_concurrent_queue_ownership_public.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),fixture,host}
    for directory in ('src','lib','release/include'):
        files.update(f for f in (root/directory).rglob('*') if f.suffix in ('.c','.h','.inc'))
    if args.public_directory:
        public=args.public_directory.resolve()
        build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(),'Stale source '+row['Path']
            files.add(Path(row['Path']))
        files.add(public/'build.json')
        cli=Path(build['Driver']); assert sha(cli)==build['DriverSha256'].lower()
        runtime=root/'release/xl.dll'
        libraries=root/'res/tcc/win64/x86_64'
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']
        links=[*objects,str(runtime),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
        include=public/'include'
    else:
        cli=root/'release/xl'; runtime=root/'release/libxl.so'; libraries=None
        files.add(root/'release/libxlc.so')
        links=['-Lrelease','-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
        include=out/'include'
    files.update((cli,runtime))
    inputs=[{'Path':str(f.resolve()),'Sha256':sha(f)} for f in sorted(files)]
    out.mkdir(parents=True)
    if args.public_directory:
        shutil.copy2(runtime,out/'xl.dll')
    else:
        private=include/'__xl_runtime'; private.mkdir(parents=True)
        for directory in (root/'src/runtime',root/'release/include'):
            for header in directory.glob('*.h'):
                shutil.copy2(header,private/header.name)
    rows,proofs,failures=[],[],[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Platform':sys.platform,
            'Full53Acceptance':False,'AotGraphRetirementAcceptance':False,'SchedulerWideOomAcceptance':False,
            'Inputs':inputs,'Rows':rows,'Proofs':proofs,'Failures':failures}
    def save():
        (out/'ownership.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        try:
            result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
            data,code=result.stdout+result.stderr,result.returncode
        except subprocess.TimeoutExpired as error:
            data,code=(error.stdout or b'')+(error.stderr or b''),'timeout-180s'
        (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':code,'Seconds':time.monotonic()-start})
        save()
        if code!=0:
            raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease',
           '-Irelease/include','-I'+str(include)]
    if not args.public_directory:
        flags+=['-D_GNU_SOURCE']
    strict=flags+['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable',
                  '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
                  '-Wno-missing-field-initializers']
    try:
        generated=out/'graph.c'; repeated=out/'graph.repeat.c'
        for label,target in [('emit',generated),('emit.repeat',repeated)]:
            command=[cli,fixture,'cgen-default',target] if args.public_directory else [cli,'cgen',fixture,'--emit',target]
            run(label,command)
        assert generated.read_bytes()==repeated.read_bytes(),'Nondeterministic language C'
        code=generated.read_text(encoding='utf-8')
        assert 'xlir_special_queue_' not in code
        for family in ('Spsc','Mpsc','Mpmc'):
            assert 'xl6'+family+'QueueCreateTakeV1(' in code
        modes=[(0,0)]+[(mode,family) for mode in (1,2,3) for family in range(6)]
        for level in (0,2):
            binary=out/(f'public-O{level}'+('.exe' if os.name=='nt' else ''))
            run(f'O{level}.strict-C',[cc,*strict,f'-O{level}','-c',generated,'-o',out/f'graph.O{level}.o'])
            run(f'O{level}.host.build',[cc,*flags,f'-O{level}',host,*links,'-o',binary])
            for mode,family in modes:
                name=f'O{level}.mode{mode}.family{family}'
                try:
                    text=run(name,[binary,mode,family,include,out/(name+'.tmp'),runtime,
                                   libraries if libraries else '',fixture])
                    assert '25 actual compile/JIT/retire lifetimes' in text and 'balanced' in text
                    proofs.append({'Optimization':level,'Mode':mode,'Family':family,'MeasuredLifetimes':25})
                    print(name+': 25 balanced actual language lifetimes',flush=True)
                except Exception as error:
                    failures.append({'Name':name,'Failure':str(error)})
                    print(failures[-1],file=sys.stderr,flush=True)
                save()
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'],'Inputs changed during batch'
        report['Complete']=not failures and len(proofs)==2*len(modes)
    except Exception as error:
        report['Error']=str(error); print(error,file=sys.stderr)
    finally:
        save()
    print(f'Queue language ownership: {len(rows)} phases, {len(proofs)} modes, complete={report["Complete"]}',flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':
    raise SystemExit(main())
