"""Offline embedded scalar-domain parity: GCC/TCC, SDK/provider TUs, O0/O2.
Declaration-only SDK clients and implementation providers are distinct TUs;
this does not promise declaration-first feature reselection in one TU.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=args.out.resolve();assert not out.exists()
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    source=root/'test/support/xrt_scalar_domain_snapshot.c'
    inputs=[Path(__file__).resolve(),source,*compilers,root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',
            root/'lib/xrt/xrt.snapshot.json',root/'lib/xruntime/xruntime.snapshot.json',
            root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',root/'src/api/xl_xrt_symbols.inc']
    inputs={str(path):sha(path) for path in inputs}
    out.mkdir(parents=True);rows=[]
    report=dict(Complete=False,Rows=rows,Inputs=inputs,InputsUnchanged=False,LogsUnchanged=False,
                NetworkTests=False,FullSourceCompilerAcceptance=False,ShippingLibraryAcceptance=False)
    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        log=out/(name+'.log')
        with log.open('wb') as stream:
            result=subprocess.run(list(map(str,command)),cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Log=str(log),Sha256=sha(log)))
        save();print(name+': '+str(result.returncode),flush=True)
        assert not result.returncode,log.read_text(errors='replace')[-4000:]
    save()
    try:
        symbols=(root/'src/api/xl_xrt_symbols.inc').read_text(encoding='utf-8')
        for symbol in ('xrtValueScalarDomainV1','xrtValueScalarDomainBindV1'):
            assert symbols.count('XL_XRT_SYMBOL('+symbol+')')==1
        libraries=[] if os.name=='nt' else ['-pthread','-lm']
        for index,cc in enumerate(compilers):
            flags=[cc,'-Wall','-Werror','-I'+str(root/'release'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/xrt')]
            if cc.stem.lower()!='tcc': flags+=['-std=c11','-Wextra']
            if os.name!='nt': flags+=['-D_GNU_SOURCE']
            for runtime in (0,1):
                for sdk in (0,1):
                    for opt in ('O0','O2'):
                        name=f'{index}.runtime{runtime}.sdk{sdk}.{opt}'
                        binary=out/(name+('.exe' if os.name=='nt' else ''))
                        objects=[]
                        if sdk:
                            provider=out/(name+'.provider.o')
                            run(name+'.provider',[*flags,'-'+opt,'-DTEST_RUNTIME='+str(runtime),
                                '-DDECLARATIONS_FIRST=0','-DSNAPSHOT_PROVIDER=1','-c',source,'-o',provider])
                            objects.append(provider)
                        run(name+'.build',[*flags,'-'+opt,'-DTEST_RUNTIME='+str(runtime),
                            '-DDECLARATIONS_FIRST='+str(sdk),source,*objects,*libraries,'-o',binary])
                        run(name+'.run',[binary])
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        report['Complete']=report['InputsUnchanged'] and report['LogsUnchanged']
        save();assert report['Complete']
    except BaseException as error:
        report['Error']=repr(error);save();raise


if __name__=='__main__':
    main()
