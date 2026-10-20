#include "net-engine-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_execution_language.h"
#include "src/api/xl_foreign_error.h"
#define XL6_TEXT_API
#include "src/runtime/xl6_net_engine_abi.h"
#include <assert.h>
#include <stdio.h>
static void balance(const xmemdebugsnapshot* before)
{xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
    after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));
    xl6_error_result clean=xl6ErrorScopeEnd(&warm);assert(clean.ok);xrtErrorFree(clean.error);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* original=xl6NetEngineConfigCreateV1();assert(original&&xl6NetEngineConfigWorkersV1(original)==0);
    void* copy=xl6NetEngineConfigWithWorkersV1(original,2);assert(copy&&xl6NetEngineConfigWorkersV1(copy)==2&&xl6NetEngineConfigWorkersV1(original)==0);
    xl6NetEngineConfigDestroyV1(original);
    uint64_t bytes=xl6NetEngineConfigBufferBlockSizeV1(copy,0),limit=xl6NetEngineConfigBufferCacheLimitV1(copy,0);
    void* buffers=xl6NetEngineConfigWithBufferClassV1(copy,0,bytes,limit+1);assert(buffers&&xl6NetEngineConfigBufferCacheLimitV1(buffers,0)==limit+1);
    xl6NetEngineConfigDestroyV1(copy);assert(xl6NetEngineConfigWorkersV1(buffers)==2&&xl6NetEngineConfigBufferBlockSizeV1(buffers,0)==bytes);
    for(unsigned mode=0;mode<7;++mode){
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));void* invalid=NULL;
        if(mode==0)invalid=xl6NetEngineConfigWithWorkersV1(buffers,257);
        else if(mode==1)invalid=xl6NetEngineConfigWithIdleWaitV1(buffers,0);
        else if(mode==2)invalid=xl6NetEngineConfigWithIdleWaitV1(buffers,INT64_MAX);
        else if(mode==3)invalid=xl6NetEngineConfigWithCommandCapacityV1(buffers,1);
        else if(mode==4)invalid=xl6NetEngineConfigWithBufferBudgetV1(buffers,0);
        else if(mode==5)invalid=xl6NetEngineConfigWithBufferClassV1(buffers,4,bytes,1);
        else invalid=xl6NetEngineConfigWithBufferClassV1(buffers,0,0,1);
        xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(!invalid&&!result.ok);xrtErrorFree(result.error);
        assert(xl6NetEngineConfigWorkersV1(buffers)==2&&xl6NetEngineConfigBufferCacheLimitV1(buffers,0)==limit+1);
    }
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(0));
    assert(!xl6NetEngineConfigWithWorkersV1(buffers,3)&&xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    xl6_error_result failed=xl6ErrorScopeEnd(&scope);assert(!failed.ok);xrtErrorFree(failed.error);
    xl6NetEngineConfigDestroyV1(buffers);balance(&before);
    puts("Engine options: independent immutable storage, native validation, exact finite/infinite units, seven checked rejections and OOM balance");return 0;
}
