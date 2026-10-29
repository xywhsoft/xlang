#!/usr/bin/env python3
"""Owned index XLIR contracts and real generated native/wire execution; not whole Array migration."""
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
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]; out=args.out.resolve(); cc=shutil.which('gcc')
    if not cc or out.exists() or (os.name=='nt' and (not args.public_directory or args.sanitize)):
        parser.error('Require gcc, fresh evidence and current-source Windows compiler; sanitizers are Linux only')
    product=root/('release/xl.dll' if os.name=='nt' else 'release/libxl.so')
    fixtures=['stdlib_array_value_read','stdlib_array_value_read_continuation']
    files={Path(__file__),Path(cc),Path(sys.executable),product}
    for directory in ('src','lib/tcc','release/include'):
        files.update(p for p in (root/directory).rglob('*') if p.suffix in ('.c','.h','.inc'))
    files.update(root/p for p in ('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'test/cases/xlir_array_value_read_contract.c','test/cases/stdlib_array_value_read_projection_host.c',
        'test/cases/stdlib_array_value_read_header.c',
        'test/cases/xlir_module_product_oom_host.c','test/support/module_product_memory.c','test/support/module_product_memory.h'))
    files.update(root/f'test/cases/{name}.xl' for name in fixtures)
    public=None
    if args.public_directory:
        public=json.loads((args.public_directory/'build.json').read_text(encoding='utf-8-sig'))
        staged=json.loads((args.public_directory/'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert not public['RuntimeOnly'] and public['SourceXrt']
        for row in public['Inputs']+public['Headers']+public['Objects']+staged['Files']:
            assert digest(Path(row['Path']))==row['Sha256'].lower(),row['Path']
            files.add(Path(row['Path']))
        cli=Path(public['Driver'])
        files.update((cli,args.public_directory/'build.json',args.public_directory/'artifact_sources.json'))
    else:
        cli=root/'release/xl'; files.update((cli,root/'release/libxlc.so'))
    inputs=[{'Path':str(p.resolve()),'Sha256':digest(p)} for p in sorted(files)]
    out.mkdir(parents=True); rows=[]; sweeps=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Rows':rows,'Sweeps':sweeps,
        'Sanitized':args.sanitize,'SharedRuntimeInstrumented':False}
    env=dict(os.environ)
    if args.sanitize: env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    links=[str(product)] if os.name=='nt' else ['-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-lm','-ldl']
    def run(name: str, command: list[str], timeout: int=180) -> str:
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=timeout)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        assert result.returncode==0,name+': '+data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')
    try:
        leaf=out/('header.exe' if os.name=='nt' else 'header')
        run('header.build',[cc,'-std=c11','-pedantic-errors','-Wall','-Wextra','-Werror','-I.',
            'test/cases/stdlib_array_value_read_header.c','-o',str(leaf)])
        run('header.run',[str(leaf)])
        private=out/'include/__xl_runtime'; private.mkdir(parents=True)
        for directory in (root/'src/runtime',root/'release/include'):
            for header in directory.glob('*.h'): shutil.copy2(header,private/header.name)
        contract=out/('contract.exe' if os.name=='nt' else 'contract')
        compiler_links=([row['Path'] for row in public['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']+
            [str(product),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if public else ['-lxlc',*links])
        run('contract.build',[cc,*flags,'-DXL_STATIC_EMBED','-Ilib/tcc','-O0',
            'test/cases/xlir_array_value_read_contract.c',*compiler_links,'-o',str(contract)],360)
        strict=[*flags,'-DXL_STATIC_EMBED','-I'+str(out/'include'),'-Wno-unused-function','-Wno-unused-parameter',
            '-Wno-unused-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
        if args.sanitize: strict+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        for name in fixtures:
            source=f'test/cases/{name}.xl'; target=out/(name+'.c'); bindings=out/'bindings.h'
            for mode,suffix in (('cgen-default','.c'),('ir-lowered','.json')):
                path=out/(name+suffix); repeat=out/(name+'.repeat'+suffix)
                for dest in (path,repeat):
                    command=[str(cli),source,mode,str(dest)] if public else ([str(cli),'ir',source,'--lowered','--format','json','--emit',str(dest)]
                        if mode=='ir-lowered' else [str(cli),'cgen',source,'--emit',str(dest)])
                    run(name+'.'+mode+('.repeat' if dest==repeat else ''),command)
                assert path.read_bytes()==repeat.read_bytes()
            original=target.read_bytes()
            proof=run(name+'.contract',[str(contract),source,str(target),*([str(bindings)] if name==fixtures[0] else [])])
            assert 'mutations refused before C/header output' in proof and target.read_bytes()==original
            code=original.decode('utf-8'); assert 'xl6_array_value_read.h' in code
            assert 'static xvalue* xlir_xvalue_array_at_i64(' not in code
            assert 'static xvalue* xlir_xvalue_index_get_xvalue_key(' not in code
            for level in (0,2):
                lane=f'{name}.O{level}'; binary=out/(lane+('.exe' if os.name=='nt' else ''))
                memory=out/f'memory.O{level}.o'
                if not memory.exists():
                    run(f'memory.O{level}.build',[cc,*strict,f'-O{level}','-c','test/support/module_product_memory.c','-o',str(memory)])
                run(lane+'.build',[cc,*strict,f'-O{level}','-include',str(target),'test/cases/xlir_module_product_oom_host.c',
                    str(memory),*links,'-o',str(binary)],360)
                assert 'balanced' in run(lane+'.run',[str(binary)])
                if name==fixtures[0]:
                    for point in range(4097):
                        proof=run(lane+f'.oom.{point}',[str(binary),str(point)])
                        assert 'balanced' in proof
                        if 'hit=0 balanced' in proof:
                            sweeps.append({'Fixture':name,'Level':level,'FirstNoHit':point}); break
                    else: raise AssertionError('Unclosed language allocation prefix')
                    projected=out/(f'projection.O{level}'+('.exe' if os.name=='nt' else ''))
                    run(f'projection.O{level}.build',[cc,*strict,f'-O{level}','-include',str(target),'-include',str(bindings),
                        'test/cases/stdlib_array_value_read_projection_host.c',*links,'-o',str(projected)],360)
                    assert '100 physical cycles' in run(f'projection.O{level}.run',[str(projected)])
            prefix=out/(name+'.product')
            if public:
                run(name+'.jit',[str(cli),source,'run',str(prefix)]); run(name+'.aot',[str(cli),source,'exe',str(prefix)])
            else:
                run(name+'.jit',[str(cli),'run',source]); run(name+'.aot.build',[str(cli),'build',source,'-o',str(prefix)])
                run(name+'.aot',[str(prefix)])
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f"Owned index language: {len(rows)} checks, complete={report['Complete']}")
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
