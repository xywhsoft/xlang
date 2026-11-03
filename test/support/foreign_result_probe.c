#include "foreign_view_native_fixture.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_foreign_view_abi.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_native_frame_graph.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_text_value.h"
typedef struct AccessWorker {xvalue* value;XL6_NativeHandleAccessV1* scope;} AccessWorker;
static int32 access_other_thread(void* raw)
{
    AccessWorker* input=raw;XL6_NativeHandleAccessV1 access={0};
    assert(!xl6NativeHandleAccessEndV1(input->scope));xrtClearError();
    assert(!xl6NativeHandleAccessBeginV1(input->value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_CALL_V1,&access));
    xrtClearError();return 0;
}
static void native_access(void)
{
    void* raw=xl6BufferCreateReserveV1(16);assert(raw);
    assert(xrtBufferAppend(raw,(xbytesview){(const uint8_t*)"abcd",4}));
    xvalue* value=xl6NativeHandleTakeV1(&raw,xl6BufferHandlePolicyV1Get());assert(value && !raw);
    XL6_NativeHandleAccessV1 first={0},second={0};XL6_BufferSpanV1 span={0},read={0};
    assert(xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_CALL_V1,&first));
    assert(xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_CALL_V1,&second));
    assert(!xl6BufferSpanBeginV1(value,true,&span));xrtClearError();
    assert(xl6NativeHandleAccessEndV1(&first) && xl6NativeHandleAccessEndV1(&second));
    assert(xl6BufferSpanBeginV1(value,true,&span) && span.bytes==4);
    assert(xl6BufferSpanBeginV1(value,true,&read));
    assert(!xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_TAKE_V1,&first));xrtClearError();
    assert(xl6BufferSpanEndV1(&span) && xl6BufferSpanEndV1(&read));
    assert(xl6BufferSpanBeginV1(value,false,&span));
    XL6_BufferSpanV1 copy=span;assert(!xl6BufferSpanEndV1(&copy));xrtClearError();
    assert(!xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_CALL_V1,&first));xrtClearError();
    AccessWorker worker={value,&span.access};xthread* thread=xrtThreadCreate(access_other_thread,&worker,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    ((unsigned char*)span.data)[0]='z';assert(xl6BufferSpanEndV1(&span));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<200;++i) {
        assert(xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_CALL_V1,&first));
        assert(xl6NativeHandleAccessEndV1(&first));
        assert(xl6BufferSpanBeginV1(value,true,&span) && ((char*)span.data)[0]=='z');
        assert(xl6BufferSpanEndV1(&span));
    }
    assert(xrtMemDebugFailAfter(-1));xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    assert(xl6NativeHandleAccessBeginV1(value,xl6BufferHandlePolicyV1Get(),XL6_NATIVE_HANDLE_ACCESS_TAKE_V1,&first));
    assert(xrtValueGetHandle(value,&raw,NULL,NULL) && !raw);
    xl6BufferDestroyV1(first.raw);assert(xl6NativeHandleAccessEndV1(&first));xrtValueRelease(value);
    raw=xl6BufferCreateReserveV1(8);assert(raw && xrtBufferAppend(raw,(xbytesview){(const uint8_t*)"test",4}));
    value=xl6NativeHandleTakeV1(&raw,xl6BufferHandlePolicyV1Get());assert(value);
    assert(xl6BufferSpanBeginV1(value,false,&span));xrtValueRelease(value);
    assert(span.bytes==4 && !memcmp(span.data,"test",4));assert(xl6BufferSpanEndV1(&span));
    assert(!xrtGetError());
}

