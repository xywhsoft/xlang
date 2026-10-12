#include "src/runtime/xl6_capture_v2.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map_key.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
static unsigned vectors,refusals,drops,probes,edges,order[3],orderCount;
static const XL6_ActivationCaptureRuntimeV2* api;
static const XL6_CaptureRuntimeV2* plain;
#ifdef _WIN32
static DWORD WINAPI freeze_probe(void* unused)
#else
static void* freeze_probe(void* unused)
#endif
{
    (void)unused; xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
static void freeze_available(void)
{
#ifdef _WIN32
    HANDLE thread=CreateThread(NULL,0,freeze_probe,NULL,0,NULL);
    assert(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread));
#else
    pthread_t thread; assert(!pthread_create(&thread,NULL,freeze_probe,NULL) && !pthread_join(thread,NULL));
#endif
    ++probes;
}
typedef struct Native { void* resource; unsigned id,mode; } Native;
static bool native_copy(ptr out,const void* in,const xrttype* type)
{ (void)type; const Native* source=in; Native* target=out; *target=*source;
  target->resource=source->resource ? xrtMalloc(1) : NULL; return !source->resource || target->resource!=NULL; }
static void native_drop(ptr raw,const xrttype* type)
{ (void)type; Native owned=*(Native*)raw; *(Native*)raw=(Native){0}; xrtFree(owned.resource); }
static const xrttypeops native_ops={.Copy=native_copy,.Drop=native_drop};
static const xrttype native_type={.Id=42,.Kind=XRT_TYPE_RECORD,.Flags=XRT_TYPE_FLAG_COPYABLE,
    .Size=sizeof(Native),.Align=_Alignof(Native),.Ops=&native_ops};
