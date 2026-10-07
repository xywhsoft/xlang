#include "src/runtime/xl6_array_import_chain.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_input.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_CHAIN=65 };
typedef struct Template {
    XL6_Sequence* owner;
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter;
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
} Import;
static void releasePreservingError(xvalue* value)
{
    xerror* prior=xrtTakeError(); xrtValueRelease(value);
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
}
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
static bool fill(Import* ctx,size_t depth,XL6_Sequence* target,const xvalue* source);
static xvalue* convert(Import* ctx,size_t depth,const xvalue* source)
{
    Memo* prior=lookup(ctx,source,depth);
    if (prior) return xrtValueRetain(prior->result);
    const Template* schema=&ctx->templates[depth];
    xvalue* value=childSeed(schema);
    if (!value) return NULL;
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,schema->layout.identity,
        schema->layout.elementIdentity,schema->layout.transfer,schema->layout.size,schema->layout.type);
    bool ok=owner && fill(ctx,depth,owner,source);
    if (owner) xl6SequenceEndV1(owner);
    if (ok) ok=remember(ctx,source,depth,value);
    if (!ok) { releasePreservingError(value); return NULL; }
    return value;
}
static bool fill(Import* ctx,size_t depth,XL6_Sequence* target,const xvalue* source)
{
    XL6_ArrayInputV1 input={0}; bool ok=false;
    if (privateSeed(ctx,source)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!xl6ArrayInputAcquireV1(source,&input)) return false;
    xtypedarray* storage=xl6SequenceStorageV1(target);
    if (!xrtTypedArrayReserve(storage,input.count)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        goto done;
    }
    for (size_t i=0;i<input.count;++i) {
        const xvalue* item=input.items[i]; xvalue* converted=NULL;
        if (privateSeed(ctx,item)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        /* Admission is a checked predicate, not a coercion. Only the explicit
         * next Array plan entry authorizes converting a wire child. */
        if (depth+1<ctx->count && xrtValueType(item)==XVALUE_ARRAY) {
            converted=convert(ctx,depth+1,item);
            if (!converted) goto done;
            item=converted;
        }
        bool pushed=xl6SequenceAdmitValueV1(target,item) &&
            xrtTypedArrayPushValue(storage,item,ctx->templates[depth].converter);
        if (!pushed && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        releasePreservingError(converted);
        if (!pushed || xrtGetError()) goto done;
    }
    ok=true;
done:
    xl6ArrayInputReleaseV1(&input); return ok && !xrtGetError();
}
static xvalue* importChain(xvalue** seeds,size_t count,const xvalue* source,bool sourceOwned)
{
    if (!seeds || !count || count>MAX_CHAIN) {
        xrtSetErrorKind(XERR_ARGUMENT);
        if (sourceOwned) releasePreservingError((xvalue*)source);
        return NULL;
    }
    xerror* incoming=xrtTakeError();
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) abort();
    Import ctx={.seeds=seeds,.count=count}; bool ok=false;
    /* Keep a borrowed input alive across all native allocation/cleanup hooks.
     * An input aliasing a consumed seed is invalid, but still retained until
     * rollback has completed. Duplicate seed entries each own one reference. */
    xvalue* observed=source ? xrtValueRetain(source) : NULL;
    if (!observed) { if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    for (size_t i=0;i<count;++i) {
        if (!seeds[i]) {
            if (!xrtGetError() && incoming) { xrtSetErrorTake(incoming); incoming=NULL; }
            if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
            goto done;
        }
        if (seeds[i]==source) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        for (size_t j=0;j<i;++j) if (seeds[j]==seeds[i]) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        Template* schema=&ctx.templates[i];
        int status=xl6SequenceBeginErasedArrayV1(seeds[i],&schema->owner,&schema->layout,&schema->converter);
        if (status<=0) { if (!status) xrtSetErrorKind(XERR_TYPE); goto done; }
        XL6_SequenceAdmissionV3 admission={0};
        if (!xl6SequenceAdmissionCopyV3(schema->owner,&admission)) goto done;
        bool admitted=admission.ops!=NULL; xl6SequenceAdmissionDropV3(&admission);
        if (!admitted || xrtTypedArrayCount(xl6SequenceStorageV1(schema->owner))) {
            xrtSetErrorKind(XERR_STATE); goto done;
        }
        if (i && !xl6SequenceNestedChildCheckV1(ctx.templates[i-1].owner,seeds[i])) goto done;
    }
    if (!xl6SequenceNestedChildCheckV1(ctx.templates[count-1].owner,NULL)) goto done;
    ok=fill(&ctx,0,ctx.templates[0].owner,observed);
done:
    /* Save the transaction's first outcome before dropping partial graphs.
     * Every pin closes before any consumed shell is destroyed. */
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    for (size_t i=count;i>0;--i) if (ctx.templates[i-1].owner) xl6SequenceEndV1(ctx.templates[i-1].owner);
    xvalue* result=ok ? seeds[0] : NULL;
    for (size_t i=0;i<count;++i) {
        xvalue* value=seeds[i]; seeds[i]=NULL;
        if (!ok || i) releasePreservingError(value);
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
    if (!outcome.ok) { xrtErrorFree(incoming); xl6ErrorScopePropagateFailure(outcome); }
    else xrtSetErrorTake(incoming);
    return result;
}
xvalue* xl6ArrayImportChainTakeV1(xvalue** seeds,size_t count,const xvalue* source)
{ return importChain(seeds,count,source,false); }
xvalue* xl6ArrayImportChainSourceTakeV1(xvalue** seeds,size_t count,xvalue* source)
{ return importChain(seeds,count,source,true); }
