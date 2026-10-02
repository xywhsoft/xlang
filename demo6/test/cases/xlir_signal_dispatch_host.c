/* Generated module preincluded. This private test process explicitly retires
 * XRT's process-global dispatcher after language/module finalization. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char** argv)
{
    size_t point=argc==2?(size_t)strtoull(argv[1],NULL,10):SIZE_MAX;
    xmemdebugsnapshot before,after;assert(argc==1 || argc==2);
    assert(xrtMemDebugEnable(true));assert(xrtSignalSupported(XSIGNAL_INT));
    xrtMemDebugSnapshot(&before);
    if(point!=SIZE_MAX)assert(xrtMemDebugFailAfter(point));
    int initialized=xlir_module_init();bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    bool validFailure=xrtErrorKind(xrtGetError())==XERR_MEMORY;
#if defined(XL6_CHANNEL_LOGICAL_LEDGER)
    /* This separate baseline channel test validates rejection and ownership,
     * not preservation of the native OOM kind: the pre-existing transport
     * deliberately surfaces ChannelError and currently discards its cause. */
    validFailure=validFailure || xlir_error_kind_is(xrtGetError(),"ChannelError");
#endif
    if(hit && (initialized || !validFailure))
        fprintf(stderr,"signal OOM mismatch: initialized=%d kind=%d message=%s\n",initialized,
            (int)xrtErrorKind(xrtGetError()),xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    if(hit)assert(!initialized && validFailure);
    else assert(initialized && !xrtGetError());
    xlir_module_fini();xrtClearError();xrtSignalShutdown();assert(!xrtGetError());
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("signal logical allocation point=%zu hit=%d balanced\n",point,hit);return 0;
}
