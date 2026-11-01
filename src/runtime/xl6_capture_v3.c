#include "src/common/xl6_xrt.h"
#include "xl6_capture_v3.h"
#include "src/api/xl_foreign_error.h"

static const XL6_NativeGraphCaptureProfileV1* graph(const XL6_CaptureSlotV2* slot)
{
    return (unsigned)slot->value.kind==XL6_CAPTURE_NATIVE_GRAPH_V3 && slot->native
        ? xl6NativeGraphCaptureProfileV1Get(slot->native()) : NULL;
}
static bool overlap(size_t a,size_t n,size_t b,size_t m)
{ return a<b+m && b<a+n; }
static bool slot_valid(const XL6_CaptureSlotV2* slot,size_t bytes)
{
    if (!slot || !slot->value.bytes || slot->value.offset>bytes || slot->value.bytes>bytes-slot->value.offset) return false;
    if ((unsigned)slot->value.kind==XL6_CAPTURE_NATIVE_GRAPH_V3) {
        const XL6_NativeGraphCaptureProfileV1* p=graph(slot);
        return p && slot->value.bytes==p->bytes && slot->value.offset%p->alignment==0;
    }
    XL6_CaptureLayoutV2 one={sizeof(one),bytes,1,slot};
    return xl6CaptureRuntimeV2Get()->Validate(&one,bytes);
}
static bool slot_data_valid(const XL6_CaptureSlotV2* slot,const void* data,size_t bytes)
{
    const void* metadata=NULL; size_t metadataBytes=0,alignment=1;
    const xrttype* type=NULL;
    if ((unsigned)slot->value.kind==XL6_CAPTURE_NATIVE_GRAPH_V3) {
        const XL6_NativeGraphCaptureProfileV1* p=graph(slot);
        metadata=p; metadataBytes=sizeof(*p); alignment=p->alignment;
    } else if ((unsigned)slot->value.kind==XL6_CAPTURE_NATIVE_LEAF_V2) {
        const XL6_NativeCaptureProfileV1* p=slot->native();
        metadata=p; metadataBytes=sizeof(*p); type=p->Type(); alignment=type->Align;
    }
    return ((uintptr_t)data+slot->value.offset)%alignment==0 &&
        (!metadata || !xrtMemRangesOverlap(data,bytes,metadata,metadataBytes)) &&
        (!type || (!xrtMemRangesOverlap(data,bytes,type,sizeof(*type)) &&
                   !xrtMemRangesOverlap(data,bytes,type->Ops,sizeof(*type->Ops))));
}
static bool slot_trace(const XL6_CaptureSlotV2* slot,const void* data,size_t bytes,
    XL6_ModuleOwnershipVisit visit,void* context)
{
    if ((unsigned)slot->value.kind==XL6_CAPTURE_NATIVE_GRAPH_V3)
        return graph(slot)->Trace((const char*)data+slot->value.offset,visit,context);
    XL6_CaptureLayoutV2 one={sizeof(one),bytes,1,slot};
    return xl6CaptureRuntimeV2Get()->Trace(&one,data,bytes,visit,context);
}
static bool layout_valid(const XL6_CaptureLayoutV2* layout,size_t bytes)
{
    if (!layout || layout->size!=sizeof(*layout) || !bytes || layout->bytes!=bytes ||
        layout->count>SIZE_MAX/sizeof(*layout->slots) || (layout->count ? !layout->slots : layout->slots!=NULL)) return false;
    for (size_t i=0;i<layout->count;++i) {
        const XL6_CaptureSlotV2* slot=&layout->slots[i];
        if (!slot_valid(slot,bytes)) return false;
        for (size_t j=0;j<i;++j)
            if (overlap(slot->value.offset,slot->value.bytes,layout->slots[j].value.offset,layout->slots[j].value.bytes)) return false;
    }
    return true;
}
static bool layout_data_valid(const XL6_CaptureLayoutV2* layout,const void* data,size_t bytes)
{
    if (!data || !layout_valid(layout,bytes) || xrtMemRangesOverlap(data,bytes,layout,sizeof(*layout)) ||
        (layout->count && xrtMemRangesOverlap(data,bytes,layout->slots,layout->count*sizeof(*layout->slots)))) return false;
    for (size_t i=0;i<layout->count;++i) if (!slot_data_valid(&layout->slots[i],data,bytes)) return false;
    return true;
}
static bool layout_trace(const XL6_CaptureLayoutV2* layout,const void* data,size_t bytes,
    XL6_ModuleOwnershipVisit visit,void* context)
{
    if (!visit || !layout_data_valid(layout,data,bytes)) return false;
    for (size_t i=0;i<layout->count;++i) if (!slot_trace(&layout->slots[i],data,bytes,visit,context)) return false;
    return true;
}
static void capture_result(xl6_error_scope* scope,xl6_error_result result)
{
    if (!result.ok) { xrtSetErrorTake(result.error); xl6ErrorScopeCaptureFailure(scope); }
}
static void slot_cleanup(const XL6_CaptureSlotV2* slot,size_t bytes,void* data,
    unsigned char* live,xl6_error_scope* errors)
{
    if (live && !*live) return;
    if ((unsigned)slot->value.kind==XL6_CAPTURE_NATIVE_GRAPH_V3) {
        if (!graph(slot)->Detach((char*)data+slot->value.offset,live) || xrtGetError())
            xl6ErrorScopeCaptureFailure(errors);
    } else if (live) {
        XL6_ActivationCaptureSlotV2 field={*slot,(size_t)(live-(unsigned char*)data)};
        const size_t order=0;
        XL6_ActivationCaptureLayoutV2 one={sizeof(one),bytes,1,&field,1,&order};
        capture_result(errors,xl6ActivationCaptureRuntimeV2Get()->Cleanup(&one,data,bytes,(xl6_error_result){true,NULL}));
    } else {
        XL6_CaptureLayoutV2 one={sizeof(one),bytes,1,slot};
        capture_result(errors,xl6CaptureRuntimeV2Get()->Cleanup(&one,data,bytes,(xl6_error_result){true,NULL}));
    }
}
static bool cleanup_begin(xl6_error_scope* scope,xl6_error_result incoming)
{
    if (!xl6ErrorScopeBegin(scope)) return false;
    xrtSetErrorTake(incoming.error);
    if (!incoming.ok) xl6ErrorScopeCaptureFailure(scope);
    return true;
}
static xl6_error_result layout_cleanup(const XL6_CaptureLayoutV2* layout,void* data,size_t bytes,xl6_error_result incoming)
{
    xl6_error_scope errors={0}; if (!cleanup_begin(&errors,incoming)) return (xl6_error_result){false,incoming.error};
    if (!layout_data_valid(layout,data,bytes)) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }
    else for (size_t i=layout->count;i;--i) slot_cleanup(&layout->slots[i-1],bytes,data,NULL,&errors);
    return xl6ErrorScopeEnd(&errors);
}
const XL6_CaptureRuntimeV2* xl6CaptureRuntimeV3Get(void)
{ static const XL6_CaptureRuntimeV2 api={layout_valid,layout_trace,layout_cleanup}; return &api; }

