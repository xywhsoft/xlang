#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_foreign_context.h"
#if defined(_WIN32)
#include <windows.h>
#endif
static void scopes(void)
{
    assert(xl6ForeignContextReadyV1() && !xl6ForeignCallbackActiveV1());
    XL6_ForeignCallbackScopeV1 a={0},b={0};
    assert(xl6ForeignCallbackEnterV1(&a) && xl6ForeignCallbackEnterV1(&b));
    assert(!xl6ForeignCallbackLeaveV1(&a));xrtClearError();
    XL6_ForeignCallbackScopeV1 copied=b;assert(!xl6ForeignCallbackLeaveV1(&copied));xrtClearError();
    assert(xl6ForeignCallbackLeaveV1(&b) && xl6ForeignCallbackLeaveV1(&a));
    assert(!xl6ForeignCallbackActiveV1());
    XL6_ForeignCallContextV1 x={0},y={0},z={0};
    assert(xl6ForeignContextCallEnterV1(&x,&x) && xl6ForeignContextCallEnterV1(&y,NULL) && xl6ForeignContextCallEnterV1(&z,NULL));
    assert(xl6ForeignContextCallOwnsV1(&x) && !xl6ForeignContextCallOwnsV1(&y));
    assert(xl6ForeignContextCallContainsV1(&x) && xl6ForeignContextCallContainsV1(&y));
    XL6_ForeignCallContextV1 bad=y;assert(!xl6ForeignContextCallContainsV1(&bad));
    xl6ForeignContextCallLeaveV1(&y);xl6ForeignContextCallLeaveV1(&x);xl6ForeignContextCallLeaveV1(&z);
    assert(!xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1));
    Native* raw=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    NativeScope nx={0},ny={0};XL6_ForeignViewCallV1 cx={0},cy={0};
    assert(xl6ForeignViewCallBeginV1(root,&cx,&nx,sizeof(nx)) && xl6ForeignViewCallBeginV1(root,&cy,&ny,sizeof(ny)));
    assert(raw->depth==2 && xl6ForeignViewCallEndV1(&cx) && raw->depth==1);
    assert(xl6ForeignViewCallEndV1(&cy) && !raw->depth && xl6ForeignViewReleaseV1(root));
}
static int32 worker(void* raw)
{
    XL6_ForeignCallbackScopeV1* outer=raw;
    assert(!xl6ForeignCallbackActiveV1() && !xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1));
    assert(!xl6ForeignCallbackLeaveV1(outer));xrtClearError();
    for(unsigned i=0;i<1000;++i){
        XL6_ForeignCallbackScopeV1 callback={0};XL6_ForeignCallContextV1 call={0};
        assert(xl6ForeignCallbackEnterV1(&callback) && xl6ForeignContextCallEnterV1(&call,NULL));
        assert(xl6ForeignCallbackActiveV1() && xl6ForeignContextCallContainsV1(&call));
        xl6ForeignContextCallLeaveV1(&call);assert(xl6ForeignCallbackLeaveV1(&callback));
    }
    return 0;
}
static void threads(void)
{
    XL6_ForeignCallbackScopeV1 callback={0};XL6_ForeignCallContextV1 call={0};
    assert(xl6ForeignCallbackEnterV1(&callback) && xl6ForeignContextCallEnterV1(&call,NULL));
    xthread* workers[4];for(unsigned i=0;i<4;++i){workers[i]=xrtThreadCreate(worker,&callback,0);assert(workers[i]);}
    for(unsigned i=0;i<4;++i){assert(xrtThreadWait(workers[i])==XWAIT_OK);xrtThreadDestroy(workers[i]);}
    assert(xl6ForeignCallbackActiveV1() && xl6ForeignContextCallContainsV1(&call));
    xl6ForeignContextCallLeaveV1(&call);assert(xl6ForeignCallbackLeaveV1(&callback));
}
#if defined(_WIN32)
typedef struct FiberState {
    void* mainFiber;Native* native;XL6_ForeignView* view;
    XL6_ForeignViewCallV1 call;NativeScope nativeCall;XL6_ForeignCallbackScopeV1 callback;
    bool ran;
} FiberState;
static VOID WINAPI other_fiber(void* data)
{
    FiberState* state=data;
    assert(!xl6ForeignCallbackActiveV1() && !xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1));
    assert(!xl6ForeignContextCallContainsV1(&state->call.execution));
    assert(!xl6ForeignViewBorrowChildV1(state->native->children,state->view,&childDescriptor,NULL));xrtClearError();
    assert(!xl6ForeignCallbackLeaveV1(&state->callback));xrtClearError();
    assert(!xl6ForeignViewCallEndV1(&state->call) && state->native->depth==1);xrtClearError();
    NativeScope native={0};XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(state->view,&call,&native,sizeof(native)) && state->native->depth==2);
    assert(xl6ForeignViewCallEndV1(&call) && state->native->depth==1);
    assert(xl6ForeignViewDrainV1());state->ran=true;SwitchToFiber(state->mainFiber);abort();
}
#endif
static void fibers(void)
{
#if defined(_WIN32)
    FiberState state={0};state.mainFiber=ConvertThreadToFiber(NULL);assert(state.mainFiber);
    state.native=native_create();native_child(state.native);
    state.view=xl6ForeignViewCreateRootV1(state.native,&rootDescriptor,NULL);assert(state.view);
    assert(xl6ForeignViewCallBeginV1(state.view,&state.call,&state.nativeCall,sizeof(state.nativeCall)));
    assert(xl6ForeignCallbackEnterV1(&state.callback));
    void* fiber=CreateFiber(65536,other_fiber,&state);assert(fiber);SwitchToFiber(fiber);assert(state.ran);
    assert(xl6ForeignCallbackActiveV1() && state.native->depth==1);
    assert(xl6ForeignCallbackLeaveV1(&state.callback) && xl6ForeignViewCallEndV1(&state.call));
    DeleteFiber(fiber);assert(xl6ForeignViewReleaseV1(state.view));assert(ConvertFiberToThread());
    puts("distinct Windows fibers preserve callback/native-call stacks and reject cross-fiber End");
#else
    puts("POSIX uses the thread execution context; Windows fiber case is platform specific");
#endif
}
static void allocation(void)
{
    XL6_ForeignCallbackScopeV1 callback={0};XL6_ForeignCallContextV1 call={0};
    /* Prime OS slots before measuring steady-state language allocator costs. */
    assert(xl6ForeignCallbackEnterV1(&callback) && xl6ForeignContextCallEnterV1(&call,NULL));
    xl6ForeignContextCallLeaveV1(&call);assert(xl6ForeignCallbackLeaveV1(&callback));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<1000;++i){
        assert(xl6ForeignCallbackEnterV1(&callback) && xl6ForeignContextCallEnterV1(&call,NULL));
        xl6ForeignContextCallLeaveV1(&call);assert(xl6ForeignCallbackLeaveV1(&callback));
    }
    xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct{const char* name;void(*run)(void);} cases[]={ {"scopes",scopes},{"threads",threads},{"fibers",fibers},{"allocation",allocation} };
    bool found=argc==1;
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2);assert(xl6ForeignViewDrainV1());xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("foreign execution contexts: scope identity, thread/fiber isolation and steady-state allocations passed");return 0;
}
