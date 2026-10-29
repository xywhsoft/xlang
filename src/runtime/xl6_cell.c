#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VALUE_CONTAINER
#include "src/common/xl6_xrt.h"
#include "xl6_cell.h"
#include "xl6_ownership_scope.h"
#include <string.h>

struct xlir_cell {
    volatile int32_t refs;
    XL6_CellKind kind;
    bool initialized;
    bool cleared;
    const void* claim;
    void* retired;
    size_t size;
    const XL6_CellLeafOpsV1* leaf;
    const void* metadata;
    union { bool b; uint32_t character; int64_t i64; uint64_t u64; double f64; xstrview string; xvalue* value; void* bytes; } payload;
};
static bool cell_owning(const XL6_Cell* cell)
{ return cell->kind==XL6_CELL_STRING || cell->kind==XL6_CELL_XVALUE || cell->kind==XL6_CELL_NATIVE_LEAF_V2; }
static void* cell_data(const XL6_Cell* cell)
{ return cell->kind==XL6_CELL_TRIVIAL_BYTES || cell->kind==XL6_CELL_NATIVE_LEAF_V2 ? cell->payload.bytes : (void*)&cell->payload; }
static bool cell_size_valid(XL6_CellKind kind, size_t size)
{
    switch(kind) {
    case XL6_CELL_BOOL: return size==sizeof(bool);
    case XL6_CELL_CHAR: return size==sizeof(uint32_t);
    case XL6_CELL_I64: return size==sizeof(int64_t);
    case XL6_CELL_U64: case XL6_CELL_TYPE_ID: return size==sizeof(uint64_t);
    case XL6_CELL_F64: return size==sizeof(double);
    case XL6_CELL_STRING: return size==sizeof(xstrview);
    case XL6_CELL_XVALUE: return size==sizeof(xvalue*);
    case XL6_CELL_TRIVIAL_BYTES: return size!=0;
    default: return false;
    }
}
static XL6_Cell* cell_create_body(XL6_CellKind kind, size_t size)
{
    XL6_Cell* cell;
    if (!cell_size_valid(kind,size)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    cell=xrtMalloc(sizeof(*cell)); if (!cell) return NULL;
    *cell=(XL6_Cell){0}; cell->refs=1; cell->kind=kind; cell->size=size;
    if(kind==XL6_CELL_TRIVIAL_BYTES) {
        cell->payload.bytes=xrtMalloc(size);
        if(!cell->payload.bytes) { xrtFree(cell); return NULL; }
        memset(cell->payload.bytes,0,size);
    }
    return cell;
}
static bool cell_valid(const XL6_Cell* cell)
{
    return cell && (cell->kind==XL6_CELL_NATIVE_LEAF_V2
        ? cell->leaf && cell->metadata && cell->leaf->Validate && cell->leaf->Destroy &&
          cell->leaf->Validate(cell->metadata,cell->size)
        : !cell->leaf && !cell->metadata && cell_size_valid(cell->kind,cell->size));
}
static XL6_Cell* cell_create_leaf_body(size_t size,const XL6_CellLeafOpsV1* ops,const void* metadata)
{
    if(!size || !ops || !ops->Validate || !ops->Destroy || !metadata || !ops->Validate(metadata,size)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    XL6_Cell* cell=xrtCalloc(1,sizeof(*cell));if(!cell)return NULL;
    cell->payload.bytes=xrtCalloc(1,size);
    if(!cell->payload.bytes){xrtFree(cell);return NULL;}
    cell->refs=1;cell->kind=XL6_CELL_NATIVE_LEAF_V2;cell->size=size;cell->leaf=ops;cell->metadata=metadata;
    return cell;
}
XL6_Cell* xl6CellCreateLeafV1(size_t size,const XL6_CellLeafOpsV1* ops,const void* metadata)
{XL6_OWNERSHIP_RETURN(XL6_Cell*,cell_create_leaf_body(size,ops,metadata));}
static XL6_Cell* cell_create(XL6_CellKind kind, size_t size)
{ XL6_OWNERSHIP_RETURN(XL6_Cell*,cell_create_body(kind,size)); }
static void cell_initialize_body(XL6_Cell* cell, const void* value, size_t size)
{
    if(!cell || cell->cleared || cell->initialized || !value || size!=cell->size) abort();
    memcpy(cell_data(cell),value,size); cell->initialized=true;
}
static void cell_initialize(XL6_Cell* cell, const void* value, size_t size)
{ XL6_OWNERSHIP_RETURN_VOID(cell_initialize_body(cell,value,size)); }
static void cell_load(const XL6_Cell* cell, void* output, size_t size)
{
    if(!cell || cell->cleared || !cell->initialized || !output || size!=cell->size) abort();
    memcpy(output,cell_data(cell),size);
}
static void cell_store_body(XL6_Cell* cell, const void* value, size_t size)
{
    if(!cell || cell->cleared || !cell->initialized || cell_owning(cell) || !value || size!=cell->size) abort();
    memcpy(cell_data(cell),value,size);
}
static void cell_store(XL6_Cell* cell, const void* value, size_t size)
{ XL6_OWNERSHIP_RETURN_VOID(cell_store_body(cell,value,size)); }
static void cell_exchange_body(XL6_Cell* cell, const void* value, void* output, size_t size)
{
    if(!cell || cell->cleared || !cell->initialized || !cell_owning(cell) || !value || !output || size!=cell->size ||
        xrtMemRangesOverlap(value,size,output,size)) abort();
    memcpy(output,cell_data(cell),size); memcpy(cell_data(cell),value,size);
}
static void cell_exchange(XL6_Cell* cell, const void* value, void* output, size_t size)
{ XL6_OWNERSHIP_RETURN_VOID(cell_exchange_body(cell,value,output,size)); }
static XL6_Cell* cell_retain_body(XL6_Cell* cell)
{ if(cell && xrtRefRetain(&cell->refs)<0) abort(); return cell; }
static XL6_Cell* cell_retain(XL6_Cell* cell)
{ XL6_OWNERSHIP_RETURN(XL6_Cell*,cell_retain_body(cell)); }
static void cell_release(XL6_Cell* cell)
{
    int32_t refs;
    xrtownershipscope mutation={0};
    if(!cell) return;
    if(!xrtOwnershipMutationBegin(&mutation)) abort();
    refs=xrtRefRelease(&cell->refs); if(refs<0) abort();
    if(refs) { if(!xrtOwnershipScopeEnd(&mutation)) abort(); return; }
    if(cell->retired) abort();
    /* Count refuses this zero-count private cell; release children outside
     * our mutation so their semantic finalizers can wait/inspect safely. */
    if(!xrtOwnershipScopeEnd(&mutation)) abort();
    if(cell->kind==XL6_CELL_STRING) xrtFree((void*)cell->payload.string.Data);
    else if(cell->kind==XL6_CELL_XVALUE) xrtValueRelease(cell->payload.value);
    else if(cell->kind==XL6_CELL_NATIVE_LEAF_V2){
        if(cell->payload.bytes){cell->leaf->Destroy(cell->metadata,cell->payload.bytes);xrtFree(cell->payload.bytes);}
    }
    else if(cell->kind==XL6_CELL_TRIVIAL_BYTES) xrtFree(cell->payload.bytes);
    xrtFree(cell);
}
static bool cell_count(const void* data, size_t* count)
{
    const XL6_Cell* cell=data; int32_t refs;
    if(!cell || cell->cleared || !count || (refs=cell->refs)<=0) return false;
    *count=(size_t)refs; return true;
}
static bool cell_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_Cell* cell=data;
    if(!cell || cell->cleared || !visit || !cell_valid(cell)) return false;
    return cell->kind!=XL6_CELL_XVALUE || visit(xrtValueOwnership(cell->payload.value),context);
}
static const xrtownershipops cell_ops={cell_count,cell_trace};
static xrtownershipref cell_ownership(const XL6_Cell* cell)
{ return (xrtownershipref){cell,cell ? &cell_ops : NULL}; }
const XL6_CellRuntimeV1* xl6CellRuntimeV1Get(void)
{
    static const XL6_CellRuntimeV1 api={cell_create,cell_initialize,cell_load,cell_store,cell_exchange,
        cell_retain,cell_release,cell_ownership};
    return &api;
}
static bool cell_hold(const void* data)
{
    XL6_Cell* cell=(XL6_Cell*)data;
    return cell->refs>0 && cell->refs<INT32_MAX && xrtRefRetain(&cell->refs)>0;
}
static void cell_drop(const void* data) { cell_release((XL6_Cell*)data); }
static bool cell_claim(const void* data,const void* token)
{
    XL6_Cell* cell=(XL6_Cell*)data;
    if(cell->cleared || (cell->claim && cell->claim!=token)) return false;
    cell->claim=token; return true;
}
static void cell_restore(const void* data,const void* token)
{
    XL6_Cell* cell=(XL6_Cell*)data;
    if(cell->claim!=token || cell->cleared) abort();
    cell->claim=NULL;
}
static void cell_clear(const void* data,const void* token)
{
    XL6_Cell* cell=(XL6_Cell*)data;
    if(cell->claim!=token || cell->cleared) abort();
    if(cell->kind==XL6_CELL_XVALUE) { cell->retired=cell->payload.value; cell->payload.value=NULL; }
    else if(cell->kind==XL6_CELL_STRING) { cell->retired=(void*)cell->payload.string.Data; cell->payload.string=(xstrview){0}; }
    else if(cell->kind==XL6_CELL_TRIVIAL_BYTES || cell->kind==XL6_CELL_NATIVE_LEAF_V2) { cell->retired=cell->payload.bytes; cell->payload.bytes=NULL; }
    cell->cleared=true;
}
static bool cell_finish(const void* data,const void* token)
{
    XL6_Cell* cell=(XL6_Cell*)data; void* retired;
    if(cell->claim!=token || !cell->cleared) abort();
    retired=cell->retired; cell->retired=NULL;
    if(cell->kind==XL6_CELL_XVALUE) xrtValueRelease(retired);
    else if(cell->kind==XL6_CELL_NATIVE_LEAF_V2 && retired){cell->leaf->Destroy(cell->metadata,retired);xrtFree(retired);}
    else xrtFree(retired);
    return true;
}
const xrtownershipadapterv1* xl6CellOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),cell_hold,cell_drop,
        cell_claim,cell_restore,NULL,cell_clear,cell_finish};
    const XL6_Cell* cell;
    if(!ref.Data || ref.Ops!=&cell_ops) return NULL;
    cell=ref.Data;
    return !cell->cleared && cell->refs>0 && cell_valid(cell) ? &adapter : NULL;
}
