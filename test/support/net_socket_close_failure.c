/* Controlled close-result adapter AFTER a real, once-only native close. This
 * tests ownership consumption/error reporting, not OS handle misuse or probes. */
#include "net-socket-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
static unsigned closes,mode;
static bool forcedClose(xnetsocket native)
{
    ++closes;assert(xrtNetSocketClose(native));
    if(mode==1)xrtSetErrorKind(XERR_IO);
    else xrtClearError();
    return false;
}
#define xrtNetSocketClose forcedClose
#include "src/runtime/xl6_net_socket_owner.c"
#undef xrtNetSocketClose
static void balance(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveBytes==after.LiveBytes&&before->LiveCount==after.LiveCount&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static void freeze(xrtownershipscope* scope)
{while(!xrtOwnershipFreezeTryBegin(scope))xrtThreadYield();}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_IO);xrtClearError();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    for(mode=1;mode<=2;++mode){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned n=closes;
        Owner* owner=xl6NetSocketOwnerOpenV1(4,1,true);assert(owner);
        assert(!xl6NetSocketOwnerCloseV1(owner)&&closes==n+1&&xl6NetSocketOwnerClosedV1(owner));
        assert(mode==1?xrtErrorKind(xrtGetError())==XERR_IO:!xrtGetError());xrtClearError();
        assert(xl6NetSocketOwnerCloseV1(owner)&&closes==n+1);xl6NetSocketOwnerReleaseV1(owner);balance(&before);
        xrtMemDebugSnapshot(&before);owner=xl6NetSocketOwnerOpenV1(4,2,true);assert(owner);n=closes;
        xl6_error_scope cleanup={0};assert(xl6ErrorScopeBegin(&cleanup));xl6NetSocketOwnerReleaseV1(owner);
        xl6_error_result result=xl6ErrorScopeEnd(&cleanup);assert(!result.ok&&closes==n+1);
        assert(mode==1?result.error!=NULL:result.error==NULL);xrtErrorFree(result.error);balance(&before);
        for(unsigned first=0;first<2;++first){
            xrtMemDebugSnapshot(&before);owner=xl6NetSocketOwnerOpenV1(4,1,true);assert(owner);n=closes;
            xrtownershipref ref=xl6NetSocketOwnershipV1(owner);xrtownershipscope frozen={0};freeze(&frozen);
            const xrtownershipadapterv1* a=xl6NetSocketOwnershipAdapterV1(ref);int token=0;
            assert(a&&a->Hold(owner)&&a->Claim(owner,&token));a->Clear(owner,&token);assert(xrtOwnershipScopeEnd(&frozen));
            if(first)xl6NetSocketOwnerReleaseV1(owner);
            assert(!a->Finish(owner,&token)&&closes==n+1);xrtClearError();
            assert(a->Finish(owner,&token)&&closes==n+1);
            if(!first)xl6NetSocketOwnerReleaseV1(owner);
            a->Drop(owner);balance(&before);
        }
    }
    puts("Socket close: 8 consumed error/NULL-error paths, repeated close/Finish and owner orders, no retry of consumed native handle");return 0;
}
