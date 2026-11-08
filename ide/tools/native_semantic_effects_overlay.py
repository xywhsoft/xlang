"""Private current checker write observation; upstream language rules stay intact."""
from pathlib import Path
import hashlib
from native_current_unified_profile import updated_pins
from native_c_interop_profile import async_corpus
from check_native_bridge_catalog import function


INCLUDES={
    'xl6_sem_c_owned.c':'d1dd260e6770a1091fc7ee22938bc92fcdde36e086a76bc886b443ee2e817adf',
    'xl6_sem_special_collection.c':'5b5433565e190455fee63e7a7c579106c7cf1501cf1f9a0e4cd029156749fc42',
    'xl6_sem_constructor.inc':'4324e6be688eedf51dada4c8ed09a1965217237b8ddf41971f940752aa5ad8e8',
}


def derive(frontend:Path,output:Path,source:str):
    audited={}
    expected=updated_pins(frontend,{'src/sem/'+name:digest for name,digest in INCLUDES.items()})
    for name,digest in expected.items():
        path=frontend/name
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
    async_seams=0
    if async_corpus(frontend):
        one=function(body,'xl6SemCOwnedAsyncResultMethodOne')
        observed=one.replace('{','{\n    xlsSemFunctionMutation(ctx);',1)
        before='    if(!xl6SemAssignEntitySymbolIds(ctx,copy))'
        if observed.count(before)!=1:raise ValueError('async worker allocation seam changed')
        observed=observed.replace(before,'    xlsSemDeclarationEffectBegin(ctx);\n'+before,1)
        before='    return checked ? copy : NULL;'
        if observed.count(before)!=1:raise ValueError('async worker completed declaration seam changed')
        observed=observed.replace(before,'    xlsSemDeclarationEffectEnd(ctx,owner,copy);\n'+before,1)
        body=body.replace(one,observed,1);async_seams=3
    ownership_seams=0
    for old,count in (('old',2),('previous',int(async_seams!=0))):
        if not count:continue
        before=f'*{old}=*expr;*expr=*replacement;'
        if body.count(before)!=count:raise ValueError('native expression replacement ownership seam changed')
        body=body.replace(before,before+f'xlsAstRestoreReplacementOwner(expr,{old});')
        ownership_seams+=count
    body='#include "analysis/ast_storage.h"\n'+body
    before='*old=*expr;parts[0]=old;parts[1]=type;*expr=*binding;'
    if source.count(before)!=1:raise ValueError('contextual method replacement ownership seam changed')
    source='#include "analysis/ast_storage.h"\n'+source.replace(before,before+'xlsAstRestoreReplacementOwner(expr,old);',1)
    ownership_seams+=1
    include='#include "src/sem/xl6_sem_c_owned.c"'
    if source.count(include)!=1:raise ValueError('shared C-owned include seam changed')
    compiled=output/'frontend/xl6_sem_c_owned.effects.c'
    compiled.write_text(body,encoding='utf8',newline='\n')
    return source.replace(include,'#include "xl6_sem_c_owned.effects.c"'),dict(
        auditedDependencies=audited,compiledSource=str(compiled),
        compiledSha256=hashlib.sha256(compiled.read_bytes()).hexdigest(),
        mutationSeams=3+async_seams,asyncResultMutationSeams=async_seams,
        replacementOwnershipSeams=ownership_seams,
        semantics='observe actual declaration emission and lexical rewrite; exact ordered declaration receipts; original checker produces every mutation')
