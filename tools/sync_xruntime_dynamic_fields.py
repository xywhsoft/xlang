"""Update the DynamicFields generated sections without changing Core's time ABI.

The consumer's bundled snapshot and the current upstream Core have different
time contracts. A full product sync must be a separate reviewed migration.
This narrow update requires exact equality of the untouched baseline module,
uses only upstream source, and reuses the normal metadata/symbol/matrix gates.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys
import sync_xrt_snapshot as snapshot
from gen_xrt_symbol_table import _api_names, _render
from gen_xrt_xlang_matrix import sync as matrix_sync


def digest(data): return hashlib.sha256(data).hexdigest().upper()


def body(data):
    text = data.decode('utf-8').replace('\r\n', '\n')
    # These two modules include only the product-local dependency headers.
    lines = text.splitlines(keepends=True)
    for line in lines:
        if line.lstrip().startswith('#include'):
            if not re.match(r'\s*#include [<"](?:xrt/|\.\./internal/)', line):
                raise ValueError('unreviewed include in DynamicFields supplier: '+line)
    return ''.join(line for line in lines if not line.lstrip().startswith('#include')).strip()


def replace(text, label, old, new):
    marker = '/* '+label+' */'
    if text.count(marker) != 1: raise ValueError('ambiguous snapshot section: '+label)
    at = text.index(marker)
    end = text.find('/* ========================================================================== */\n/* ', at+len(marker))
    if end < 0: end = len(text)
    part = text[at:end]
    # Amalgamation may pad section boundaries/blank lines. Match every
    # nonempty source line exactly; never ignore code or comment differences.
    pattern=r'\n(?:[ \t]*\n)*'.join(re.escape(line) for line in old.splitlines() if line.strip())
    matches=list(re.finditer(pattern,part))
    if len(matches) != 1:
        raise ValueError('baseline module differs from consumer; refuse inferred rebase: '+label)
    match=matches[0]
    return text[:at]+part[:match.start()]+new+part[match.end():]+text[end:]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--xrt-root',type=Path,required=True)
    p.add_argument('--baseline',required=True,help='Reviewed supplier commit before this module-only update')
    a=p.parse_args(); root=a.root.resolve(); supplier=a.xrt_root.resolve()
    paths=('extlibs/xruntime/include/xrt/runtime_field.h',
           'extlibs/xruntime/src/runtime/runtime_dynamic_field.c')
    rows=[]; replacements=[]
    for path in paths:
        old=subprocess.check_output(['git','show',a.baseline+':'+path],cwd=supplier)
        new=(supplier/path).read_bytes(); replacements.append((path,body(old),body(new)))
        rows.append(dict(Path=path,BaselineSha256=digest(old),Sha256=digest(new)))
    for command in ([sys.executable,str(supplier/'tools/check_release_maturity.py'),'--release','--manifest','extlibs/xruntime/config/modules.json'],
                    [sys.executable,str(supplier/'tools/amalgamate.py'),'--check','--manifest','extlibs/xruntime/config/modules.json']):
        subprocess.run(command,cwd=supplier,check=True)
    header=root/'lib/xruntime/xruntime.h'; declaration=root/'release/include/xruntime_decl.h'
    old_header=header.read_bytes(); old_decl=declaration.read_bytes()
    implementation=old_header.decode('utf-8').replace('\r\n','\n')
    declarations=old_decl.decode('utf-8').replace('\r\n','\n')
    for path,old,new in replacements:
        public=path==paths[0]; label=('public: ' if public else 'source: ')+path
        implementation=replace(implementation,label,old,new)
        if public: declarations=replace(declarations,label,old,new)
    # Everything outside the two baseline module bodies remains untouched.
    snapshot._write_if_changed(header,implementation.encode())
    snapshot._write_if_changed(declaration,declarations.encode())
    core_names=set(_api_names(root/'lib/xrt/xrt.h')); names=set(_api_names(header))
    metadata=snapshot._metadata('xruntime','xrt/extlibs/xruntime/single/xruntime.h',
        'xrt/extlibs/xruntime/single/xruntime_decl.h',header.read_bytes(),declaration.read_bytes(),
        len(names),len(names-core_names))
    snapshot._write_if_changed(root/'lib/xruntime/xruntime.snapshot.json',
        (json.dumps(metadata,indent=2)+'\n').encode())
    snapshot._write_if_changed(root/'src/api/xl_xrt_symbols.inc',_render(header).encode())
    matrix_sync(root)
    assert snapshot._check_snapshot(root)
    proof=dict(Schema=1,Baseline=a.baseline,Module='runtime_dynamic_field',Sources=rows,
        ImplementationBefore=digest(old_header),ImplementationAfter=digest(header.read_bytes()),
        DeclarationsBefore=digest(old_decl),DeclarationsAfter=digest(declaration.read_bytes()),
        CoreUnchanged=True,OtherModuleBodiesUnchanged=True)
    snapshot._write_if_changed(root/'lib/xruntime/dynamic_fields.snapshot.json',
        (json.dumps(proof,indent=2)+'\n').encode())
    print('Exact baseline DynamicFields module update complete')
    return 0


if __name__=='__main__': raise SystemExit(main())
