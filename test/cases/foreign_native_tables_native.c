#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_native_graph_future_payload.h"
#include "src/runtime/xl6_foreign_view_value.h"
#include <assert.h>
static int body(void* environment,int value){return *(const unsigned char*)environment+value;}
static bool destroy_root(void* raw){xrtFree(raw);return true;}
int foreign_native_tables_probe(const XL6_NativeCallbackRuntimeV1* callbacks,
    const XL6_ClosureRuntimeV3* closures,const XL6_NativeCaptureProfileV1* view,
    const XL6_NativeFuturePayloadRuntimeV1* futures,const XL6_OwnershipDomainV1* values)
{
    assert(callbacks && closures && view && futures && futures->Allocate && futures->Copy && values && values->Resolve);
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
    assert(callbacks->DropNative(record) && !xl6ForeignCallbackActiveV1());
    static const XL6_ForeignViewDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_VIEW_V1_TAG,"tables.root",
        0,1,NULL,NULL,destroy_root,0,1,NULL,NULL,NULL,NULL};
    void* raw=xrtCalloc(1,1);assert(raw);
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,NULL);assert(root);
    XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(root,&call,NULL,0));
    XL6_ForeignView* owner=xl6ForeignViewBorrowRootV1(raw,root);assert(owner==root);
    xvalue* boxed=xl6ForeignViewBoxV1(root,778);assert(boxed);
    XL6_ForeignView* copy=NULL;assert(xl6ForeignViewUnboxV1(boxed,778,false,&copy) && copy==root);
    assert(xl6ForeignViewCallEndV1(&call) && xl6ForeignViewReleaseV1(copy) && xl6ForeignViewReleaseV1(owner));
    assert(xl6ForeignViewReleaseV1(root));xrtValueRelease(boxed);assert(xl6ForeignViewDrainV1());return 0;
}