typedef struct Captured {XL6_ForeignView* view;unsigned char live;} Captured;
typedef struct Continued {
    Captured capture;XL6_ContinuationTicket ticket;xfuturewatch watch;xfuture* source;xpromise* outer;
} Continued;
static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Captured,view),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get};
static const XL6_CaptureLayoutV2 captures={sizeof(captures),sizeof(Captured),1,&slot};
static const XL6_ActivationCaptureSlotV2 activeSlot={{{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Captured,view),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get},offsetof(Captured,live)};
static const size_t cleanup[]={0};
static const XL6_ActivationCaptureLayoutV2 active={sizeof(active),sizeof(Captured),1,&activeSlot,1,cleanup};
static const XL6_ActivationCaptureLayoutV2 continued={sizeof(continued),sizeof(Continued),1,&activeSlot,1,cleanup};
static const XL6_ContinuationFrameLayoutV3 transport={sizeof(transport),&continued,offsetof(Continued,ticket),offsetof(Continued,watch),offsetof(Continued,source),offsetof(Continued,outer)};
static const XL6_CaptureSlotV2 itemSlot={{XL6_CAPTURE_NATIVE_GRAPH_V3,0,sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get};
static const XL6_CaptureLayoutV2 item={sizeof(item),sizeof(XL6_ForeignView*),1,&itemSlot};
static XL6_TaskOutcome task(xcancel* cancel,void* data,XL6_TaskResult* result)
{(void)cancel;(void)data;(void)result;return XL6_TASK_SUCCESS;}
static void callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{(void)input;(void)output;(void)cancel;(void)data;}
static bool resume(XL6_Generator* generator)
{xl6GeneratorRuntimeV1Get()->Finish(generator);return false;}
static void graph_frames(void)
{
    Native* raw=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    assert(!xl6TaskRuntimeV3Get()->Allocate(NULL,sizeof(Captured),task,&captures));xrtClearError();
    Captured* a=xl6TaskFrameAllocateGraphV1(NULL,sizeof(Captured),task,&captures);assert(a);
    *a=(Captured){xl6ForeignViewRetainV1(root),1};xl6TaskRuntimeV1Get()->Discard(a);
    assert(!xl6FutureCallbackRuntimeV5Get()->Allocate(NULL,sizeof(Captured),callback,&captures));xrtClearError();
    a=xl6CallbackFrameAllocateGraphV1(NULL,sizeof(Captured),callback,&captures);assert(a);
    *a=(Captured){xl6ForeignViewRetainV1(root),1};xl6FutureCallbackRuntimeV1Get()->Discard(a);
    assert(!xl6ContinuationFrameRuntimeV5Get()->Allocate(sizeof(Continued),&transport));xrtClearError();
    Continued* b=xl6ContinuationFrameAllocateGraphV1(sizeof(Continued),&transport);assert(b);
    b->capture=(Captured){xl6ForeignViewRetainV1(root),1};xl6ContinuationFrameRuntimeV2Get()->Discard(b);
    assert(!xl6GeneratorRuntimeV3Get()->FrameAllocate(sizeof(Captured),&active));xrtClearError();
    a=xl6GeneratorFrameAllocateGraphV1(sizeof(Captured),&active);assert(a);
    *a=(Captured){xl6ForeignViewRetainV1(root),1};
    XL6_Generator* gen=xl6GeneratorCreateGraphV1(NULL,a,resume,&item);assert(gen);
    xl6GeneratorRuntimeV1Get()->Destroy(gen);
    xl6ForeignViewDropValueV1(root);
    assert(xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0) && !xrtGetError());
}
static void scoped_unobserved_close(void)
{
    Native* raw=native_create();
    XL6_ForeignViewDescriptorV1 policy=rootDescriptor;
    policy.Watch=NULL;policy.Unwatch=NULL;policy.watchBytes=0;policy.watchAlignment=1;
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&policy,NULL);assert(root);
    NativeScope native={0};XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
    assert(xl6ForeignViewCloseV1(root));
    assert(xl6ForeignViewCallEndV1(&call));
    assert(!xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));xrtClearError();
    assert(xl6ForeignViewCloseV1(root) && xl6ForeignViewReleaseV1(root) && xl6ForeignViewDrainV1());
}
typedef struct AdmissionReentry {Native* raw;unsigned references,calls;} AdmissionReentry;
static int admission_acquire(void* data)
{
    AdmissionReentry* state=data;++state->references;++state->calls;
    Native* raw=state->raw;state->raw=NULL;assert(raw && destroy_native(raw));return 1;
}
static void admission_release(void* data)
{AdmissionReentry* state=data;assert(state->references==2);--state->references;}
static void admission_reentry(void)
{
    AdmissionReentry state={native_create(),1,0};
    Native* output=state.raw;XL6_ForeignResultProbeV1 probe={0};Watch storage={0};
    XL6_CodeOwner owner={sizeof(owner),&state,admission_acquire,admission_release};
    assert(xl6ForeignResultProbePrepareV1(&probe,&rootDescriptor,true,&storage,sizeof(storage)));
    assert(xl6ForeignResultProbeObserveV1(&probe,output));
    assert(!xl6ForeignViewCreateRootProbedV1(output,&probe,&owner));xrtClearError();
    assert(state.calls==1 && state.references==1 && !state.raw && !xl6ForeignResultProbeAliveV1(&probe));
    assert(xl6ForeignResultProbeEndV1(&probe) && !storage.slot);
    assert(xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0) && !xrtGetError());
}
typedef struct ClosingAdmission {XL6_ForeignView* root;unsigned references,calls;} ClosingAdmission;
static int closing_acquire(void* data)
{ClosingAdmission* state=data;++state->references;++state->calls;assert(!xl6ForeignViewCloseV1(state->root));return 1;}
static void closing_release(void* data)
{ClosingAdmission* state=data;assert(state->references==2);--state->references;}
static void admission_parent_close(void)
{
    Native* raw=native_create();Native* child=native_child(raw);
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    NativeScope native={0};XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
    raw->failures=1;raw->failWithoutDiagnostic=true;
    ClosingAdmission state={root,1,0};XL6_CodeOwner owner={sizeof(owner),&state,closing_acquire,closing_release};
    assert(!xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&owner));xrtClearError();
    assert(state.references==1 && state.calls==1 && !child->watch);
    assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(root) && xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0) && !xrtGetError());
}
static void deferred_callbacks(void)
{
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    uint64_t count=0;
    xl6ForeignViewRecordCallbackFailureV1(root,(xl6_error_result){false,NULL});
    xl6ForeignViewRecordCallbackFailureV1(root,(xl6_error_result){false,NULL});
    assert(xl6ForeignViewCallbackFailureCountV1(root,&count) && count==2);
    XL6_ForeignViewCallV1 call={0};NativeScope scope={0};
    assert(!xl6ForeignViewCallBeginV1(root,&call,&scope,sizeof(scope)) && !xrtGetError());
    assert(xl6ForeignViewCallBeginV1(root,&call,&scope,sizeof(scope)) && xl6ForeignViewCallEndV1(&call));
    xerror* first=xrtErrorCreate(XERR_ARGUMENT,"callback",1,"first");
    xerror* second=xrtErrorCreate(XERR_STATE,"callback",2,"second");assert(first && second);
    xl6ForeignViewRecordCallbackFailureV1(root,(xl6_error_result){false,first});
    xl6ForeignViewRecordCallbackFailureV1(root,(xl6_error_result){false,second});
    assert(!xl6ForeignViewThrowIfCallbackFailedV1(root) && xrtTakeError()==first);xrtErrorFree(first);
    assert(xl6ForeignViewThrowIfCallbackFailedV1(root));
    assert(xl6ForeignViewCallbackFailureCountV1(root,&count) && count==4);
    xl6ForeignViewRecordCallbackFailureV1(root,(xl6_error_result){false,NULL});
    assert(!xl6ForeignViewCloseV1(root) && !xrtGetError());
    assert(xl6ForeignViewReleaseV1(root));
    raw=native_create();root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    xl6ForeignViewRecordCallbackFailureV1(root,(xl6_error_result){false,NULL});
    assert(!xl6ForeignViewReleaseV1(root) && !xrtGetError());
    assert(xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0));
}
static int32 wrong_thread(void* raw)
{
    XL6_ForeignResultProbeV1* probe=raw;
    assert(!xl6ForeignResultProbeAliveV1(probe));
    assert(!xl6ForeignResultProbeEndV1(probe));xrtClearError();return 0;
}
int main(void)
{
    XL6_ForeignResultProbeV1 probe={0};Watch storage={0};
    assert(!xl6ForeignResultProbePrepareV1(&probe,&rootDescriptor,true,&storage,sizeof(storage)-1));xrtClearError();
    assert(!probe.self && !storage.slot);
    Native* raw=native_create();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignResultProbePrepareV1(&probe,&rootDescriptor,true,&storage,sizeof(storage)));
    assert(!xl6ForeignResultProbePrepareV1(&probe,&rootDescriptor,true,&storage,sizeof(storage)));xrtClearError();
    assert(xl6ForeignResultProbeObserveV1(&probe,raw));
    assert(xl6ForeignResultProbeAliveV1(&probe));
    XL6_ForeignResultProbeV1 copied=probe;
    assert(!xl6ForeignResultProbeEndV1(&copied));xrtClearError();
    assert(!xl6ForeignResultProbeObserveV1(&probe,raw));xrtClearError();
    assert(xl6ForeignResultProbeEndV1(&probe) && xl6ForeignResultProbeEndV1(&probe));
    assert(xrtMemDebugFailAfter(-1));xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    assert(!raw->watch && destroy_native(raw));
    raw=native_create();
    assert(xl6ForeignResultProbePrepareV1(&probe,&rootDescriptor,true,&storage,sizeof(storage)));
    assert(xl6ForeignResultProbeObserveV1(&probe,raw));
    xthread* thread=xrtThreadCreate(wrong_thread,&probe,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(destroy_native(raw));
    assert(!xl6ForeignResultProbeAliveV1(&probe) && xl6ForeignResultProbeEndV1(&probe));
    assert(xl6ForeignResultProbePrepareV1(&probe,&rootDescriptor,true,&storage,sizeof(storage)));
    assert(xl6ForeignResultProbeObserveV1(&probe,NULL));
    assert(!xl6ForeignResultProbeAliveV1(&probe) && xl6ForeignResultProbeEndV1(&probe));
    raw=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    XL6_ForeignView* copy=NULL;assert(xl6ForeignViewCopyValueV1(root,&copy) && copy==root);
    assert(!xl6ForeignViewCopyValueV1(root,&copy));xrtClearError();
    assert(xl6ForeignViewEqualValueV1(root,copy) && xl6ForeignViewCloseV1(root));
    XL6_ForeignView* closed=NULL;assert(xl6ForeignViewCopyValueV1(root,&closed));
    xl6ForeignViewDropValueV1(closed);xl6ForeignViewDropValueV1(copy);xl6ForeignViewDropValueV1(root);
    assert(xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0) && !xrtGetError());
    scoped_unobserved_close();graph_frames();native_access();deferred_callbacks();admission_reentry();admission_parent_close();puts("foreign result probe and graph frames: passed");return 0;
}
