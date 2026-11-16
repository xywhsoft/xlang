#include "analysis/ir_type_link.h"
#include <xrt.h>
#include <stdint.h>
#include <string.h>
_Static_assert(sizeof(XL6_IrTypeId)==4,"Audit IR type width");
_Static_assert(sizeof(XL6_SymbolId)==4,"Audit symbol width");
_Static_assert(sizeof(XL6_TypeId)==4,"Audit source type width");

struct xls_ir_type_link {
    XL6_IrTypePool pool;size_t bytes,peak,retained;xls_ir_table_unit** units;
};
typedef struct type_state {
    const XL6_IrTypePool* current;const xls_ir_type_binding* binding;
    size_t index;XL6_IrTypeId source,destination;xls_ir_type_error* error;
} type_state;
static int add(size_t* total,size_t n){if(n>SIZE_MAX-*total)return 0;*total+=n;return 1;}
static int region(size_t* bytes,size_t n,size_t width,size_t align,size_t* at)
{
    if(!add(bytes,(align-*bytes%align)%align) || n>SIZE_MAX/width)return 0;
    *at=*bytes;return add(bytes,n*width);
}
static int fail(type_state* s,xls_ir_type_reason why,const char* field,xls_ir_type_domain d,uint64_t value)
{
    if(s->error)*s->error=(xls_ir_type_error){why,s->index,s->source,s->destination,d,value,field};
    return 0;
}
static int reference(type_state* s,xls_ir_type_domain d,const char* field,uint64_t value,uint64_t* mapped)
{
    if(!value){*mapped=0;return 1;}
    if(!s->binding->map(s->binding->user,d,value,mapped))return fail(s,XLS_IR_TYPE_UNMAPPED,field,d,value);
    if(!*mapped || *mapped>UINT32_MAX || (d==XLS_IR_TYPE_REFERENCE && *mapped>s->current->count))
        return fail(s,XLS_IR_TYPE_RANGE,field,d,value);
    return 1;
}
static int textEqual(const char* a,const char* b)
{return (!a && !b) || (a && b && !strcmp(a,b));}
static int equal(const XL6_IrType* a,const XL6_IrType* b)
{
#define TYPE_VALUE(f) if(a->f!=b->f)return 0
#define TYPE_ARRAY(f) if(memcmp(a->f,b->f,sizeof(a->f)))return 0
#define TYPE_TEXT(f) if(!textEqual(a->f,b->f))return 0
#define TYPE_VECTOR(f,count) if(!!a->f!=!!b->f || (a->f && a->count && memcmp(a->f,b->f,a->count*sizeof(*a->f))))return 0
#include "ir-type-compare.inc"
#undef TYPE_VECTOR
#undef TYPE_TEXT
#undef TYPE_ARRAY
#undef TYPE_VALUE
    return 1;
}
static int relocate(type_state* s,XL6_IrType* dst)
{
    uint64_t mapped;
#define TYPE_REFERENCE(f,d) do{if(!reference(s,XLS_IR_TYPE_##d,#f,dst->f,&mapped))return 0;dst->f=mapped;}while(0)
#include "ir-type-reference.inc"
#undef TYPE_REFERENCE
    return 1;
}
void xlsIrTypeLinkDestroy(xls_ir_type_link* image)
{
    if(!image)return;
    for(size_t i=0;i<image->retained;++i)xlsIrTableUnitRelease(image->units[i]);
    xrtFree(image);
}
xls_ir_type_link* xlsIrTypeLink(const XL6_IrTypePool* current,const xls_ir_type_binding* bindings,
    size_t count,size_t limit,int(*cancelled)(void*),void* user,
    xls_ir_graph_status* status,xls_ir_type_error* error)
{
    xls_ir_type_link* image=NULL;uint32_t* destinations=NULL;xls_ir_graph_status result=XLS_IR_GRAPH_INVALID;
    type_state state={.current=current,.error=error};if(error)*error=(xls_ir_type_error){0};
    if(!current || current->count>current->capacity || current->count>UINT32_MAX ||
       (current->count && !current->items) || (current->slotCapacity && !current->slots) ||
       count!=current->count || (count && !bindings)){
        fail(&state,XLS_IR_TYPE_INPUT,"pool",XLS_IR_TYPE_REFERENCE,0);goto failed;
    }
    if(cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
    size_t bytes=sizeof(*image),items,units,slots=0,vectors,scratch,peak;
    result=XLS_IR_GRAPH_BUDGET;
    if(!region(&bytes,count,sizeof(XL6_IrType),_Alignof(XL6_IrType),&items) ||
       !region(&bytes,count,sizeof(xls_ir_table_unit*),_Alignof(xls_ir_table_unit*),&units) ||
       (current->slots && !region(&bytes,current->slotCapacity ? current->slotCapacity : 1,sizeof(size_t),_Alignof(size_t),&slots)))goto failed;
    vectors=bytes;
    for(size_t i=0;i<count;++i){
        state.index=i;state.binding=&bindings[i];const xls_ir_type_binding* b=&bindings[i];
        if(!(i%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        result=XLS_IR_GRAPH_INVALID;
        if(!b->unit || !b->map || strcmp(xlsIrTableUnitName(b->unit),"types.items") ||
           xlsIrTableUnitRows(b->unit)!=1 || xlsIrTableUnitRowBytes(b->unit)!=sizeof(XL6_IrType) || xlsIrTableUnitFirstRow(b->unit)>=UINT32_MAX){
            fail(&state,XLS_IR_TYPE_INPUT,"unit",XLS_IR_TYPE_REFERENCE,0);goto failed;}
        if(!b->producer || b->producer!=xlsIrTableUnitProducer(b->unit)){fail(&state,XLS_IR_TYPE_NAMESPACE,"producer",XLS_IR_TYPE_REFERENCE,0);goto failed;}
        const XL6_IrType* src=xlsIrTableUnitRow(b->unit,0);size_t ignored;
        if((src->paramCount && !src->params) || (src->returnCount && !src->returns)){
            fail(&state,XLS_IR_TYPE_INPUT,"vectors",XLS_IR_TYPE_REFERENCE,0);goto failed;}
        result=XLS_IR_GRAPH_BUDGET;
        if((src->params && !region(&bytes,src->paramCount ? src->paramCount : 1,sizeof(XL6_IrTypeId),_Alignof(XL6_IrTypeId),&ignored)) ||
           (src->returns && !region(&bytes,src->returnCount ? src->returnCount : 1,sizeof(XL6_IrTypeId),_Alignof(XL6_IrTypeId),&ignored)))goto failed;
    }
    scratch=0;size_t ignored;
    if(!region(&scratch,count,sizeof(uint32_t),_Alignof(uint32_t),&ignored))goto failed;
    peak=bytes;if(!add(&peak,scratch) || peak>limit)goto failed;
    result=XLS_IR_GRAPH_MEMORY;image=xrtCalloc(1,bytes);if(!image)goto failed;
    image->bytes=bytes;image->peak=peak;image->pool=(XL6_IrTypePool){.count=count,.capacity=count,.slotCapacity=current->slotCapacity};
    image->pool.items=count ? (XL6_IrType*)((unsigned char*)image+items) : NULL;
    image->units=count ? (xls_ir_table_unit**)((unsigned char*)image+units) : NULL;
    image->pool.slots=current->slots ? (size_t*)((unsigned char*)image+slots) : NULL;
    if(count){destinations=xrtMalloc(scratch);if(!destinations)goto failed;}
    result=XLS_IR_GRAPH_INVALID;
    for(size_t i=0;i<current->slotCapacity;++i){
        if(!(i%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        /* Native-owned lookup metadata is opaque here,not a second ID map. */
        image->pool.slots[i]=current->slots[i];
    }
    for(size_t i=0;i<count;++i){
        if(!(i%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        state.index=i;state.binding=&bindings[i];state.source=(XL6_IrTypeId)(xlsIrTableUnitFirstRow(bindings[i].unit)+1);state.destination=0;
        uint64_t id;if(!reference(&state,XLS_IR_TYPE_REFERENCE,"typeId",state.source,&id))goto failed;
        state.destination=(XL6_IrTypeId)id;
        if(image->units[id-1]){fail(&state,XLS_IR_TYPE_DUPLICATE,"typeId",XLS_IR_TYPE_REFERENCE,state.source);goto failed;}
        image->units[id-1]=bindings[i].unit;destinations[i]=(uint32_t)id;
    }
    for(size_t i=0;i<count;++i){
        if(!(i%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        state.index=i;state.binding=&bindings[i];state.source=(XL6_IrTypeId)(xlsIrTableUnitFirstRow(bindings[i].unit)+1);state.destination=destinations[i];
        const XL6_IrType* src=xlsIrTableUnitRow(bindings[i].unit,0);const XL6_IrType* target=&current->items[state.destination-1];XL6_IrType* dst=&image->pool.items[state.destination-1];
        memcpy(dst,src,sizeof(*dst));size_t at;
        if(src->paramCount!=target->paramCount || src->returnCount!=target->returnCount){fail(&state,XLS_IR_TYPE_DIFFERENT,"arity",XLS_IR_TYPE_REFERENCE,0);goto failed;}
#define VECTOR(f,n) do{if(src->f){if(!region(&vectors,src->n ? src->n : 1,sizeof(*src->f),_Alignof(XL6_IrTypeId),&at) || vectors>bytes)goto failed; \
    dst->f=(void*)((unsigned char*)image+at);for(size_t j=0;j<src->n;++j){ \
        if(!(j%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;} \
        uint64_t value;if(!reference(&state,XLS_IR_TYPE_REFERENCE,#f,src->f[j],&value))goto failed;dst->f[j]=(XL6_IrTypeId)value;}}}while(0)
        VECTOR(params,paramCount);VECTOR(returns,returnCount);
#undef VECTOR
        if(!relocate(&state,dst))goto failed;
        /* These are current producer-derived data,never reconstructed from
         * stale integer spellings or a second implementation of type ABI. */
        memcpy(dst->cName,target->cName,sizeof(dst->cName));
        memcpy(dst->errorResultCName,target->errorResultCName,sizeof(dst->errorResultCName));
        dst->hash=target->hash;
        if(dst->kind==XL6_IR_TYPE_TYPE_PARAM){uint64_t value;
            if(!reference(&state,XLS_IR_TYPE_SOURCE,"sourceTypeId",src->sourceTypeId,&value))goto failed;
            dst->sourceTypeId=(XL6_TypeId)value;
        }else dst->sourceTypeId=target->sourceTypeId;
        if(!equal(dst,target)){fail(&state,XLS_IR_TYPE_DIFFERENT,"record",XLS_IR_TYPE_REFERENCE,0);goto failed;}
    }
    if(vectors!=bytes)goto failed;
    if(cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
    for(size_t i=0;i<count;++i)xlsIrTableUnitRetain(image->units[i]);
    image->retained=count;xrtFree(destinations);if(status)*status=XLS_IR_GRAPH_OK;return image;
failed:
    xrtFree(destinations);xlsIrTypeLinkDestroy(image);if(status)*status=result;return NULL;
}
const XL6_IrTypePool* xlsIrTypeLinkPool(const xls_ir_type_link* image){return image ? &image->pool : NULL;}
size_t xlsIrTypeLinkBytes(const xls_ir_type_link* image){return image ? image->bytes : 0;}
size_t xlsIrTypeLinkPeakBytes(const xls_ir_type_link* image){return image ? image->peak : 0;}
