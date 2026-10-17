#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "xl6_thread_local.h"
#include "xl6_thread_exit.h"
#include "src/api/xl_foreign_error.h"
#include <stdatomic.h>
#include <limits.h>

typedef struct Local Local;
typedef struct Registry Registry;
typedef struct KeyOwner { size_t refs; xthreadkey* key; } KeyOwner;
/* State owns the value slot. Registry owns an INDEX/storage reference only,
 * never the managed value or a Local reference. State/registry links and both
 * metadata references are changed under the gate and world mutation. Closing
 * State removes registry indices before releasing any managed child. */
typedef struct Entry {
    unsigned refs;
    Local* owner; Registry* registry;
    int64_t id;
    xvalue* value;
    struct Entry *previous, *next, *retiredNext;
} Entry;
struct Registry { KeyOwner* key; xintmap index; };
struct Local {
    volatile int32_t refs;
    size_t active;
    int64_t id;
    KeyOwner* key;
    const void* claim;
    bool closed, cleared;
    Entry *entries, *retired;
};
static _Atomic(uint32_t) gate;
static KeyOwner* sharedKey;
static int64_t nextId;
static void registryDrop(void*);
static bool clearCurrent(void);
static void end(Local*);

static void enter(xrtownershipscope* mutation)
{
    if (!xrtOwnershipMutationBegin(mutation)) abort();
    uint32_t expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&gate, &expected, 1,
        memory_order_acq_rel, memory_order_acquire)) { expected = 0; xrtThreadYield(); }
}
static void leave(xrtownershipscope* mutation)
{
    atomic_store_explicit(&gate, 0, memory_order_release);
    if (!xrtOwnershipScopeEnd(mutation)) abort();
}
static void entryDropLocked(Entry* entry)
{
    if (!entry->refs) abort();
    if (!--entry->refs) { if (entry->owner || entry->registry || entry->value) abort(); xrtFree(entry); }
}
static void keyDrop(KeyOwner* key)
{
    xrtownershipscope mutation = {0}; enter(&mutation);
    if (!key || !key->refs) abort();
    bool last = !--key->refs;
    if (last) { if (sharedKey != key) abort(); sharedKey = NULL; }
    leave(&mutation);
    if (!last) return;
    /* All State and Registry owners are gone. No live TLS slot can make the
     * native Destroy fail before consuming its creator reference. A current
     * native exit callback may still own the native key's separate slot pin. */
    if (!xrtThreadKeyDestroy(key->key)) {
        xl6_error_result failure = {false, xrtTakeError()};
        xl6ErrorScopePropagateFailure(failure);
    }
    xrtFree(key);
}
static void unlinkStateLocked(Entry* entry)
{
    Local* state = entry->owner;
    if (!state) abort();
    if (entry->previous) entry->previous->next = entry->next;
    else state->entries = entry->next;
    if (entry->next) entry->next->previous = entry->previous;
    entry->previous = entry->next = NULL; entry->owner = NULL;
}
static void unlinkIndexLocked(Entry* entry)
{
    Registry* registry = entry->registry;
    if (!registry) return;
    void* removed = NULL;
    if (!xrtIntMapTakePtr(&registry->index, entry->id, &removed) || removed != entry) abort();
    entry->registry = NULL; entryDropLocked(entry);
    if (!xrtIntMapCount(&registry->index)) (void)xrtIntMapTrim(&registry->index, 0);
}
static Entry* detachAllLocked(Local* state)
{
    Entry* retired = NULL;
    while (state->entries) {
        Entry* entry = state->entries;
        unlinkStateLocked(entry); unlinkIndexLocked(entry);
        entry->retiredNext = retired; retired = entry;
    }
    return retired;
}
static bool dropEntries(Entry* list)
{
    xl6_error_scope cleanup = {0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort();
    while (list) {
        Entry* entry = list; list = entry->retiredNext;
        xrtownershipscope mutation = {0}; enter(&mutation);
        xvalue* value = entry->value; entry->value = NULL; entry->retiredNext = NULL;
        entryDropLocked(entry); leave(&mutation);
        xrtValueRelease(value);
    }
    xl6_error_result outcome = xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    else xrtErrorFree(outcome.error);
    return outcome.ok;
}
static void registryDrop(void* raw)
{
    Registry* registry = raw;
    if (!registry) return;
    Entry* retired = NULL;
    xrtownershipscope mutation = {0}; enter(&mutation);
    Entry** slot;
    int64 id;
    while ((slot = xrtIntMapFirst(&registry->index, &id)) != NULL) {
        Entry* entry = *slot;
        /* Transfer the registry's storage reference to this private tail. */
        if (!xrtIntMapRemove(&registry->index, id)) abort();
        entry->registry = NULL;
        if (entry->owner && !entry->owner->claim) {
            unlinkStateLocked(entry);
            entryDropLocked(entry); /* State's metadata ref; private ref remains. */
            entry->retiredNext = retired; retired = entry;
        } else {
            /* A claimed State must keep its owning slots stable. Mark the
             * index gone; its transaction or a later touch/close drains them. */
            entryDropLocked(entry);
        }
    }
    xrtIntMapUnit(&registry->index);
    leave(&mutation);
    dropEntries(retired);
    KeyOwner* key = registry->key;
    xrtFree(registry); keyDrop(key);
}
static void clearEmpty(KeyOwner* key)
{
    Registry* registry = xrtThreadKeyGet(key->key);
    if (!registry) return;
    xrtownershipscope mutation = {0}; enter(&mutation);
    bool empty = !xrtIntMapCount(&registry->index);
    leave(&mutation);
    if (empty) {
        void* taken = xrtThreadKeyTake(key->key);
        if (taken) { if (taken != registry) abort(); registryDrop(taken); }
    }
}
static void release(Local* state)
{
    if (!state) return;
    xrtownershipscope mutation = {0}; enter(&mutation);
    int32_t refs = xrtRefRelease(&state->refs);
    if (refs < 0) abort();
    if (refs) { leave(&mutation); return; }
    if (state->active || state->retired || (state->claim && !state->cleared)) abort();
    state->closed = true;
    Entry* retired = detachAllLocked(state);
    KeyOwner* key = state->key;
    leave(&mutation);
    dropEntries(retired); clearEmpty(key);
    keyDrop(key); xrtFree(state);
}
static Local* begin(void* raw, bool permitClosed)
{
    Local* state = raw;
    xrtownershipscope mutation = {0}; enter(&mutation);
    bool ok = state && !state->cleared && !state->claim && (permitClosed || !state->closed) &&
        state->active < SIZE_MAX && state->refs > 0 && state->refs < INT32_MAX;
    Entry* retired = NULL;
    if (ok) {
        ++state->active; xrtRefRetain(&state->refs);
        /* An exit concurrent with a lifecycle claim cannot alter owning
         * slots. Drain those now after the transaction restored admission. */
        for (Entry* entry = state->entries; entry;) {
            Entry* next = entry->next;
            if (!entry->registry) {
                unlinkStateLocked(entry); entry->retiredNext = retired; retired = entry;
            }
            entry = next;
        }
    }
    leave(&mutation);
    if (!ok) { xrtSetErrorKind(XERR_STATE); return NULL; }
    if (!dropEntries(retired)) { end(state); return NULL; }
    return state;
}
static void end(Local* state)
{
    if (!state) return;
    clearEmpty(state->key);
    xrtownershipscope mutation = {0}; enter(&mutation);
    if (!state->active) abort();
    --state->active; leave(&mutation); release(state);
}
static Registry* current(Local* state, bool create)
{
    Registry* registry = xrtThreadKeyGet(state->key->key);
    if (registry || !create) return registry;
    registry = xrtCalloc(1, sizeof(*registry));
    if (!registry) return NULL;
    if (!xrtIntMapInit(&registry->index, sizeof(Entry*))) { xrtFree(registry); return NULL; }
    registry->key = state->key;
    xrtownershipscope mutation = {0}; enter(&mutation);
    if (state->key->refs == SIZE_MAX) { leave(&mutation); xrtIntMapUnit(&registry->index); xrtFree(registry); xrtSetErrorKind(XERR_RANGE); return NULL; }
    ++state->key->refs; leave(&mutation);
    if (!xrtThreadKeySet(state->key->key, registry)) { registryDrop(registry); return NULL; }
    return registry;
}
void* xl6ThreadLocalCreateV1(void)
{
    Local* state = xrtCalloc(1, sizeof(*state));
    if (!state) return NULL;
    if (!xl6ThreadExitRegisterLocalV1(clearCurrent)) { xrtFree(state); xrtSetErrorKind(XERR_STATE); return NULL; }
    xrtownershipscope mutation = {0}; enter(&mutation);
    if (nextId == INT64_MAX || (sharedKey && sharedKey->refs == SIZE_MAX)) {
        leave(&mutation); xrtFree(state); xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    KeyOwner* key = sharedKey;
    if (!key) {
        key = xrtCalloc(1, sizeof(*key));
        if (key && !(key->key = xrtThreadKeyCreate(registryDrop))) { xrtFree(key); key = NULL; }
    }
    if (key) { ++key->refs; sharedKey = key; state->id = ++nextId; state->key = key; state->refs = 1; }
    leave(&mutation);
    if (!key) { xrtFree(state); return NULL; }
    return state;
}
void xl6ThreadLocalDestroyV1(void* raw) { release(raw); }

static Entry* lookupLocked(Registry* registry, Local* state)
{ return registry ? xrtIntMapGetPtr(&registry->index, state->id) : NULL; }
xvalue* xl6ThreadLocalGetV1(void* raw)
{
    Local* state = begin(raw, false);
    if (!state) return NULL;
    Registry* registry = current(state, false);
    xrtownershipscope mutation = {0}; enter(&mutation);
    Entry* entry = lookupLocked(registry, state);
    xvalue* value = !state->closed && entry ? xrtValueRetain(entry->value) : NULL;
    bool closed = state->closed; leave(&mutation);
    if (closed) xrtSetErrorKind(XERR_STATE);
    else if (!value) value = xrtValueNull();
    end(state); return value;
}
bool xl6ThreadLocalHasV1(void* raw)
{
    Local* state = begin(raw, false);
    if (!state) return false;
    Registry* registry = current(state, false);
    xrtownershipscope mutation = {0}; enter(&mutation);
    bool closed = state->closed;
    bool present = !closed && lookupLocked(registry, state);
    leave(&mutation); if (closed) xrtSetErrorKind(XERR_STATE);
    end(state); return present;
}
void xl6ThreadLocalSetV1(void* raw, const xvalue* value)
{
    Local* state = begin(raw, false);
    if (!state) return;
    xvalue* held = value ? xrtValueRetain(value) : NULL;
    Registry* registry = held ? current(state, true) : NULL;
    xvalue* prior = NULL;
    if (!value) xrtSetErrorKind(XERR_ARGUMENT);
    if (registry) {
        xrtownershipscope mutation = {0}; enter(&mutation);
        Entry* entry = lookupLocked(registry, state);
        if (state->closed) xrtSetErrorKind(XERR_STATE);
        else if (entry) { prior = entry->value; entry->value = held; held = NULL; }
        else {
            entry = xrtCalloc(1, sizeof(*entry));
            if (entry) {
                entry->refs = 1; entry->id = state->id;
                if (xrtIntMapSetPtr(&registry->index, state->id, entry)) {
                    entry->registry = registry; ++entry->refs;
                    entry->owner = state; entry->value = held; held = NULL;
                    entry->next = state->entries;
                    if (entry->next) entry->next->previous = entry;
                    state->entries = entry;
                } else xrtFree(entry);
            }
        }
        leave(&mutation);
    }
    xrtValueRelease(prior); xrtValueRelease(held); end(state);
}
static xvalue* take(void* raw, bool* present)
{
    Local* state = begin(raw, false);
    if (!state) return NULL;
    Registry* registry = current(state, false);
    xrtownershipscope mutation = {0}; enter(&mutation);
    Entry* entry = lookupLocked(registry, state);
    xvalue* value = NULL;
    bool closed = state->closed;
    if (!closed && entry) {
        value = entry->value; entry->value = NULL; *present = true;
        unlinkStateLocked(entry); unlinkIndexLocked(entry); entryDropLocked(entry);
    }
    leave(&mutation); if (closed) xrtSetErrorKind(XERR_STATE);
    end(state); return value;
}
xvalue* xl6ThreadLocalTakeV1(void* raw)
{
    bool present = false;
    xvalue* value = take(raw, &present);
    return value ? value : xrtGetError() ? NULL : xrtValueNull();
}
bool xl6ThreadLocalRemoveV1(void* raw)
{
    bool present = false; xvalue* value = take(raw, &present);
    xrtValueRelease(value); return present;
}
void xl6ThreadLocalCloseV1(void* raw)
{
    Local* state = begin(raw, true);
    if (!state) return;
    xrtownershipscope mutation = {0}; enter(&mutation);
    state->closed = true; Entry* retired = detachAllLocked(state); leave(&mutation);
    dropEntries(retired); end(state);
}
bool xl6ThreadLocalClosedV1(void* raw)
{
    Local* state = begin(raw, true);
    if (!state) return true;
    xrtownershipscope mutation = {0}; enter(&mutation);
    bool closed = state->closed; leave(&mutation); end(state); return closed;
}
static bool clearCurrent(void)
{
    xl6_error_scope cleanup = {0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort();
    bool done = false;
    for (unsigned pass = 0; pass != 9; ++pass) {
        xrtownershipscope mutation = {0}; enter(&mutation);
        KeyOwner* key = sharedKey;
        if (key) { if (key->refs == SIZE_MAX) abort(); ++key->refs; }
        leave(&mutation);
        if (!key) { done = true; break; }
        Registry* registry = xrtThreadKeyGet(key->key);
        if (!registry) done = true;
        else if (pass < 8) {
            void* taken = xrtThreadKeyTake(key->key);
            if (taken) { if (taken != registry) abort(); registryDrop(taken); }
            else xl6ErrorScopeCaptureFailure(&cleanup);
        }
        keyDrop(key);
        if (done) break;
    }
    if (!done) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&cleanup); }
    xl6_error_result outcome = xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    else xrtErrorFree(outcome.error);
    return outcome.ok;
}
static bool count(const void* raw, size_t* result)
{
    const Local* state = raw;
    /* Active calls carry real external refs. Count them instead of reporting
     * an unknown node: the frozen metadata/value slots are fully observable,
     * and exclusivity will correctly remain ROOTED until those calls finish. */
    if (!state || !result || state->refs <= 0 || state->cleared) return false;
    *result = (size_t)state->refs; return true;
}
static bool trace(const void* raw, xrtownershipvisitor visitor, void* context)
{
    const Local* state = raw; size_t refs;
    if (!visitor || !count(raw, &refs)) return false;
    for (const Entry* entry = state->entries; entry; entry = entry->next)
        if (!visitor(xrtValueOwnership(entry->value), context)) return false;
    return true;
}
static const xrtownershipops ops = {count, trace};
static bool handleTrace(const xvalue* value, xrtownershipvisitor visitor, void* context)
{
    void* raw = NULL;
    return visitor && xrtValueGetHandle(value, &raw, NULL, NULL) && raw &&
        visitor((xrtownershipref){raw, &ops}, context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ThreadLocalHandlePolicyV1Get, xl6ThreadLocalDestroyV1, handleTrace)
static bool hold(const void* raw)
{ Local* state = (Local*)raw; return state && state->refs > 0 && state->refs < INT32_MAX && xrtRefRetain(&state->refs) > 0; }
static void drop(const void* raw) { release((Local*)raw); }
static bool claim(const void* raw, const void* token)
{
    Local* state = (Local*)raw;
    if (!token || state->cleared || state->active || (state->claim && state->claim != token)) return false;
    state->claim = token; return true;
}
static void restore(const void* raw, const void* token)
{
    Local* state = (Local*)raw;
    if (state->claim != token || state->cleared) abort();
    state->claim = NULL;
}
static void clear(const void* raw, const void* token)
{
    Local* state = (Local*)raw;
    if (state->claim != token || state->cleared || state->active || state->retired) abort();
    state->retired = detachAllLocked(state); state->closed = state->cleared = true;
}
static bool finish(const void* raw, const void* token)
{
    Local* state = (Local*)raw;
    xrtownershipscope mutation = {0}; enter(&mutation);
    if (state->claim != token || !state->cleared) abort();
    Entry* retired = state->retired; state->retired = NULL; leave(&mutation);
    dropEntries(retired); clearEmpty(state->key); return true;
}
const xrtownershipadapterv1* xl6ThreadLocalOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), hold, drop, claim, restore, NULL, clear, finish};
    size_t refs;
    return ref.Data && ref.Ops == &ops && count(ref.Data, &refs) ? &adapter : NULL;
}
