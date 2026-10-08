#!/usr/bin/env python3
"""Resident Tree-reference admission and legacy nested Sequence regression.

Compile all three changed provider bodies and both hosts at O0/O2. Complete
allocation-failure prefixes cover shallow construction and mixed depth64.
This does not certify a rebuilt shipping library or full language surface.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    for name in ('public','runtime'):
        parser.add_argument('--'+name,type=Path)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists() and bool(args.public)!=bool(args.runtime)
    providers=['src/runtime/xl6_tree.c','src/runtime/xl6_tree_admission.c','src/runtime/xl6_array_sequence_admission.c']
    hosts=['test/cases/stdlib_tree_admission_native_host.c','test/cases/stdlib_array_sequence_admission_native_host.c']
    support=['test/support/module_product_memory.c','test/support/module_product_memory.h']
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable)}
    files.update(root/name for name in providers+hosts+support)
    for directory in ('src','lib','release/include'):
        files.update(f for f in (root/directory).rglob('*') if f.suffix in ('.c','.h','.inc'))
    if args.public:
        assert not args.sanitize and sys.platform=='win32'
        manifest=args.public.resolve()/'build.json'; files.add(manifest)
        build=json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        omitted={'xlir_public_source_driver.o','xl6_tree.o','xl6_tree_admission.o','xl6_array_sequence_admission.o'}
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in omitted]
        dll=root/'release/xl.dll'; files.add(dll)
        links=[*objects,str(dll),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']; suffix='.exe'
    else:
        assert sys.platform=='linux'; runtime=args.runtime.resolve(); suffix=''
        files.update(runtime/name for name in ('libxl.so','libxlc.so'))
        links=['-L'+str(runtime),'-lxlc','-lxl','-Wl,-rpath,'+str(runtime),'-pthread','-ldl','-lm']
    inputs=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(files)]
    out.mkdir(parents=True)
    if args.public:
        shutil.copy2(dll,out/'xl.dll')
    rows,prefixes=[],[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Platform':sys.platform,
            'Full53Acceptance':False,'LanguageGraphAcceptance':False,'ShippingLibraryAcceptance':False,
            'WholeRuntimeInstrumented':False,'ProvidersInstrumented':args.sanitize,'HostInstrumented':args.sanitize,
            'Inputs':inputs,'Rows':rows,'Prefixes':prefixes}

    def save():
        (out/'tree.admission.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start})
        save()
        if result.returncode:
            raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')

    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include']
    if args.sanitize:
        flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    try:
        for level in (0,2):
            lane='O'+str(level)
            for host in hosts:
                label='tree' if 'tree_admission' in host else 'sequence'
                binary=out/(label+'-'+lane+suffix)
                run(label+'.'+lane+'.build',[cc,*flags,'-O'+str(level),*providers,host,
                                             *([support[0]] if label=='tree' else []),*links,'-o',binary])
                text=run(label+'.'+lane+'.vectors',[binary])
                assert 'balanced' in text
                if label=='tree':
                    for operation in ('create','deep'):
                        for point in range(4097):
                            text=run(lane+'.'+operation+'.prefix.'+str(point),[binary,operation,point])
                            if 'hit=0 complete-prefix balanced' in text:
                                prefixes.append({'Optimization':lane,'Operation':operation,
                                                 'FirstNoHit':point,'Runs':point+1}); break
                        else:
                            raise AssertionError('Allocation prefix did not close: '+operation)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); raise
    finally:
        save()
    print(f'Tree composite admission: {len(rows)} phases, {len(prefixes)} complete OOM prefixes, balanced',flush=True)


if __name__=='__main__':
    main()
