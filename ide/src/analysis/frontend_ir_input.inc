/* Optional key-equivalence proofs for the current immutable typed-input path.
 * Original key serialization,live lookups and raw restore checks remain the
 * fallback. No AST/result/memo is retained by one proof. */
#include "analysis/typed_module_internal.h"
#include "analysis/entity_store.h"
#define XLS_IR_INPUT_BYTES (1024u*1024u)
#define XLS_IR_INPUT_QUERIES 1024u
typedef struct xls_ir_entity_projection {XL6_SymbolId native;uint64_t identity;} xls_ir_entity_projection;
struct xls_ir_input_proof {
    xatomic32 references;
    xls_typed_input_identity* identity;
    xls_entity_store* store;uint64_t domain;
    xarray lexical,entities;
    size_t bytes;
};
typedef struct xls_ir_input_scope {
    struct xls_ir_input_scope* previous;
    xls_sem_memo* semantic;const xls_typed_module* input;const XL6_AstNode* root;
} xls_ir_input_scope;
static _Thread_local xls_ir_input_scope* xlsActiveIrInput;
typedef struct xls_ir_input_builder {
    xmap seen;xarray entities;int initialized,okay;
} xls_ir_input_builder;
typedef struct xls_ir_input_key {
    xls_typed_input_identity* identity;
    const xls_ir_input_proof* prior;
    xls_ir_input_builder builder;
    int borrowed,published;
} xls_ir_input_key;
_Static_assert(sizeof(xls_lexical_symbol)==8,"audit padding-free lexical projection");
static xls_ir_input_proof* xlsIrInputProofRetain(xls_ir_input_proof* proof)
{if(proof)xrtAtomic32FetchAdd(&proof->references,1,XMEMORY_RELAXED);return proof;}
static void xlsIrInputProofRelease(xls_ir_input_proof* proof)
{
    if(!proof || xrtAtomic32FetchSub(&proof->references,1,XMEMORY_ACQ_REL)!=1)return;
    xrtArrayUnit(&proof->lexical);xrtArrayUnit(&proof->entities);
    xlsTypedInputIdentityRelease(proof->identity);xlsEntityStoreRelease(proof->store);xrtFree(proof);
}
static void xlsIrInputBuilderClear(xls_ir_input_builder* builder)
{
    if(builder->initialized){xrtMapUnit(&builder->seen);xrtArrayUnit(&builder->entities);}
    *builder=(xls_ir_input_builder){0};
}
static void xlsIrInputObserve(void* user,XL6_SymbolId native,uint64_t identity)
{
    xls_ir_input_builder* builder=user;if(!builder->okay)return;
    /* xlsEntityFind rejects native0 before consulting a view. The encoder
     * still executed that original lookup;no zero-row metadata is needed. */
    if(!native){if(identity)builder->okay=0;return;}
    xbytesview key={(const unsigned char*)&native,sizeof(native)};
    const size_t* old=xrtMapGet(&builder->seen,key);
    if(old){
        const xls_ir_entity_projection* value=xrtArrayConstGet(&builder->entities,*old);
        if(!value || value->identity!=identity)builder->okay=0;
        return;
    }
    size_t at=builder->entities.Count;
    xls_ir_entity_projection value={native,identity};
    if(at>=XLS_IR_INPUT_QUERIES || !xrtArrayPush(&builder->entities,&value) ||
       builder->entities.Capacity>XLS_IR_INPUT_BYTES/sizeof(value) ||
       !xrtMapSet(&builder->seen,key,&at))builder->okay=0;
}
static int xlsIrInputMatch(const xls_ir_input_proof* proof,xls_typed_input_identity* identity,
    const xls_lexical_symbols* symbols,const xls_entity_view* entities)
{
    if(!proof || !identity || proof->identity!=identity || !symbols->ready ||
       proof->store!=xlsEntityViewStore(entities) || proof->domain!=xlsEntityViewInfo(entities).domain ||
       proof->lexical.Count!=symbols->order.Count)return 0;
    if(proof->lexical.Count && memcmp(proof->lexical.Data,symbols->order.Data,
        proof->lexical.Count*sizeof(xls_lexical_symbol)))return 0;
    for(size_t i=0;i<proof->entities.Count;++i){
        const xls_ir_entity_projection* query=xrtArrayConstGet(&proof->entities,i);
        if(xlsEntityViewSymbol(entities,query->native)!=query->identity)return 0;
    }
    return 1;
}
static void xlsIrInputBeginMemo(xls_ir_memo* memo,const XL6_AstNode* root,size_t limit)
{
    if(!xlsActiveIrInput || xlsActiveIrInput->root!=root || !xlsActiveIrInput->semantic ||
       memo->bytes>limit || memo->entries.Count>(limit-memo->bytes)/sizeof(xls_ir_input_proof*))return;
    if(!xrtArrayInit(&memo->inputs,sizeof(xls_ir_input_proof*)) ||
       !xrtArrayResize(&memo->inputs,memo->entries.Count) ||
       memo->inputs.Capacity>(limit-memo->bytes)/sizeof(xls_ir_input_proof*)){
        xrtArrayUnit(&memo->inputs);return;
    }
    size_t bytes=memo->inputs.Capacity*sizeof(xls_ir_input_proof*);
    memo->bytes+=bytes;memo->inputWork.bytes+=bytes;
}
static int xlsIrInputKeyBegin(xls_ir_input_key* state,xls_ir_memo_build* build,
    const XL6_AstNode* source,size_t slot,const xls_ir_unit* old,size_t oldSlot,
    const xls_lexical_symbols* symbols,xstrbuf* key)
{
    xls_ir_input_scope* scope=xlsActiveIrInput;
    if(scope && scope->root==build->root && scope->semantic)
        state->identity=xlsFrontendSemMemoIrIdentity(scope->semantic,scope->input,slot);
    xls_ir_input_proof* const* prior=old && build->baseline && oldSlot<build->baseline->inputs.Count ?
        xrtArrayConstGet(&build->baseline->inputs,oldSlot) : NULL;
    if(prior && xlsIrInputMatch(*prior,state->identity,symbols,build->memo->entities)){
        state->prior=*prior;state->borrowed=1;*key=old->key;
        ++build->memo->inputWork.reused;build->memo->inputWork.avoidedBytes+=key->Size;
        return 1;
    }
    ++build->memo->inputWork.encoded;
    if(state->identity && slot<build->memo->inputs.Count){
        state->builder.initialized=1;
        state->builder.okay=xrtMapInit(&state->builder.seen,sizeof(size_t)) &&
            xrtArrayInit(&state->builder.entities,sizeof(xls_ir_entity_projection));
    }
    if(state->builder.okay)return xlsFrontendIrAstKeyEntitiesObserved(key,source,symbols,
        build->memo->entities,xlsIrInputObserve,&state->builder);
    return xlsFrontendIrAstKeyEntities(key,source,symbols,build->memo->entities);
}
static int xlsIrInputKeyOwn(xls_ir_input_key* state,xls_ir_memo_build* build,xstrbuf* key,size_t limit)
{
    if(!state->borrowed)return 1;
    xstrbuf copy={0};
    if(!xlsIrMemoData(&copy,key->Data,key->Size,limit)){xrtStrBufFree(&copy);return 0;}
    build->memo->inputWork.copiedBytes+=key->Size;
    *key=copy;state->borrowed=0;return 1;
}
static xls_ir_input_proof* xlsIrInputCreate(xls_ir_input_key* state,
    const xls_lexical_symbols* symbols,const xls_entity_view* entities,size_t budget)
{
    if(!state->identity || !state->builder.okay || budget<=sizeof(xls_ir_input_proof))return NULL;
    if(budget>XLS_IR_INPUT_BYTES)budget=XLS_IR_INPUT_BYTES;
    xls_ir_input_proof* proof=xrtCalloc(1,sizeof(*proof));if(!proof)return NULL;
    xrtAtomic32Init(&proof->references,1);
    proof->identity=xlsTypedInputIdentityRetain(state->identity);
    proof->store=xlsEntityStoreRetain((xls_entity_store*)xlsEntityViewStore(entities));
    proof->domain=xlsEntityViewInfo(entities).domain;
    proof->entities=state->builder.entities;state->builder.entities=(xarray){0};
    if(!xrtArrayInit(&proof->lexical,sizeof(xls_lexical_symbol)) ||
       !xrtArrayResize(&proof->lexical,symbols->order.Count))goto failed;
    if(symbols->order.Count)memcpy(proof->lexical.Data,symbols->order.Data,
        symbols->order.Count*sizeof(xls_lexical_symbol));
    proof->bytes=sizeof(*proof)+xlsTypedInputIdentityOwnedBytes(proof->identity)+
        proof->entities.Capacity*sizeof(xls_ir_entity_projection)+proof->lexical.Capacity*sizeof(xls_lexical_symbol);
    if(proof->bytes>budget)goto failed;
    return proof;
failed:xlsIrInputProofRelease(proof);return NULL;
}
static void xlsIrInputKeyPublish(xls_ir_input_key* state,xls_ir_memo_build* build,
    size_t slot,const xls_lexical_symbols* symbols)
{
    if(slot>=build->memo->inputs.Count || !state->identity || build->memo->bytes>build->limit)return;
    size_t budget=build->limit-build->memo->bytes;
    xls_ir_input_proof* proof=state->prior && state->prior->bytes<=budget ?
        xlsIrInputProofRetain((xls_ir_input_proof*)state->prior) :
        xlsIrInputCreate(state,symbols,build->memo->entities,budget);
    if(!proof)return;
    *(xls_ir_input_proof**)xrtArrayGet(&build->memo->inputs,slot)=proof;
    build->memo->bytes+=proof->bytes;build->memo->inputWork.bytes+=proof->bytes;
    if(!state->prior)++build->memo->inputWork.captured;
    state->published=1;
}
static void xlsIrInputKeyEnd(xls_ir_input_key* state,xls_ir_memo_build* build,xstrbuf* key)
{
    if(state->borrowed)*key=(xstrbuf){0};
    if(state->identity && !state->published)++build->memo->inputWork.declined;
    xlsIrInputBuilderClear(&state->builder);
}
