"""Synchronize only exact-baseline Value graph sections in both products.

Core time ABI and every other source module remain on the reviewed consumer
snapshot. No public API is added; the symbol table must remain unchanged.
"""
from pathlib import Path
import argparse, json, re, subprocess
import sync_xrt_snapshot as snapshot
from sync_xruntime_dynamic_fields import body, replace, digest
from gen_xrt_symbol_table import _api_names, _render
from gen_xrt_xlang_matrix import sync as matrix_sync

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--xrt-root',type=Path,required=True)
    parser.add_argument('--baseline',required=True)
    args=parser.parse_args(); root=args.root.resolve(); supplier=args.xrt_root.resolve()
    sources=('include/xrt/value.h','src/value/value_graph.c')
    rows=[]; replacements=[]
    for path in sources:
        old=subprocess.check_output(['git','show',args.baseline+':'+path],cwd=supplier)
        new=(supplier/path).read_bytes()
        old_body,new_body=body(old),body(new)
        if path==sources[0]:
            # The supplier's Value time comment is milliseconds; this consumer
            # is still microseconds. Match only the complete graph-rule block,
            # never rewrite or normalize an unrelated source line.
            marker='/* Exact nominal sequence policies for data projection.'
            finish='} xvaluegraphsequencev1;'
            old_body=old_body[old_body.index(marker):old_body.index(finish)+len(finish)]
            new_body=new_body[new_body.index(marker):new_body.index(finish)+len(finish)]
            strip_comments=lambda text: re.sub(r'/\*.*?\*/','',text,flags=re.S).split()
            assert strip_comments(old_body)==strip_comments(new_body), 'Unreviewed public graph ABI change'
        replacements.append((path,old_body,new_body))
        rows.append(dict(Path=path,BaselineSha256=digest(old),Sha256=digest(new)))
    prepared={}; before={}
    # Preflight every target before publishing any generated section.
    for product in ('xrt','xruntime'):
        for declaration in (False,True):
            path=root/('release/include/'+product+'_decl.h' if declaration else
                'lib/'+product+'/'+product+'.h')
            data=path.read_bytes(); before[path]=data
            text=data.decode().replace('\r\n','\n')
            for source,old,new in replacements:
                public=source==sources[0]
                if declaration and not public: continue
                text=replace(text,('public: ' if public else 'source: ')+source,old,new)
            prepared[path]=text.encode()
    for path,data in prepared.items(): snapshot._write_if_changed(path,data)
    core=root/'lib/xrt/xrt.h'; runtime=root/'lib/xruntime/xruntime.h'
    core_names=set(_api_names(core)); runtime_names=set(_api_names(runtime))
    assert (root/'src/api/xl_xrt_symbols.inc').read_text(encoding='utf-8')==_render(runtime)
    for product,count,extra in (('xrt',len(core_names),None),
        ('xruntime',len(runtime_names),len(runtime_names-core_names))):
        header=root/('lib/'+product+'/'+product+'.h')
        declarations=root/('release/include/'+product+'_decl.h')
        prefix='xrt/single/' if product=='xrt' else 'xrt/extlibs/xruntime/single/'
        metadata=snapshot._metadata(product,prefix+product+'.h',prefix+product+'_decl.h',
            header.read_bytes(),declarations.read_bytes(),count,extra)
        snapshot._write_if_changed(root/('lib/'+product+'/'+product+'.snapshot.json'),
            (json.dumps(metadata,indent=2)+'\n').encode())
    matrix_sync(root); assert snapshot._check_snapshot(root)
    proof=dict(Schema=1,Baseline=args.baseline,Module='value_graph',Sources=rows,
        Targets=[dict(Path=path.relative_to(root).as_posix(),Before=digest(before[path]),
            After=digest(path.read_bytes())) for path in prepared],
        PublicMatchScope='Exact complete xvaluegraphsequencev1 documentation and declaration block',
        OtherModuleBodiesUnchanged=True,PublicApiUnchanged=True)
    snapshot._write_if_changed(root/'lib/xruntime/value_graph.snapshot.json',
        (json.dumps(proof,indent=2)+'\n').encode())
    print('Exact-baseline Value graph sections synchronized; other modules unchanged')
    return 0
if __name__=='__main__': raise SystemExit(main())
