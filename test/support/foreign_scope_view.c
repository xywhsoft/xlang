#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_foreign_scope_view.h"
#include "src/runtime/xl6_foreign_scope_value.h"
#if defined(_WIN32)
#include <windows.h>
#endif

typedef struct Ephemeral {int code;unsigned char bytes[8];} Ephemeral;
typedef struct Worker {XL6_ForeignScopeViewV1 value;XL6_ForeignScopeLifetimeV1* scope;} Worker;
static void reject(XL6_ForeignScopeViewV1 value)
{
    assert(!xl6ForeignScopeViewAliveV1(value));
    void* raw=(void*)(uintptr_t)1;XL6_ForeignView* owner=(void*)(uintptr_t)1;
    assert(!xl6ForeignScopeViewGetV1(value,&raw,&owner));
    assert(raw==(void*)(uintptr_t)1 && owner==(void*)(uintptr_t)1);
    xrtClearError();
}
static int32 worker(void* raw)
{
    Worker* input=raw;reject(input->value);
    assert(!xl6ForeignScopeEndV1(input->scope));xrtClearError();return 0;
}
#if defined(_WIN32)
static VOID WINAPI fiber(void* raw)
{
    void** data=raw;Worker* input=data[0];reject(input->value);
    assert(!xl6ForeignScopeEndV1(input->scope));xrtClearError();SwitchToFiber(data[1]);
}
#endif
int main(void)
{
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL);assert(root);
    XL6_ForeignViewCallV1 call={0};NativeScope native={0};
    XL6_ForeignCallbackScopeV1 callback={0};
    assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
    XL6_ForeignScopeLifetimeV1 a={0},b={0};Ephemeral event={42,{0,1,0,255}};
    assert(!xl6ForeignScopeBeginV1(&a,&call));xrtClearError();
    assert(xl6ForeignCallbackEnterV1(&callback));
    assert(xl6ForeignScopeBeginV1(&a,&call));
    XL6_ForeignScopeViewV1 saved={0},child={0},null={0};
    const Ephemeral readOnly={7,{1,2,3}};
    XL6_ForeignScopeViewV1 constView={0};
    assert(xl6ForeignScopeViewMakeV1(&a,&readOnly,&constView));
    assert(xl6ForeignScopeViewDeriveV1(constView,readOnly.bytes,&child));
    assert(xl6ForeignScopeViewMakeV1(&a,&event,&saved));
    assert(xl6ForeignScopeViewMakeV1(&a,NULL,&null) && !null.address && !null.lifetime);
    assert(xl6ForeignScopeViewAliveV1(saved));
    void* pointer=NULL;XL6_ForeignView* owner=NULL;
    assert(xl6ForeignScopeViewGetV1(saved,&pointer,&owner));
    assert(pointer==&event && owner==root && ((Ephemeral*)pointer)->code==42);
    assert(xl6ForeignScopeViewDeriveV1(saved,event.bytes,&child));
    assert(child.lifetime==saved.lifetime && child.address==(uintptr_t)event.bytes);
    assert(xrtTypeValidate(xl6ScopeViewTypeV1()) && xrtTypeValidate(xl6NullableScopeViewTypeV1()));
    assert(xl6NativeCaptureProfileValidV1(xl6ScopeViewCaptureV1()) && xl6NativeCaptureProfileValidV1(xl6NullableScopeViewCaptureV1()));
    xvalue* boxed=xl6ScopeViewBoxV1(saved,17);assert(boxed);
    XL6_ForeignScopeViewV1 unboxed={0};assert(xl6ScopeViewUnboxV1(boxed,17,false,&unboxed) && xl6ScopeViewEqualV1(saved,unboxed));
    XL6_NullableForeignScopeViewV1 optional={0};assert(xl6NullableScopeViewUnboxV1(boxed,17,true,&optional) && optional.hasValue);
    assert(!xl6ScopeViewUnboxV1(boxed,18,false,&null));xrtClearError();
    XL6_ForeignScopeLifetimeV1 copied=a;
    assert(!xl6ForeignScopeEndV1(&copied));xrtClearError();
    assert(!xl6ForeignScopeViewMakeV1(&copied,&event,&null));xrtClearError();
    Worker input={saved,&a};xthread* thread=xrtThreadCreate(worker,&input,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
#if defined(_WIN32)
    void* original=ConvertThreadToFiber(NULL);assert(original);
    void* data[2]={&input,original};void* other=CreateFiber(0,fiber,data);assert(other);
    SwitchToFiber(other);DeleteFiber(other);assert(ConvertFiberToThread());
#endif
    assert(xl6ForeignScopeViewAliveV1(saved));
    assert(xl6ForeignScopeBeginV1(&b,&call));
    XL6_ForeignScopeViewV1 nested={0};assert(xl6ForeignScopeViewMakeV1(&b,&event,&nested));
    assert(saved.lifetime!=nested.lifetime && xl6ForeignScopeViewAliveV1(saved));
    assert(xl6ForeignScopeEndV1(&a));reject(saved);reject(child);reject(unboxed);reject(optional.value);
    xvalue* cloned=xrtValueClone(boxed);assert(cloned);unboxed=(XL6_ForeignScopeViewV1){0};
    assert(xl6ScopeViewUnboxV1(cloned,17,false,&unboxed));reject(unboxed);xrtValueRelease(cloned);xrtValueRelease(boxed);
    assert(xl6ForeignScopeViewAliveV1(nested));assert(xl6ForeignScopeEndV1(&b));reject(nested);
    assert(!xl6ForeignScopeViewDeriveV1(saved,&event,&null));xrtClearError();
    /* Same stack token and same payload address must never resurrect a value. */
    assert(xl6ForeignScopeBeginV1(&a,&call));
    XL6_ForeignScopeViewV1 current={0};assert(xl6ForeignScopeViewMakeV1(&a,&event,&current));
    reject(saved);assert(current.lifetime!=saved.lifetime);assert(xl6ForeignScopeEndV1(&a));
    /* Warm callback registrations/accesses allocate no wrappers or payloads. */
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<200;++i){
        XL6_ForeignScopeViewV1 value={0},derived={0};
        assert(xl6ForeignScopeBeginV1(&a,&call));
        assert(xl6ForeignScopeViewMakeV1(&a,&event,&value));
        assert(xl6ForeignScopeViewGetV1(value,&pointer,&owner));
        assert(xl6ForeignScopeViewDeriveV1(value,event.bytes,&derived));
        assert(xl6ForeignScopeEndV1(&a));
    }
    assert(xrtMemDebugFailAfter(-1));xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    /* Misordered native teardown expires tokens before payload destruction. */
    assert(xl6ForeignScopeBeginV1(&a,&call));current=(XL6_ForeignScopeViewV1){0};
    assert(xl6ForeignScopeViewMakeV1(&a,&event,&current));
    assert(xl6ForeignViewCloseV1(root));assert(xl6ForeignViewCallEndV1(&call));reject(current);
    assert(xl6ForeignScopeEndV1(&a));assert(xl6ForeignCallbackLeaveV1(&callback));
    assert(xl6ForeignViewReleaseV1(root) && xl6ForeignViewDrainV1());reject(saved);
    assert(!xl6ForeignViewPendingV1(0) && !xrtGetError());
    puts("foreign callback scope views: passed");return 0;
}