static bool activation_valid(const XL6_ActivationCaptureLayoutV2* layout,size_t bytes)
{
    if (!layout || layout->size!=sizeof(*layout) || !bytes || layout->bytes!=bytes ||
        layout->count>SIZE_MAX/sizeof(*layout->slots) || (layout->count ? !layout->slots : layout->slots!=NULL) ||
        layout->cleanupCount>layout->count || (layout->cleanupCount ? !layout->cleanup : layout->cleanup!=NULL)) return false;
    size_t owning=0;
    for (size_t i=0;i<layout->count;++i) {
        const XL6_ActivationCaptureSlotV2* field=&layout->slots[i];
        if (!slot_valid(&field->value,bytes)) return false;
        if (field->value.value.kind==XL6_CAPTURE_TRIVIAL) { if (field->initialized!=SIZE_MAX) return false; }
        else { if (field->initialized>=bytes) return false; ++owning; }
        for (size_t j=0;j<layout->count;++j) {
            const XL6_ActivationCaptureSlotV2* other=&layout->slots[j];
            if (other->value.value.offset>bytes || other->value.value.bytes>bytes-other->value.value.offset) return false;
            if (field->initialized!=SIZE_MAX &&
                ((field->initialized>=other->value.value.offset && field->initialized-other->value.value.offset<other->value.value.bytes) ||
                 (i!=j && field->initialized==other->initialized))) return false;
            if (j<i && overlap(field->value.value.offset,field->value.value.bytes,other->value.value.offset,other->value.value.bytes)) return false;
        }
    }
    if (owning!=layout->cleanupCount) return false;
    for (size_t i=0;i<layout->cleanupCount;++i) {
        if (layout->cleanup[i]>=layout->count || layout->slots[layout->cleanup[i]].value.value.kind==XL6_CAPTURE_TRIVIAL) return false;
        for (size_t j=0;j<i;++j) if (layout->cleanup[j]==layout->cleanup[i]) return false;
    }
    return true;
}
static bool activation_data_valid(const XL6_ActivationCaptureLayoutV2* layout,const void* data,size_t bytes)
{
    if (!data || !activation_valid(layout,bytes) || xrtMemRangesOverlap(data,bytes,layout,sizeof(*layout)) ||
        (layout->count && xrtMemRangesOverlap(data,bytes,layout->slots,layout->count*sizeof(*layout->slots))) ||
        (layout->cleanupCount && xrtMemRangesOverlap(data,bytes,layout->cleanup,layout->cleanupCount*sizeof(*layout->cleanup)))) return false;
    for (size_t i=0;i<layout->count;++i) {
        const XL6_ActivationCaptureSlotV2* field=&layout->slots[i];
        if (!slot_data_valid(&field->value,data,bytes) ||
            (field->initialized!=SIZE_MAX && ((const unsigned char*)data)[field->initialized]>1)) return false;
    }
    return true;
}
static bool activation_trace(const XL6_ActivationCaptureLayoutV2* layout,const void* data,size_t bytes,
    XL6_ModuleOwnershipVisit visit,void* context)
{
    if (!visit || !activation_data_valid(layout,data,bytes)) return false;
    for (size_t i=0;i<layout->count;++i) {
        const XL6_ActivationCaptureSlotV2* field=&layout->slots[i];
        if (field->initialized!=SIZE_MAX && ((const unsigned char*)data)[field->initialized] &&
            !slot_trace(&field->value,data,bytes,visit,context)) return false;
    }
    return true;
}
static xl6_error_result activation_cleanup(const XL6_ActivationCaptureLayoutV2* layout,void* data,size_t bytes,xl6_error_result incoming)
{
    xl6_error_scope errors={0}; if (!cleanup_begin(&errors,incoming)) return (xl6_error_result){false,incoming.error};
    if (!activation_data_valid(layout,data,bytes)) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }
    else for (size_t i=0;i<layout->cleanupCount;++i) {
        const XL6_ActivationCaptureSlotV2* field=&layout->slots[layout->cleanup[i]];
        slot_cleanup(&field->value,bytes,data,(unsigned char*)data+field->initialized,&errors);
    }
    return xl6ErrorScopeEnd(&errors);
}
const XL6_ActivationCaptureRuntimeV2* xl6ActivationCaptureRuntimeV3Get(void)
{ static const XL6_ActivationCaptureRuntimeV2 api={activation_valid,activation_trace,activation_cleanup}; return &api; }
