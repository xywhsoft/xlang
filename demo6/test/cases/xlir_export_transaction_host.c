#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../build/test/xlir_export_transaction_bind.h"
/* Inject at the generated adapter's allocator/releaser boundary, never by
 * changing generated control flow. Runtime internals continue to use XRT. */
#define xrtMalloc tx_malloc
#define xrtFree tx_free
#define xrtValueRelease tx_value_release
#include "../../build/test/xlir_export_transaction.c"
#undef xrtMalloc
#undef xrtFree
#undef xrtValueRelease
extern void* xrtMalloc(size_t);
extern void xrtFree(void*);
extern void xrtValueRelease(xvalue*);
void tx_error_oom(void);

static int mode, body_calls, inject_malloc = -1, track_bridges, bridge_count;
static int cleanup_failure, secondary_frees, primary_null;
static void* bridges[2];
static void* secondary_string;
static xvalue* secondary_value;
void tx_test_mode(int value) { mode = value; }

static void publish(int failureMode, int code)
{
    xl6_error_result failure = {false, NULL};
    if (failureMode == 1)
        failure.error = xrtErrorCreate(15, "export-transaction", code, "export failure");
    else if (failureMode == 3) tx_error_oom();
    if (failureMode != 0) xl6ErrorScopePropagateFailure(failure);
}

void export_transaction_fail(void) { ++body_calls; publish(mode, 101); }

void* tx_malloc(size_t size)
{
    void* result;
    if (inject_malloc == 0) return NULL;
    if (inject_malloc > 0) --inject_malloc;
    result = xrtMalloc(size);
    if (track_bridges && result != NULL) {
        assert(bridge_count < 2); bridges[bridge_count++] = result;
    }
    return result;
}

void tx_free(void* pointer)
{
    int i, bridge = 0;
    for (i = 0; i < bridge_count; ++i)
        if (bridges[i] == pointer && pointer != NULL) { bridges[i] = NULL; bridge = 1; }
    xrtFree(pointer);
    if (pointer != NULL && pointer == secondary_string) {
        secondary_string = NULL; ++secondary_frees; publish(1, 202);
    }
    if (bridge && cleanup_failure) {
        ++secondary_frees;
        publish(primary_null && secondary_frees == 1 ? 2 : 1,
            secondary_frees == 1 ? 303 : 404);
    }
}

void tx_value_release(xvalue* value)
{
    xrtValueRelease(value);
    if (value != NULL && value == secondary_value) {
        secondary_value = NULL; ++secondary_frees; publish(1, 202);
    }
}

static int64_t constant_callback(void) { return 29; }
static int64_t reentrant_callback(void)
{
    assert(tx_scalar() == 0);
    /* The nested export's failure remains in the invocation scope even when
     * native code consumes its diagnostic before returning a normal value. */
    xrtErrorFree(xrtTakeError());
    return 29;
}
static xlir_string owned_string_callback(void)
{
    char* value = xrtStrDup("callback-owned");
    assert(value != NULL); secondary_string = value;
    publish(mode, 101);
    return (xlir_string){ value, strlen(value) };
}
static xvalue* owned_value_callback(void)
{
    xvalue* value = xrtValueInt(42);
    assert(value != NULL); secondary_value = value;
    publish(mode, 101);
    return value;
}

static void check_outcome(xl6_error_result outcome, int failed, int code)
{
    assert(outcome.ok == !failed);
    if (!failed || code == 0) assert(outcome.error == NULL);
    else assert(outcome.error != NULL && xrtErrorCode(outcome.error) == code);
    xrtErrorFree(outcome.error);
    assert(xrtGetError() == NULL);
}

