#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_sync_callback.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_foreign_scope_view.h"
#include <assert.h>
#include <stdio.h>
static void entry(void){}
static int acquire(void* p){++*(unsigned*)p;return 1;}
static void release(void* p){++*(unsigned*)p;}
static void clean_result(xl6_error_result result){assert(result.ok && !result.error);}
int main(void)
{
    assert(xrtMemDebugEnable(true));unsigned hooks=0,callbackHooks=0;
    XL6_CodeOwner owner={sizeof(owner),&hooks,acquire,release},callbackOwner={sizeof(owner),&callbackHooks,acquire,release};
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(&callbackOwner,entry,NULL,NULL,NULL,NULL,NULL,0);assert(closure && callbackHooks==1);
    XL6_SyncCallbackContextV1 context={0};xl6_error_scope outer={0};assert(xl6ErrorScopeBegin(&outer));
    assert(xl6SyncCallbackBeginV1(&context,&owner,closure,7) && xl6CodeOwnerScopeActive(&owner) && xl6CodeOwnerScopeActive(&callbackOwner) && !hooks);
    XL6_SyncCallbackScopeV1 scope={0};assert(xl6SyncCallbackEnterV1(&context,7,&scope));
    XL6_SyncCallbackScopeV1 nested={0};assert(xl6SyncCallbackEnterV1(&context,7,&nested));
    clean_result(xl6SyncCallbackLeaveV1(&nested,(xl6_error_result){true,NULL}));
    XL6_ForeignScopeLifetimeV2 lifetime={0};XL6_ForeignScopeViewV1 view={0},derived={0};int value=42;void* raw=NULL;
    assert(xl6ForeignScopeBeginV2(&lifetime,NULL) && xl6ForeignScopeViewMakeV2(&lifetime,&value,&view));
    assert(xl6ForeignScopeViewDeriveV2(view,&value,&derived) && xl6ForeignScopeViewGetV2(derived,&raw,NULL) && raw==&value);
    assert(xl6ForeignScopeEndV2(&lifetime));assert(!xl6ForeignScopeViewGetV2(view,&raw,NULL));xrtClearError();
    clean_result(xl6SyncCallbackLeaveV1(&scope,(xl6_error_result){true,NULL}));clean_result(xl6SyncCallbackCompleteV1(&context));
    assert(xl6CodeOwnerScopeActive(&callbackOwner));assert(!xl6SyncCallbackEnterV1(&context,7,&scope));xrtClearError();
    clean_result(xl6SyncCallbackEndV1(&context));
    assert(!xl6CodeOwnerScopeActive(&owner) && !xl6CodeOwnerScopeActive(&callbackOwner) && callbackHooks==1 && !hooks);clean_result(xl6ErrorScopeEnd(&outer));
    for(unsigned mode=0;mode<3;++mode){
        assert(xl6ErrorScopeBegin(&outer));assert(xl6SyncCallbackBeginV1(&context,NULL,closure,7));
        if(mode==2){xrtSetErrorKind(XERR_ARGUMENT);xl6ErrorScopeCaptureFailure(&outer);}
        xerror* first=mode ? xrtErrorCreate(XERR_STATE,"sync",17,"first callback") : NULL;
        xl6SyncCallbackReportV1(&context,(xl6_error_result){false,first});
        xl6SyncCallbackReportV1(&context,(xl6_error_result){false,xrtErrorCreate(XERR_RANGE,"sync",18,"later callback")});
        xrtSetErrorKind(XERR_IO);xl6_error_result result=xl6SyncCallbackEndV1(&context);assert(!result.ok);
        if(mode==0)assert(!result.error);else assert(xrtErrorKind(result.error)==(mode==2?XERR_ARGUMENT:XERR_STATE));
        xrtErrorFree(result.error);xrtClearError();result=xl6ErrorScopeEnd(&outer);
        assert(!result.ok);
        if(mode==0)assert(!result.error);else assert(xrtErrorKind(result.error)==(mode==2?XERR_ARGUMENT:XERR_STATE));
        xrtErrorFree(result.error);
    }
    assert(xl6SyncCallbackBeginV1(&context,NULL,closure,7));assert(!xl6SyncCallbackEnterV1(&context,8,&scope));xrtClearError();clean_result(xl6SyncCallbackEndV1(&context));
    assert(xl6SyncCallbackBeginV1(&context,NULL,closure,7));clean_result(xl6SyncCallbackCompleteV1(&context));
    xl6SyncCallbackReportV1(&context,(xl6_error_result){false,NULL});xl6_error_result late=xl6SyncCallbackEndV1(&context);assert(!late.ok && !late.error);
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<1000;++i){
        assert(xl6SyncCallbackBeginV1(&context,NULL,closure,7));assert(xl6SyncCallbackEnterV1(&context,7,&scope));
        clean_result(xl6SyncCallbackLeaveV1(&scope,(xl6_error_result){true,NULL}));clean_result(xl6SyncCallbackEndV1(&context));
    }
    assert(xrtMemDebugFailAfter(-1));xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    xl6ClosureRuntimeV1Get()->Release(closure);assert(!xrtGetError() && callbackHooks==2 && !hooks);puts("sync runtime: checked first failure, rootless epoch, code activity, 1000 allocation-free calls under OOM");return 0;
}
