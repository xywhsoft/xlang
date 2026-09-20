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

static long live, allocated, freed;
static int fail_at, attempts;
static char* callback_strdup(const char* text)
{
    char* copy = xrtStrDup(text);
    if (copy != NULL) { ++live; ++allocated; }
    return copy;
}
static void* callback_alloc(size_t size)
{
    void* pointer;
    if (fail_at && ++attempts == fail_at) return NULL;
    pointer = xrtMalloc(size);
    if (pointer != NULL) { ++live; ++allocated; }
    return pointer;
}
static void callback_free(void* pointer)
{
    if (pointer != NULL) { --live; ++freed; assert(live >= 0); }
    xrtFree(pointer);
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
    long baseline;
    assert(xlir_module_init());
    baseline = live;
    assert(baseline > 0);
    (void)callback_product(triple);
    assert(live == baseline);
    assert(callback_optional(NULL) == 13);
    assert(callback_optional(triple) == 42);
    assert(live == baseline);
    assert(xl6_export_callbackObject(return_borrowed_marker) == 41);
    assert(destroyed == 1 && live == baseline);
    destructor_error = 1;
    assert(xl6_export_callbackObject(return_marker_error) == 0);
    assert(xrtGetError() == reported_error);
    xrtSetErrorTake(NULL);
    destructor_error = 0;
    assert(destroyed == 3 && live == baseline);
    {
        char* text = xl6_export_callbackText(return_text);
        assert(text != NULL && strcmp(text, "payload") == 0);
        callback_free(text);
        assert(live == baseline);
        assert(xl6_export_callbackText(return_text_error) == NULL);
        assert(xrtGetError() == reported_error);
        xrtSetErrorTake(NULL);
        assert(live == baseline);
    }
    for (i = 0; i < 200; ++i) {
        assert(callback_pair(triple, triple, 7) == 42);
        assert(live == baseline);
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
        assert(live == baseline);
    }
    callback_save(triple);
    assert(callback_saved(14) == 42);
    assert(live == baseline);
    callback_save(NULL);
    assert(callback_saved(14) == -1);
    assert(live == baseline - 1);
    callback_throw(triple);
    assert(xrtGetError() != NULL);
    assert(strstr(xrtErrorMessage(xrtGetError()), "export-callback-failure-sentinel") != NULL);
    xrtSetErrorTake(NULL);
    assert(callback_saved(14) == 42);
    assert(live == baseline);
    before = triple_calls;
    assert(callback_pair(native_error, triple, 7) == 0);
    assert(xrtGetError() == reported_error);
    assert(triple_calls == before);
    xrtSetErrorTake(NULL);
    assert(live == baseline);
    assert(callback_catch_native(native_error) == 42);
    assert(xrtGetError() == NULL);
    assert(live == baseline);
    xlir_module_fini();
    xlir_module_fini();
    assert(live == 0 && allocated == freed);
    printf("export callback allocations balanced: %ld; partial acquisition, escape, null, native error and throw ok\n", allocated);
    return 0;
}