void tx_direct_case(int failureMode, int kind)
{
    xl6_error_scope scope = {0};
    xlir_string string;
    xvalue* value;
    TX_PRODUCT product;
    mode = failureMode;
    assert(xl6ForeignCallBegin(&scope));
    switch (kind) {
    case 0: assert(tx_scalar() == (mode == 0 ? 42 : 0)); break;
    case 1: tx_void(); break;
    case 2:
        string = tx_string(); assert((string.Data != NULL) == (mode == 0));
        if (mode == 0) assert(string.Size == 6 && memcmp(string.Data, "ow\0ned", 6) == 0);
        xrtFree((void*)string.Data); break;
    case 3:
        value = tx_value(); assert((value != NULL) == (mode == 0)); xrtValueRelease(value); break;
    default:
        product = tx_product(); assert((product.e0.Data != NULL) == (mode == 0));
        if (mode == 0) assert(product.e0.Size == 6 && memcmp(product.e0.Data, "ow\0ned", 6) == 0);
        assert(product.e1 == (mode == 0 ? 42 : 0)); xrtFree((void*)product.e0.Data); break;
    }
    check_outcome(xl6ForeignCallEnd(&scope), mode != 0, mode == 1 ? 101 : 0);
}

void tx_callback_case(int failureMode, int kind)
{
    xl6_error_scope scope = {0};
    mode = failureMode; secondary_frees = 0;
    assert(xl6ForeignCallBegin(&scope));
    if (kind == 0) assert(tx_catch_callback(reentrant_callback) == 17);
    else if (kind == 1) assert(tx_apply_callback(reentrant_callback) == 0);
    else if (kind == 2) assert(tx_string_callback(owned_string_callback).Data == NULL);
    else assert(tx_value_callback(owned_value_callback) == NULL);
    check_outcome(xl6ForeignCallEnd(&scope), kind != 0,
        kind != 0 && mode == 1 ? 101 : 0);
    assert(secondary_string == NULL && secondary_value == NULL);
    assert(secondary_frees == (kind >= 2 ? 1 : 0));
}

void tx_pending_case(int no_diagnostic, int kind)
{
    xl6_error_scope scope = {0};
    TX_ACCEPT_PRODUCT product;
    bridge_count = 0; track_bridges = 1; cleanup_failure = 1;
    secondary_frees = 0; primary_null = no_diagnostic;
    assert(xl6ForeignCallBegin(&scope));
    if (kind == 0) assert(tx_accept_string(constant_callback, constant_callback).Data == NULL);
    else if (kind == 1) assert(tx_accept_value(constant_callback, constant_callback) == NULL);
    else {
        product = tx_accept_product(constant_callback, constant_callback);
        assert(product.e0.Data == NULL && product.e1 == 0);
    }
    assert(bridge_count == 2 && secondary_frees == 2 && bridges[0] == NULL && bridges[1] == NULL);
    track_bridges = 0; cleanup_failure = 0;
    check_outcome(xl6ForeignCallEnd(&scope), 1, no_diagnostic ? 0 : 303);
}

void tx_bridge_case(int fail_after, int injected)
{
    xl6_error_scope scope = {0};
    xl6_error_result outcome;
    if (injected) inject_malloc = fail_after;
    bridge_count = 0; track_bridges = 1;
    assert(xl6ForeignCallBegin(&scope));
    assert(tx_accept_string(constant_callback, constant_callback).Data == NULL);
    outcome = xl6ForeignCallEnd(&scope);
    assert(!outcome.ok && bridge_count == fail_after);
    if (fail_after == 1) assert(bridges[0] == NULL);
    track_bridges = 0; inject_malloc = -1;
    xrtErrorFree(outcome.error); assert(xrtGetError() == NULL);
}

void tx_entry_failure_case(void)
{
    xl6_error_scope scope = {0};
    int before = body_calls;
    /* Module state 1 is the real reentrant-initialization rejection state. */
    xlir_module_initialized = 1;
    assert(xl6ForeignCallBegin(&scope));
    assert(tx_scalar() == 0);
    check_outcome(xl6ForeignCallEnd(&scope), 1, 0);
    assert(body_calls == before);
    xlir_module_initialized = 2;
}

void tx_previous_diagnostic_case(void)
{
    xerror* previous = xrtErrorCreate(15, "native-caller", 707, "saved diagnostic");
    assert(previous != NULL);
    xrtSetErrorTake(previous);
    assert(tx_pure(7) == 7 && xrtGetError() == previous);
    xrtErrorFree(xrtTakeError());
}

void tx_finish(void) { xlir_module_fini(); assert(xrtGetError() == NULL); }
