"""Private current checker write observation; upstream language rules stay intact."""
from pathlib import Path
import hashlib


INCLUDES={
    'xl6_sem_c_owned.c':'d1dd260e6770a1091fc7ee22938bc92fcdde36e086a76bc886b443ee2e817adf',
    'xl6_sem_special_collection.c':'5b5433565e190455fee63e7a7c579106c7cf1501cf1f9a0e4cd029156749fc42',
    'xl6_sem_constructor.inc':'4324e6be688eedf51dada4c8ed09a1965217237b8ddf41971f940752aa5ad8e8',
}


def derive(frontend:Path,output:Path,source:str):
    audited={}
    for name,digest in INCLUDES.items():
        path=frontend/'src/sem'/name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError('shared semantic include changed; audit reads/writes: '+name)
        audited[str(path)]=digest
    original=frontend/'src/sem/xl6_sem_c_owned.c'
    body=original.read_text(encoding='utf8')
    changes={
        '    if(!specialized) {\n        specialized=signature;signature=NULL;':
            '    if(!specialized) {\n        xlsSemDeclarationEffectBegin(ctx);\n        specialized=signature;signature=NULL;',
        '    }\n    xl6AstFree(signature);\n    xrtFree(call->children[0]->text);call->children[0]->text=xrtStrDup(name);':
            '        xlsSemDeclarationEffectEnd(ctx,owner,specialized);\n    }\n    xl6AstFree(signature);\n    xlsSemFunctionMutation(ctx);\n    xrtFree(call->children[0]->text);call->children[0]->text=xrtStrDup(name);',
    }
    for before,after in changes.items():
        if body.count(before)!=1:raise ValueError('shared C-owned mutation seam changed')
        body=body.replace(before,after)
    include='#include "src/sem/xl6_sem_c_owned.c"'
    if source.count(include)!=1:raise ValueError('shared C-owned include seam changed')
    compiled=output/'frontend/xl6_sem_c_owned.effects.c'
    compiled.write_text(body,encoding='utf8',newline='\n')
    return source.replace(include,'#include "xl6_sem_c_owned.effects.c"'),dict(
        auditedDependencies=audited,compiledSource=str(compiled),
        compiledSha256=hashlib.sha256(compiled.read_bytes()).hexdigest(),
        mutationSeams=3,semantics='observe actual declaration emission and lexical rewrite; exact ordered declaration receipts; original checker produces every mutation')
