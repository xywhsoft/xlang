#!/usr/bin/env python3
"""Current enum C ABI on real products: strict raw C and shared hosts, not retired HIR layouts."""
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
    parser.add_argument('--sanitize',action='store_true',help='Linux generated C/host only; shared runtime not instrumented')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]; out=args.out.resolve()
    cc=shutil.which('gcc'); cli=root/('release/xl.exe' if os.name=='nt' else 'release/xl')
    product=root/('release/xl.dll' if os.name=='nt' else 'release/libxl.so')
    compiler=root/('release/xlc.dll' if os.name=='nt' else 'release/libxlc.so')
    if out.exists() or not cc or (args.sanitize and os.name=='nt'):
        parser.error('Require gcc, fresh output and Linux for sanitizers')
    source='test/cases/export_enum_abi.xl'; host='test/cases/export_enum_abi_host.c'
    files={Path(__file__),Path(cc),Path(sys.executable),cli,product,compiler,root/source,root/host}
    files.update(p for p in (root/'src').rglob('*') if p.suffix in ('.c','.h','.inc'))
    files.update((root/'release/include').rglob('*.h'))
    inputs=[{'Path':str(p.resolve()),'Sha256':digest(p)} for p in sorted(files)]
    rows=[]; report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Rows':rows,
        'Scope':__doc__,'Sanitized':args.sanitize,'SharedRuntimeInstrumented':False}
    out.mkdir(parents=True); private=out/'include/__xl_runtime'; private.mkdir(parents=True)
    for folder in [root/'src/runtime',root/'release/include']:
        for header in folder.glob('*.h'): shutil.copy2(header,private/header.name)
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    env=dict(os.environ)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-I'+str(out),'-I'+str(out/'include'),'-Irelease/include']
    links=[str(product)] if os.name=='nt' else ['-Lrelease','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    if args.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(name,command,timeout=120):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=timeout)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Complete':result.returncode==0})
        assert result.returncode==0,name+': '+data[-4000:].decode(errors='replace')
        return data.decode(errors='replace')
    try:
        run('abi',[str(cli),'abi',source,'--json','--emit',str(out/'abi.json')])
        abi=json.loads((out/'abi.json').read_text(encoding='utf-8'))
        numeric=next(s for s in abi['symbols'] if s['kind']=='numeric_enum' and s['name']=='HttpStatus')
        assert numeric['backing']=='uint16' and [v['value'] for v in numeric['variants']]==['200','201','404']
        packet=next(s for s in abi['symbols'] if s['kind']=='tagged_enum' and s['name']=='Packet')
        assert len(packet['variants'])==3 and packet['variants'][2]['name']=='failure'
        generated=out/'export_enum_abi.c'; header=out/'export_enum_abi.h'
        run('cgen',[str(cli),'cgen',source,'--inc-path',str(out/'include'),
            '--emit',str(generated),'--emit-header',str(header)])
        declaration=header.read_text(encoding='utf-8')
        assert 'xvalue* enum_make_packet(int64_t' in declaration and 'enum_make_failure(xlir_string' in declaration
        assert not any(t in declaration for t in ['XL_Enum_Packet','xl6_enum_copy_','xl6_enum_type_'])
        shared=out/('export_enum_abi.dll' if os.name=='nt' else 'export_enum_abi.so')
        run('shared',[str(cli),'build',source,'-shared','--inc-path',str(out/'include'),
            '-o',str(shared),'--emit-header',str(out/'export_enum_abi_shared.h')])
        for level in [0,2]:
            for lane in ['raw','shared']:
                binary=out/(f'{lane}.O{level}'+('.exe' if os.name=='nt' else ''))
                selected=[str(generated),host] if lane=='raw' else ['-DXL6_USE_SHARED',host,str(shared)]
                run(f'{lane}.O{level}.build',[cc,*flags,f'-O{level}',*selected,*links,'-o',str(binary)])
                log=run(f'{lane}.O{level}.run',[str(binary)])
                assert 'enum closed C ABI:' in log and 'foreign errors passed' in log
        report['InputsUnchanged']=all(digest(Path(row['Path']))==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); print(error,file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Enum C ABI: {len(rows)} checks, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
