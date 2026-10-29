"""Narrow, exact-baseline TypedSet supplier update; retain consumer Core ABI.

This changes only the public typed_set.h and implementation typed_set.c
sections. It refuses inferred rebases, regenerates normal symbol/metadata
outputs, and records provenance. It is not a full supplier Core migration.
"""
from pathlib import Path
import argparse, json, subprocess, sys
import sync_xrt_snapshot as snapshot
from gen_xrt_symbol_table import _api_names, _render
from gen_xrt_xlang_matrix import sync as matrix_sync
from sync_xruntime_dynamic_fields import body, replace, digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--xrt-root',type=Path,required=True)
    parser.add_argument('--baseline',required=True)
    args=parser.parse_args(); root=args.root.resolve(); supplier=args.xrt_root.resolve()
    paths=('extlibs/xruntime/include/xrt/typed_set.h','extlibs/xruntime/src/runtime/typed_set.c')
    rows=[]; sections=[]
    for path in paths:
        old=subprocess.check_output(['git','show',args.baseline+':'+path],cwd=supplier)
        new=(supplier/path).read_bytes(); sections.append((path,body(old),body(new)))
        rows.append(dict(Path=path,BaselineSha256=digest(old),Sha256=digest(new)))
    for name,options in (('check_release_maturity.py',['--release']),('amalgamate.py',['--check'])):
        subprocess.run([sys.executable,str(supplier/'tools'/name),*options,
            '--manifest','extlibs/xruntime/config/modules.json'],cwd=supplier,check=True)
    header=root/'lib/xruntime/xruntime.h'; declaration=root/'release/include/xruntime_decl.h'
    before=header.read_bytes(); before_decl=declaration.read_bytes()
    implementation=before.decode().replace('\r\n','\n')
    declarations=before_decl.decode().replace('\r\n','\n')
    for path,old,new in sections:
        public=path==paths[0]; label=('public: ' if public else 'source: ')+path
        implementation=replace(implementation,label,old,new)
        if public: declarations=replace(declarations,label,old,new)
    snapshot._write_if_changed(header,implementation.encode())
    snapshot._write_if_changed(declaration,declarations.encode())
    core=set(_api_names(root/'lib/xrt/xrt.h')); names=set(_api_names(header))
    metadata=snapshot._metadata('xruntime','xrt/extlibs/xruntime/single/xruntime.h',
        'xrt/extlibs/xruntime/single/xruntime_decl.h',header.read_bytes(),declaration.read_bytes(),
        len(names),len(names-core))
    snapshot._write_if_changed(root/'lib/xruntime/xruntime.snapshot.json',
        (json.dumps(metadata,indent=2)+'\n').encode())
    snapshot._write_if_changed(root/'src/api/xl_xrt_symbols.inc',_render(header).encode())
    matrix_sync(root); assert snapshot._check_snapshot(root)
    proof=dict(Schema=1,Baseline=args.baseline,Module='typed_set',Sources=rows,
        ImplementationBefore=digest(before),ImplementationAfter=digest(header.read_bytes()),
        DeclarationsBefore=digest(before_decl),DeclarationsAfter=digest(declaration.read_bytes()),
        CoreUnchanged=True,OtherModuleBodiesUnchanged=True)
    snapshot._write_if_changed(root/'lib/xruntime/typed_set.snapshot.json',
        (json.dumps(proof,indent=2)+'\n').encode())
    print('Exact-baseline TypedSet sync complete: '+str(len(names))+' APIs')
    return 0


if __name__=='__main__': raise SystemExit(main())
