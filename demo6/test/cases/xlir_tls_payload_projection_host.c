/* Generated TLS fixture and verified field bindings are preincluded.
 * Do not run its module initializer: scheduling/transport admission is separate. */
#include <assert.h>
static void borrowed_sentinel_drop(void* value, void* data)
{ (void)data; assert(*(int*)value == 42); }
static const xvaluehandleops borrowed_sentinel_ops = { NULL, borrowed_sentinel_drop, NULL, NULL };
int main(void)
{
    int round, failure, failures = 0, successes = 0;
    assert(xrtMemDebugEnable(true));
    assert(xl6_tls_stream_result_handle(NULL) == NULL);
    assert(xl6_tls_bytes_result_handle(NULL) == NULL);
    for (round = 0; round < 50; ++round)
    for (failure = -1; failure < 16; ++failure) {
        xmemdebugsnapshot before, after;
        XL6_TlsStream* native;
        xvalue* object;
        xvalue* bytes;
        xvalue* handle;
        int payload = 42;
        void* borrowed = &payload;
        xrtMemDebugSnapshot(&before);
        native = (XL6_TlsStream*)xrtCalloc(1, sizeof(*native));
        assert(native);
        if (failure >= 0) assert(xrtMemDebugFailAfter(failure));
        object = xlir_net_tls_stream_wrap_owned(native);
        if (xrtMemDebugFailTriggered()) { assert(!object); ++failures; }
        else { assert(object); ++successes; }
        xrtMemDebugFailClear();
        if (object) assert(xl6_tls_stream_result_handle(object) == native);
        xrtValueRelease(object);
        xrtClearError();
        /* A borrowed sentinel tests representation, not NetBytes lifetime. */
        bytes = xrtValueObjectLifo();
        handle = xrtValueHandleTake(&borrowed, &borrowed_sentinel_ops, NULL);
        assert(bytes && handle);
        assert(xrtValueObjectSetNew(bytes, xrtStrView(XLIR_TLS_BYTES_FIELD), handle));
        assert(xl6_tls_bytes_result_handle(bytes) == &payload);
        xrtValueRelease(bytes);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    assert(failures > 0 && successes > 0);
    printf("TLS payload projection: 850 cases, %d allocation failures, %d successes, balanced memory\n", failures, successes);
    return 0;
}
