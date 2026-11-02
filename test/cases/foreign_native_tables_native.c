#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_native_graph_future_payload.h"
#include <assert.h>
static int body(void* environment,int value){return *(const unsigned char*)environment+value;}
int foreign_native_tables_probe(const XL6_NativeCallbackRuntimeV1* callbacks,
    const XL6_ClosureRuntimeV3* closures,const XL6_NativeCaptureProfileV1* view,
    const XL6_NativeFuturePayloadRuntimeV1* futures)
{
    assert(callbacks && closures && view && futures && futures->Allocate && futures->Copy);
    assert(view->size>sizeof(*view) && !view->Type && !view->DetachCleanup);
    static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_TRIVIAL,0,1},NULL};
    static const XL6_CaptureLayoutV2 layout={sizeof(layout),1,1,&slot};
    unsigned char* environment=xrtMalloc(1);assert(environment);*environment=3;
    XL6_Closure* closure=closures->Create(NULL,(void(*)(void))body,NULL,environment,1,&layout,NULL,NULL,0);assert(closure);
    XL6_NativeCallback* record=callbacks->Create(NULL,closure,NULL,987);assert(record);
    xl6ClosureRuntimeV1Get()->Release(closure);
    assert(callbacks->HoldNative(record) && callbacks->Release(record));
    XL6_NativeCallbackScopeV1 scope={0};assert(callbacks->Enter(record,987,&scope,NULL,0));
    int result=((int(*)(void*,int))scope.closure.closure->invoke)(scope.closure.closure->environment,32);
    xl6_error_result outcome=callbacks->Leave(&scope,(xl6_error_result){true,NULL});
    assert(outcome.ok && !outcome.error && result==35);
    assert(callbacks->DropNative(record) && !xl6ForeignCallbackActiveV1());return 0;
}
