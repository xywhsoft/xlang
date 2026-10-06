#include "xl6_array_value_iterator.h"
#include "xl6_value_iterator.h"
#include "xl6_ownership_scope.h"

struct xlir_xvalue_iterator {
    volatile int32_t refs;
    xvaluecursor* cursor;
    xvaluecursor* retired;
    /* Native snapshot and its current owned projection are distinct physical
     * edges. A View only borrows the latter; never trace it a second time. */
    xvalue* nativeCursor;
    xvalue* nativeItem;
    xvalue* retiredNativeCursor;
    xvalue* retiredNativeItem;
    XL6_SequenceLayoutV1 nativeLayout;
    const xvalueconverter* nativeProjection;
    XL6_ValueIteratorView view;
    size_t nextOrdinal;
    const void* claim;
    bool active, cleared, failed;
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
static XL6_ValueIterator* iterator_create(const xvalue* source)
{
    XL6_ValueIterator* state = xrtMalloc(sizeof(*state));
    if (!state) return NULL;
    *state = (XL6_ValueIterator){0}; state->refs = 1;
    int native=xl6ArrayIteratorCreateErasedV1(source,&state->nativeCursor,
        &state->nativeLayout,&state->nativeProjection);
    if (native) { if (native<0) { xrtFree(state); return NULL; } return state; }
    state->cursor = xrtValueCursorCreate(source);
    if (!state->cursor) { xrtFree(state); return NULL; }
    return state;
}
static bool iterator_advance(XL6_ValueIterator* state, bool* found)
{
    xrtownershipscope mutation = {0}; xvaluekey key = {0}; xvalue* item = NULL; int result;
    if (!found) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *found=false;
    mutation_begin(&mutation);
    if (!state || state->refs <= 0 || state->active || state->cleared || state->failed ||
        (!state->cursor && !state->nativeCursor)) {
        mutation_end(&mutation); xrtSetErrorKind(XERR_STATE); return false;
    }
    state->active = true; state->view=(XL6_ValueIteratorView){0};
    xvalue* prior=state->nativeItem; state->nativeItem=NULL;
    mutation_end(&mutation); release_values(prior,NULL);
    if (state->nativeCursor) {
        const XL6_SequenceLayoutV1* layout=&state->nativeLayout;
        bool hasItem=false; size_t ordinal=0;
        bool ok=xl6ArrayIteratorAdvanceValueV1(state->nativeCursor,layout->identity,
            layout->elementIdentity,layout->transfer,layout->size,layout->type,
            state->nativeProjection,&hasItem,&item,&ordinal);
        result=ok ? (hasItem ? 1 : 0) : -1;
        if (result>0) { key.Type=XVALUE_KEY_INDEX; key.Index=ordinal; }
    } else result = xrtValueCursorAdvance(state->cursor, &key, &item);
    if (result > 0 && (!item || (unsigned int)key.Type>3u)) {
        xrtSetErrorKind(XERR_STATE); result=-1;
    }
    if (result > 0 && state->nextOrdinal>(size_t)INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); result=-1;
    }
    if (result<0 && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    /* Checked native advancement transfers ownership even though the public
     * View is borrowed. Withdraw rejected projections before graph publication. */
    if (state->nativeCursor && result<=0) { release_values(item,NULL); item=NULL; }
    mutation_begin(&mutation);
    state->failed=result<0;
    if (state->nativeCursor) state->nativeItem=item;
    if (result > 0) {
        state->view.item = item; state->view.ordinal = state->nextOrdinal++;
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
    if (state->retired || state->retiredNativeCursor || state->retiredNativeItem) abort();
    cursor = state->cursor; state->cursor = NULL; state->view = (XL6_ValueIteratorView){0};
    xvalue* nativeCursor=state->nativeCursor; state->nativeCursor=NULL;
    xvalue* nativeItem=state->nativeItem; state->nativeItem=NULL;
    mutation_end(&mutation);
    xerror* saved=xrtTakeError();
    xrtValueCursorRelease(cursor); release_values(nativeItem,nativeCursor); xrtFree(state);
    xrtClearError(); xrtSetErrorTake(saved);
}
static bool iterator_count(const void* data, size_t* count)
{
    const XL6_ValueIterator* state = data;
    if (!state || !count || state->refs <= 0 || state->active || state->cleared ||
        (!state->cursor && !state->nativeCursor)) return false;
    *count = (size_t)state->refs; return true;
}
static bool iterator_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_ValueIterator* state = data; size_t count;
    if (!visit || !iterator_count(data,&count)) return false;
    if (state->cursor) return visit(xrtValueCursorOwnership(state->cursor),context);
    return visit(xrtValueOwnership(state->nativeCursor),context) &&
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
    state->retiredNativeCursor=state->nativeCursor; state->nativeCursor=NULL;
    state->retiredNativeItem=state->nativeItem; state->nativeItem=NULL;
    state->view = (XL6_ValueIteratorView){0}; state->cleared = true;
}
static bool iterator_finish(const void* data, const void* token)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data; xvaluecursor* cursor;
    if (state->claim != token || !state->cleared) abort();
    cursor = state->retired; state->retired = NULL;
    xvalue* nativeCursor=state->retiredNativeCursor; state->retiredNativeCursor=NULL;
    xvalue* nativeItem=state->retiredNativeItem; state->retiredNativeItem=NULL;
    xerror* saved=xrtTakeError(); xrtValueCursorRelease(cursor);
    release_values(nativeItem,nativeCursor); xrtClearError(); xrtSetErrorTake(saved); return true;
}
const xrtownershipadapterv1* xl6ValueIteratorOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 api = {sizeof(api), iterator_hold, iterator_drop,
        iterator_claim, iterator_restore, NULL, iterator_clear, iterator_finish};
    size_t count;
    return ref.Ops == &iterator_ops && iterator_count(ref.Data, &count) ? &api : NULL;
}
