#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <__xl_runtime/xruntime_decl.h>

/* Observe foreign allocations independently of the XRT fault allocator. */
static size_t allocated, freed;
static void* trackedMalloc(size_t size) { void* p = malloc(size); if (p) ++allocated; return p; }
static void trackedFree(void* p) { if (p) ++freed; free(p); }
#define malloc trackedMalloc
#define free trackedFree
#include "xlir_c_owned_native.c"
#undef malloc
#undef free
#define main unusedGeneratedMain
#include XL6_C_OWNED_EXPORT_PROGRAM
#undef main

static void checkProduct(XLTestOwnedProduct value)
{
    xbytesview bytes = {0};
    assert(value.e0 != NULL);
    assert(value.e1.Size == 5 && !memcmp(value.e1.Data, "hello", 5));
    assert(!c_owned_text_is_original(value.e1.Data));
    assert(xrtValueGetBytes(value.e2, &bytes) && bytes.Size == 4);
    assert(!c_owned_binary_span_original(bytes) && !memcmp(bytes.Data, "\0\xff\x11*", 4));
    assert(value.e3 == 42);
}
static void dropProduct(XLTestOwnedProduct value)
{ xrtValueRelease(value.e0); xrtFree((void*)value.e1.Data); xrtValueRelease(value.e2); }

int main(void)
{
    assert(xrtMemDebugEnable(true) && xrtMemDebugEnabled());
    assert(xlir_module_init());
    assert(allocated == freed);
    xl6_foreign_error_scope entry = {0}; xl6_foreign_error_result outcome;
    assert(xl6ForeignCallBegin(&entry));
    xlir_string text = owned_text();
    assert(text.Data && text.Size == 5 && !memcmp(text.Data, "hello", 5));
    assert(!c_owned_text_is_original(text.Data));
    xrtFree((void*)text.Data);
    xvalue* data = owned_bytes(); xbytesview bytes = {0};
    assert(xrtValueGetBytes(data, &bytes) && bytes.Size == 4 && !c_owned_binary_span_original(bytes));
    xrtValueRelease(data);
    assert(owned_optional_text(false).Data == NULL);
    data = owned_optional_bytes(false); assert(xrtValueType(data) == XVALUE_NULL); xrtValueRelease(data);
    text = owned_optional_text(true); assert(text.Size == 5); xrtFree((void*)text.Data);
    data = owned_optional_bytes(true); assert(xrtValueType(data) == XVALUE_BYTES); xrtValueRelease(data);
    text = owned_empty_text(); assert(text.Data && text.Size == 0); xrtFree((void*)text.Data);
    data = owned_empty_bytes(); assert(xrtValueGetBytes(data, &bytes) && bytes.Size == 0); xrtValueRelease(data);
    XLTestOwnedProduct product = owned_pair(); checkProduct(product); dropProduct(product);
    outcome = xl6ForeignCallEnd(&entry); assert(outcome.ok && !outcome.error);
    assert(allocated == freed);

    assert(xl6ForeignCallBegin(&entry)); text = owned_failure(); outcome = xl6ForeignCallEnd(&entry);
    assert(!outcome.ok && outcome.error && !text.Data);
    assert(strstr(xrtErrorMessage(outcome.error), "export body failed")); xrtErrorFree(outcome.error);
    assert(allocated == freed);

    size_t failures = 0, successes = 0;
    xmemdebugsnapshot baseline = {0}; xrtMemDebugSnapshot(&baseline);
    for (unsigned step = 0; step < 32; ++step) {
        assert(xl6ForeignCallBegin(&entry));
        assert(xrtMemDebugFailAfter(step));
        product = owned_pair();
        bool injected = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        outcome = xl6ForeignCallEnd(&entry);
        if (outcome.ok) { assert(!injected); checkProduct(product); ++successes; }
        else {
            assert(injected && !product.e0 && !product.e1.Data && !product.e2 && !product.e3); ++failures;
        }
        dropProduct(product); xrtErrorFree(outcome.error); xrtClearError();
        assert(allocated == freed);
        xmemdebugsnapshot after = {0}; xrtMemDebugSnapshot(&after);
        assert(after.LiveCount == baseline.LiveCount);
    }
    assert(failures && successes);
    printf("C owner export host: pointer isolation, null/empty, binary NUL, body failure; %zu allocation failures and %zu successes, no leaks\n", failures, successes);
    return 0;
}
