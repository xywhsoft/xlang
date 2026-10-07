#!/usr/bin/env python3
"""Actual Array constructors: nine layout families, rejecting IR contracts, strict C, JIT and AOT."""
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
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve(); cc=shutil.which('gcc')
    if not cc or out.exists() or (os.name=='nt' and not args.public_directory):
        parser.error('Require gcc, fresh evidence and a Windows current-source driver or Linux current products')
    public=None; manifest=None
    if args.public_directory:
        manifest=args.public_directory.resolve()/'build.json'
        public=json.loads(manifest.read_text(encoding='utf-8-sig')); cli=Path(public['Driver'])
    else: cli=root/'release/xl'
    source=root/'test/cases/stdlib_array_create_language.xl'
    contract_source=Path(__file__).resolve().parents[1]/'test/cases/xlir_array_create_contract.c'
    product=root/('release/xl.dll' if os.name=='nt' else 'release/libxlc.so')
    host=Path(__file__).resolve().parents[1]/'test/cases/stdlib_array_create_host.c'
    test_root=Path(__file__).resolve().parents[1]
    oom_source=test_root/'test/cases/stdlib_array_create_oom.xl'
    oom_host=root/'test/cases/xlir_module_product_oom_host.c'
    memory_source=root/'test/support/module_product_memory.c'
    memory_header=root/'test/support/module_product_memory.h'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),cli,source,contract_source,product,host}
    files.update((oom_source,oom_host,memory_source,memory_header))
    if public:
        files.add(manifest)
        for row in public['Inputs']+public['Headers']+public['Objects']:
            path=Path(row['Path'])
            if digest(path).lower()!=row['Sha256'].lower(): parser.error('Stale source compiler: '+str(path))
            files.add(path)
        staged=args.public_directory.resolve()/'artifact_sources.json'
        package=json.loads(staged.read_text(encoding='utf-8-sig')); files.add(staged)
        for row in package['Files']:
            path=Path(row['Path'])
            if digest(path).lower()!=row['Sha256'].lower(): parser.error('Stale staged source: '+str(path))
            files.add(path)
    else:
        files.add(root/'release/libxl.so'); files.add(root/'build_linux.sh')
        for directory in ('src','lib','release/include'):
            files.update(p for p in (root/directory).rglob('*') if p.suffix in ('.c','.h','.inc'))
    inputs=[{'Path':str(path),'Sha256':digest(path)} for path in sorted(files)]
    rows=[]; sweeps=[]; report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,'Sweeps':sweeps,
        'Scope':__doc__,'Sanitized':False,'WholeModuleOOM':False,'PrunedGccLink':False,
        'WindowsPublicSourceCompiler':bool(public),'LinuxSharedCompilerProduct':not bool(public)}
    out.mkdir(parents=True)
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else:
        runtime_include=out/'include/__xl_runtime'
        shutil.copytree(root/'src/runtime',runtime_include)
        for header in (root/'release/include').glob('*.h'):
            shutil.copy2(header,runtime_include/header.name)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(args.public_directory.resolve()/'include' if public else out/'include')]
    if public:
        links=[row['Path'] for row in public['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']
        links += [str(product),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        flags+=['-D_GNU_SOURCE']
        links=['-Lrelease','-lxlc','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    strict=flags+['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    def run(name,command,timeout=180):
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=timeout)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        if not re.fullmatch(r'oom\.O[02]\.\d+',name) or result.returncode:
            print(f'{name}: {result.returncode}',flush=True)
        if result.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def command(mode,target,input_source=source):
        if public: return [str(cli),str(input_source),mode,str(target)]
        if mode=='ir-lowered': return [str(cli),'ir',str(input_source),'--lowered','--format','json','--emit',str(target)]
        if mode=='cgen-default': return [str(cli),'cgen',str(input_source),'--inc-path',str(out/'include'),'--emit',str(target)]
        if mode=='run': return [str(cli),'run',str(input_source),'--inc-path',str(out/'include')]
        return [str(cli),'build',str(input_source),'--inc-path',str(out/'include'),'-o',str(target)]
    try:
        contract=out/'contract.exe'; generated=out/'create.c'
        run('contract.build',[cc,*flags,'-O0',str(contract_source),*links,'-o',str(contract)],360)
        log=run('contract.run',[str(contract),str(source),str(generated)])
        match=re.search(r'Array create: (\d+) producers; (\d+) mutations refused before C/header; layout-mask=(\d+)',log)
        assert match and int(match[2])==int(match[1])*13 and int(match[3]).bit_count()==9
        report['Producers']=int(match[1]); report['RefusedMutations']=int(match[2]); report['LayoutMask']=int(match[3])
        dynamic=re.search(r'Dynamic arithmetic: (\d+) checked producers; (\d+) mutations refused before C/header',log)
        assert dynamic and int(dynamic[1])>=14 and int(dynamic[2])==int(dynamic[1])*2
        report['DynamicProducers']=int(dynamic[1]); report['DynamicRefusedMutations']=int(dynamic[2])
        text=generated.read_text(encoding='utf-8')
        assert 'xl6ArrayProjectionTakeV1(' in text
        assert not re.search(r'= xrtValueArray\(',text), 'Typed construction still allocates a wire Array'
        for mode,suffix in (('ir-lowered','.json'),('cgen-default','.repeat.c')):
            run(mode,command(mode,out/('create'+suffix)))
        assert generated.read_bytes()==(out/'create.repeat.c').read_bytes()
        for level in (0,2):
            binary=out/f'create.O{level}.exe'
            run(f'O{level}.compile',[cc,*strict,f'-O{level}',str(generated),str(host),*links,'-o',str(binary)],360)
            run(f'O{level}.run',[str(binary)])
        run('jit',command('run',out/'jit'))
        run('aot.emit',command('exe',out/'aot'))
        run('aot.run',[str(out/('aot.exe' if os.name=='nt' else 'aot'))])
        oom_c=out/'create.oom.c'
        run('oom.cgen',command('cgen-default',oom_c,oom_source))
        for level in (0,2):
            binary=out/f'create.oom.O{level}.exe'
            memory=out/f'memory.O{level}.o'
            run(f'oom.O{level}.memory',[cc,*strict,f'-O{level}','-c',str(memory_source),'-o',str(memory)])
            run(f'oom.O{level}.compile',[cc,*strict,f'-O{level}','-DXL6_PRODUCT_USER_ENTRY',
                '-include',str(oom_c),str(oom_host),str(memory),*links,'-o',str(binary)],360)
            assert 'balanced' in run(f'oom.O{level}.baseline',[str(binary)])
            for point in range(4097):
                proof=run(f'oom.O{level}.{point}',[str(binary),str(point)])
                assert 'balanced' in proof
                if 'hit=0 balanced' in proof:
                    assert point>0; sweeps.append({'Level':level,'FirstNoHit':point})
                    print(f'O{level}: complete allocation prefix, first no-hit {point}',flush=True); break
            else: raise RuntimeError('Incomplete caller-thread module allocation prefix')
        report['WholeModuleOOM']=True
        report['OOMScope']='Synchronous caller-thread complete module initialization/entry/retirement prefix; not scheduler-wide Future OOM'
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
