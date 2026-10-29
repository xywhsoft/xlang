#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void* callback_alloc(size_t size);
static void callback_free(void* pointer);
static char* callback_strdup(const char* text);
#define xrtMalloc callback_alloc
#define xrtFree callback_free
#define xrtStrDup callback_strdup
#include "../../build/test/xlir_export_callback_lifetime.c"
#undef xrtMalloc
#undef xrtFree
#undef xrtStrDup
extern void* xrtMalloc(size_t);
extern void xrtFree(void*);
extern char* xrtStrDup(const char*);

static long generated_allocations;
static int fail_at, attempts;
static char* callback_strdup(const char* text)
{
    char* copy = xrtStrDup(text);
    if (copy != NULL) ++generated_allocations;
    return copy;
}
static void* callback_alloc(size_t size)
{
    void* pointer;
    if (fail_at && ++attempts == fail_at) return NULL;
    pointer = xrtMalloc(size);
    if (pointer != NULL) ++generated_allocations;
    return pointer;
}
static void callback_free(void* pointer)
{
    xrtFree(pointer);
}
static void assert_runtime_balanced(const xmemdebugsnapshot* expected)
{
    xmemdebugsnapshot current;
    xrtMemDebugSnapshot(&current);
    if (current.LiveCount != expected->LiveCount ||
        current.LiveBytes != expected->LiveBytes)
        fprintf(stderr, "runtime balance mismatch: expected %zu/%zu, current %zu/%zu\n",
            expected->LiveCount, expected->LiveBytes,
            current.LiveCount, current.LiveBytes);
    assert(current.LiveCount == expected->LiveCount);
    assert(current.LiveBytes == expected->LiveBytes);
}
static int triple_calls;
static xerror* reported_error;
static int destroyed, destructor_error;
void callback_marker_destroyed(int64_t id)
{
    ++destroyed;
    if (id == 42 && destructor_error)
        xrtSetErrorTake(xlir_error_create("RuntimeError", "callback-result-destructor-error"));
}
static int triple(int value) { ++triple_calls; return value * 3; }
static int native_error(int value)
{
    (void)value;
    reported_error = xlir_error_create("RuntimeError", "native-callback-sentinel");
    assert(reported_error != NULL);
    xrtSetErrorTake(reported_error);
    return 0;
}
static xvalue* return_borrowed_marker(xvalue* marker) { return xrtValueRetain(marker); }
static xvalue* return_marker_error(xvalue* marker)
{
    xvalue* result;
    (void)marker;
    result = xl6_export_makeCallbackMarker(42);
    (void)native_error(0);
    return result;
}
static char* return_text(const char* text) { return callback_strdup(text); }
static char* return_text_error(const char* text)
{
    char* result = callback_strdup(text);
    (void)native_error(0);
    return result;
}

int main(void)
{
    int i, before;
    xmemdebugsnapshot initial, baseline, saved;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&initial);
    assert(xlir_module_init());
    xrtMemDebugSnapshot(&baseline);
    (void)callback_product(triple);
    assert(callback_saved(14) == 14);
    assert_runtime_balanced(&baseline);
    assert(callback_optional(NULL) == 13);
    assert(callback_optional(triple) == 42);
    assert_runtime_balanced(&baseline);
    assert(xl6_export_callbackObject(return_borrowed_marker) == 41);
    assert(destroyed == 1);
    assert_runtime_balanced(&baseline);
    destructor_error = 1;
    assert(xl6_export_callbackObject(return_marker_error) == 0);
    assert(xrtGetError() == reported_error);
    xrtSetErrorTake(NULL);
    destructor_error = 0;
    assert(destroyed == 3);
    assert_runtime_balanced(&baseline);
    {
        char* text = xl6_export_callbackText(return_text);
        assert(text != NULL && strcmp(text, "payload") == 0);
        callback_free(text);
        assert_runtime_balanced(&baseline);
        assert(xl6_export_callbackText(return_text_error) == NULL);
        assert(xrtGetError() == reported_error);
        xrtSetErrorTake(NULL);
        assert_runtime_balanced(&baseline);
    }
    for (i = 0; i < 200; ++i) {
        assert(callback_pair(triple, triple, 7) == 42);
        assert_runtime_balanced(&baseline);
    }
    for (i = 1; i <= 2; ++i) {
        before = callback_entered();
        fail_at = i; attempts = 0;
        assert(callback_pair(triple, triple, 7) == 0);
        fail_at = 0;
        assert(xrtGetError() != NULL);
        assert(strstr(xrtErrorMessage(xrtGetError()), "C callback export bridge allocation failed") != NULL);
        xrtSetErrorTake(NULL);
        assert(callback_entered() == before);
        assert_runtime_balanced(&baseline);
    }
    callback_save(triple);
    assert(callback_saved(14) == 42);
    xrtMemDebugSnapshot(&saved);
    assert(saved.LiveCount > baseline.LiveCount);
    callback_save(NULL);
    assert(callback_saved(14) == -1);
    /* The initialized baseline owns the default identity closure. Clearing
     * the mutable global legitimately releases that last module allocation. */
    assert_runtime_balanced(&initial);
    callback_throw(triple);
    assert(xrtGetError() != NULL);
    assert(strstr(xrtErrorMessage(xrtGetError()), "export-callback-failure-sentinel") != NULL);
    xrtSetErrorTake(NULL);
    assert(callback_saved(14) == 42);
    assert_runtime_balanced(&saved);
    before = triple_calls;
    assert(callback_pair(native_error, triple, 7) == 0);
    assert(xrtGetError() == reported_error);
    assert(triple_calls == before);
    xrtSetErrorTake(NULL);
    assert_runtime_balanced(&saved);
    assert(callback_catch_native(native_error) == 42);
    assert(xrtGetError() == NULL);
    assert_runtime_balanced(&saved);
    xlir_module_fini();
    xlir_module_fini();
    assert_runtime_balanced(&initial);
    printf("export callback memory balanced after %ld generated allocations; partial acquisition, escape, null, native error and throw ok\n",
        generated_allocations);
    return 0;
}
