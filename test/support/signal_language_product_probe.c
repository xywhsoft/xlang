/* Transparent consumer call-site observation. The actual product is called
 * exactly once; no allocator, callback, error or provider is substituted.
 * An optional callback diagnostic must not replace the original exception. */
#include "xruntime_decl.h"
#include <assert.h>
#include <string.h>
#ifndef XL6_SIGNAL_CALLBACK_EXPECTED
#define XL6_SIGNAL_CALLBACK_EXPECTED 0
#endif

extern void __real_xl6ForeignCallbackFail(void);
extern bool __real_xrtValueInvoke(const xvalue*,const xrtcallframe*,xrtcallresult*);
static _Thread_local unsigned preservedDiagnostics;
typedef struct Invocation {
    const xerror* original;
    const xerror* diagnostic;
    bool triggeredAtCallbackExit;
} Invocation;
static _Thread_local Invocation* invocation;
static void text(xstrview actual,const char* expected)
{size_t size=strlen(expected);assert(actual.Size==size && !memcmp(actual.Data,expected,size));}
static void originalError(const xerror* original)
{
    assert(original && (XL6_SIGNAL_CALLBACK_EXPECTED==1 || XL6_SIGNAL_CALLBACK_EXPECTED==2));
    assert(xrtErrorKind(original)==(XL6_SIGNAL_CALLBACK_EXPECTED==1?XERR_RANGE:XERR_STATE));
    text(xrtErrorDomainView(original),"xlang");text(xrtErrorOperationView(original),"throw");
    text(xrtErrorDataView(original),XL6_SIGNAL_CALLBACK_EXPECTED==1?"RangeError":"RuntimeError");
    text(xrtErrorMessageView(original),XL6_SIGNAL_CALLBACK_EXPECTED==1?
        "original signal handler failure":"signal route channel is closed");
    assert(xrtErrorLine(original)>0 && xrtErrorColumn(original)>0 && !xrtErrorCause(original));
}
static void preserved(void)
{
#if !defined(XL6_SIGNAL_SKIP_DIAGNOSTIC_WITNESS)
    ++preservedDiagnostics;
#endif
}

void __wrap_xl6ForeignCallbackFail(void)
{
    const xerror* original=xrtGetError();bool prior=xrtMemDebugFailTriggered();
    __real_xl6ForeignCallbackFail();
    if(invocation){
        invocation->original=original;invocation->diagnostic=xrtGetError();
        invocation->triggeredAtCallbackExit=xrtMemDebugFailTriggered();
    }
    if(!prior && xrtMemDebugFailTriggered()){
        /* Identity also proves the immutable payload, cause and location were
         * not replaced by a newly constructed MemoryError or a fake success. */
        assert(original && xrtGetError()==original);
        originalError(original);preserved();
    }
}
bool __wrap_xrtValueInvoke(const xvalue* value,const xrtcallframe* frame,xrtcallresult* result)
{
    Invocation current={0};Invocation* previous=invocation;invocation=&current;
    bool prior=xrtMemDebugFailTriggered();unsigned before=preservedDiagnostics;
    bool success=__real_xrtValueInvoke(value,frame,result);invocation=previous;
    /* XRT may allocate its own call-context diagnostic AFTER the language
     * callback returned. Only a newly triggered failure after that boundary,
     * with the identical native callback diagnostic, is this second fallback.
     * Ordinary argument/body OOM has no such witness and must still escape. */
    if(!prior && xrtMemDebugFailTriggered() && preservedDiagnostics==before &&
        current.original && !current.triggeredAtCallbackExit){
        assert(!success && current.diagnostic && xrtGetError()==current.diagnostic);
        originalError(current.original);
        assert(xrtErrorKind(current.diagnostic)==xrtErrorKind(current.original));
        text(xrtErrorOperationView(current.diagnostic),"callback");
        text(xrtErrorMessageView(current.diagnostic),XL6_SIGNAL_CALLBACK_EXPECTED==1?
            "original signal handler failure":"signal route channel is closed");
        preserved();
    }
    return success;
}
unsigned testSignalPreservedDiagnosticFailures(void)
{return preservedDiagnostics;}
