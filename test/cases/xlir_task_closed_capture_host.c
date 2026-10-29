#define XRT_DECLARATIONS 1
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_closure.h"
#include "xlir_task_closed_capture.c"
#include "xlir_task_closed_capture.h.bind.h"
#include <assert.h>
#include <stdio.h>
/* The verified language closure ABI returns status and writes its value;
 * this is not the raw native callback ABI. */
static bool seven(void* environment,int64_t* result)
{assert(!environment&&result);*result=7;return true;}
int64_t closed_capture_touch(int64_t mode){if(mode)xrtSetErrorKind(XERR_STATE);return 11;}
xfuture* closed_capture_source(void)
{xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);xrtPromiseDestroy(promise);return source;}
static unsigned runs,failures,rollbacks;
static void transaction(int mode,int fault)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);assert(xlir_module_init());
    xlir_string text={xrtStrDup("capture"),7};xvalue* value=xrtValueInt(42);xfuture* source=NULL;
    xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);xrtPromiseDestroy(promise);
    XL6_Closure* callback=xl6ClosureRuntimeV1Get()->Create(NULL,(void(*)(void))seven,NULL,NULL,NULL,NULL,NULL,0,NULL);
    xerror* error=xrtErrorCreate(XERR_STATE,"input",53,"retained input");assert(text.Data&&value&&source&&callback&&error);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    if(fault>=0)assert(xrtMemDebugFailAfter((uint64_t)fault));
    xlir_result_future result=CLOSED_CAPTURE_START(mode,text,value,source,callback,error);
    if(!result.ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    bool triggered=xrtMemDebugFailTriggered();if(fault>=0)xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(result.ok==(result.value!=NULL)&&outcome.ok==result.ok);
    xrtFree((void*)text.Data);xrtValueRelease(value);xrtFutureDestroy(source);xl6ClosureRuntimeV1Get()->Release(callback);xrtErrorFree(error);
    if(!result.ok){assert(triggered&&outcome.error&&xrtErrorKind(outcome.error)==XERR_MEMORY);++rollbacks;}
    else{
        assert(xrtFutureWait(result.value)==0);
        if((mode&&xrtFutureState(result.value)!=XFUTURE_FAILED)||
            (!mode&&xrtFutureState(result.value)!=XFUTURE_RESOLVED))
            fprintf(stderr,"Closed capture mode=%d fault=%d triggered=%d state=%d error=%d\n",
                mode,fault,(int)triggered,(int)xrtFutureState(result.value),
                (int)xrtErrorKind(xrtFutureError(result.value)));
        if(mode){assert(xrtFutureState(result.value)==XFUTURE_FAILED&&xrtErrorKind(xrtFutureError(result.value))==XERR_STATE);++failures;}
        else{assert(xrtFutureState(result.value)==XFUTURE_RESOLVED&&*(int64_t*)xrtFutureValue(result.value)==11);++runs;}
    }
    xrtErrorFree(outcome.error);xlir_module_fini();assert(!xrtGetError());xrtFutureDestroy(result.value);
    xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&before.InvalidFreeCount==after.InvalidFreeCount&&
        before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    assert(CLOSED_CAPTURE_LAYOUT.count==6);for(unsigned i=0;i<6;++i){
        assert(CLOSED_CAPTURE_LAYOUT.slots[i].value.value.kind==(XL6_CaptureKind)(i+1));
        assert(!CLOSED_CAPTURE_LAYOUT.slots[i].value.native);
    }
    for(unsigned n=0;n<50;++n){transaction(0,-1);transaction(1,-1);}
    for(int fault=0;fault<24;++fault)transaction(0,fault);
    assert(runs>=50&&failures==50&&rollbacks);
    printf("Generated closed async captures: 100 success/failure transactions and 24 OOM budgets; actual six-slot CaptureV2 layout, pre-publication cleanup and balanced allocations\n");return 0;
}
