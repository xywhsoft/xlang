#include "xl6_array_value_iterator.h"
#include "xl6_value_iterator.h"
#include "xl6_value_iterator_internal.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"

struct xlir_xvalue_iterator {
    volatile int32_t refs;
    xvaluecursor* cursor;
    xvaluecursor* retired;
    /* Native snapshot and its current owned projection are distinct physical
     * edges. A View only borrows the latter; never trace it a second time. */
    XL6_ValueIteratorNativeSnapshotV1 native;
    const XL6_ValueIteratorNativeOpsV1* nativeOps;
    xvalue* nativeItem;
    xvalue* retiredNativeCursor;
    xvalue* retiredNativeItem;
    XL6_ValueIteratorNativePairSnapshotV1 pair;
    const XL6_ValueIteratorNativePairOpsV1* pairOps;
    xvalue* retiredPairKeys;
    xvalue* retiredPairValues;
    XL6_ValueIteratorView view;
    size_t nextOrdinal;
    const void* claim;
    bool active, cleared, failed, currentValid;
};
static void mutation_begin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutation_end(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void release_values(xvalue* item, xvalue* cursor)
{
    /* The wire path owns neither. Do not touch error TLS or enter XRT's
     * ownership mutation scopes for two NULL releases on every hot step. */
    if (!item && !cursor) return;
    xerror* saved=xrtTakeError();
    xrtValueRelease(item); xrtValueRelease(cursor);
    xrtClearError(); xrtSetErrorTake(saved);
}
/* Pair lanes may contain checked finalizers. Continue every release and keep
 * the first physical failure, even if no diagnostic allocation succeeded.
 * Advance cleanup is an isolated operation; final retirement joins the
 * already escaping outcome. Legacy V2 paths retain their original contract. */
static bool release_pair(xvalue* item,xvalue* keys,xvalue* values,bool finalizing)
{
    if (!item && !keys && !values) return true;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope scope={0};
    bool entered=finalizing ? xl6ErrorScopeBeginCleanup(&scope,false) : xl6ErrorScopeBegin(&scope);
    if (!entered) {
        xerror* prior=incoming ? incoming : xrtTakeError(); xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)}); xrtClearError();
        xrtValueRelease(item); xrtClearError(); xrtValueRelease(keys); xrtClearError();
        xrtValueRelease(values); xrtClearError(); xrtSetErrorTake(prior); return false;
    }
    xrtErrorFree(incoming);
    xvalue* owned[3]={item,keys,values};
    for (unsigned i=0;i<3;++i) if (owned[i]) {
        xrtValueRelease(owned[i]);
        if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
XL6_ValueIterator* xl6ValueIteratorCreatePairTakeV1(
    XL6_ValueIteratorNativePairSnapshotV1* input,const XL6_ValueIteratorNativePairOpsV1* ops)
{
    if (!input) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_ValueIteratorNativePairSnapshotV1 snapshot=*input; *input=(XL6_ValueIteratorNativePairSnapshotV1){0};
    if (!ops || !ops->Advance || snapshot.mode>3 || snapshot.count>(size_t)INT64_MAX ||
        !!snapshot.keys!=!!(snapshot.mode&XL6_VALUE_ITERATOR_PAIR_KEY_V1) ||
        !!snapshot.values!=!!(snapshot.mode&XL6_VALUE_ITERATOR_PAIR_ITEM_V1) ||
        !!ops->CopyKey!=!!snapshot.keys || (snapshot.keys && snapshot.keys==snapshot.values)) {
        xrtSetErrorKind(XERR_ARGUMENT); (void)release_pair(NULL,snapshot.keys,snapshot.values,true); return NULL;
    }
    XL6_ValueIterator* state=xrtCalloc(1,sizeof(*state));
    if (!state) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        (void)release_pair(NULL,snapshot.keys,snapshot.values,true); return NULL;
    }
    state->refs=1; state->pair=snapshot; state->pairOps=ops; return state;
}
static int array_create(const xvalue* source,XL6_ValueIteratorNativeSnapshotV1* snapshot)
{
    return xl6ArrayIteratorCreateErasedV1(source,&snapshot->cursor,
        &snapshot->layout,&snapshot->projection);
}
static bool array_advance(const XL6_ValueIteratorNativeSnapshotV1* snapshot,
    bool* found,xvalue** item,xvaluekey* key)
{
    const XL6_SequenceLayoutV1* layout=&snapshot->layout; size_t ordinal=0;
    bool ok=xl6ArrayIteratorAdvanceValueV1(snapshot->cursor,layout->identity,
        layout->elementIdentity,layout->transfer,layout->size,layout->type,
        snapshot->projection,found,item,&ordinal);
    if (ok && *found) { key->Type=XVALUE_KEY_INDEX; key->Index=ordinal; }
    return ok;
}
static const XL6_ValueIteratorNativeOpsV1 array_ops={array_create,array_advance};
static XL6_ValueIterator* create_resident(const xvalue* source,
    const XL6_ValueIteratorNativeOpsV1* extension,xvaluetype expectedWire)
{
    XL6_ValueIterator* state = xrtMalloc(sizeof(*state));
    if (!state) return NULL;
    *state = (XL6_ValueIterator){0}; state->refs = 1;
    int native=extension ? extension->Create(source,&state->native) : 0;
    if (native) state->nativeOps=extension;
    else {
        native=array_ops.Create(source,&state->native);
        if (native) state->nativeOps=&array_ops;
    }
    if (native) { if (native<0) { xrtFree(state); return NULL; } return state; }
    if (expectedWire!=XVALUE_NULL && (!source || xrtValueType(source)!=expectedWire)) {
        xrtFree(state);
        xrtSetErrorInfo(XERR_TYPE,"xlang",0,"for in value must be array, list, set or dict");
        return NULL;
    }
    state->cursor = xrtValueCursorCreate(source);
    if (!state->cursor) { xrtFree(state); return NULL; }
    return state;
}
XL6_ValueIterator* xl6ValueIteratorCreateResidentV1(const xvalue* source,
    const XL6_ValueIteratorNativeOpsV1* extension)
{ return create_resident(source,extension,XVALUE_NULL); }
XL6_ValueIterator* xl6ArrayValueIteratorCreateCheckedV1(const xvalue* source)
{ return create_resident(source,NULL,XVALUE_ARRAY); }
static XL6_ValueIterator* iterator_create(const xvalue* source)
{ return xl6ValueIteratorCreateResidentV1(source,NULL); }
static bool iterator_advance(XL6_ValueIterator* state, bool* found)
{
    xrtownershipscope mutation = {0}; xvaluekey key = {0}; xvalue* item = NULL; int result;
    if (!found) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *found=false;
    mutation_begin(&mutation);
    if (!state || state->refs <= 0 || state->active || state->cleared || state->failed ||
        (!state->cursor && !state->native.cursor && !state->pairOps)) {
        mutation_end(&mutation); xrtSetErrorKind(XERR_STATE); return false;
    }
    state->active = true; state->currentValid=false; state->view=(XL6_ValueIteratorView){0};
    xvalue* prior=state->nativeItem; state->nativeItem=NULL;
    mutation_end(&mutation);
    bool cleaned=true;
    if (state->pairOps) cleaned=release_pair(prior,NULL,NULL,false);
    else release_values(prior,NULL);
    if (!cleaned) result=-1;
    else if (state->pairOps) {
        bool hasItem=false;
        bool ok=state->pairOps->Advance(&state->pair,state->nextOrdinal,&hasItem,&item);
        result=ok ? (hasItem ? 1 : 0) : -1;
    } else if (state->native.cursor) {
        bool hasItem=false;
        bool ok=state->nativeOps->Advance(&state->native,&hasItem,&item,&key);
        result=ok ? (hasItem ? 1 : 0) : -1;
    } else result = xrtValueCursorAdvance(state->cursor, &key, &item);
    if (result > 0 && (state->pairOps
            ? !!item!=!!(state->pair.mode&XL6_VALUE_ITERATOR_PAIR_ITEM_V1)
            : (!item || (unsigned int)key.Type>3u))) {
        xrtSetErrorKind(XERR_STATE); result=-1;
    }
    if (result > 0 && state->nextOrdinal>(size_t)INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); result=-1;
    }
    if (result<0 && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    /* Checked native advancement transfers ownership even though the public
     * View is borrowed. Withdraw rejected projections before graph publication. */
    if ((state->native.cursor || state->pairOps) && result<=0) {
        if (state->pairOps) (void)release_pair(item,NULL,NULL,true);
        else release_values(item,NULL);
        item=NULL;
    }
    mutation_begin(&mutation);
    state->failed=result<0;
    if (state->native.cursor || state->pairOps) state->nativeItem=item;
    if (result > 0) {
        state->view.item = item; state->view.ordinal = state->nextOrdinal++;
        state->currentValid=true;
        state->view.key.Type = key.Type;
        switch (key.Type) {
        case 0: break;
        case 1: state->view.key.Index = key.Index; break;
        case 2: state->view.key.Integer = key.Integer; break;
        case 3: state->view.key.String.Data = key.String.Data; state->view.key.String.Size = key.String.Size; break;
        default: break; /* Validated before publication. */
        }
    }
    state->active = false; mutation_end(&mutation);
    *found=result>0; return result>=0;
}
bool xl6ValueIteratorKeyCopyV3(XL6_ValueIterator* state,XL6_SlotMapKeyV1* output)
{
    if (!output || output->instance || output->slot) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xrtownershipscope mutation={0}; mutation_begin(&mutation);
    if (!state || state->refs<=0 || state->active || state->cleared || state->failed ||
        !state->currentValid || !state->pairOps || !state->pairOps->CopyKey) {
        mutation_end(&mutation); xrtSetErrorKind(XERR_STATE); return false;
    }
    state->active=true; mutation_end(&mutation);
    bool ok=state->pairOps->CopyKey(&state->pair,state->view.ordinal,output);
    mutation_begin(&mutation); state->active=false;
    xvalue* rejected=NULL;
    if (!ok) {
        state->failed=true; state->currentValid=false; state->view=(XL6_ValueIteratorView){0};
        rejected=state->nativeItem; state->nativeItem=NULL;
    }
    mutation_end(&mutation);
    if (!ok) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        (void)release_pair(rejected,NULL,NULL,true);
    }
    return ok;
}
static const XL6_ValueIteratorView* iterator_view(const XL6_ValueIterator* state)
{
    if (!state) return NULL;
    if (state->active || state->cleared || state->failed || state->refs <= 0) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    return &state->view;
}
static void iterator_destroy(XL6_ValueIterator* state)
{
    xrtownershipscope mutation = {0}; xvaluecursor* cursor; int32_t refs;
    if (!state) return;
    mutation_begin(&mutation);
    if (state->active || (refs = xrtRefRelease(&state->refs)) < 0) abort();
    if (refs) { mutation_end(&mutation); return; }
    if (state->retired || state->retiredNativeCursor || state->retiredNativeItem ||
        state->retiredPairKeys || state->retiredPairValues) abort();
    cursor = state->cursor; state->cursor = NULL; state->view = (XL6_ValueIteratorView){0};
    xvalue* nativeCursor=state->native.cursor; state->native.cursor=NULL;
    xvalue* nativeItem=state->nativeItem; state->nativeItem=NULL;
    xvalue* pairKeys=state->pair.keys; state->pair.keys=NULL;
    xvalue* pairValues=state->pair.values; state->pair.values=NULL;
    mutation_end(&mutation);
    xerror* saved=xrtTakeError();
    xrtValueCursorRelease(cursor);
    if (state->pairOps) {
        xrtSetErrorTake(saved); saved=NULL;
        (void)release_pair(nativeItem,pairKeys,pairValues,true); xrtFree(state); return;
    }
    release_values(nativeItem,nativeCursor); xrtFree(state);
    xrtClearError(); xrtSetErrorTake(saved);
}
static bool iterator_count(const void* data, size_t* count)
{
    const XL6_ValueIterator* state = data;
    if (!state || !count || state->refs <= 0 || state->active || state->cleared ||
        (!state->cursor && !state->native.cursor && !state->pairOps)) return false;
    *count = (size_t)state->refs; return true;
}
static bool iterator_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_ValueIterator* state = data; size_t count;
    if (!visit || !iterator_count(data,&count)) return false;
    if (state->cursor) return visit(xrtValueCursorOwnership(state->cursor),context);
    return (!state->native.cursor || visit(xrtValueOwnership(state->native.cursor),context)) &&
        (!state->pair.keys || visit(xrtValueOwnership(state->pair.keys),context)) &&
        (!state->pair.values || visit(xrtValueOwnership(state->pair.values),context)) &&
        (!state->nativeItem || visit(xrtValueOwnership(state->nativeItem),context));
}
static const xrtownershipops iterator_ops = {iterator_count, iterator_trace};
static xrtownershipref iterator_ownership(const XL6_ValueIterator* state)
{ return (xrtownershipref){state, state ? &iterator_ops : NULL}; }
const XL6_ValueIteratorRuntimeV2* xl6ValueIteratorRuntimeV2Get(void)
{
    static const XL6_ValueIteratorRuntimeV2 api = {iterator_create, iterator_advance, iterator_view,
        iterator_destroy, iterator_ownership}; return &api;
}
static bool iterator_hold(const void* data)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data;
    return state->refs > 0 && state->refs < INT32_MAX && xrtRefRetain(&state->refs) > 0;
}
static void iterator_drop(const void* data) { iterator_destroy((XL6_ValueIterator*)data); }
static bool iterator_claim(const void* data, const void* token)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data;
    if (state->active || state->cleared || (state->claim && state->claim != token)) return false;
    state->claim = token; return true;
}
static void iterator_restore(const void* data, const void* token)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data;
    if (state->claim != token || state->cleared) abort();
    state->claim = NULL;
}
static void iterator_clear(const void* data, const void* token)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data;
    if (state->claim != token || state->cleared || state->active) abort();
    state->retired = state->cursor; state->cursor = NULL;
    state->retiredNativeCursor=state->native.cursor; state->native.cursor=NULL;
    state->retiredNativeItem=state->nativeItem; state->nativeItem=NULL;
    state->retiredPairKeys=state->pair.keys; state->pair.keys=NULL;
    state->retiredPairValues=state->pair.values; state->pair.values=NULL;
    state->view = (XL6_ValueIteratorView){0}; state->currentValid=false; state->cleared = true;
}
static bool iterator_finish(const void* data, const void* token)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data; xvaluecursor* cursor;
    if (state->claim != token || !state->cleared) abort();
    cursor = state->retired; state->retired = NULL;
    xvalue* nativeCursor=state->retiredNativeCursor; state->retiredNativeCursor=NULL;
    xvalue* nativeItem=state->retiredNativeItem; state->retiredNativeItem=NULL;
    xvalue* pairKeys=state->retiredPairKeys; state->retiredPairKeys=NULL;
    xvalue* pairValues=state->retiredPairValues; state->retiredPairValues=NULL;
    xerror* saved=xrtTakeError(); xrtValueCursorRelease(cursor);
    if (state->pairOps) {
        xrtSetErrorTake(saved);
        return release_pair(nativeItem,pairKeys,pairValues,true);
    }
    release_values(nativeItem,nativeCursor); xrtClearError(); xrtSetErrorTake(saved); return true;
}
const xrtownershipadapterv1* xl6ValueIteratorOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 api = {sizeof(api), iterator_hold, iterator_drop,
        iterator_claim, iterator_restore, NULL, iterator_clear, iterator_finish};
    size_t count;
    return ref.Ops == &iterator_ops && iterator_count(ref.Data, &count) ? &api : NULL;
}