static const xrttype* native_type_get(void) { return &native_type; }
static xrttype altered_type;
static const xrttype* altered_type_get(void) { return &altered_type; }
static void native_detach(void* raw,unsigned char* live)
{
    Native* slot=raw; Native owned; xrtownershipscope mutation={0};
    assert(xrtOwnershipMutationBegin(&mutation)); owned=*slot; *slot=(Native){0}; if(live) *live=0;
    assert(xrtOwnershipScopeEnd(&mutation));
    if(!owned.resource) return;
    assert(!slot->resource && (!live || !*live)); freeze_available();
    xrtFree(owned.resource); assert(orderCount<3); order[orderCount++]=owned.id; ++drops;
    if(owned.mode==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else if(owned.mode==2) xrtSetErrorKind(XERR_INTERNAL);
}
static const XL6_NativeCaptureProfileV1 native_profile={sizeof(native_profile),43,native_type_get,native_detach};
static bool visit(xrtownershipref ref,void* context)
{ (void)context; assert(ref.Data && ref.Ops); ++edges; return true; }
static bool refuse(xrtownershipref ref,void* context)
{ (void)ref; (void)context; return false; }
static void invoke(void) {}
static void generator_drop(void* data) {xl6GeneratorRuntimeV1Get()->FrameFree(data);}
static bool generator_resume(XL6_Generator* g) {xl6GeneratorRuntimeV1Get()->Finish(g); return false;}
static bool empty_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static void legacy_all(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    struct Legacy {xstrview text; xvalue* value; xfuture* future; XL6_Closure* closure;
        xerror* error; XL6_Cell* cell; XL6_ValueIterator* iterator; XL6_Generator* generator; unsigned char live[8];} p={0};
    XL6_ActivationCaptureSlotV2 slots[8];
#define LEGACY_SLOT(K,F,I) slots[I]=(XL6_ActivationCaptureSlotV2){{{K,offsetof(struct Legacy,F),sizeof(p.F)},NULL},offsetof(struct Legacy,live)+I}
    LEGACY_SLOT(XL6_CAPTURE_STRING,text,0); LEGACY_SLOT(XL6_CAPTURE_XVALUE,value,1); LEGACY_SLOT(XL6_CAPTURE_FUTURE,future,2);
    LEGACY_SLOT(XL6_CAPTURE_CLOSURE,closure,3); LEGACY_SLOT(XL6_CAPTURE_ERROR,error,4); LEGACY_SLOT(XL6_CAPTURE_CELL,cell,5);
    LEGACY_SLOT(XL6_CAPTURE_VALUE_ITERATOR,iterator,6); LEGACY_SLOT(XL6_CAPTURE_GENERATOR,generator,7);
#undef LEGACY_SLOT
    const size_t order[]={6,1,3,7,2,0,5,4}; XL6_ActivationCaptureLayoutV2 l={sizeof(l),sizeof(p),8,slots,8,order};
    p.text=(xstrview){xrtStrDupN("a\0b",3),3}; p.value=xrtValueInt(3);
    xpromise* promise=xrtPromiseCreate(&p.future,NULL); assert(promise); xrtPromiseDestroy(promise);
    p.closure=xl6ClosureRuntimeV1Get()->Create(NULL,invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);
    p.error=xrtErrorCreate(XERR_STATE,"legacy",1,"legacy");
    p.cell=xl6CellRuntimeV1Get()->Create(XL6_CELL_I64,sizeof(int64_t)); int64_t value=7;
    xl6CellRuntimeV1Get()->Initialize(p.cell,&value,sizeof(value));
    xvalue* source=xrtValueArray(); assert(source && xrtValueArrayAppendNew(source,xrtValueInt(8)));
    p.iterator=xl6ValueIteratorRuntimeV2Get()->Create(source); xrtValueRelease(source);
    void* frame=xl6GeneratorRuntimeV1Get()->FrameAllocate(1); assert(frame);
    p.generator=xl6GeneratorRuntimeV1Get()->Create(NULL,frame,generator_resume,generator_drop,sizeof(int64_t),NULL,empty_trace,empty_trace);
    assert(p.text.Data && p.value && p.future && p.closure && p.error && p.cell && p.iterator && p.generator);
    memset(p.live,1,sizeof(p.live)); assert(api->Validate(&l,sizeof(p))); unsigned before=edges;
    assert(api->Trace(&l,&p,sizeof(p),visit,NULL) && edges-before==7);
    assert(xrtMemDebugFailAfter(0)); xl6_error_result result=api->Cleanup(&l,&p,sizeof(p),(xl6_error_result){true,NULL});
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); assert(result.ok && !result.error && !hit);
    assert(!p.text.Data && !p.text.Size && !p.value && !p.future && !p.closure && !p.error && !p.cell && !p.iterator && !p.generator);
    for(unsigned i=0;i<8;++i) assert(!p.live[i]);
    result=api->Cleanup(&l,&p,sizeof(p),result); assert(result.ok && !result.error);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); ++vectors;
}
typedef struct Payload {
    XL6_SlotMapKeyV1 first,second; xstrview text; xvalue* value; uint64_t number; unsigned char live[4];
} Payload;
static void slots_make(XL6_ActivationCaptureSlotV2* slots)
{
    const XL6_NativeCaptureProfileV1* key=xl6SlotMapKeyCaptureProfileV1Get();
    slots[0]=(XL6_ActivationCaptureSlotV2){{{(XL6_CaptureKind)XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(Payload,first),sizeof(XL6_SlotMapKeyV1)},key},offsetof(Payload,live)};
    slots[1]=(XL6_ActivationCaptureSlotV2){{{(XL6_CaptureKind)XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(Payload,second),sizeof(XL6_SlotMapKeyV1)},key},offsetof(Payload,live)+1};
    slots[2]=(XL6_ActivationCaptureSlotV2){{{XL6_CAPTURE_STRING,offsetof(Payload,text),sizeof(xstrview)},NULL},offsetof(Payload,live)+2};
    slots[3]=(XL6_ActivationCaptureSlotV2){{{XL6_CAPTURE_XVALUE,offsetof(Payload,value),sizeof(xvalue*)},NULL},offsetof(Payload,live)+3};
    slots[4]=(XL6_ActivationCaptureSlotV2){{{XL6_CAPTURE_TRIVIAL,offsetof(Payload,number),sizeof(uint64_t)},NULL},SIZE_MAX};
}
static void sparse(unsigned mask)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    Payload p={0}; XL6_ActivationCaptureSlotV2 slots[5]; slots_make(slots);
    const size_t cleanup[]={2,1,3,0}; XL6_ActivationCaptureLayoutV2 l={sizeof(l),sizeof(p),5,slots,4,cleanup};
    XL6_SlotMapKeyV1 source={xl6SlotMapInstanceCreateV1(),UINT64_C(0x100000001)}; assert(source.instance);
    /* Poison in inactive owning slots must never be observed or destroyed. */
    p.first=p.second=(XL6_SlotMapKeyV1){(XL6_SlotMapInstanceV1*)(uintptr_t)1,1};
    p.text=(xstrview){(const char*)(uintptr_t)1,7}; p.value=(xvalue*)(uintptr_t)1; p.number=99;
    if(mask&1) { p.first=(XL6_SlotMapKeyV1){0}; assert(xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&p.first,&source)); p.live[0]=1; }
    if(mask&2) { p.second=(XL6_SlotMapKeyV1){0}; assert(xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&p.second,&source)); p.live[1]=1; }
    if(mask&4) { p.text=(xstrview){xrtStrDupN("a\0b",3),3}; assert(p.text.Data); p.live[2]=1; }
    if(mask&8) { p.value=xrtValueInt(7); assert(p.value); p.live[3]=1; }
    xl6SlotMapKeyDropValueV1(source);
    assert(api->Validate(&l,sizeof(p))); unsigned beforeEdges=edges;
    assert(api->Trace(&l,&p,sizeof(p),visit,NULL)); assert(edges-beforeEdges==((mask&8)!=0));
    assert(api->Trace(&l,&p,sizeof(p),refuse,NULL)==((mask&8)==0));
    assert(xrtMemDebugFailAfter(0)); xl6_error_result result=api->Cleanup(&l,&p,sizeof(p),(xl6_error_result){true,NULL});
    bool allocation=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); assert(result.ok && !result.error && !allocation);
    for(unsigned i=0;i<4;++i) assert(!p.live[i]);
    if(mask&1) assert(!p.first.instance && !p.first.slot);
    if(mask&2) assert(!p.second.instance && !p.second.slot);
    assert(p.number==99); result=api->Cleanup(&l,&p,sizeof(p),result); assert(result.ok && !result.error);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); ++vectors;
}
static void ordered(unsigned mode,bool sparseLayout)
{
    xl6ProductMemoryBegin(SIZE_MAX); orderCount=0; Native values[3]={{0}}; unsigned char live[3]={1,1,1};
    struct Ordered {Native values[3]; unsigned char live[3];} p={0};
    XL6_ActivationCaptureSlotV2 slots[3]; XL6_CaptureSlotV2 all[3]; const size_t cleanup[]={1,2,0};
    for(unsigned i=0;i<3;++i) {
        p.values[i]=(Native){xrtMalloc(1),i,i==1 ? 1u : 2u}; assert(p.values[i].resource); p.live[i]=live[i];
        all[i]=(XL6_CaptureSlotV2){{(XL6_CaptureKind)XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(struct Ordered,values)+i*sizeof(Native),sizeof(Native)},&native_profile};
        slots[i]=(XL6_ActivationCaptureSlotV2){all[i],offsetof(struct Ordered,live)+i};
    }
    (void)values;
    XL6_ActivationCaptureLayoutV2 a={sizeof(a),sizeof(p),3,slots,3,cleanup};
    XL6_CaptureLayoutV2 l={sizeof(l),sizeof(p),3,all};
    xerror* ambient=xrtErrorCreate(XERR_ARGUMENT,"ambient",1,"ambient"); assert(ambient); xrtSetErrorTake(ambient);
    xerror* original=mode==2 ? xrtErrorCreate(XERR_STATE,"primary",2,"primary") : NULL; assert(mode!=2 || original);
    xl6_error_result result=sparseLayout ? api->Cleanup(&a,&p,sizeof(p),(xl6_error_result){mode==0,original}) :
        plain->Cleanup(&l,&p,sizeof(p),(xl6_error_result){mode==0,original});
    assert(!result.ok && (mode==0 && !sparseLayout ? result.error!=NULL : result.error==original) && xrtGetError()==ambient);
    assert(orderCount==3);
    assert(order[0]==(sparseLayout ? 1u : 2u) && order[1]==(sparseLayout ? 2u : 1u) && order[2]==0);
    for(unsigned i=0;i<3;++i) assert(!p.values[i].resource && (!sparseLayout || !p.live[i]));
    result=sparseLayout ? api->Cleanup(&a,&p,sizeof(p),result) : plain->Cleanup(&l,&p,sizeof(p),result);
    assert(!result.ok && xrtGetError()==ambient && orderCount==3);
    xrtErrorFree(result.error); xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); ++vectors;
}
static void refused(const XL6_ActivationCaptureLayoutV2* l,Payload* p)
{
    Payload before=*p; assert(!api->Validate(l,sizeof(*p)) || !api->Trace(l,p,sizeof(*p),visit,NULL));
    assert(!api->Trace(l,p,sizeof(*p),visit,NULL)); xl6_error_result result=api->Cleanup(l,p,sizeof(*p),(xl6_error_result){true,NULL});
    assert(!result.ok && !memcmp(p,&before,sizeof(before))); xrtErrorFree(result.error); xrtClearError(); ++refusals;
}
static void invalid(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); Payload p={0}; XL6_ActivationCaptureSlotV2 slots[5]; slots_make(slots);
    size_t order[4]={2,1,3,0}; XL6_ActivationCaptureLayoutV2 l={sizeof(l),sizeof(p),5,slots,4,order};
    XL6_ActivationCaptureLayoutV2 saved=l; XL6_ActivationCaptureSlotV2 first=slots[0];
