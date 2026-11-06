#include "xl6_value_read_view.h"
#include "xl6_dict_graph_access.h"
#include "xl6_string_type.h"
#include <string.h>

XL_ValueBorrowV1 xlValueBorrowV1(const xvalue* source)
{
    XL_ValueBorrowV1 value={0};
    if (!source) return value;
    value.kind=xrtValueType(source); value.ref=source;
    switch (value.kind) {
    case XVALUE_BOOL: (void)xrtValueGetBool(source,&value.data.boolean); break;
    case XVALUE_INT: (void)xrtValueGetInt(source,&value.data.integer); break;
    case XVALUE_UINT: (void)xrtValueGetUInt(source,&value.data.unsignedInteger); break;
    case XVALUE_FLOAT: (void)xrtValueGetFloat(source,&value.data.number); break;
    case XVALUE_STRING: (void)xrtValueGetString(source,&value.data.text); break;
    case XVALUE_BYTES: (void)xrtValueGetBytes(source,&value.data.bytes); break;
    case XVALUE_POINTER: { ptr raw=NULL; (void)xrtValueGetPointer(source,&raw);
        value.data.unsignedInteger=(uintptr_t)raw; break; }
    default: break;
    }
    return value;
}
static bool readValid(const XL_ValueReadV1* view)
{
    if (!view || view->self!=view || !view->source || !view->mode) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    return true;
}
bool xlValueReadBeginV1(XL_ValueBorrowV1 value,XL_ValueReadV1* view)
{
    if (!view || view->self || view->source || view->mode || !value.ref) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    const xvalue* source=value.ref;
    xvaluetype kind=xrtValueType(source);
    if (kind==XVALUE_ARRAY || kind==XVALUE_OBJECT) {
        if (!xrtValueViewBeginV1(source,&view->wire)) return false;
        view->count=view->wire.Count; view->kind=kind; view->mode=1;
    } else if (kind==XVALUE_HANDLE) {
        ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
        if (!xrtValueGetHandle(source,&raw,&ops,&user)) return false;
        XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout={0}; const xvalueconverter* converter=NULL;
        int found=xl6SequenceGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&owner,&layout,&converter);
        if (found<0) return false;
        if (found) {
            view->owner=owner; view->storage=xl6SequenceStorageV1(owner);
            view->count=xrtTypedArrayCount(view->storage); view->kind=XVALUE_ARRAY; view->mode=2;
        } else {
            XL6_DictGraphViewV1 dict={0};
            found=xl6DictGraphBeginV1(ops,raw,user,xrtValueTypeId(source),&dict);
            if (found<0) return false;
            if (!found) { xrtSetErrorKind(XERR_TYPE); return false; }
            view->owner=dict.owner; view->storage=dict.storage; layout=dict.layout.value;
            view->count=xrtTypedDictCount(dict.storage); view->kind=XVALUE_OBJECT; view->mode=3;
            view->identity=dict.layout.identity; view->slotIdentity=layout.identity;
        }
        if (view->mode==2) view->identity=layout.identity;
        view->elementIdentity=layout.elementIdentity; view->transfer=layout.transfer;
        view->slotSize=layout.size; view->slotType=layout.type;
    } else { xrtSetErrorKind(XERR_TYPE); return false; }
    view->source=source; view->self=view; return true;
}
bool xlValueReadEndV1(XL_ValueReadV1* view)
{
    if (view && !view->self && !view->source && !view->mode) return true;
    if (!readValid(view)) return false;
    if (view->mode==1) {
        if (!xrtValueViewEndV1(&view->wire)) return false;
    } else if (view->mode==2) xl6SequenceEndV1(view->owner);
    else if (view->mode==3) {
        XL6_DictGraphViewV1 dict={view->owner,
            {view->identity,{0,view->elementIdentity,view->transfer,view->slotSize,view->slotType}},view->storage};
        /* Dict's value layout includes its admitted witness identity. */
        dict.layout.value.identity=view->slotIdentity;
        xl6DictGraphEndV1(&dict);
    } else { xrtSetErrorKind(XERR_STATE); return false; }
    *view=(XL_ValueReadV1){0}; return true;
}
static XL_ValueBorrowV1 readSlot(const XL_ValueReadV1* view,const void* slot)
{
    XL_ValueBorrowV1 value={0};
    if (!slot) return value;
    if (view->transfer==3) {
        const xvalue* child=*(xvalue* const*)slot;
        return xlValueBorrowV1(child ? child : xrtValueNull());
    }
    const xrttype* type=view->slotType;
    if (view->transfer==2 && view->slotSize==sizeof(xstrview) &&
        (xl6StringTypeAbiV1(type) || type==xrtTypeStringView())) {
        value.kind=XVALUE_STRING; memcpy(&value.data.text,slot,sizeof(xstrview)); return value;
    }
#define SCALAR(Type,Kind,Member,CType) \
    if (type==Type() && view->slotSize==sizeof(CType)) { CType raw; memcpy(&raw,slot,sizeof(raw)); value.kind=Kind; value.data.Member=raw; return value; }
    if (view->transfer==1) {
        if (type==xrtTypePointer() && view->slotSize==sizeof(ptr)) {
            ptr raw=NULL; memcpy(&raw,slot,sizeof(raw)); value.kind=XVALUE_POINTER;
            value.data.unsignedInteger=(uintptr_t)raw; return value;
        }
        SCALAR(xrtTypeBool,XVALUE_BOOL,boolean,bool)
        SCALAR(xrtTypeInt8,XVALUE_INT,integer,int8_t)
        SCALAR(xrtTypeInt16,XVALUE_INT,integer,int16_t)
        SCALAR(xrtTypeInt32,XVALUE_INT,integer,int32_t)
        SCALAR(xrtTypeInt64,XVALUE_INT,integer,int64_t)
        SCALAR(xrtTypeUInt8,XVALUE_UINT,unsignedInteger,uint8_t)
        SCALAR(xrtTypeUInt16,XVALUE_UINT,unsignedInteger,uint16_t)
        SCALAR(xrtTypeUInt32,XVALUE_UINT,unsignedInteger,uint32_t)
        SCALAR(xrtTypeUInt64,XVALUE_UINT,unsignedInteger,uint64_t)
        SCALAR(xrtTypeFloat32,XVALUE_FLOAT,number,float)
        SCALAR(xrtTypeFloat64,XVALUE_FLOAT,number,double)
    }
#undef SCALAR
    xrtSetErrorKind(XERR_TYPE); return value;
}
XL_ValueBorrowV1 xlValueReadAtV1(const XL_ValueReadV1* view,size_t index,xstrview* key)
{
    XL_ValueBorrowV1 empty={0};
    if (!readValid(view)) return empty;
    if (index>=view->count) { xrtSetErrorKind(XERR_RANGE); return empty; }
    if (view->mode==1) return xlValueBorrowV1(xrtValueViewAtV1(&view->wire,index,key));
    if (view->mode==2) {
        if (key) *key=(xstrview){0};
        return readSlot(view,xrtTypedArrayConstGet(view->storage,index));
    }
    xtypeddictiter iterator={0}; xstrview name={0}; const void* slot=NULL;
    if (!xrtTypedDictIterBegin(view->storage,&iterator)) return empty;
    for (size_t i=0;i<=index;++i) slot=xrtTypedDictIterNext(&iterator,&name);
    xrtTypedDictIterEnd(&iterator);
    if (key) *key=name;
    return readSlot(view,slot);
}
XL_ValueBorrowV1 xlValueReadFindV1(const XL_ValueReadV1* view,xstrview key)
{
    XL_ValueBorrowV1 empty={0};
    if (!readValid(view)) return empty;
    if (view->kind!=XVALUE_OBJECT) { xrtSetErrorKind(XERR_TYPE); return empty; }
    if (view->mode==1) return xlValueBorrowV1(xrtValueViewFindV1(&view->wire,key));
    return readSlot(view,xrtTypedDictConstGet(view->storage,key));
}
xvalue* xlValueBorrowCopyV1(XL_ValueBorrowV1 value)
{
    if (value.ref) return xrtValueRetain(value.ref);
    switch (value.kind) {
    case XVALUE_NULL: return xrtValueRetain(xrtValueNull());
    case XVALUE_BOOL: return xrtValueRetain(xrtValueBool(value.data.boolean));
    case XVALUE_INT: return xrtValueInt(value.data.integer);
    case XVALUE_UINT: return xrtValueUInt(value.data.unsignedInteger);
    case XVALUE_FLOAT: return xrtValueFloat(value.data.number);
    case XVALUE_STRING: return xrtValueString(value.data.text);
    case XVALUE_BYTES: return xrtValueBytes(value.data.bytes);
    case XVALUE_POINTER: return xrtValuePointer((ptr)(uintptr_t)value.data.unsignedInteger);
    default: xrtSetErrorKind(XERR_TYPE); return NULL;
    }
}
