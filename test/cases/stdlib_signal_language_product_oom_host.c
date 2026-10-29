/* Preinclude unmodified actual-product emitted C. Preserve the original signal
 * host's OOM/error/lifetime assertions, with one separately observed contract:
 * optional callback diagnostics preserve an already escaping user exception.
 * All language assertions remain unchanged and must complete in that case. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
extern unsigned testSignalPreservedDiagnosticFailures(void);

int main(int argc,char** argv)
{
    assert(argc==1 || argc==2);uint64_t point=UINT64_MAX;
    if(argc==2){
        char* end=NULL;errno=0;point=strtoull(argv[1],&end,10);
        assert(*argv[1] && *argv[1]!='-' && end && !*end && !errno);
    }
    assert(xrtMemDebugEnable(true) && xrtSignalSupported(XSIGNAL_INT));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    if(point!=UINT64_MAX)assert(xrtMemDebugFailAfter(point));
    int initialized=xlir_module_init();bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    unsigned preserved=testSignalPreservedDiagnosticFailures();
    bool fallback=hit && preserved==1 && initialized && !xrtGetError();
    bool valid=hit?fallback || (!preserved && !initialized && xrtErrorKind(xrtGetError())==XERR_MEMORY):
        !preserved && initialized && !xrtGetError();
    if(!valid)fprintf(stderr,"language-signal point=%llu hit=%d initialized=%d preserved=%u kind=%d\n",
        (unsigned long long)point,hit,initialized,preserved,(int)xrtErrorKind(xrtGetError()));
    assert(valid && (point!=0 || hit));
    xlir_module_fini();xrtClearError();assert(xrtSignalShutdown() && !xrtGetError());
    assert(!xlir_module_initialized);xlir_module_fini();assert(!xrtGetError());
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("language-signal point=%llu hit=%d balanced=1 outcome=%s callback-diagnostic-failures=%u\n",
        (unsigned long long)point,hit,fallback?"preserved-callback":hit?"memory":"success",preserved);
    return 0;
}
