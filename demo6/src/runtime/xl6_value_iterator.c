#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "xl6_value_iterator.h"
#include "xl6_ownership_scope.h"

struct xlir_xvalue_iterator {
    volatile int32_t refs;
    xvaluecursor* cursor;
    xvaluecursor* retired;
    XL6_ValueIteratorView view;
    size_t nextOrdinal;
    const void* claim;
    bool active, cleared;
};
static void mutation_begin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutation_end(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static XL6_ValueIterator* iterator_create(const xvalue* source)
{
    XL6_ValueIterator* state = xrtMalloc(sizeof(*state));
    if (!state) return NULL;
    *state = (XL6_ValueIterator){0}; state->refs = 1;
    state->cursor = xrtValueCursorCreate(source);
    if (!state->cursor) { xrtFree(state); return NULL; }
    return state;
}
static bool iterator_advance(XL6_ValueIterator* state)
{
    xrtownershipscope mutation = {0}; xvaluekey key = {0}; xvalue* item = NULL; int result;
    mutation_begin(&mutation);
    if (!state || state->refs <= 0 || state->active || state->cleared || !state->cursor) abort();
    state->active = true; mutation_end(&mutation);
    result = xrtValueCursorAdvance(state->cursor, &key, &item);
    if (result < 0) abort();
    mutation_begin(&mutation);
    state->view = (XL6_ValueIteratorView){0};
    if (result > 0) {
        if (state->nextOrdinal == SIZE_MAX) abort();
        state->view.item = item; state->view.ordinal = state->nextOrdinal++;
        state->view.key.Type = key.Type;
        switch (key.Type) {
        case 0: break;
        case 1: state->view.key.Index = key.Index; break;
        case 2: state->view.key.Integer = key.Integer; break;
        case 3: state->view.key.String.Data = key.String.Data; state->view.key.String.Size = key.String.Size; break;
        default: abort();
        }
    }
    state->active = false; mutation_end(&mutation);
    return result > 0;
}
static const XL6_ValueIteratorView* iterator_view(const XL6_ValueIterator* state)
{
    if (!state) return NULL;
    if (state->active || state->cleared || state->refs <= 0) abort();
    return &state->view;
}
static void iterator_destroy(XL6_ValueIterator* state)
{
    xrtownershipscope mutation = {0}; xvaluecursor* cursor; int32_t refs;
    if (!state) return;
    mutation_begin(&mutation);
    if (state->active || (refs = xrtRefRelease(&state->refs)) < 0) abort();
    if (refs) { mutation_end(&mutation); return; }
    if (state->retired) abort();
    cursor = state->cursor; state->cursor = NULL; state->view = (XL6_ValueIteratorView){0};
    mutation_end(&mutation);
    xrtValueCursorRelease(cursor); xrtFree(state);
}
static bool iterator_count(const void* data, size_t* count)
{
    const XL6_ValueIterator* state = data;
    if (!state || !count || state->refs <= 0 || state->active || state->cleared || !state->cursor) return false;
    *count = (size_t)state->refs; return true;
}
static bool iterator_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_ValueIterator* state = data; size_t count;
    return visit && iterator_count(data, &count) && visit(xrtValueCursorOwnership(state->cursor), context);
}
static const xrtownershipops iterator_ops = {iterator_count, iterator_trace};
static xrtownershipref iterator_ownership(const XL6_ValueIterator* state)
{ return (xrtownershipref){state, state ? &iterator_ops : NULL}; }
const XL6_ValueIteratorRuntimeV1* xl6ValueIteratorRuntimeV1Get(void)
{
    static const XL6_ValueIteratorRuntimeV1 api = {iterator_create, iterator_advance, iterator_view,
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
    state->view = (XL6_ValueIteratorView){0}; state->cleared = true;
}
static bool iterator_finish(const void* data, const void* token)
{
    XL6_ValueIterator* state = (XL6_ValueIterator*)data; xvaluecursor* cursor;
    if (state->claim != token || !state->cleared) abort();
    cursor = state->retired; state->retired = NULL; xrtValueCursorRelease(cursor); return true;
}
const xrtownershipadapterv1* xl6ValueIteratorOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 api = {sizeof(api), iterator_hold, iterator_drop,
        iterator_claim, iterator_restore, NULL, iterator_clear, iterator_finish};
    size_t count;
    return ref.Ops == &iterator_ops && iterator_count(ref.Data, &count) ? &api : NULL;
}
