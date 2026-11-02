/* Original canonical strings only. These assets own no AST,type registry,
 * source object,memo,report or prior result. Live semantic receipts,not this
 * owner or its address,authorize using them in another compilation. */
#include "analysis/frontend_sem.h"
#include "analysis/frontend_index.h"
#include "analysis/frontend_cancel.h"
#define XLS_INDEX_CANONICAL_BYTES (8u*1024u*1024u)
typedef struct xls_index_canonical_row {char* value;XL6_AstKind kind;} xls_index_canonical_row;
struct xls_index_canonical {xatomic32 references;xarray rows;size_t bytes;};
typedef struct xls_index_canonical_scope {
    struct xls_index_canonical_scope* previous;
    XL6_IndexBuild* build;const xls_index_storage* baseline;
    const xls_sem_memo *memo,*previousMemo;const XL6_AstNode* previousRoot;
    size_t limit;xls_index_canonical_work work;
} xls_index_canonical_scope;
static _Thread_local xls_index_canonical_scope* xlsIndexCanonicalScope;
static _Thread_local xls_index_canonical_control* xlsIndexCanonicalControl;
static size_t xlsIndexCanonicalBudget(void)
{return xlsIndexCanonicalControl ? xlsIndexCanonicalControl->bytes : XLS_INDEX_CANONICAL_BYTES;}
void xlsFrontendIndexCanonicalBegin(xls_index_canonical_control* control,size_t bytes)
{if(control){size_t limit=xlsIndexCanonicalBudget();*control=(xls_index_canonical_control){xlsIndexCanonicalControl,bytes<limit ? bytes : limit};xlsIndexCanonicalControl=control;}}
void xlsFrontendIndexCanonicalEnd(xls_index_canonical_control* control)
{if(control && xlsIndexCanonicalControl==control)xlsIndexCanonicalControl=control->previous;}
static void xlsIndexCanonicalRelease(xls_index_canonical* owner)
{
    if(!owner || xrtAtomic32FetchSub(&owner->references,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t i=0;i<owner->rows.Count;++i)xrtFree(((xls_index_canonical_row*)xrtArrayGet(&owner->rows,i))->value);
    xrtArrayUnit(&owner->rows);xrtFree(owner);
}
static int xlsIndexCanonicalAdmission(const xls_index_canonical_scope* scope,size_t slot,int previous,size_t* count)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    return xlsFrontendSemMemoIndexEquivalent(scope->memo,scope->build->root,
        previous ? scope->previousMemo : NULL,previous ? scope->previousRoot : NULL,slot,count);
#else
    (void)scope;(void)slot;(void)previous;*count=0;return 0;
#endif
}
static int xlsIndexCanonicalPrepare(XL6_IndexBuild* build,XL6_IndexFact* fact)
{
    xls_index_canonical_scope* scope=xlsIndexCanonicalScope;
    if(!scope || scope->build!=build || !build->storage || !build->storage->active || !scope->limit)return 1;
    xls_index_storage* storage=build->storage;
    if(storage->active>=storage->slots)return 0;
    xls_function_index_asset* chunk=&storage->chunks[storage->active];
    if(!chunk->canonicalChecked){
        chunk->canonicalChecked=1;size_t slot=storage->active-1;
        if(!xlsIndexCanonicalAdmission(scope,slot,0,&chunk->canonicalCount))return 1;
        const xls_function_index_asset* old=scope->baseline && storage->active<scope->baseline->slots ?
            &scope->baseline->chunks[storage->active] : NULL;
        size_t count=0;xls_index_canonical* owner=old ? old->canonical : NULL;
        if(owner && owner->rows.Count==chunk->canonicalCount &&
           xlsIndexCanonicalAdmission(scope,slot,1,&count) && count==owner->rows.Count &&
           owner->bytes<=scope->limit-scope->work.retainedBytes){
            size_t metadata=sizeof(*owner)+owner->rows.Capacity*sizeof(xls_index_canonical_row);
            /* Record/canonical payload keeps the original collector's charge.
             * The independent owner and actual vector capacity are additional.
             * This is a retained-result quota,not a transient RSS promise. */
            if(metadata<=XL6_INDEX_BYTES-build->budget){
                (void)xl6IndexBudget(build,metadata);chunk->canonical=owner;
                xrtAtomic32FetchAdd(&owner->references,1,XMEMORY_RELAXED);
                scope->work.retainedBytes+=owner->bytes;++scope->work.reused;
            }else ++scope->work.declined;
        }
    }
    if(chunk->canonical){
        const xls_index_canonical_row* row=xrtArrayConstGet(&chunk->canonical->rows,chunk->count-1);
        if(!row || row->kind!=fact->node->kind || !!row->value!=!!fact->type){
            build->error="canonical receipt shape mismatch";return 0;
        }
        fact->canonical=row->value;
    }
    return 1;
}
static char* xlsIndexCanonicalValue(XL6_IndexBuild* build,XL6_IndexFact* fact)
{
    xls_index_canonical_scope* scope=xlsIndexCanonicalScope;
    if(scope && scope->build==build){
        if(fact->canonical){++scope->work.valuesReused;return fact->canonical;}
        ++scope->work.calls;
    }
    return xl6TypeToCanonicalStringLimited(build->root,fact->type,8192,128,8192);
}
static int xlsIndexCanonicalSeal(xls_index_canonical_scope* scope)
{
    XL6_IndexBuild* build=scope->build;xls_index_storage* storage=build->storage;
    if(!storage || !scope->limit)return 1;
    for(size_t slot=1;slot<storage->slots;++slot){
        if(xlsFrontendCancelPoll())return 1;
        xls_function_index_asset* chunk=&storage->chunks[slot];
        if(chunk->canonical){
            if(chunk->count!=chunk->canonical->rows.Count){build->error="canonical receipt node count mismatch";return 0;}
            continue;
        }
        if(!chunk->storage || !chunk->canonicalCount || chunk->count!=chunk->canonicalCount)continue;
        size_t left=scope->limit-scope->work.retainedBytes;
        if(left<sizeof(xls_index_canonical)){++scope->work.declined;continue;}
        xls_index_canonical* owner=xrtCalloc(1,sizeof(*owner));
        if(!owner){++scope->work.declined;continue;}
        int okay=xrtArrayInit(&owner->rows,sizeof(xls_index_canonical_row)) && xrtArrayResize(&owner->rows,chunk->count);
        size_t bytes=sizeof(*owner);
        if(okay && owner->rows.Capacity<=(left-bytes)/sizeof(xls_index_canonical_row))bytes+=owner->rows.Capacity*sizeof(xls_index_canonical_row);
        else okay=0;
        size_t metadata=bytes;
        for(size_t i=0;okay && i<chunk->count;++i){
            if((i&63u)==0 && xlsFrontendCancelPoll()){okay=0;break;}
            const XL6_IndexFact* fact=&chunk->facts[i];
            /* The original builder's64-byte minimum and6*length allowance.
             * Already charged once by this same collector for every row. */
            size_t value=fact->canonical ? 64+6*strlen(fact->canonical) : 0;
            if(value>left-bytes){okay=0;break;}bytes+=value;
            *(xls_index_canonical_row*)xrtArrayGet(&owner->rows,i)=(xls_index_canonical_row){fact->canonical,fact->node->kind};
        }
        if(okay && metadata<=XL6_INDEX_BYTES-build->budget){
            xrtAtomic32Init(&owner->references,1);owner->bytes=bytes;chunk->canonical=owner;
            (void)xl6IndexBudget(build,metadata);scope->work.retainedBytes+=bytes;++scope->work.captured;
        }else{
            /* Transfer has not committed. The original rows still own ALL
             * strings,including any pointers copied into this private vector. */
            xrtArrayUnit(&owner->rows);xrtFree(owner);++scope->work.declined;
        }
    }
    return 1;
}
