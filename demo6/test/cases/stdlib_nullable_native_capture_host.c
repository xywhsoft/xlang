#include "src/runtime/xl6_nullable_slot_map_key_capture.h"
#include "src/runtime/xl6_nullable_slot_map_key.h"
#include "src/runtime/xl6_closure_capture_v3.h"
#include "src/type/xl6_native_value_ids.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
typedef struct Frame {XL6_NullableSlotMapKeyV1 key;unsigned char live;} Frame;
static Frame* inspected;
static unsigned detachChecks;
#ifdef _WIN32
static DWORD WINAPI freeze_probe(void* unused)
#else
static void* freeze_probe(void* unused)
#endif
{
    (void)unused;xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope) && xrtOwnershipScopeEnd(&scope));
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
extern void __real_xl6NullableSlotMapKeyDropValueV1(XL6_NullableSlotMapKeyV1);
void __wrap_xl6NullableSlotMapKeyDropValueV1(XL6_NullableSlotMapKeyV1 owned)
{
    if(inspected){
        assert(!inspected->live && !inspected->key.hasValue && !inspected->key.value.instance && !inspected->key.value.slot);
#ifdef _WIN32
        HANDLE thread=CreateThread(NULL,0,freeze_probe,NULL,0,NULL);assert(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread));
#else
        pthread_t thread;assert(!pthread_create(&thread,NULL,freeze_probe,NULL) && !pthread_join(thread,NULL));
#endif
        ++detachChecks;
    }
    __real_xl6NullableSlotMapKeyDropValueV1(owned);
}
static bool visit(xrtownershipref ref,void* context){(void)ref;(void)context;assert(false);return false;}
static xrttype changed;
static const xrttype* changedType(void){return &changed;}
static void invoke(void){}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    const XL6_NativeCaptureProfileV1* profile=xl6NullableSlotMapKeyCaptureProfileV1Get();
    assert(xl6NativeCaptureProfileValidV1(profile) && profile->identity==XL6_NULLABLE_SLOT_MAP_KEY_LANGUAGE_VALUE_ID);
    XL6_NativeCaptureProfileV1 corrupt=*profile;corrupt.Type=changedType;changed=*profile->Type();
    changed.Flags&=~XRT_TYPE_FLAG_NULLABLE;assert(!xl6NativeCaptureProfileValidV1(&corrupt));
    changed=*profile->Type();changed.Flags|=XRT_TYPE_FLAG_REFERENCE;assert(!xl6NativeCaptureProfileValidV1(&corrupt));
    changed=*profile->Type();changed.Kind=XRT_TYPE_ARRAY;assert(!xl6NativeCaptureProfileValidV1(&corrupt));
    Frame frame={0};
    XL6_ActivationCaptureSlotV2 slot={{{XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(Frame,key),sizeof(frame.key)},xl6NullableSlotMapKeyCaptureProfileV1Get},offsetof(Frame,live)};
    const size_t order=0;XL6_ActivationCaptureLayoutV2 layout={sizeof(layout),sizeof(frame),1,&slot,1,&order};
    const XL6_ActivationCaptureRuntimeV2* api=xl6ActivationCaptureRuntimeV2Get();assert(api->Validate(&layout,sizeof(frame)));
    for(unsigned i=0;i<1000;++i){
        XL6_SlotMapKeyV1 key={xl6SlotMapInstanceCreateV1(),7};assert(key.instance);
        assert(xl6NullableSlotMapKeyWrapV1(key,&frame.key));frame.live=1;xl6SlotMapKeyDropValueV1(key);
        assert(api->Trace(&layout,&frame,sizeof(frame),visit,NULL));
        inspected=&frame;assert(xrtMemDebugFailAfter(0));
        xl6_error_result outcome=api->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL});
        assert(outcome.ok && !outcome.error && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();inspected=NULL;
        assert(!frame.live && !frame.key.hasValue && !frame.key.value.instance);
        unsigned prior=detachChecks;assert(api->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL}).ok && detachChecks==prior);
    }
    frame.key=(XL6_NullableSlotMapKeyV1){true,{(void*)(uintptr_t)1,7}};frame.live=0;
    Frame before=frame;assert(api->Trace(&layout,&frame,sizeof(frame),visit,NULL));
    assert(api->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL}).ok && !memcmp(&frame,&before,sizeof(frame)));
    XL6_ActivationCaptureLayoutV2 invalid=layout;invalid.bytes-=1;
    assert(!api->Validate(&invalid,sizeof(frame)));
    assert(!api->Cleanup(&invalid,&frame,sizeof(frame),(xl6_error_result){true,NULL}).ok && !memcmp(&frame,&before,sizeof(frame)));xrtClearError();
    frame=(Frame){.key={true,{0}},.live=1};inspected=&frame;
    assert(api->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL}).ok);inspected=NULL;
    frame=(Frame){0};frame.key.value=(XL6_SlotMapKeyV1){xl6SlotMapInstanceCreateV1(),9};assert(frame.key.value.instance);frame.live=1;
    inspected=&frame;assert(api->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL}).ok);inspected=NULL;
    XL6_CaptureSlotV2 plainSlot=slot.value;XL6_CaptureLayoutV2 plain={sizeof(plain),sizeof(frame),1,&plainSlot};
    XL6_SlotMapKeyV1 key={xl6SlotMapInstanceCreateV1(),13};assert(key.instance);assert(xl6NullableSlotMapKeyWrapV1(key,&frame.key));xl6SlotMapKeyDropValueV1(key);
    inspected=&frame;assert(xl6CaptureRuntimeV2Get()->Cleanup(&plain,&frame,sizeof(frame),(xl6_error_result){true,NULL}).ok);inspected=NULL;
    Frame* environment=xrtCalloc(1,sizeof(*environment));assert(environment);
    key=(XL6_SlotMapKeyV1){xl6SlotMapInstanceCreateV1(),17};assert(key.instance);
    assert(xl6NullableSlotMapKeyWrapV1(key,&environment->key));xl6SlotMapKeyDropValueV1(key);
    const XL6_ClosureRuntimeV3* closures=xl6ClosureRuntimeV3Get();
    assert(!closures->Create(NULL,invoke,NULL,environment,sizeof(*environment)-1,&plain,NULL,NULL,0));xrtClearError();
    assert(!closures->DropEnvironment(&plain,environment,sizeof(*environment)-1) && environment->key.hasValue);xrtClearError();
    XL6_Closure* closure=closures->Create(NULL,invoke,NULL,environment,sizeof(*environment),&plain,NULL,NULL,0);assert(closure);
    xrtownershipref reference=xl6ClosureRuntimeV1Get()->Ownership(closure);
    assert(reference.Ops->Trace(reference.Data,visit,NULL) && xl6ClosureOwnershipAdapterV1(reference));
    inspected=environment;xl6ClosureRuntimeV1Get()->Release(closure);inspected=NULL;
    assert(detachChecks==1004 && !xl6ProductMemoryEnd());xl6ProductMemoryVerify();printf("nullable native capture detach=%u; balanced\n",detachChecks);return 0;
}
