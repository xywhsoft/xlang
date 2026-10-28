"""Resolve every authoritative public XRT/xruntime function from both libraries."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--runtime-directory',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    root,out=args.root.resolve(),args.out.resolve()
    runtime=args.runtime_directory.resolve() if args.runtime_directory else root/'release'
    assert not out.exists(), 'Preserve previous export evidence'
    cc=shutil.which('gcc'); assert cc
    helper=Path(__file__).resolve().parents[1]/'test/cases/xrt_exports_all.c'
    generator=root/'tools/gen_xrt_symbol_table.py'
    spec=importlib.util.spec_from_file_location('export_names',generator)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    header=root/'lib/xruntime/xruntime.h'
    table=root/'src/api/xl_xrt_symbols.inc'
    expected=module._api_names(header)
    actual=re.findall(r'^XL_XRT_SYMBOL\((xrt\w+)\)$',table.read_text(encoding='utf-8-sig'),re.M)
    assert expected==actual, 'Do not certify a stale or incomplete generated table'
    libraries=[runtime/name for name in (('xl.dll','xlc.dll') if os.name=='nt' else ('libxl.so','libxlc.so'))]
    inputs={str(path):sha(path) for path in [Path(__file__).resolve(),Path(cc),helper,generator,header,table,*libraries]}
    out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,Functions=len(expected),Inputs=inputs,Rows=[],
        Scope='Export presence only, not exhaustive runtime semantics or sandbox certification')
    def run(name,command):
        command=list(map(str,command));result=subprocess.run(command,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout)
        report['Rows'].append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),Sha256=sha(log)))
        assert result.returncode==0,(name,result.stdout[-6000:].decode(errors='replace'))
    try:
        binary=out/('exports.exe' if os.name=='nt' else 'exports')
        run('build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-I'+str(root),helper,
            *([] if os.name=='nt' else ['-ldl']),'-o',binary])
        for library in libraries: run(library.stem,[binary,library])
        report['InputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
        assert report['InputsUnchanged'];report['Complete']=True
    except Exception as error:
        report['Error']=repr(error);raise
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('All public XRT exports resolved:',len(expected),'functions from each library')


if __name__=='__main__':
    main()
