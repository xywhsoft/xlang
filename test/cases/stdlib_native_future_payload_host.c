#include "src/runtime/xl6_native_future_payload.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map_key.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static const XL6_NativeFuturePayloadRuntimeV1* api;
static unsigned vectors,refusals,visits,mode;
static bool armed;
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* output,const void* input)
{
    if(!armed)return __real_xrtTypeCopyValue(type,output,input);
    if(mode==0)return false;
    bool copied=__real_xrtTypeCopyValue(type,output,input);assert(copied);
    if(mode==2)xrtSetErrorKind(XERR_STATE);
    return mode==2;
}
static bool visit(xrtownershipref ref,void* context)
{ (void)context;assert(ref.Ops);++visits;return true; }
static XL6_SlotMapKeyV1* payload(void)
{
    XL6_SlotMapKeyV1* value=api->Allocate(xl6SlotMapKeyCaptureProfileV1Get());if(!value)return NULL;
    assert(!value->instance && !value->slot);
    value->instance=xl6SlotMapInstanceCreateV1();if(!value->instance){api->Drop(value,NULL);return NULL;}
    value->slot=UINT64_C(0x100000001);return value;
}
static void scenario(unsigned kind)
{
    XL6_SlotMapKeyV1* value=payload();assert(value);xfuture* future=NULL;
    xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    const XL6_NativeCaptureProfileV1* profile=xl6SlotMapKeyCaptureProfileV1Get();
    if(kind==0) {
        XL6_SlotMapKeyV1 out={0};assert(api->Copy(value,profile,&out));assert(xl6SlotMapKeyEqualV1(*value,out));
        XL6_NativeCaptureProfileV1 wrong=*profile;assert(!api->Copy(value,&wrong,&(XL6_SlotMapKeyV1){0}));++refusals;xrtClearError();
        assert(!api->Copy(value,profile,value));++refusals;xrtClearError();xl6SlotMapKeyDropValueV1(out);
        assert(api->ResolveOwned(promise,value));value=NULL;
        const void* borrowed=xrtFutureValue(future);assert(borrowed);
        visits=0;xrtownershipref ref=xrtFutureOwnership(future);size_t count=0;
        assert(api->OwnershipAdapter(ref) && ref.Ops->Count(ref.Data,&count) && count);
        assert(api->Policy()->Trace(borrowed,NULL,visit,NULL) && !visits);
        /* The enclosing Future still owns its real cancellation token. Only
         * the native value is a leaf; never erase transport graph edges. */
        assert(ref.Ops->Trace(ref.Data,visit,NULL) && visits==1);
        XL6_SlotMapKeyV1 first={0},second={0};assert(api->Copy(borrowed,profile,&first) && api->Copy(borrowed,profile,&second));
        assert(xl6SlotMapKeyEqualV1(first,second));xrtPromiseDestroy(promise);promise=NULL;
        assert(xrtMemDebugFailAfter(0));xrtFutureDestroy(future);future=NULL;
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(first.instance && second.instance);
        xl6SlotMapKeyDropValueV1(first);xl6SlotMapKeyDropValueV1(second);
    } else if(kind==1) {
        assert(xrtPromiseCancel(promise));assert(!api->ResolveOwned(promise,value));assert(value->instance);xrtClearError();
        assert(xrtMemDebugFailAfter(0));api->Drop(value,NULL);value=NULL;assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    } else {
        XL6_SlotMapKeyV1 out={0};xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        mode=kind-2;armed=true;assert(!api->Copy(value,profile,&out));armed=false;
        assert(!out.instance && !out.slot && value->instance && scope.failed);
        xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(!result.ok);
        if(mode<2)assert(!result.error);else assert(result.error && xrtErrorKind(result.error)==XERR_STATE);
        xrtErrorFree(result.error);xrtClearError();
        assert(api->Copy(value,profile,&out));xl6SlotMapKeyDropValueV1(out);
    }
    if(value)api->Drop(value,NULL);
    xrtPromiseDestroy(promise);xrtFutureDestroy(future);assert(!xrtGetError());++vectors;
}
int main(int argc,char** argv)
{
    api=xl6NativeFuturePayloadRuntimeV1Get();xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==2) {
        size_t point=(size_t)strtoull(argv[1],NULL,10);assert(xrtMemDebugFailAfter(point));
        XL6_SlotMapKeyV1* value=payload();bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert((value!=NULL)==!hit);if(value)api->Drop(value,NULL);xrtClearError();
        printf("prefix=%zu hit=%u\n",point,(unsigned)hit);
    } else {
        assert(argc==1);assert(!api->Allocate(NULL));++refusals;xrtClearError();
        XL6_NativeCaptureProfileV1 bad=*xl6SlotMapKeyCaptureProfileV1Get();bad.identity=0;
        assert(!api->Allocate(&bad));++refusals;xrtClearError();
        for(unsigned i=0;i<200;++i)for(unsigned kind=0;kind<5;++kind)scenario(kind);
        printf("vectors=%u refusals=%u\n",vectors,refusals);
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
