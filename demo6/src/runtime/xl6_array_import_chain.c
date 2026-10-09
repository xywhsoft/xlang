#include "src/runtime/xl6_array_import_chain.h"
#include "src/runtime/xl6_collection_import_chain.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_persistence_restore.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_CHAIN=65 };
typedef struct Template {
    XL6_Sequence* owner;
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter;
    const XL6_CollectionImportProviderV1* provider;
    ptr context;
} Template;
typedef struct Memo {
    const xvalue* source;
    xvalue* result;
    size_t depth;
} Memo;
typedef struct Import {
    xvalue** seeds;
    size_t count;
    Template templates[MAX_CHAIN];
    Memo* memo;
    size_t used,capacity;
    const xvalue* active[MAX_CHAIN];
    const XL6_CollectionImportPlanV1* plan;
    xl6_error_scope* operation;
} Import;
static void releasePreservingError(xvalue* value)
{
    if (!value) return;
    xerror* prior=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xrtErrorFree(prior); xrtValueRelease(value);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    } else {
        if (!prior) prior=xrtTakeError();
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(prior);
    }
}
static bool successful(const Import* ctx)
{ return !xrtGetError() && !ctx->operation->failed; }
static bool admits(const Template* schema,const xvalue* value)
{ return schema->provider ? schema->provider->Admits(schema->context,value) : xl6SequenceAdmitValueV1(schema->owner,value); }
static size_t hash(const xvalue* source,size_t depth)
{
    /* Unsigned wrap is intentional. Pointer equality, not the hash, decides
     * identity; uintptr_t is never cast back into or dereferenced as a pointer. */
    size_t value=(size_t)(uintptr_t)source;
    value^=value>>4;
    return value*(size_t)UINT32_C(2654435761)+depth*(size_t)UINT32_C(2246822519);
}
static Memo* lookup(Import* ctx,const xvalue* source,size_t depth)
{
    if (!ctx->capacity) return NULL;
    size_t at=hash(source,depth)&(ctx->capacity-1);
    while (ctx->memo[at].source) {
        if (ctx->memo[at].source==source && ctx->memo[at].depth==depth) return &ctx->memo[at];
        at=(at+1)&(ctx->capacity-1);
    }
    return NULL;
}
static bool remember(Import* ctx,const xvalue* source,size_t depth,xvalue* result)
{
    if (ctx->used>=ctx->capacity/2) {
        if (ctx->capacity>SIZE_MAX/2/sizeof(Memo)) { xrtSetErrorKind(XERR_RANGE); return false; }
        size_t capacity=ctx->capacity ? ctx->capacity*2 : 16;
        Memo* prepared=xrtMalloc(capacity*sizeof(*prepared));
        if (!prepared) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
        memset(prepared,0,capacity*sizeof(*prepared));
        for (size_t i=0;i<ctx->capacity;++i) if (ctx->memo[i].source) {
            size_t at=hash(ctx->memo[i].source,ctx->memo[i].depth)&(capacity-1);
            while (prepared[at].source) at=(at+1)&(capacity-1);
            prepared[at]=ctx->memo[i];
        }
        xrtFree(ctx->memo); ctx->memo=prepared; ctx->capacity=capacity;
    }
    xvalue* ownedSource=xrtValueRetain(source);
    if (!ownedSource) return false;
    xvalue* ownedResult=xrtValueRetain(result);
    if (!ownedResult) { releasePreservingError(ownedSource); return false; }
    size_t at=hash(source,depth)&(ctx->capacity-1);
    while (ctx->memo[at].source) at=(at+1)&(ctx->capacity-1);
    ctx->memo[at]=(Memo){ownedSource,ownedResult,depth}; ++ctx->used;
    return true;
}
static bool privateSeed(const Import* ctx,const xvalue* value)
{
    for (size_t i=0;i<ctx->count;++i) if (ctx->seeds[i]==value) return true;
    return false;
}
static xvalue* childSeed(const Template* schema)
{
    if (schema->provider) return schema->provider->Empty(schema->context);
    XL6_SequenceAdmissionV3 admission={0};
    if (!xl6SequenceAdmissionCopyV3(schema->owner,&admission)) return NULL;
    xvalue* value=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,schema->layout.identity,&admission);
    if (!value) return NULL;
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,schema->layout.identity,
        schema->layout.elementIdentity,schema->layout.transfer,schema->layout.size,schema->layout.type);
    bool ok=owner && xl6SequenceBindProjectionV1(owner,schema->converter);
    if (owner) xl6SequenceEndV1(owner);
    if (!ok) { releasePreservingError(value); return NULL; }
    return value;
}
static bool fill(Import* ctx,size_t depth,xvalue* value,XL6_Sequence* target,const xvalue* source);
static xvalue* convert(Import* ctx,size_t depth,const xvalue* source)
{
    Memo* prior=lookup(ctx,source,depth);
    if (prior) return xrtValueRetain(prior->result);
    const Template* schema=&ctx->templates[depth];
    xvalue* value=childSeed(schema);
    if (!value) return NULL;
    XL6_Sequence* owner=schema->provider ? NULL : xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,schema->layout.identity,
        schema->layout.elementIdentity,schema->layout.transfer,schema->layout.size,schema->layout.type);
    bool ok=successful(ctx) && (schema->provider || owner) && fill(ctx,depth,value,owner,source);
    if (owner) xl6SequenceEndV1(owner);
    if (ok) ok=remember(ctx,source,depth,value) && successful(ctx);
    if (!ok) { releasePreservingError(value); return NULL; }
    return value;
}
static xvalue* restoreLeaf(const XL6_CollectionImportPlanV1* plan,const xvalue* source)
{
    uint64_t identity=xrtValueTypeId(source);
    if (plan->leafRule!=XL6_VALUE_ADMIT_ANY && identity && identity!=plan->leafIdentity) {
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    if (xrtValueType(source)==XVALUE_NULL) {
        if (plan->leafNullable || plan->leafRule==XL6_VALUE_ADMIT_ANY) return xrtValueRetain(source);
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    xvalue* value=xl6PersistenceRestoreScalarV1(source,plan->leafRule,
        (plan->flags&XL6_COLLECTION_IMPORT_JSON_V1)!=0);
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE); return NULL; }
    /* Clone of an immutable primitive may be the SAME source shell. Never
     * retag it to manufacture the destination's primitive identity. */
    if (plan->leafIdentity && xrtValueTypeId(value)!=plan->leafIdentity &&
        xrtValueType(value)!=XVALUE_BOOL && value==source) {
        xvalue* fresh=NULL; xstrview text={0}; xbytesview bytes={0}; xtime time={0};
        if (plan->leafRule==XL6_VALUE_ADMIT_STRING && xrtValueGetString(source,&text)) fresh=xrtValueString(text);
        else if (plan->leafRule==XL6_VALUE_ADMIT_BYTES && xrtValueGetBytes(source,&bytes)) fresh=xrtValueBytes(bytes);
        else if (plan->leafRule==XL6_VALUE_ADMIT_TIME && xrtValueGetTime(source,&time)) fresh=xrtValueTime(time);
        releasePreservingError(value); value=fresh;
        if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    }
    if (plan->leafIdentity && xrtValueType(value)!=XVALUE_BOOL &&
        xrtValueTypeId(value)!=plan->leafIdentity && !xrtValueTypeIdBind(value,plan->leafIdentity)) {
        releasePreservingError(value); return NULL;
    }
    return value;
}
static bool fill(Import* ctx,size_t depth,xvalue* value,XL6_Sequence* target,const xvalue* source)
{
    XL6_ArrayInputV1 input={0}; bool ok=false;
    if (privateSeed(ctx,source)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    for (size_t i=0;i<depth;++i) if (ctx->active[i]==source) { xrtSetErrorKind(XERR_VALUE); return false; }
    ctx->active[depth]=source;
    if (!xl6ArrayInputAcquireV1(source,&input)) goto done;
    const Template* schema=&ctx->templates[depth];
    xtypedarray* storage=target ? xl6SequenceStorageV1(target) : NULL;
    if (storage && !xrtTypedArrayReserve(storage,input.count)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        goto done;
    }
    for (size_t i=0;i<input.count;++i) {
        const xvalue* item=input.items[i]; xvalue* converted=NULL;
        if (privateSeed(ctx,item)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        /* Only an explicit next family/template authorizes wire conversion.
         * A native child keeps its actual identity and full owned metadata. */
        if (depth+1<ctx->count && xrtValueType(item)==XVALUE_ARRAY) {
            converted=convert(ctx,depth+1,item);
            if (!converted) goto done;
            item=converted;
        } else if (depth+1==ctx->count && ctx->plan &&
            (ctx->plan->flags&XL6_COLLECTION_IMPORT_RESTORE_V1)) {
            converted=restoreLeaf(ctx->plan,item);
            if (!converted || !successful(ctx)) { releasePreservingError(converted); goto done; }
            item=converted;
        }
        bool pushed=schema->provider ? schema->provider->Append(value,(int64_t)i,item) :
            xl6SequenceAdmitValueV1(target,item) && xrtTypedArrayPushValue(storage,item,schema->converter);
        if (!pushed && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        releasePreservingError(converted);
        if (!pushed || !successful(ctx)) goto done;
    }
    ok=true;
done:
    ctx->active[depth]=NULL;
    xl6ArrayInputReleaseV1(&input); return ok && successful(ctx);
}
static bool validPlan(const XL6_CollectionImportPlanV1* plan,size_t count)
{
    if (!plan || plan->size!=sizeof(*plan) ||
        (plan->flags&~(XL6_COLLECTION_IMPORT_RESTORE_V1|XL6_COLLECTION_IMPORT_JSON_V1)) ||
        ((plan->flags&XL6_COLLECTION_IMPORT_JSON_V1) && !(plan->flags&XL6_COLLECTION_IMPORT_RESTORE_V1))) return false;
    if (plan->flags&XL6_COLLECTION_IMPORT_RESTORE_V1) {
        if (plan->leafRule<XL6_VALUE_ADMIT_ANY || plan->leafRule>XL6_VALUE_ADMIT_TIME ||
            (plan->leafRule==XL6_VALUE_ADMIT_ANY ? plan->leafIdentity!=0 : !plan->leafIdentity)) return false;
    } else if (plan->leafRule || plan->leafIdentity || plan->leafNullable) return false;
    for (size_t i=0;plan->providers && i<count;++i) {
        const XL6_CollectionImportProviderV1* p=plan->providers[i];
        if (p && (!p->Begin || !p->End || !p->Empty || !p->Admits || !p->Append)) return false;
    }
    return true;
}
static xvalue* importChain(xvalue** seeds,size_t count,const xvalue* source,bool sourceOwned,
    const XL6_CollectionImportPlanV1* plan,bool legacy)
{
    if (!seeds || !count || count>MAX_CHAIN) {
        xrtSetErrorKind(XERR_ARGUMENT);
        if (sourceOwned) releasePreservingError((xvalue*)source);
        return NULL;
    }
    bool upstream=!source;
    for (size_t i=0;i<count;++i) upstream=upstream || !seeds[i];
    xerror* incoming=xrtErrorRef(xrtGetError());
    xl6_error_scope operation={0};
    if (!(upstream ? xl6ErrorScopeBeginCleanup(&operation,true) : xl6ErrorScopeBegin(&operation))) {
        xerror* failure=upstream ? incoming : xrtErrorRef(xrtGetError());
        if (!upstream) xrtErrorFree(incoming);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,failure});
        for (size_t i=0;i<count;++i) { xvalue* owned=seeds[i]; seeds[i]=NULL; releasePreservingError(owned); }
        if (sourceOwned) releasePreservingError((xvalue*)source);
        return NULL;
    }
    xrtErrorFree(incoming);
    Import ctx={.seeds=seeds,.count=count,.plan=plan,.operation=&operation}; bool ok=false;
    xvalue* result=NULL; xvalue* observed=NULL;
    if (operation.failed) goto done;
    if (!legacy && !validPlan(plan,count)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    if (plan && (plan->flags&XL6_COLLECTION_IMPORT_JSON_V1) &&
        (plan->leafRule==XL6_VALUE_ADMIT_BYTES || plan->leafRule==XL6_VALUE_ADMIT_TIME)) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    /* Keep a borrowed input alive across all native allocation/cleanup hooks.
     * An input aliasing a consumed seed is invalid, but still retained until
     * rollback has completed. Duplicate seed entries each own one reference. */
    observed=source ? xrtValueRetain(source) : NULL;
    if (!observed) { if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    for (size_t i=0;i<count;++i) {
        if (!seeds[i]) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
            goto done;
        }
        if (seeds[i]==source) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        for (size_t j=0;j<i;++j) if (seeds[j]==seeds[i]) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        Template* schema=&ctx.templates[i];
        schema->provider=plan && plan->providers ? plan->providers[i] : NULL;
        if (schema->provider) {
            if (!schema->provider->Begin(seeds[i],&schema->context) || !schema->context || !successful(&ctx)) goto done;
        } else {
            int status=xl6SequenceBeginErasedArrayV1(seeds[i],&schema->owner,&schema->layout,&schema->converter);
            if (status<=0) { if (!status) xrtSetErrorKind(XERR_TYPE); goto done; }
            XL6_SequenceAdmissionV3 admission={0};
            if (!xl6SequenceAdmissionCopyV3(schema->owner,&admission)) goto done;
            bool admitted=admission.ops!=NULL; xl6SequenceAdmissionDropV3(&admission);
            if (!admitted || xrtTypedArrayCount(xl6SequenceStorageV1(schema->owner))) {
                xrtSetErrorKind(XERR_STATE); goto done;
            }
        }
        if (i && (legacy ? !xl6SequenceNestedChildCheckV1(ctx.templates[i-1].owner,seeds[i]) :
            !admits(&ctx.templates[i-1],seeds[i]))) { if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE); goto done; }
        if (!successful(&ctx)) goto done;
    }
    if (legacy) {
        if (!xl6SequenceNestedChildCheckV1(ctx.templates[count-1].owner,NULL)) goto done;
        ok=fill(&ctx,0,seeds[0],ctx.templates[0].owner,observed);
        if (ok) result=seeds[0];
    } else {
        result=convert(&ctx,0,observed); ok=result && successful(&ctx);
    }
done:
    if (!ok || xrtGetError() || operation.failed) xl6ErrorScopeCaptureFailure(&operation);
    /* Unpin every template before consuming any shell. A provider End may
     * complete a finalizer, so its failure also withdraws the unpublished root. */
    for (size_t i=count;i>0;--i) {
        Template* schema=&ctx.templates[i-1];
        if (schema->provider && schema->context) schema->provider->End(schema->context);
        if (schema->owner) xl6SequenceEndV1(schema->owner);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    }
    for (size_t i=0;i<count;++i) {
        xvalue* value=seeds[i]; seeds[i]=NULL;
        if (!legacy || !ok || i) releasePreservingError(value);
    }
    for (size_t i=0;i<ctx.capacity;++i) if (ctx.memo[i].source) {
        releasePreservingError(ctx.memo[i].result);
        releasePreservingError((xvalue*)ctx.memo[i].source);
    }
    xrtFree(ctx.memo); releasePreservingError(observed);
    if (sourceOwned) releasePreservingError((xvalue*)source);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { releasePreservingError(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
xvalue* xl6ArrayImportChainTakeV1(xvalue** seeds,size_t count,const xvalue* source)
{ return importChain(seeds,count,source,false,NULL,true); }
xvalue* xl6ArrayImportChainSourceTakeV1(xvalue** seeds,size_t count,xvalue* source)
{ return importChain(seeds,count,source,true,NULL,true); }
xvalue* xl6CollectionImportChainTakeV1(xvalue** seeds,size_t count,const xvalue* source,const XL6_CollectionImportPlanV1* plan)
{ return importChain(seeds,count,source,false,plan,false); }
xvalue* xl6CollectionImportChainSourceTakeV1(xvalue** seeds,size_t count,xvalue* source,const XL6_CollectionImportPlanV1* plan)
{ return importChain(seeds,count,source,true,plan,false); }
