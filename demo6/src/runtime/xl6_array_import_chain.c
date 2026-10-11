#include "src/runtime/xl6_array_import_chain.h"
#include "src/runtime/xl6_collection_import_chain.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_persistence_restore.h"
#include "src/runtime/xl6_persistence_restore_plan.h"
#include "src/runtime/xl6_serialization_read.h"
#include "src/runtime/xl6_ownership_read.h"
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
    const xvalue* instance; /* Independent owned pin, including when == result. */
    xvalue* base; /* Actual first initialized object; never rerun its factory. */
    size_t pass;
    bool isolated;
} Memo;
typedef struct WireMemo {
    xvalue* source;
    xvalue* snapshot;
} WireMemo;
typedef struct StagedFields {
    const xvalue* receiver; /* Borrowed from the independently owned record. */
    xvalueobjectfieldsv1* fields;
} StagedFields;
typedef struct Import {
    xvalue** seeds;
    size_t count;
    Template* templates;
    Memo* memo;
    size_t used,capacity;
    const xvalue* active[MAX_CHAIN];
    const XL6_CollectionImportPlanV1* plan;
    xl6_error_scope* operation;
    bool nullableRead,wireRejected;
    StagedFields* staged;
    size_t stagedCount,stagedCapacity;
    WireMemo* wire;
    size_t wireUsed,wireCapacity,pass;
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
static bool restoring(const Import* ctx)
{ return successful(ctx) && !ctx->wireRejected; }
/* Only call at a proven wire-shape/domain rejection. Failures from factories,
 * native providers, converters or retirement keep their checked outcome. */
static void rejectWire(Import* ctx,xerrkind kind)
{
    if (!successful(ctx)) return;
    if (ctx->nullableRead) ctx->wireRejected=true;
    else xrtSetErrorKind(kind);
}
static bool admits(const Template* schema,const xvalue* value)
{ return schema->provider ? schema->provider->Admits(schema->context,value) : xl6SequenceAdmitValueV1(schema->owner,value); }
static bool textKeys(const Template* schema)
{ return schema->provider && schema->provider->AppendText; }
static bool providerValid(const XL6_CollectionImportProviderV1* p)
{
    return !p || (p->Begin && p->End && p->Empty && p->Admits &&
        (!!p->Append != !!p->AppendText) && (!!p->InputBegin == !!p->InputEnd) &&
        (!p->AppendText || p->InputBegin));
}
static bool familyInputValid(const Template* schema,const XL6_CollectionImportInputV1* input)
{
    if ((input->count && !input->items) || (textKeys(schema) ?
        (input->keys || (input->count && !input->textKeys)) :
        (input->textKeys || (!input->keys && input->count>INT64_MAX)))) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    for (size_t i=0;textKeys(schema) && i<input->count;++i)
        if (input->textKeys[i].Size && !input->textKeys[i].Data) {
            xrtSetErrorKind(XERR_ARGUMENT); return false;
        }
    return true;
}
static size_t hash(const xvalue* source,size_t depth,const xvalue* instance)
{
    /* Unsigned wrap is intentional. Pointer equality, not the hash, decides
     * identity; uintptr_t is never cast back into or dereferenced as a pointer. */
    size_t value=(size_t)(uintptr_t)source;
    value^=value>>4;
    size_t witness=(size_t)(uintptr_t)instance;
    witness^=witness>>4;
    return value*(size_t)UINT32_C(2654435761)+depth*(size_t)UINT32_C(2246822519)+
        witness*(size_t)UINT32_C(3266489917);
}
static Memo* lookup(Import* ctx,const xvalue* source,size_t depth,const xvalue* instance)
{
    if (!ctx->capacity) return NULL;
    size_t at=hash(source,depth,instance)&(ctx->capacity-1);
    while (ctx->memo[at].source) {
        if (ctx->memo[at].source==source && ctx->memo[at].depth==depth &&
            ctx->memo[at].instance==instance) return &ctx->memo[at];
        at=(at+1)&(ctx->capacity-1);
    }
    return NULL;
}
static bool remember(Import* ctx,const xvalue* source,size_t depth,xvalue* result,const xvalue* instance)
{
    if (ctx->used>=ctx->capacity/2) {
        if (ctx->capacity>SIZE_MAX/2/sizeof(Memo)) { xrtSetErrorKind(XERR_RANGE); return false; }
        size_t capacity=ctx->capacity ? ctx->capacity*2 : 16;
        Memo* prepared=xrtMalloc(capacity*sizeof(*prepared));
        if (!prepared) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
        memset(prepared,0,capacity*sizeof(*prepared));
        for (size_t i=0;i<ctx->capacity;++i) if (ctx->memo[i].source) {
            size_t at=hash(ctx->memo[i].source,ctx->memo[i].depth,ctx->memo[i].instance)&(capacity-1);
            while (prepared[at].source) at=(at+1)&(capacity-1);
            prepared[at]=ctx->memo[i];
        }
        xrtFree(ctx->memo); ctx->memo=prepared; ctx->capacity=capacity;
    }
    xvalue* ownedSource=xrtValueRetain(source);
    if (!ownedSource) return false;
    xvalue* ownedResult=xrtValueRetain(result);
    if (!ownedResult) { releasePreservingError(ownedSource); return false; }
    xvalue* ownedInstance=instance ? xrtValueRetain(instance) : NULL;
    if (instance && !ownedInstance) {
        releasePreservingError(ownedResult); releasePreservingError(ownedSource); return false;
    }
    size_t at=hash(source,depth,instance)&(ctx->capacity-1);
    while (ctx->memo[at].source) at=(at+1)&(ctx->capacity-1);
    ctx->memo[at]=(Memo){.source=ownedSource,.result=ownedResult,.depth=depth,.instance=ownedInstance}; ++ctx->used;
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
/* Only a genuine projected Array with the resident ANY policy can cross a
 * nested language erasure boundary. Typed native children retain their full
 * schema and ordinary reference semantics; a foreign/wrong typed handle is
 * never converted merely because its contents happen to fit. */
static bool languageArrayConversion(const Template* schema,const xvalue* source,bool* required)
{
    *required=false;
    XL6_SequenceAdmissionV3 policy={0};
    if (!xl6SequenceAdmissionCopyV3(schema->owner,&policy)) return false;
    bool matches=xl6SequenceAdmitsSchemaV1(source,XL6_SEQUENCE_ARRAY_V1,&schema->layout,&policy);
    xl6SequenceAdmissionDropV3(&policy);
    if (xrtGetError()) return false;
    if (matches) return true; /* Matching BUSY children need no slot reads. */
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout={0};
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    if (status<0) return false;
    if (!status) return true;
    bool erased=xl6SequenceReferenceAnyPolicyV1(owner,layout.elementIdentity);
    xl6SequenceEndV1(owner);
    if (xrtGetError()) return false;
    if (!erased) return true;
    *required=true;
    return !xrtGetError();
}
static xvalue* convert(Import* ctx,size_t depth,const xvalue* source)
{
    Memo* prior=lookup(ctx,source,depth,NULL);
    if (prior) return xrtValueRetain(prior->result);
    const Template* schema=&ctx->templates[depth];
    xvalue* value=childSeed(schema);
    if (!value) return NULL;
    XL6_Sequence* owner=schema->provider ? NULL : xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,schema->layout.identity,
        schema->layout.elementIdentity,schema->layout.transfer,schema->layout.size,schema->layout.type);
    bool ok=successful(ctx) && (schema->provider || owner) && fill(ctx,depth,value,owner,source);
    if (owner) xl6SequenceEndV1(owner);
    if (ok) ok=remember(ctx,source,depth,value,NULL) && successful(ctx);
    if (!ok) { releasePreservingError(value); return NULL; }
    return value;
}
static xvalue* restoreLeaf(const XL6_CollectionImportPlanV1* plan,const xvalue* source,bool* rejected)
{
    if (rejected) *rejected=false;
    uint64_t identity=xrtValueTypeId(source);
    if (plan->leafRule!=XL6_VALUE_ADMIT_ANY && identity && identity!=plan->leafIdentity) {
        if (rejected) *rejected=true; else xrtSetErrorKind(XERR_TYPE);
        return NULL;
    }
    if (xrtValueType(source)==XVALUE_NULL) {
        if (plan->leafNullable || plan->leafRule==XL6_VALUE_ADMIT_ANY) return xrtValueRetain(source);
        if (rejected) *rejected=true; else xrtSetErrorKind(XERR_TYPE);
        return NULL;
    }
    xvalue* value=xl6PersistenceRestoreScalarCheckedV1(source,plan->leafRule,
        (plan->flags&XL6_COLLECTION_IMPORT_JSON_V1)!=0,rejected);
    if (!value) {
        if (!(rejected && *rejected) && !xrtGetError()) xrtSetErrorKind(XERR_TYPE);
        return NULL;
    }
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
    XL6_CollectionImportInputV1 family={0};
    const Template* schema=&ctx->templates[depth];
    bool language=ctx->plan && (ctx->plan->flags&XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1);
    bool custom=schema->provider && schema->provider->InputBegin && (language || textKeys(schema));
    if (privateSeed(ctx,source)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    for (size_t i=0;i<depth;++i) if (ctx->active[i]==source) { xrtSetErrorKind(XERR_VALUE); return false; }
    ctx->active[depth]=source;
    if (textKeys(schema) && !language && xrtValueType(source)!=XVALUE_OBJECT) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    if (custom ? !schema->provider->InputBegin(source,&family) : !xl6ArrayInputAcquireV1(source,&input)) goto done;
    size_t count=custom ? family.count : input.count;
    if (custom && !familyInputValid(schema,&family)) goto done;
    xtypedarray* storage=target ? xl6SequenceStorageV1(target) : NULL;
    if (storage && !xrtTypedArrayReserve(storage,count)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        goto done;
    }
    for (size_t i=0;i<count;++i) {
        const xvalue* item=custom ? family.items[i] : input.items[i]; xvalue* converted=NULL;
        if (privateSeed(ctx,item)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        /* The next closed schema authorizes wire or exact ANY-erased input,
         * not arbitrary native retagging or an unchecked handle conversion. */
        bool erasedArray=false;
        if (depth+1<ctx->count && !ctx->templates[depth+1].provider &&
            (!ctx->plan || (ctx->plan->flags&XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1)) &&
            !languageArrayConversion(&ctx->templates[depth+1],item,&erasedArray)) goto done;
        bool erasedFamily=false;
        if (depth+1<ctx->count && ctx->plan &&
            (ctx->plan->flags&XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1)) {
            const Template* child=&ctx->templates[depth+1];
            if (child->provider && child->provider->InputConversion &&
                !child->provider->InputConversion(child->context,item,&erasedFamily)) goto done;
        }
        bool wireList=depth+1<ctx->count && ctx->plan &&
            (ctx->plan->flags&XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1) &&
            ctx->templates[depth+1].provider && ctx->templates[depth+1].provider->InputBegin &&
            !textKeys(&ctx->templates[depth+1]) &&
            xrtValueType(item)==XVALUE_INT_MAP;
        bool wireChild=depth+1<ctx->count && xrtValueType(item)==
            (textKeys(&ctx->templates[depth+1]) ? XVALUE_OBJECT : XVALUE_ARRAY);
        if (depth+1<ctx->count && (wireChild || wireList || erasedArray || erasedFamily)) {
            converted=convert(ctx,depth+1,item);
            if (!converted) goto done;
            item=converted;
        } else if (depth+1==ctx->count && ctx->plan &&
            (ctx->plan->flags&XL6_COLLECTION_IMPORT_RESTORE_V1)) {
            converted=restoreLeaf(ctx->plan,item,NULL);
            if (!converted || !successful(ctx)) { releasePreservingError(converted); goto done; }
            item=converted;
        }
        bool pushed;
        if (textKeys(schema)) pushed=schema->provider->AppendText(value,family.textKeys[i],item);
        else {
            int64_t key=custom && family.keys ? family.keys[i] : (int64_t)i;
            pushed=schema->provider ? schema->provider->Append(value,key,item) :
                xl6SequenceAdmitValueV1(target,item) && xrtTypedArrayPushValue(storage,item,schema->converter);
        }
        if (!pushed || !successful(ctx)) xl6ErrorScopeCaptureFailure(ctx->operation);
        releasePreservingError(converted);
        if (!pushed || !successful(ctx)) goto done;
    }
    ok=true;
done:
    /* A false callback without a diagnostic is still the first checked
     * failure. Latch it before snapshot retirement can report another error. */
    if (!ok || !successful(ctx)) xl6ErrorScopeCaptureFailure(ctx->operation);
    ctx->active[depth]=NULL;
    if (custom) schema->provider->InputEnd(&family);
    xl6ArrayInputReleaseV1(&input); return ok && successful(ctx);
}
static bool validPlan(const XL6_CollectionImportPlanV1* plan,size_t count)
{
    if (!plan || plan->size!=sizeof(*plan) ||
        (plan->flags&~(XL6_COLLECTION_IMPORT_RESTORE_V1|XL6_COLLECTION_IMPORT_JSON_V1|XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1)) ||
        ((plan->flags&XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1) &&
         (plan->flags&(XL6_COLLECTION_IMPORT_RESTORE_V1|XL6_COLLECTION_IMPORT_JSON_V1))) ||
        ((plan->flags&XL6_COLLECTION_IMPORT_JSON_V1) && !(plan->flags&XL6_COLLECTION_IMPORT_RESTORE_V1))) return false;
    if (plan->flags&XL6_COLLECTION_IMPORT_RESTORE_V1) {
        if (plan->leafRule<XL6_VALUE_ADMIT_ANY || plan->leafRule>XL6_VALUE_ADMIT_TIME ||
            (plan->leafRule==XL6_VALUE_ADMIT_ANY ? plan->leafIdentity!=0 : !plan->leafIdentity)) return false;
    } else if (plan->leafRule || plan->leafIdentity || plan->leafNullable) return false;
    for (size_t i=0;plan->providers && i<count;++i) {
        const XL6_CollectionImportProviderV1* p=plan->providers[i];
        if (!providerValid(p) || (p && (plan->flags&XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1) && !p->InputBegin)) return false;
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
    Template templates[MAX_CHAIN]={0};
    Import ctx={.seeds=seeds,.count=count,.templates=templates,.plan=plan,.operation=&operation}; bool ok=false;
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
        releasePreservingError((xvalue*)ctx.memo[i].instance);
        releasePreservingError(ctx.memo[i].base);
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

/* Object/field and collection nodes deliberately reuse the above memo,
 * template construction and first-error retirement. No nested import call or
 * second graph copier can split the caller's explicit restoration domain. */
static bool viewValid(xstrview key) { return key.Data || !key.Size; }
static bool keyEqual(xstrview a,xstrview b)
{ return a.Size==b.Size && (!a.Size || !memcmp(a.Data,b.Data,a.Size)); }
static bool graphPlanValid(const XL6_RestorePlanV1* plan,size_t count)
{
    if (!plan || plan->size!=sizeof(*plan) || !plan->nodeCount || !plan->nodes ||
        plan->root>=plan->nodeCount || plan->nodeCount>SIZE_MAX/sizeof(*plan->nodes) ||
        plan->fieldCount>SIZE_MAX/sizeof(*plan->fields) || (plan->fieldCount && !plan->fields)) return false;
    if (plan->readBoundary && (plan->readBoundary->size!=sizeof(*plan->readBoundary) ||
        !plan->readBoundary->Begin || !plan->readBoundary->Admits || !plan->readBoundary->End)) return false;
    for (size_t i=0;i<plan->nodeCount;++i) {
        const XL6_RestoreNodeV1* node=&plan->nodes[i];
        if (node->flags&~(XL6_RESTORE_NULLABLE_V1|XL6_RESTORE_FIELD_INSTANCE_V1)) return false;
        if (node->clone && node->kind!=XL6_RESTORE_OBJECT_V1) return false;
        bool instance=(node->flags&XL6_RESTORE_FIELD_INSTANCE_V1)!=0;
        if (instance && (node->kind!=XL6_RESTORE_OBJECT_V1 ||
            (node->flags&XL6_RESTORE_NULLABLE_V1) || i==plan->root || node->as.object.factory)) return false;
        if (node->kind==XL6_RESTORE_SCALAR_V1) {
            if (node->as.scalar<XL6_VALUE_ADMIT_ANY || node->as.scalar>XL6_VALUE_ADMIT_TIME ||
                (node->as.scalar==XL6_VALUE_ADMIT_ANY ? node->identity!=0 : !node->identity)) return false;
        } else if (node->kind==XL6_RESTORE_SEQUENCE_V1) {
            const XL6_CollectionImportProviderV1* p=node->as.sequence.provider;
            if (!node->identity || node->as.sequence.seed>=count || node->as.sequence.element>=plan->nodeCount ||
                !providerValid(p)) return false;
            if (plan->nodes[node->as.sequence.element].flags&XL6_RESTORE_FIELD_INSTANCE_V1) return false;
            for (size_t j=0;j<i;++j) if (plan->nodes[j].kind==XL6_RESTORE_SEQUENCE_V1 &&
                plan->nodes[j].as.sequence.seed==node->as.sequence.seed) return false;
        } else if (node->kind==XL6_RESTORE_OBJECT_V1) {
            if (!node->identity || node->as.object.runtimeTag<=0 || node->as.object.first>plan->fieldCount ||
                node->as.object.count>plan->fieldCount-node->as.object.first || !node->as.object.provider ||
                !node->as.object.provider->IsNative || !node->as.object.provider->Admits ||
                !node->as.object.provider->OwnershipAdmits) return false;
            for (size_t j=0;j<node->as.object.count;++j) {
                const XL6_RestoreFieldV1* field=&plan->fields[node->as.object.first+j];
                if (!viewValid(field->source) || !viewValid(field->target) || field->node>=plan->nodeCount ||
                    (field->flags&~(XL6_RESTORE_REQUIRED_V1|XL6_RESTORE_IN_PLACE_V1)) ||
                    keyEqual(field->target,XRT_STR_LITERAL("$xlir.class.runtime-type"))) return false;
                if (((field->flags&XL6_RESTORE_IN_PLACE_V1)!=0) !=
                    ((plan->nodes[field->node].flags&XL6_RESTORE_FIELD_INSTANCE_V1)!=0)) return false;
                for (size_t k=0;k<j;++k) if (keyEqual(field->target,
                    plan->fields[node->as.object.first+k].target)) return false;
            }
            if (node->open && (!node->as.object.provider->PrepareOpen ||
                !xl6RestoreOpenRecipeValidV1(node->open))) return false;
            for (size_t j=0;node->open && j<node->as.object.count;++j) {
                const XL6_RestoreFieldV1* field=&plan->fields[node->as.object.first+j];
                if (!xl6RestoreOpenClaimsKeyV1(node->open,field->source) ||
                    !xl6RestoreOpenClaimsKeyV1(node->open,field->target) ||
                    keyEqual(field->target,node->open->storage)) return false;
            }
        } else return false;
        if (node->kind!=XL6_RESTORE_OBJECT_V1 && node->open) return false;
    }
    for (size_t i=0;i<count;++i) {
        bool used=false;
        for (size_t j=0;j<plan->nodeCount;++j) if (plan->nodes[j].kind==XL6_RESTORE_SEQUENCE_V1 &&
            plan->nodes[j].as.sequence.seed==i) used=true;
        if (!used) return false;
    }
    return true;
}
static bool objectMatches(const xvalue* value,const XL6_RestoreNodeV1* node)
{
    return node->as.object.provider->Admits(value,node->identity,node->as.object.runtimeTag);
}
static xvalue* restoreNode(Import*,const XL6_RestorePlanV1*,size_t,size_t,const xvalue*,xvalue*);
static xvalueobjectfieldsv1* stageFields(Import* ctx,xvalue* value)
{
    for (size_t i=0;i<ctx->stagedCount;++i) if (ctx->staged[i].receiver==value) {
        /* Memo sharing must have returned before this point. A second recipe
         * for the same actual receiver is ambiguous, not a second write. */
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    if (ctx->stagedCount==ctx->stagedCapacity) {
        if (ctx->stagedCapacity>SIZE_MAX/2/sizeof(*ctx->staged)) { xrtSetErrorKind(XERR_RANGE); return NULL; }
        size_t capacity=ctx->stagedCapacity ? 2*ctx->stagedCapacity : 8;
        StagedFields* grown=xrtRealloc(ctx->staged,capacity*sizeof(*grown));
        if (!grown) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
        ctx->staged=grown; ctx->stagedCapacity=capacity;
    }
    xvalueobjectfieldsv1* fields=xrtValueObjectFieldsPrepareV1(value);
    if (fields) ctx->staged[ctx->stagedCount++]=(StagedFields){value,fields};
    return fields;
}
static void closeTemplates(Import* ctx)
{
    for (size_t i=ctx->count;ctx->templates && i>0;--i) {
        Template* schema=&ctx->templates[i-1];
        if (schema->provider && schema->context) {
            ptr context=schema->context; schema->context=NULL; schema->provider->End(context);
        }
        if (schema->owner) {
            XL6_Sequence* owner=schema->owner; schema->owner=NULL; xl6SequenceEndV1(owner);
        }
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(ctx->operation);
    }
}
typedef struct RestoreAdmission {
    const XL6_RestorePlanV1* plan;
    const XL6_OwnershipReadScopeV1* read;
} RestoreAdmission;
static bool restoreOwnershipAdmits(xrtownershipref ref,ptr opaque)
{
    const RestoreAdmission* admission=opaque;
    const XL6_RestorePlanV1* plan=admission->plan;
    for (size_t i=0;i<plan->nodeCount;++i) if (plan->nodes[i].kind==XL6_RESTORE_OBJECT_V1 &&
        plan->nodes[i].as.object.provider->OwnershipAdmits(ref)) return true;
    for (size_t i=0;i<plan->nodeCount;++i) if (plan->nodes[i].kind==XL6_RESTORE_SEQUENCE_V1) {
        const XL6_CollectionImportProviderV1* provider=plan->nodes[i].as.sequence.provider;
        if (provider && provider->OwnershipAdapter && provider->OwnershipAdapter(ref)) return true;
    }
    return admission->read && plan->readBoundary->Admits(admission->read,ref);
}
static xvalueobjectfieldsresultv1 commitFields(Import* ctx,const XL6_RestorePlanV1* plan,
    xvalue* result,const xvalue* observed,size_t* shared)
{
    if (!ctx->stagedCount) return XVALUE_OBJECT_FIELDS_COMMITTED_V1;
    if (ctx->used>(SIZE_MAX-2-ctx->count)/4 ||
        ctx->stagedCount>SIZE_MAX/sizeof(xvalueobjectfieldsv1*)) {
        xrtSetErrorKind(XERR_RANGE); return XVALUE_OBJECT_FIELDS_ERROR_V1;
    }
    size_t count=ctx->count+4*ctx->used+2;
    if (ctx->wireUsed>(SIZE_MAX-count)/2 || count+2*ctx->wireUsed>SIZE_MAX/sizeof(xrtownershipref)) {
        xrtSetErrorKind(XERR_RANGE); return XVALUE_OBJECT_FIELDS_ERROR_V1;
    }
    count+=2*ctx->wireUsed;
    xrtownershipref* slots=xrtMalloc(count*sizeof(*slots));
    xvalueobjectfieldsv1** fields=xrtMalloc(ctx->stagedCount*sizeof(*fields));
    if (!slots || !fields) {
        xrtFree(slots); xrtFree(fields); if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        return XVALUE_OBJECT_FIELDS_ERROR_V1;
    }
    size_t used=0;
    /* Each entry is an ACTUAL owned reference, even if pointers coincide.
     * Record receivers/observations/stores are already traced by their unique
     * records. Borrowed anchors/provider tables are never fabricated slots. */
    slots[used++]=xrtValueOwnership(result); slots[used++]=xrtValueOwnership(observed);
    for (size_t i=0;i<ctx->count;++i) slots[used++]=xrtValueOwnership(ctx->seeds[i]);
    for (size_t i=0;i<ctx->capacity;++i) if (ctx->memo[i].source) {
        slots[used++]=xrtValueOwnership(ctx->memo[i].source);
        slots[used++]=xrtValueOwnership(ctx->memo[i].result);
        if (ctx->memo[i].instance) slots[used++]=xrtValueOwnership(ctx->memo[i].instance);
        if (ctx->memo[i].base) slots[used++]=xrtValueOwnership(ctx->memo[i].base);
    }
    for (size_t i=0;i<ctx->wireCapacity;++i) if (ctx->wire[i].source) {
        slots[used++]=xrtValueOwnership(ctx->wire[i].source);
        slots[used++]=xrtValueOwnership(ctx->wire[i].snapshot);
    }
    for (size_t i=0;i<ctx->stagedCount;++i) fields[i]=ctx->staged[i].fields;
    XL6_OwnershipReadScopeV1 read={0};
    RestoreAdmission admission={plan,NULL};
    xvalueobjectfieldsresultv1 outcome=XVALUE_OBJECT_FIELDS_ERROR_V1;
    if (plan->readBoundary) {
        int status=plan->readBoundary->Begin(&read);
        if (status!=XL6_OWNERSHIP_READ_READY_V1) {
            if (status==XL6_OWNERSHIP_READ_BUSY_V1) outcome=XVALUE_OBJECT_FIELDS_BUSY_V1;
            else if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
            goto done;
        }
        admission.read=&read;
    }
    outcome=xrtValueObjectFieldsCommitV2(fields,ctx->stagedCount,
        slots,used,restoreOwnershipAdmits,&admission,shared);
    if (admission.read) plan->readBoundary->End(&read);
done:
    xrtFree(fields); xrtFree(slots);
    return outcome;
}
static void retireFields(xvalueobjectfieldsv1* fields)
{
    /* As with owned Value retirement, preserve explicit boolean failures in
     * addition to diagnostic objects; no cleanup runs under the commit fence. */
    xerror* prior=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xrtErrorFree(prior); xrtValueObjectFieldsFreeV1(fields);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    } else {
        if (!prior) prior=xrtTakeError();
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError(); xrtValueObjectFieldsFreeV1(fields); xrtClearError(); xrtSetErrorTake(prior);
    }
}
static void retireStaged(Import* ctx)
{
    size_t count=ctx->stagedCount; ctx->stagedCount=0;
    for (size_t i=count;i>0;--i) retireFields(ctx->staged[i-1].fields);
}
static const xvalue* wireSnapshot(Import* ctx,const xvalue* source)
{
    if (ctx->wireCapacity) {
        size_t at=hash(source,0,NULL)&(ctx->wireCapacity-1);
        while (ctx->wire[at].source) {
            if (ctx->wire[at].source==source) return ctx->wire[at].snapshot;
            at=(at+1)&(ctx->wireCapacity-1);
        }
    }
    if (ctx->wireUsed>=ctx->wireCapacity/2) {
        if (ctx->wireCapacity>SIZE_MAX/2/sizeof(*ctx->wire)) { xrtSetErrorKind(XERR_RANGE); return NULL; }
        size_t capacity=ctx->wireCapacity ? ctx->wireCapacity*2 : 16;
        WireMemo* grown=xrtMalloc(capacity*sizeof(*grown));
        if (!grown) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
        memset(grown,0,capacity*sizeof(*grown));
        for (size_t i=0;i<ctx->wireCapacity;++i) if (ctx->wire[i].source) {
            size_t at=hash(ctx->wire[i].source,0,NULL)&(capacity-1);
            while (grown[at].source) at=(at+1)&(capacity-1);
            grown[at]=ctx->wire[i];
        }
        xrtFree(ctx->wire); ctx->wire=grown; ctx->wireCapacity=capacity;
    }
    xvalue* input=xrtValueRetain(source),*snapshot=input ? xrtValueClone(source) : NULL;
    if (!snapshot || !restoring(ctx)) {
        releasePreservingError(snapshot); releasePreservingError(input); return NULL;
    }
    size_t at=hash(source,0,NULL)&(ctx->wireCapacity-1);
    while (ctx->wire[at].source) at=(at+1)&(ctx->wireCapacity-1);
    ctx->wire[at]=(WireMemo){input,snapshot}; ++ctx->wireUsed; return snapshot;
}
static bool restoreObject(Import* ctx,const XL6_RestorePlanV1* plan,const XL6_RestoreNodeV1* node,
    size_t depth,xvalue* value,const xvalue* snapshot)
{
    /* The first observed wire store survives semantic clone calls and every
     * restaging pass. Neither a borrowed field nor a new observation of a
     * callback-modified wire object replaces that restoration domain. */
    xvalueobjectfieldsv1* fields=stageFields(ctx,value);
    if (!fields || !restoring(ctx)) return false;
    bool ok=true;
    for (size_t i=0;ok && i<node->as.object.count;++i) {
        const XL6_RestoreFieldV1* field=&plan->fields[node->as.object.first+i];
        const xvalue* borrowed=xrtValueObjectGet(snapshot,field->source);
        if (!borrowed) {
            if (field->flags&XL6_RESTORE_REQUIRED_V1) rejectWire(ctx,XERR_TYPE);
            ok=restoring(ctx); continue;
        }
        xvalue* input=xrtValueRetain(borrowed),*prepared=NULL;
        if (field->flags&XL6_RESTORE_IN_PLACE_V1) {
            const xvalue* child=xrtValueObjectGet(value,field->target);
            prepared=child ? xrtValueRetain(child) : NULL;
            if (!prepared && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
        }
        xvalue* item=NULL;
        if (input && restoring(ctx) && (!(field->flags&XL6_RESTORE_IN_PLACE_V1) || prepared)) {
            item=restoreNode(ctx,plan,field->node,depth+1,input,prepared); prepared=NULL;
        }
        releasePreservingError(prepared);
        ok=item && restoring(ctx);
        if (ok) ok=xrtValueObjectFieldsSetTakeV1(fields,field->target,&item) && restoring(ctx);
        releasePreservingError(item); releasePreservingError(input);
    }
    if (ok && restoring(ctx) && node->open) {
        xvalue* storage=NULL,*handle=NULL;
        ok=node->as.object.provider->PrepareOpen(node->open,value,snapshot,&storage,&handle) && restoring(ctx);
        if (ok && ((storage==NULL)!=(handle==NULL))) { xrtSetErrorKind(XERR_STATE); ok=false; }
        if (ok && storage) {
            xvalueobjectfieldsv1* replacement=stageFields(ctx,storage);
            ok=replacement && restoring(ctx) &&
                xrtValueObjectFieldsSetTakeV1(replacement,node->open->handle,&handle) && restoring(ctx) &&
                xrtValueObjectFieldsSetTakeV1(fields,node->open->storage,&storage) && restoring(ctx);
        }
        if (!ok && !ctx->wireRejected) xl6ErrorScopeCaptureFailure(ctx->operation);
        releasePreservingError(handle); releasePreservingError(storage);
    }
    if (!ok && restoring(ctx)) xrtSetErrorKind(XERR_TYPE);
    return ok && restoring(ctx);
}
static xvalue* restoreNode(Import* ctx,const XL6_RestorePlanV1* plan,size_t domain,size_t depth,
    const xvalue* source,xvalue* prepared)
{
    const XL6_RestoreNodeV1* node=&plan->nodes[domain]; xvalue* value=NULL;
    if (!restoring(ctx)) goto rejected;
    if (!source || privateSeed(ctx,source)) { xrtSetErrorKind(XERR_ARGUMENT); goto rejected; }
    const xvalue* instance=NULL;
    if (node->flags&XL6_RESTORE_FIELD_INSTANCE_V1) {
        /* The recipe is not the instance. Authenticate every supplied child
         * before looking up a prior conversion or accepting nullable bad wire. */
        if (!prepared || privateSeed(ctx,prepared) || prepared==source) {
            xrtSetErrorKind(XERR_ARGUMENT); goto rejected;
        }
        if (!objectMatches(prepared,node) || !successful(ctx)) {
            if (successful(ctx)) xrtSetErrorKind(XERR_TYPE);
            goto rejected;
        }
        instance=prepared;
    }
    uint64_t identity=xrtValueTypeId(source);
    if (identity && identity!=node->identity && !(node->kind==XL6_RESTORE_SCALAR_V1 &&
        node->as.scalar==XL6_VALUE_ADMIT_ANY)) { rejectWire(ctx,XERR_TYPE); goto rejected; }
    if (xrtValueType(source)==XVALUE_NULL) {
        if ((node->flags&XL6_RESTORE_NULLABLE_V1) || (node->kind==XL6_RESTORE_SCALAR_V1 &&
            node->as.scalar==XL6_VALUE_ADMIT_ANY)) value=xrtValueRetain(source);
        else rejectWire(ctx,XERR_TYPE);
        goto rejected;
    }
    if (node->kind==XL6_RESTORE_SCALAR_V1) {
        XL6_CollectionImportPlanV1 scalar={.flags=XL6_COLLECTION_IMPORT_RESTORE_V1|
            (plan->json ? XL6_COLLECTION_IMPORT_JSON_V1 : 0),.leafRule=node->as.scalar,.leafIdentity=node->identity};
        bool mismatch=false;
        value=restoreLeaf(&scalar,source,ctx->nullableRead ? &mismatch : NULL);
        if (mismatch) rejectWire(ctx,XERR_TYPE);
        goto rejected;
    }
    if (depth>=MAX_CHAIN) { xrtSetErrorKind(XERR_RANGE); goto rejected; }
    for (size_t i=0;i<depth;++i) if (ctx->active[i]==source) { rejectWire(ctx,XERR_VALUE); goto rejected; }
    Memo* prior=lookup(ctx,source,domain,instance);
    if (prior && (!prior->base || prior->pass==ctx->pass)) {
        value=xrtValueRetain(prior->result); goto rejected;
    }
    ctx->active[depth]=source;
    if (node->kind==XL6_RESTORE_SEQUENCE_V1) {
        const Template* schema=&ctx->templates[node->as.sequence.seed]; XL6_ArrayInputV1 input={0};
        XL6_CollectionImportInputV1 family={0}; bool text=textKeys(schema);
        if (xrtValueType(source)!=(text ? XVALUE_OBJECT : XVALUE_ARRAY)) { rejectWire(ctx,XERR_TYPE); goto done; }
        XL6_Sequence* owner=NULL;
        const xvalue* snapshot=wireSnapshot(ctx,source);
        if (!snapshot || !(text ? schema->provider->InputBegin(snapshot,&family) :
            xl6ArrayInputAcquireV1(snapshot,&input)) || !restoring(ctx)) goto sequenceDone;
        if (text && !familyInputValid(schema,&family)) goto sequenceDone;
        value=childSeed(schema);
        if (!value || !restoring(ctx)) goto sequenceDone;
        if (!schema->provider) owner=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,schema->layout.identity,
            schema->layout.elementIdentity,schema->layout.transfer,schema->layout.size,schema->layout.type);
        bool ok=restoring(ctx) && (schema->provider || owner);
        xtypedarray* storage=owner ? xl6SequenceStorageV1(owner) : NULL;
        size_t count=text ? family.count : input.count;
        if (ok && storage) ok=xrtTypedArrayReserve(storage,count);
        for (size_t i=0;ok && i<count;++i) {
            xvalue* item=restoreNode(ctx,plan,node->as.sequence.element,depth+1,
                text ? family.items[i] : input.items[i],NULL);
            ok=item && restoring(ctx);
            if (ok) ok=text ? schema->provider->AppendText(value,family.textKeys[i],item) :
                schema->provider ? schema->provider->Append(value,(int64_t)i,item) :
                    xl6SequenceAdmitValueV1(owner,item) && xrtTypedArrayPushValue(storage,item,schema->converter);
            if (!ok && !ctx->wireRejected) xl6ErrorScopeCaptureFailure(ctx->operation);
            releasePreservingError(item); ok=ok && restoring(ctx);
        }
        if (!ok && !ctx->wireRejected) xl6ErrorScopeCaptureFailure(ctx->operation);
sequenceDone:
        if ((!value || !restoring(ctx)) && !ctx->wireRejected) xl6ErrorScopeCaptureFailure(ctx->operation);
        if (owner) xl6SequenceEndV1(owner);
        if (text) schema->provider->InputEnd(&family);
        xl6ArrayInputReleaseV1(&input);
    } else {
        if (xrtValueType(source)!=XVALUE_OBJECT || node->as.object.provider->IsNative(source)) {
            rejectWire(ctx,XERR_TYPE); goto done;
        }
        if (!successful(ctx)) goto done;
        const xvalue* snapshot=wireSnapshot(ctx,source);
        if (!snapshot || !restoring(ctx)) goto done;
        bool cached=prior!=NULL;
        if (cached) value=xrtValueRetain(prior->result);
        else { value=prepared; prepared=NULL;
            if (!value && node->as.object.factory) value=node->as.object.factory(); }
        if (!value || !successful(ctx)) goto done;
        if (value==source || privateSeed(ctx,value)) {
            xrtSetErrorKind(XERR_TYPE); goto done;
        }
        bool accepted=(!cached && instance) || objectMatches(value,node);
        if (!accepted || !successful(ctx)) {
            if (!xrtGetError() && !ctx->operation->failed) xrtSetErrorKind(XERR_TYPE);
            goto done;
        }
        for (size_t i=0;i<depth;++i) if (ctx->active[i]==value) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        if (!cached) {
            for (size_t i=0;i<ctx->capacity;++i) if (ctx->memo[i].source &&
                (ctx->memo[i].source==value || ctx->memo[i].result==value)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
            if (!remember(ctx,source,domain,value,instance)) goto done;
            Memo* memo=lookup(ctx,source,domain,instance);
            memo->base=xrtValueRetain(value); if (!memo->base) goto done;
        }
        if (!restoreObject(ctx,plan,node,depth,value,snapshot)) goto done;
        /* Recursion can rehash the memo table; never keep its old slot address. */
        lookup(ctx,source,domain,instance)->pass=ctx->pass;
    }
    if (value && restoring(ctx)) {
        if (node->kind==XL6_RESTORE_SEQUENCE_V1 && !remember(ctx,source,domain,value,instance))
            xl6ErrorScopeCaptureFailure(ctx->operation);
    } else if (!ctx->wireRejected && !ctx->operation->failed) xl6ErrorScopeCaptureFailure(ctx->operation);
done:
    ctx->active[depth]=NULL;
rejected:
    releasePreservingError(prepared);
    if (!value || !restoring(ctx)) { releasePreservingError(value); return NULL; }
    return value;
}
static bool openTemplates(Import* ctx,const XL6_RestorePlanV1* plan,const xvalue* source,const xvalue* target)
{
    if (ctx->count) memset(ctx->templates,0,ctx->count*sizeof(*ctx->templates));
    for (size_t i=0;i<plan->nodeCount;++i) if (plan->nodes[i].kind==XL6_RESTORE_SEQUENCE_V1) {
        const XL6_RestoreNodeV1* node=&plan->nodes[i]; size_t at=node->as.sequence.seed;
        Template* schema=&ctx->templates[at]; schema->provider=node->as.sequence.provider;
        if (ctx->seeds[at]==source || ctx->seeds[at]==target || xrtValueTypeId(ctx->seeds[at])!=node->identity) {
            xrtSetErrorKind(XERR_ARGUMENT); return false;
        }
        for (size_t j=0;j<at;++j) if (ctx->seeds[j]==ctx->seeds[at]) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
        if (schema->provider) {
            if (!schema->provider->Begin(ctx->seeds[at],&schema->context) || !schema->context || !successful(ctx)) return false;
        } else {
            int status=xl6SequenceBeginErasedArrayV1(ctx->seeds[at],&schema->owner,&schema->layout,&schema->converter);
            if (status<=0) { if (!status) xrtSetErrorKind(XERR_TYPE); return false; }
            XL6_SequenceAdmissionV3 admission={0};
            if (!xl6SequenceAdmissionCopyV3(schema->owner,&admission)) return false;
            bool admitted=admission.ops!=NULL; xl6SequenceAdmissionDropV3(&admission);
            if (!admitted || xrtTypedArrayCount(xl6SequenceStorageV1(schema->owner))) { xrtSetErrorKind(XERR_STATE); return false; }
        }
        if (!successful(ctx)) return false;
    }
    for (size_t i=0;i<plan->nodeCount;++i) if (plan->nodes[i].kind==XL6_RESTORE_SEQUENCE_V1) {
        const XL6_RestoreNodeV1* node=&plan->nodes[i],*child=&plan->nodes[node->as.sequence.element];
        if (child->kind==XL6_RESTORE_SEQUENCE_V1 &&
            !admits(&ctx->templates[node->as.sequence.seed],ctx->seeds[child->as.sequence.seed])) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            return false;
        }
        if (!successful(ctx)) return false;
    }
    return true;
}
static bool discardSequences(Import* ctx)
{
    Memo* kept=ctx->capacity ? xrtMalloc(ctx->capacity*sizeof(*kept)) : NULL;
    if (ctx->capacity && !kept) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    if (ctx->capacity) memset(kept,0,ctx->capacity*sizeof(*kept));
    size_t count=0;
    for (size_t i=0;i<ctx->capacity;++i) if (ctx->memo[i].source && ctx->memo[i].base) {
        size_t at=hash(ctx->memo[i].source,ctx->memo[i].depth,ctx->memo[i].instance)&(ctx->capacity-1);
        while (kept[at].source) at=(at+1)&(ctx->capacity-1);
        kept[at]=ctx->memo[i]; ++count;
    }
    Memo* old=ctx->memo; ctx->memo=kept; ctx->used=count;
    for (size_t i=0;i<ctx->capacity;++i) if (old[i].source && !old[i].base) {
        releasePreservingError(old[i].result); releasePreservingError((xvalue*)old[i].source);
        releasePreservingError((xvalue*)old[i].instance);
    }
    xrtFree(old); return restoring(ctx);
}
static bool isolateShared(Import* ctx,const XL6_RestorePlanV1* plan,const xvalue* receiver)
{
    Memo* memo=NULL;
    for (size_t i=0;i<ctx->capacity;++i) if (ctx->memo[i].base && ctx->memo[i].result==receiver) {
        if (memo) { xrtSetErrorKind(XERR_STATE); return false; }
        memo=&ctx->memo[i];
    }
    if (!memo || memo->isolated || !plan->nodes[memo->depth].clone) { xrtSetErrorKind(XERR_STATE); return false; }
    const XL6_RestoreNodeV1* node=&plan->nodes[memo->depth];
    /* No field record/COW observation, template BUSY scope or freeze remains
     * across arbitrary declared language code. The original default has its
     * independent memo base pin; the callback is not a structural clone. */
    xvalue* result=node->clone(receiver);
    bool valid=result && restoring(ctx) && result!=receiver && result!=memo->base &&
        !privateSeed(ctx,result) && result!=memo->source;
    if (valid) valid=objectMatches(result,node) && restoring(ctx);
    if (!valid) {
        if (restoring(ctx)) xrtSetErrorKind(XERR_STATE);
        releasePreservingError(result); return false;
    }
    xvalue* old=memo->result; memo->result=result; memo->isolated=true;
    releasePreservingError(old); return restoring(ctx);
}
static xvalue* restorePlan(xvalue** seeds,size_t count,xvalue* target,const xvalue* source,
    const XL6_RestorePlanV1* plan,bool nullableRead,xvalue** output)
{
    if ((count && !seeds) || count>SIZE_MAX/sizeof(Template)) {
        xrtSetErrorKind(XERR_ARGUMENT); releasePreservingError(target); return NULL;
    }
    bool upstream=!source;
    for (size_t i=0;i<count;++i) upstream=upstream || !seeds[i];
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope operation={0};
    if (!(upstream ? xl6ErrorScopeBeginCleanup(&operation,true) : xl6ErrorScopeBegin(&operation))) {
        xerror* failure=upstream ? incoming : xrtErrorRef(xrtGetError());
        if (!upstream) xrtErrorFree(incoming);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,failure});
        for (size_t i=0;i<count;++i) { xvalue* owned=seeds[i]; seeds[i]=NULL; releasePreservingError(owned); }
        releasePreservingError(target); return NULL;
    }
    xrtErrorFree(incoming);
    Import ctx={.seeds=seeds,.count=count,.operation=&operation,.nullableRead=nullableRead,.pass=1};
    xvalue* result=NULL,*observed=NULL;
    if (operation.failed) goto finish;
    if (nullableRead) {
        if (!output || *output) { xrtSetErrorKind(XERR_ARGUMENT); goto finish; }
        for (size_t i=0;i<count;++i) if (output==&seeds[i]) { xrtSetErrorKind(XERR_ARGUMENT); goto finish; }
    }
    if (!graphPlanValid(plan,count) || (target && (plan->nodes[plan->root].kind!=XL6_RESTORE_OBJECT_V1 || target==source))) {
        xrtSetErrorKind(XERR_ARGUMENT); goto finish;
    }
    for (size_t i=0;plan->json && i<plan->nodeCount;++i) if (plan->nodes[i].kind==XL6_RESTORE_SCALAR_V1 &&
        (plan->nodes[i].as.scalar==XL6_VALUE_ADMIT_BYTES || plan->nodes[i].as.scalar==XL6_VALUE_ADMIT_TIME)) {
        xrtSetErrorKind(XERR_TYPE); goto finish;
    }
    /* A supplied construction capability is operational metadata. Validate
     * it even when the wire shape will be rejected, so a malformed native
     * target cannot be mistaken for benign nullable input. */
    if (target && (!objectMatches(target,&plan->nodes[plan->root]) || !successful(&ctx))) {
        if (successful(&ctx)) xrtSetErrorKind(XERR_TYPE);
        goto finish;
    }
    observed=xrtValueRetain(source); if (!observed || !successful(&ctx)) goto finish;
    ctx.templates=count ? xrtMalloc(count*sizeof(*ctx.templates)) : NULL;
    if (count && !ctx.templates) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto finish; }
    /* Zero even before an early refusal, so common cleanup sees only real pins. */
    if (count) memset(ctx.templates,0,count*sizeof(*ctx.templates));
    while (restoring(&ctx)) {
        if (!openTemplates(&ctx,plan,source,target)) break;
        result=restoreNode(&ctx,plan,plan->root,0,observed,target); target=NULL;
        closeTemplates(&ctx);
        if (!result || !restoring(&ctx)) break;
        size_t shared=SIZE_MAX;
        xvalueobjectfieldsresultv1 outcome=commitFields(&ctx,plan,result,observed,&shared);
        if (outcome==XVALUE_OBJECT_FIELDS_COMMITTED_V1) break;
        if (outcome!=XVALUE_OBJECT_FIELDS_SHARED_V1 || shared>=ctx.stagedCount || !restoring(&ctx)) {
            if (restoring(&ctx)) xrtSetErrorKind(XERR_STATE);
            break;
        }
        const xvalue* receiver=ctx.staged[shared].receiver;
        retireStaged(&ctx); releasePreservingError(result); result=NULL;
        if (!restoring(&ctx) || !discardSequences(&ctx) || !isolateShared(&ctx,plan,receiver)) break;
        if (ctx.pass==SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); break; }
        ++ctx.pass;
    }
finish:
    if ((!result && !ctx.wireRejected) || !successful(&ctx)) xl6ErrorScopeCaptureFailure(&operation);
    closeTemplates(&ctx);
    retireStaged(&ctx);
    for (size_t i=0;i<count;++i) { xvalue* owned=seeds[i]; seeds[i]=NULL; releasePreservingError(owned); }
    for (size_t i=0;i<ctx.capacity;++i) if (ctx.memo[i].source) {
        releasePreservingError(ctx.memo[i].result); releasePreservingError((xvalue*)ctx.memo[i].source);
        releasePreservingError((xvalue*)ctx.memo[i].instance);
        releasePreservingError(ctx.memo[i].base);
    }
    for (size_t i=0;i<ctx.wireCapacity;++i) if (ctx.wire[i].source) {
        releasePreservingError(ctx.wire[i].snapshot); releasePreservingError(ctx.wire[i].source);
    }
    xrtFree(ctx.wire); xrtFree(ctx.staged); xrtFree(ctx.templates); xrtFree(ctx.memo);
    releasePreservingError(target); releasePreservingError(observed);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { releasePreservingError(result); result=NULL; }
    else if (nullableRead && ctx.wireRejected) {
        result=xrtValueRetain(xrtValueNull());
        if (!result || !successful(&ctx)) {
            xl6ErrorScopeCaptureFailure(&operation); releasePreservingError(result); result=NULL;
        }
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
xvalue* xl6PersistenceRestorePlanTakeV1(xvalue** seeds,size_t count,xvalue* target,const xvalue* source,
    const XL6_RestorePlanV1* plan)
{ return restorePlan(seeds,count,target,source,plan,false,NULL); }
bool xl6PersistenceRestorePlanReadTakeV1(xvalue** seeds,size_t count,xvalue* target,const xvalue* source,
    const XL6_RestorePlanV1* plan,xvalue** output)
{
    xvalue* value=restorePlan(seeds,count,target,source,plan,true,output);
    if (!value) return false;
    *output=value; return true;
}

static bool parserWireRejected(const xerror* error,bool file,bool json)
{
    if (!error) return false;
    const char* domain=xrtErrorDomain(error);
    int code=xrtErrorCode(error); xerrkind kind=xrtErrorKind(error);
    if (!domain) return false;
    bool codec=!strcmp(domain,json ? "xrt.json" : "xrt.xlon");
    if (codec && (kind==XERR_PROTOCOL || kind==XERR_VALUE || kind==XERR_RANGE))
        return json ? code==XJSON_ERROR_SYNTAX || code==XJSON_ERROR_NUMBER :
            code==XXLON_ERROR_SYNTAX || code==XXLON_ERROR_NUMBER || code==XXLON_ERROR_TAG;
    if (!file || (kind!=XERR_IO && kind!=XERR_NOT_FOUND && kind!=XERR_PERMISSION)) return false;
    return !strcmp(domain,"xrt.file") || !strcmp(domain,"xrt.path") || !strcmp(domain,"xrt.io") ||
        (codec && code==(json ? XJSON_ERROR_IO : XXLON_ERROR_IO));
}

bool xl6SerializeRestoreReadFinishTakeV1(xvalue* source,xvalue* target,
    bool (*assign)(xvalue*,const xvalue*),xerror* prior,xvalue** output)
{
    bool ok=xl6SerializeRestoreReadFinishV1(source,target,assign,prior,output);
    xl6_error_scope cleanup={0};
    if (!(ok ? xl6ErrorScopeBegin(&cleanup) : xl6ErrorScopeBeginCleanup(&cleanup,true))) abort();
    releasePreservingError(target);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
    if (ok && cleanup.failed) { releasePreservingError(*output); *output=NULL; ok=false; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return ok && outcome.ok;
}

bool xl6PersistenceRestoreParsedTakeV1(xvalue** seeds,size_t count,xvalue* target,
    xvalue* source,bool parserOk,xerror* parserError,bool preparationOk,xerror* preparationError,
    bool file,const XL6_RestorePlanV1* plan,xvalue** output)
{
    xl6_error_scope operation={0};
    bool rejected=!parserOk && !source && plan && parserWireRejected(parserError,file,plan->json);
    if (!xl6ErrorScopeBegin(&operation)) {
        xerror* opening=xrtTakeError(); xerror* first;
        if ((!parserOk || parserError) && !rejected) { first=parserError; parserError=NULL; }
        else if (!preparationOk || preparationError) { first=preparationError; preparationError=NULL; }
        else { first=opening; opening=NULL; }
        xrtErrorFree(opening); xrtErrorFree(parserError); xrtErrorFree(preparationError);
        /* Latch even a diagnosticless failure BEFORE retirement. The common
         * controller consumes every transferred seed/target; no second memo,
         * alternate traversal, leaked source, or cleanup abort is permitted. */
        xl6ErrorScopePropagateFailure((xl6_error_result){false,first});
        (void)xl6PersistenceRestorePlanReadTakeV1(seeds,count,target,NULL,plan,output);
        releasePreservingError(source); return false;
    }
    if (rejected) { xrtErrorFree(parserError); parserError=NULL; }
    else if (!parserOk || parserError) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,parserError}); parserError=NULL;
    }
    if (!preparationOk || preparationError)
        xl6ErrorScopePropagateFailure((xl6_error_result){false,preparationError});
    /* Canonical null is a proven wire rejection, not a fabricated upstream
     * success. The SAME controller still validates the plan/target/seeds and
     * performs all checked retirement, even for rejected parser input. */
    bool ok=xl6PersistenceRestorePlanReadTakeV1(seeds,count,target,
        operation.failed ? NULL : rejected ? xrtValueNull() : source,plan,output);
    if (!ok) xl6ErrorScopeCaptureFailure(&operation);
    releasePreservingError(source);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (ok && operation.failed) { releasePreservingError(*output); *output=NULL; ok=false; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return ok && outcome.ok;
}
