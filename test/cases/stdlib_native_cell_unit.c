#include "src/runtime/xl6_cell_native.h"
#include "src/runtime/xl6_nullable_slot_map_key_capture.h"
#include "src/runtime/xl6_nullable_slot_map_key.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_array_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI freeze_probe(void* unused)
#else
#include <pthread.h>
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
static XL6_SlotMapKeyV1 genuine(void)
{
    XL6_SlotMapLayoutV1 l={0xAA01,{0xAA02,0xAA03,1,sizeof(int64_t),xrtTypeInt64()}};
    xvalue* schema=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,l.value.identity,
        l.value.elementIdentity,1,sizeof(int64_t),l.value.type,XL6_VALUE_ADMIT_I64,false,l.value.elementIdentity);
    assert(schema);xvalue* map=xl6SlotMapCreateTakeV1(&l,schema);assert(map);
    XL6_SlotMapKeyV1 key={0};assert(xl6SlotMapInsertV1(map,&l,&(int64_t){7},&key));xrtValueRelease(map);return key;
}
static bool visits(xrtownershipref ref,void* context)
{(void)ref;++*(unsigned*)context;return true;}
static unsigned destroyed;
static void counted(void* raw,unsigned char* live)
{
    ++destroyed;xl6NullableSlotMapKeyCaptureProfileV1Get()->DetachCleanup(raw,live);
    if(destroyed==1 || destroyed==1001){
#ifdef _WIN32
        HANDLE thread=CreateThread(NULL,0,freeze_probe,NULL,0,NULL);assert(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread));
#else
        pthread_t thread;assert(!pthread_create(&thread,NULL,freeze_probe,NULL) && !pthread_join(thread,NULL));
#endif
    }
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);const XL6_CellRuntimeV1* api=xl6CellRuntimeV1Get();
    static XL6_NativeCaptureProfileV1 profile;
    profile=*xl6NullableSlotMapKeyCaptureProfileV1Get();profile.DetachCleanup=counted;
    for(unsigned round=0;round<1000;++round){
        XL6_NullableSlotMapKeyV1 input={true,genuine()},observed={0},acquired={0},prior={0},replacement={0};
        XL6_Cell* cell=xl6CellCreateNativeV2(&profile);assert(cell);
        api->Initialize(cell,&input,sizeof(input));input=(XL6_NullableSlotMapKeyV1){0};
        api->Load(cell,&observed,sizeof(observed));assert(observed.hasValue && observed.value.instance);
        assert(xl6NullableSlotMapKeyCopyValueV1(observed,&acquired));
        XL6_Cell* sibling=api->Retain(cell);assert(sibling==cell);
        api->Exchange(cell,&replacement,&prior,sizeof(prior));
        assert(xl6NullableSlotMapKeyEqualV1(prior,acquired));
        xl6NullableSlotMapKeyDropValueV1(prior);
        api->Load(sibling,&observed,sizeof(observed));assert(!observed.hasValue && !observed.value.instance);
        unsigned edges=0;xrtownershipref ref=api->Ownership(cell);size_t count=0;
        assert(ref.Ops->Count(ref.Data,&count) && count==2 && ref.Ops->Trace(ref.Data,visits,&edges) && !edges);
        assert(xrtMemDebugFailAfter(0));api->Release(sibling);api->Release(cell);
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        assert(acquired.hasValue && acquired.value.instance);xl6NullableSlotMapKeyDropValueV1(acquired);
    }
    /* Both ordinary terminal release and graph retirement run the descriptor.
     * Clear detaches raw owned storage; Finish drops only after freeze ended. */
    XL6_Cell* graph=xl6CellCreateNativeV2(&profile);assert(graph);
    XL6_NullableSlotMapKeyV1 input={true,genuine()};api->Initialize(graph,&input,sizeof(input));input=(XL6_NullableSlotMapKeyV1){0};
    xrtownershipref ref=api->Ownership(graph);const xrtownershipadapterv1* adapter=xl6CellOwnershipAdapterV1(ref);assert(adapter);
    const void* token=&ref;assert(adapter->Hold(ref.Data));
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Claim(ref.Data,token));adapter->Clear(ref.Data,token);assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtMemDebugFailAfter(0));assert(adapter->Finish(ref.Data,token));adapter->Drop(ref.Data);api->Release(graph);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    XL6_Cell* empty=xl6CellCreateNativeV2(&profile);assert(empty);api->Release(empty); /* no Initialize */
    XL6_Cell* plain=xl6CellCreateNativeV2(xl6SlotMapKeyCaptureProfileV1Get());assert(plain);
    XL6_SlotMapKeyV1 key=genuine();api->Initialize(plain,&key,sizeof(key));api->Release(plain);
    XL6_NativeCaptureProfileV1 bad=profile;bad.identity=0;
    assert(!xl6CellCreateNativeV2(&bad) && xrtGetError());xrtClearError();
    assert(!api->Create(XL6_CELL_NATIVE_LEAF_V2,sizeof(input)) && xrtGetError());xrtClearError();
    for(size_t at=0;at<2;++at){assert(xrtMemDebugFailAfter(at));assert(!xl6CellCreateNativeV2(&profile));
        assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();}
    assert(destroyed==1002 && !xrtGetError());assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    puts("native cells: 1000 shared exchanges, detach=1002, graph retirement, owning leaf; balanced");return 0;
}