#define BAD(Field,Value) do {l.Field=(Value); refused(&l,&p); l=saved;} while(0)
    BAD(size,0); BAD(bytes,sizeof(p)+1); BAD(count,SIZE_MAX); BAD(slots,NULL); BAD(cleanupCount,3); BAD(cleanup,NULL);
#undef BAD
#define BAD_SLOT(Field,Value) do {slots[0].Field=(Value); refused(&l,&p); slots[0]=first;} while(0)
    BAD_SLOT(initialized,SIZE_MAX); BAD_SLOT(initialized,sizeof(p)); BAD_SLOT(initialized,offsetof(Payload,first));
    BAD_SLOT(initialized,slots[1].initialized); BAD_SLOT(value.value.offset,1); BAD_SLOT(value.value.offset,SIZE_MAX);
    BAD_SLOT(value.value.bytes,1); BAD_SLOT(value.value.bytes,SIZE_MAX); BAD_SLOT(value.native,NULL);
    BAD_SLOT(value.value.kind,XL6_CAPTURE_XVALUE);
#undef BAD_SLOT
    order[0]=5; refused(&l,&p); order[0]=2; order[1]=2; refused(&l,&p); order[1]=1;
    p.live[0]=2; refused(&l,&p); p.live[0]=0;
    XL6_NativeCaptureProfileV1 profile=*first.value.native; slots[0].value.native=&profile;
    profile.size=0; refused(&l,&p); profile=*first.value.native;
    profile.identity=0; refused(&l,&p); profile=*first.value.native;
    profile.Type=NULL; refused(&l,&p); profile=*first.value.native;
    profile.DetachCleanup=NULL; refused(&l,&p); slots[0]=first;
    profile=*first.value.native; profile.Type=altered_type_get; slots[0].value.native=&profile;
    altered_type=*xl6SlotMapKeyTypeV1(); altered_type.Size=0; refused(&l,&p);
    altered_type=*xl6SlotMapKeyTypeV1(); altered_type.Align=3; refused(&l,&p);
    altered_type=*xl6SlotMapKeyTypeV1(); altered_type.Flags|=XRT_TYPE_FLAG_REFERENCE; refused(&l,&p);
    altered_type=*xl6SlotMapKeyTypeV1(); altered_type.Ops=NULL; refused(&l,&p); slots[0]=first;
    slots[4].value.native=first.value.native; refused(&l,&p); slots_make(slots);
    /* Same bytes are not permission to reinterpret or extend V1 descriptors. */
    XL6_CaptureLayoutV1 old={sizeof(old),sizeof(p),1,&slots[0].value.value};
    assert(!xl6CaptureRuntimeV1Get()->Validate(&old,sizeof(p))); ++refusals;
    assert(api->Validate(&l,sizeof(p)) && !plain->Validate(NULL,sizeof(p)) && !xl6NativeCaptureProfileValidV1(NULL));
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
static void metadata_alias(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    struct Aliased {Payload payload; XL6_NativeCaptureProfileV1 profile;} data={0};
    data.profile=*xl6SlotMapKeyCaptureProfileV1Get();
    XL6_ActivationCaptureSlotV2 slots[5]; slots_make(slots); slots[0].value.native=&data.profile;
    const size_t order[]={2,1,3,0}; XL6_ActivationCaptureLayoutV2 l={sizeof(l),sizeof(data),5,slots,4,order};
    assert(api->Validate(&l,sizeof(data)) && !api->Trace(&l,&data,sizeof(data),visit,NULL));
    struct Aliased before=data;
    xl6_error_result result=api->Cleanup(&l,&data,sizeof(data),(xl6_error_result){true,NULL});
    assert(!result.ok && !memcmp(&before,&data,sizeof(data))); xrtErrorFree(result.error); xrtClearError(); ++refusals;
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
static void unaligned(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); _Alignas(max_align_t) unsigned char data[sizeof(Payload)+1]={0};
    XL6_ActivationCaptureSlotV2 slots[5]; slots_make(slots); const size_t order[]={2,1,3,0};
    XL6_ActivationCaptureLayoutV2 l={sizeof(l),sizeof(Payload),5,slots,4,order};
    assert(!api->Trace(&l,data+1,sizeof(Payload),visit,NULL)); xl6_error_result result=api->Cleanup(&l,data+1,sizeof(Payload),(xl6_error_result){true,NULL});
    assert(!result.ok); xrtErrorFree(result.error); xrtClearError(); ++refusals;
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
}
int main(void)
{
    api=xl6ActivationCaptureRuntimeV2Get(); plain=xl6CaptureRuntimeV2Get();
    assert(xl6NativeCaptureProfileValidV1(xl6SlotMapKeyCaptureProfileV1Get()) && xl6NativeCaptureProfileValidV1(&native_profile));
    for(unsigned mask=0;mask<16;++mask) sparse(mask);
    for(unsigned mode=0;mode<3;++mode) {ordered(mode,true); ordered(mode,false);}
    legacy_all(); invalid(); unaligned(); metadata_alias(); assert(drops==18 && probes==18);
    printf("native capture vectors=%u refusals=%u drops=%u outside-freeze=%u; balanced\n",vectors,refusals,drops,probes);
    return 0;
}
