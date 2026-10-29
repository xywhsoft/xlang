#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map_key.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Frame {XL6_SlotMapKeyV1 key; unsigned char live; unsigned mode;} Frame;
static const XL6_GeneratorRuntimeV1* ops;
static const XL6_GeneratorRuntimeV3* api;
static const XL6_ActivationCaptureSlotV2 slots[]={
    {{{(XL6_CaptureKind)XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(Frame,key),sizeof(XL6_SlotMapKeyV1)},xl6SlotMapKeyCaptureProfileV1Get},offsetof(Frame,live)},
    {{{XL6_CAPTURE_TRIVIAL,offsetof(Frame,mode),sizeof(unsigned)},NULL},SIZE_MAX}};
static const size_t order[]={0};
static const XL6_ActivationCaptureLayoutV2 layout={sizeof(layout),sizeof(Frame),2,slots,1,order};
static const XL6_CaptureSlotV2 itemInt={{XL6_CAPTURE_TRIVIAL,0,sizeof(int64_t)},NULL};
static const XL6_CaptureLayoutV2 intLayout={sizeof(intLayout),sizeof(int64_t),1,&itemInt};
static const XL6_CaptureSlotV2 itemKey={{(XL6_CaptureKind)XL6_CAPTURE_NATIVE_LEAF_V2,0,sizeof(XL6_SlotMapKeyV1)},xl6SlotMapKeyCaptureProfileV1Get};
static const XL6_CaptureLayoutV2 keyLayout={sizeof(keyLayout),sizeof(XL6_SlotMapKeyV1),1,&itemKey};
static unsigned resumes,edges,vectors;
static bool visit(xrtownershipref ref,void* context)
{(void)context;assert(ref.Data && ref.Ops);++edges;return true;}
static bool resume(XL6_Generator* g)
{
    Frame* f=g->frame;assert(f && f->live && f->key.instance);++resumes;
    if(g->closing || g->state){
        unsigned mode=f->mode;ops->Finish(g);
        if(mode==2)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        return false;
    }
    if(f->mode==1){
        XL6_SlotMapKeyV1 key={0};assert(xl6SlotMapKeyCopyValueV1(f->key,&key));
        assert(ops->Put(g,&key,sizeof(key)));key=(XL6_SlotMapKeyV1){0};
    }else{int64_t value=72;assert(ops->Put(g,&value,sizeof(value)));}
    ops->SetState(g,1);return true;
}
static Frame* frame_new(unsigned mode)
{
    Frame* f=api->FrameAllocate(sizeof(*f),&layout);if(!f)return NULL;
    *f=(Frame){0};f->mode=mode;
    f->key=(XL6_SlotMapKeyV1){xl6SlotMapInstanceCreateV1(),UINT64_C(0x100000001)};
    if(!f->key.instance){api->FrameDiscard(f);return NULL;}f->live=1;return f;
}
static XL6_Generator* create(unsigned mode,Frame** frame)
{
    *frame=frame_new(mode);if(!*frame)return NULL;
    XL6_Generator* g=api->Create(NULL,*frame,resume,mode==1?&keyLayout:&intLayout);
    if(!g){api->FrameDiscard(*frame);*frame=NULL;}return g;
}
static void scenario(unsigned mode,unsigned early)
{
    Frame* f=NULL;XL6_Generator* g=create(mode,&f);assert(g);
    assert(api->Retain(g)==g);void* pin=api->FrameRetain(f);assert(pin==f);
    xrtownershipref frame=api->FrameOwnership(pin),control=ops->Ownership(g);size_t count=0;
    assert(frame.Ops->Count(frame.Data,&count) && count==2);
    edges=0;assert(frame.Ops->Trace(frame.Data,visit,NULL) && !edges);
    const xrtownershippreparationv1* prep=NULL;
    assert(xl6GeneratorOwnershipAdapterV1(frame,&prep) && prep);
    assert(xl6GeneratorOwnershipAdapterV1(control,&prep) && prep);
    ops->Destroy(g); /* Keep the separately retained owner. */
    if(early!=1){
        XL6_GeneratorStep step=ops->Next(g);assert(step.ok && step.value);
        if(mode==1 && !early){
            XL6_SlotMapKeyV1 owned={0};assert(ops->ItemGet(g,&owned,sizeof(owned),true));
            xl6_error_result r=ops->Close(g);assert(r.ok && !r.error);
            assert(!f->live && !f->key.instance && !f->key.slot);
            ops->Destroy(g);api->FrameRelease(pin);
            assert(owned.instance && owned.slot==UINT64_C(0x100000001));xl6SlotMapKeyDropValueV1(owned);
            ++vectors;return;
        }
        if(mode==0){int64_t number=0;assert(ops->ItemGet(g,&number,sizeof(number),false) && number==72);}
    }
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xl6_error_result r=ops->Close(g),outer=xl6ErrorScopeEnd(&scope);
    assert(r.ok==(mode!=2 || early) && !r.error && outer.ok && !outer.error);
    assert(!f->live && !f->key.instance && !f->key.slot);
    assert(frame.Ops->Count(frame.Data,&count) && count==1);
    assert(frame.Ops->Trace(frame.Data,visit,NULL));
    r=ops->Close(g);assert(r.ok && !r.error);ops->Destroy(g);api->FrameRelease(pin);++vectors;
}
static void refusals(void)
{
    XL6_ActivationCaptureLayoutV2 bad=layout;bad.bytes++;
    assert(!api->FrameAllocate(sizeof(Frame),&bad));xrtClearError();
    Frame* f=frame_new(0);assert(f);
    XL6_SlotMapKeyV1 duplicate={0};assert(xl6SlotMapKeyCopyValueV1(f->key,&duplicate));
    XL6_SlotMapKeyV1 saved=duplicate;
    assert(!xl6SlotMapKeyCopyValueV1(f->key,&duplicate) && xl6SlotMapKeyEqualV1(saved,duplicate));xrtClearError();
    xl6SlotMapKeyDropValueV1(duplicate);duplicate=(XL6_SlotMapKeyV1){0};
    assert(!xl6SlotMapKeyCopyValueV1((XL6_SlotMapKeyV1){NULL,1},&duplicate));xrtClearError();
    assert(!duplicate.instance && !duplicate.slot);
    _Alignas(max_align_t) unsigned char bytes[sizeof(XL6_SlotMapKeyV1)+1]={0};
    assert(!xl6SlotMapKeyCopyValueV1(f->key,(XL6_SlotMapKeyV1*)(bytes+1)));xrtClearError();
    xrtSetErrorKind(XERR_INTERNAL);const xerror* ambient=xrtGetError();assert(ambient);
    assert(xl6SlotMapKeyCopyValueV1(f->key,&duplicate) && xrtGetError()==ambient);
    xrtClearError();xl6SlotMapKeyDropValueV1(duplicate);
    XL6_CaptureLayoutV2 badItem=intLayout;badItem.count=0;
    assert(!api->Create(NULL,f,resume,&badItem));xrtClearError();
    /* V2 and V3 descriptors have separate admission; no structural cast. */
    XL6_CaptureSlot oldSlot={XL6_CAPTURE_TRIVIAL,0,sizeof(int64_t)};
    XL6_CaptureLayoutV1 old={sizeof(old),sizeof(int64_t),1,&oldSlot};
    assert(!xl6GeneratorRuntimeV2Get()->Create(NULL,f,resume,&old));xrtClearError();
    api->FrameDiscard(f);++vectors;
}
int main(int argc,char** argv)
{
    ops=xl6GeneratorRuntimeV1Get();api=xl6GeneratorRuntimeV3Get();xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==2){
        size_t point=(size_t)strtoull(argv[1],NULL,10);assert(xrtMemDebugFailAfter(point));
        Frame* f=NULL;XL6_Generator* g=create(0,&f);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert((g!=NULL)==!hit);if(g)ops->Destroy(g);xrtClearError();scenario(0,0);
        printf("prefix=%zu hit=%u\n",point,(unsigned)hit);
    }else{
        assert(argc==1);for(unsigned round=0;round<1000;++round){
            scenario(0,0);scenario(0,1);scenario(1,0);scenario(1,1);scenario(1,2);scenario(2,0);scenario(2,1);
        }refusals();
    }
    assert(!xrtGetError() && !xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    printf("native generator vectors=%u resumes=%u; balanced\n",vectors,resumes);return 0;
}
