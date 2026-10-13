#include "xl6_capture_v2.h"
#include "xl6_text_value.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>

bool xl6NativeCaptureProfileValidV1(const XL6_NativeCaptureProfileV1* p)
{
    const xrttype* t=p && p->size==sizeof(*p) && p->Type ? p->Type() : NULL;
    return t && p->identity && p->DetachCleanup && t->Id &&
        (t->Kind==XRT_TYPE_RECORD || (t->Kind==XRT_TYPE_OPTIONAL && (t->Flags&XRT_TYPE_FLAG_NULLABLE))) &&
        t->Size && t->Align && t->Align<=_Alignof(max_align_t) &&
        (_Alignof(max_align_t)%t->Align)==0 && t->Size%t->Align==0 &&
        t->Ops && t->Ops->Copy && t->Ops->Drop && (t->Flags&XRT_TYPE_FLAG_COPYABLE) &&
        !(t->Flags&XRT_TYPE_FLAG_REFERENCE);
}
static bool overlap(size_t a,size_t n,size_t b,size_t m)
{ return a<b+m && b<a+n; } /* Callers prove both sums fit first. */
static bool slot_valid(const XL6_CaptureSlotV2* s,size_t bytes)
{
    if(!s || !s->value.bytes || s->value.offset>bytes || s->value.bytes>bytes-s->value.offset) return false;
    if((unsigned)s->value.kind==XL6_CAPTURE_NATIVE_LEAF_V2) {
        if(!s->native || !xl6NativeCaptureProfileValidV1(s->native())) return false;
        const xrttype* t=s->native()->Type();
        return s->value.bytes==t->Size && s->value.offset%t->Align==0;
    }
    XL6_CaptureLayoutV1 one={sizeof(one),bytes,1,&s->value};
    return !s->native && xl6CaptureRuntimeV1Get()->Validate(&one,bytes);
}
static bool slot_address_valid(const XL6_CaptureSlotV2* s,const void* data)
{
    return (unsigned)s->value.kind!=XL6_CAPTURE_NATIVE_LEAF_V2 ||
        ((uintptr_t)data+s->value.offset)%s->native()->Type()->Align==0;
}
static bool slot_metadata_disjoint(const XL6_CaptureSlotV2* s,const void* data,size_t bytes)
{
    if((unsigned)s->value.kind!=XL6_CAPTURE_NATIVE_LEAF_V2) return true;
    const XL6_NativeCaptureProfileV1* native=s->native();const xrttype* t=native->Type();
    return !xrtMemRangesOverlap(data,bytes,native,sizeof(*native)) &&
        !xrtMemRangesOverlap(data,bytes,t,sizeof(*t)) &&
        !xrtMemRangesOverlap(data,bytes,t->Ops,sizeof(*t->Ops));
}
static bool slot_trace(const XL6_CaptureSlotV2* s,const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{
    if((unsigned)s->value.kind==XL6_CAPTURE_NATIVE_LEAF_V2) return true;
    XL6_CaptureLayoutV1 one={sizeof(one),SIZE_MAX,1,&s->value};
    /* Use the actual bounded span; SIZE_MAX is never a payload range. */
    one.bytes=s->value.offset+s->value.bytes;
    return xl6CaptureRuntimeV1Get()->Trace(&one,data,visit,context);
}
static void slot_cleanup(const XL6_CaptureSlotV2* s,size_t bytes,void* data,unsigned char* live)
{
    if(live && !*live) return;
    if((unsigned)s->value.kind==XL6_CAPTURE_NATIVE_LEAF_V2) {
        s->native()->DetachCleanup((char*)data+s->value.offset,live); return;
    }
    /* Reuse V1 sparse detach semantics for every established owning kind;
     * generated backends do not acquire a second cleanup implementation. */
    if(live) {
        XL6_ActivationCaptureSlot slot={s->value,(size_t)(live-(unsigned char*)data)};
        const size_t order=0;
        XL6_ActivationCaptureLayoutV1 one={sizeof(one),bytes,1,&slot,1,&order};
        xl6_error_result result=xl6ActivationCaptureRuntimeV1Get()->Cleanup(&one,data,(xl6_error_result){true,NULL});
        if(!result.ok) xl6ErrorScopePropagateFailure(result);
    } else {
        XL6_CaptureLayoutV1 one={sizeof(one),bytes,1,&s->value};
        xl6CaptureRuntimeV1Get()->Cleanup(&one,data);
    }
}
static bool layout_valid(const XL6_CaptureLayoutV2* l,size_t bytes)
{
    if(!l || l->size!=sizeof(*l) || !bytes || l->bytes!=bytes || l->count>SIZE_MAX/sizeof(*l->slots) ||
        (l->count ? !l->slots : l->slots!=NULL)) return false;
    for(size_t i=0;i<l->count;++i) {
        if(!slot_valid(&l->slots[i],bytes)) return false;
        for(size_t j=0;j<i;++j) if(overlap(l->slots[i].value.offset,l->slots[i].value.bytes,
            l->slots[j].value.offset,l->slots[j].value.bytes)) return false;
    }
    return true;
}
static bool layout_data_valid(const XL6_CaptureLayoutV2* l,const void* data,size_t bytes)
{
    if(!data || !layout_valid(l,bytes) || xrtMemRangesOverlap(data,l->bytes,l,sizeof(*l)) ||
        (l->count && xrtMemRangesOverlap(data,l->bytes,l->slots,l->count*sizeof(*l->slots)))) return false;
    for(size_t i=0;i<l->count;++i)
        if(!slot_address_valid(&l->slots[i],data) || !slot_metadata_disjoint(&l->slots[i],data,l->bytes)) return false;
    return true;
}
static bool layout_trace(const XL6_CaptureLayoutV2* l,const void* data,size_t bytes,XL6_ModuleOwnershipVisit visit,void* context)
{
    if(!visit || !layout_data_valid(l,data,bytes)) return false;
    for(size_t i=0;i<l->count;++i) if(!slot_trace(&l->slots[i],data,visit,context)) return false;
    return true;
}
static bool cleanup_begin(xl6_error_scope* s,xl6_error_result incoming)
{
    if(!xl6ErrorScopeBegin(s)) return false;
    xrtSetErrorTake(incoming.error);
    if(!incoming.ok) xl6ErrorScopeCaptureFailure(s);
    return true;
}
static xl6_error_result layout_cleanup(const XL6_CaptureLayoutV2* l,void* data,size_t bytes,xl6_error_result incoming)
{
    xl6_error_scope s={0}; if(!cleanup_begin(&s,incoming)) return (xl6_error_result){false,incoming.error};
    if(!layout_data_valid(l,data,bytes)) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&s); }
    else for(size_t i=l->count;i;--i) {
        slot_cleanup(&l->slots[i-1],l->bytes,data,NULL);
        if(xrtGetError()) xl6ErrorScopeCaptureFailure(&s);
    }
    return xl6ErrorScopeEnd(&s);
}
const XL6_CaptureRuntimeV2* xl6CaptureRuntimeV2Get(void)
{ static const XL6_CaptureRuntimeV2 api={layout_valid,layout_trace,layout_cleanup}; return &api; }
static bool activation_valid(const XL6_ActivationCaptureLayoutV2* l,size_t bytes)
{
    if(!l || l->size!=sizeof(*l) || !bytes || l->bytes!=bytes || l->count>SIZE_MAX/sizeof(*l->slots) ||
        (l->count ? !l->slots : l->slots!=NULL) || l->cleanupCount>l->count ||
        (l->cleanupCount ? !l->cleanup : l->cleanup!=NULL)) return false;
    size_t owning=0;
    for(size_t i=0;i<l->count;++i) {
        const XL6_ActivationCaptureSlotV2* s=&l->slots[i];
        if(!slot_valid(&s->value,bytes)) return false;
        if(s->value.value.kind==XL6_CAPTURE_TRIVIAL) { if(s->initialized!=SIZE_MAX) return false; }
        else { if(s->initialized>=bytes) return false; ++owning; }
        for(size_t j=0;j<l->count;++j) {
            const XL6_ActivationCaptureSlotV2* other=&l->slots[j];
            if(other->value.value.offset>bytes || other->value.value.bytes>bytes-other->value.value.offset) return false;
            if(s->initialized!=SIZE_MAX &&
                ((s->initialized>=other->value.value.offset && s->initialized-other->value.value.offset<other->value.value.bytes) ||
                 (i!=j && s->initialized==other->initialized))) return false;
            if(j<i && overlap(s->value.value.offset,s->value.value.bytes,other->value.value.offset,other->value.value.bytes)) return false;
        }
    }
    if(owning!=l->cleanupCount) return false;
    for(size_t i=0;i<l->cleanupCount;++i) {
        if(l->cleanup[i]>=l->count || l->slots[l->cleanup[i]].value.value.kind==XL6_CAPTURE_TRIVIAL) return false;
        for(size_t j=0;j<i;++j) if(l->cleanup[j]==l->cleanup[i]) return false;
    }
    return true;
}
static bool activation_data_valid(const XL6_ActivationCaptureLayoutV2* l,const void* data,size_t bytes)
{
    if(!data || !activation_valid(l,bytes) || xrtMemRangesOverlap(data,l->bytes,l,sizeof(*l)) ||
        (l->count && xrtMemRangesOverlap(data,l->bytes,l->slots,l->count*sizeof(*l->slots))) ||
        (l->cleanupCount && xrtMemRangesOverlap(data,l->bytes,l->cleanup,l->cleanupCount*sizeof(*l->cleanup)))) return false;
    for(size_t i=0;i<l->count;++i) {
        const XL6_ActivationCaptureSlotV2* s=&l->slots[i];
        if(!slot_address_valid(&s->value,data) || !slot_metadata_disjoint(&s->value,data,l->bytes) ||
            (s->initialized!=SIZE_MAX && ((const unsigned char*)data)[s->initialized]>1)) return false;
    }
    return true;
}
static bool activation_trace(const XL6_ActivationCaptureLayoutV2* l,const void* data,size_t bytes,XL6_ModuleOwnershipVisit visit,void* context)
{
    if(!visit || !activation_data_valid(l,data,bytes)) return false;
    for(size_t i=0;i<l->count;++i) {
        const XL6_ActivationCaptureSlotV2* s=&l->slots[i];
        if(s->initialized!=SIZE_MAX && ((const unsigned char*)data)[s->initialized] && !slot_trace(&s->value,data,visit,context)) return false;
    }
    return true;
}
static xl6_error_result activation_cleanup(const XL6_ActivationCaptureLayoutV2* l,void* data,size_t bytes,xl6_error_result incoming)
{
    xl6_error_scope s={0}; if(!cleanup_begin(&s,incoming)) return (xl6_error_result){false,incoming.error};
    if(!activation_data_valid(l,data,bytes)) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&s); }
    else for(size_t i=0;i<l->cleanupCount;++i) {
        const XL6_ActivationCaptureSlotV2* slot=&l->slots[l->cleanup[i]];
        slot_cleanup(&slot->value,l->bytes,data,(unsigned char*)data+slot->initialized);
        if(xrtGetError()) xl6ErrorScopeCaptureFailure(&s);
    }
    return xl6ErrorScopeEnd(&s);
}
const XL6_ActivationCaptureRuntimeV2* xl6ActivationCaptureRuntimeV2Get(void)
{ static const XL6_ActivationCaptureRuntimeV2 api={activation_valid,activation_trace,activation_cleanup}; return &api; }
