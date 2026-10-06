#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_array_index.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

static XL6_Sequence* begin(const xvalue* value, XL6_SequenceLayoutV1* layout,
    const xvalueconverter** converter)
{
    XL6_Sequence* owner=NULL;
    int status=xl6SequenceBeginErasedArrayV1(value,&owner,layout,converter);
    if (!status) xrtSetErrorKind(XERR_TYPE);
    return status==1 ? owner : NULL;
}
static bool allocation(bool ok)
{ if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return ok; }
xvalue* xl6ArrayEraseV1(const xvalue* source, uint64_t identity, uint64_t elementIdentity,
    unsigned int transfer, size_t size, const xrttype* type, const xvalueconverter* converter)
{
    if (transfer==3 && converter) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,
        identity,elementIdentity,transfer,size,type);
    if (!owner) return NULL;
    const xvalueconverter* policy=transfer==3 ? xl6ArrayReferenceConverterV1() : converter;
    xvalue* result=NULL;
    if (xl6SequenceBindProjectionV1(owner,policy)) result=xrtValueRetain(source);
    xl6SequenceEndV1(owner); return result;
}
static xvalue* read(const xvalue* source, int64_t index, bool take)
{
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter;
    XL6_Sequence* owner=begin(source,&layout,&converter);
    if (!owner) return NULL;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    xvalue* result=NULL;
    if (xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),false,&at)) {
        result=xl6SequenceProjectionTakeV1(owner,xrtTypedArrayGetValue(storage,at,converter));
        (void)allocation(result!=NULL);
        if (result && take && !xrtTypedArrayRemove(storage,at,1)) {
            xerror* error=xrtTakeError(); xrtValueRelease(result); result=NULL;
            xrtClearError(); xrtSetErrorTake(error);
        }
    }
    xl6SequenceEndV1(owner); return result;
}
xvalue* xl6ArrayErasedReadV1(const xvalue* source, int64_t index) { return read(source,index,false); }
xvalue* xl6ArrayErasedTakeV1(const xvalue* source, int64_t index) { return read(source,index,true); }
typedef enum Write { SET, PUSH, INSERT } Write;
static bool writePinned(XL6_Sequence* owner, const xvalueconverter* converter,
    int64_t index, const xvalue* item, Write operation)
{
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    bool ok=false;
    if (operation==PUSH) ok=xl6SequenceAdmitValueV1(owner,item) && allocation(xrtTypedArrayPushValue(storage,item,converter));
    else if (xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),operation==INSERT,&at))
        ok=xl6SequenceAdmitValueV1(owner,item) && allocation(operation==SET ? xrtTypedArraySetValue(storage,at,item,converter) :
                                     xrtTypedArrayInsertValue(storage,at,item,converter));
    return ok;
}
static bool write(const xvalue* source, int64_t index, const xvalue* item, Write operation)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter;
    XL6_Sequence* owner=begin(source,&layout,&converter);
    if (!owner) return false;
    bool ok=writePinned(owner,converter,index,item,operation);
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayErasedSetV1(const xvalue* source, int64_t index, const xvalue* item) { return write(source,index,item,SET); }
bool xl6ArrayErasedPushV1(const xvalue* source, const xvalue* item) { return write(source,0,item,PUSH); }
bool xl6ArrayErasedInsertV1(const xvalue* source, int64_t index, const xvalue* item) { return write(source,index,item,INSERT); }
static void consume(xvalue* item)
{
    if (!item) return;
    /* A language destructor reports to the immediate callback boundary as
     * well as to XRT's diagnostic slot. Join the primary outcome before
     * dropping, so neither channel can replace an insertion failure. */
    xl6_error_scope cleanup={0};
    xerror* primary=xrtTakeError();
    /* This operation owns its own diagnostic, not an outer cleanup's earlier
     * error. A nested native call still reports its local failure to its
     * caller; propagation keeps an already-failed parent latched separately. */
    if (!xl6ErrorScopeBegin(&cleanup)) abort();
    if (primary) {
        xrtSetErrorTake(primary);
        xl6ErrorScopeCaptureFailure(&cleanup);
    }
    xrtValueRelease(item);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}
static bool writeTake(xvalue* source, int64_t index, xvalue* item, Write operation)
{
    if (!source || !item) {
        xrtSetErrorKind(XERR_ARGUMENT); consume(item); return false;
    }
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    bool ok=false;
    if (status>0) {
        ok=writePinned(owner,converter,index,item,operation);
        /* Drop the consumed shell while the native operation pin still owns
         * the receiver. Cleanup cannot retire storage or reenter its mutation. */
        consume(item); xl6SequenceEndV1(owner); return ok;
    }
    if (!status) {
        if (xrtValueType(source)!=XVALUE_ARRAY) xrtSetErrorKind(XERR_TYPE);
        /* Take leaves the item with us on failure. The New shortcut drops
         * it before this boundary can protect the operation's first error. */
        else if (operation==PUSH) ok=xrtValueArrayAppendTake(source,&item);
        else {
            size_t at;
            if (xl6ArrayIndexV1(index,xrtValueCount(source),false,&at))
                ok=xrtValueArraySetTake(source,at,&item);
        }
    }
    consume(item); return ok;
}
bool xl6ArrayValueAppendTakeV1(xvalue* source, xvalue* item)
{ return writeTake(source,0,item,PUSH); }
bool xl6ArrayValueSetTakeV1(xvalue* source, int64_t index, xvalue* item)
{ return writeTake(source,index,item,SET); }
bool xl6ValueIndexSetTakeV1(xvalue* source, const xvalue* key, xvalue* item)
{
    if (!source || !key || !item) {
        xrtSetErrorKind(XERR_ARGUMENT); consume(item); return false;
    }
    int64_t index; xstrview text;
    if (xrtValueType(key)==XVALUE_INT && xrtValueGetInt(key,&index)) {
        if (xrtValueType(source)==XVALUE_INT_MAP) {
            bool ok=xrtValueIntMapSetTake(source,index,&item);
            consume(item); return ok;
        }
        return xl6ArrayValueSetTakeV1(source,index,item);
    }
    if (xrtValueType(key)==XVALUE_STRING && xrtValueType(source)==XVALUE_OBJECT &&
        xrtValueGetString(key,&text)) {
        bool ok=xrtValueObjectSetTake(source,text,&item);
        consume(item); return ok;
    }
    xrtSetErrorKind(XERR_TYPE); consume(item); return false;
}
int64_t xl6ArrayErasedSizeV1(const xvalue* source)
{
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter;
    XL6_Sequence* owner=begin(source,&layout,&converter);
    if (!owner) return 0;
    size_t count=xrtTypedArrayCount(xl6SequenceStorageV1(owner));
    xl6SequenceEndV1(owner);
    if (count>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)count;
}
xvalue* xl6ArrayErasedCloneV1(const xvalue* source)
{
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter;
    XL6_Sequence* owner=begin(source,&layout,&converter);
    if (!owner) return NULL;
    xtypedarray* storage=xrtTypedArrayClone(xl6SequenceStorageV1(owner));
    XL6_SequenceAdmissionV3 admission={0};
    if (storage && !xl6SequenceAdmissionCopyV3(owner,&admission)) {
        xerror* error=xrtTakeError(); xrtTypedArrayDestroy(storage); storage=NULL;
        xrtClearError(); xrtSetErrorTake(error);
    }
    xl6SequenceEndV1(owner);
    xvalue* result=xl6SequenceWrapV1(storage,layout.identity,layout.elementIdentity,
        layout.transfer,layout.size,layout.type);
    if (!result) { xl6SequenceAdmissionDropV3(&admission); return NULL; }
    owner=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,layout.identity,
        layout.elementIdentity,layout.transfer,layout.size,layout.type);
    if (!owner || !xl6SequenceAdmissionTakeV3(owner,&admission) || !xl6SequenceBindProjectionV1(owner,converter)) {
        if (owner) xl6SequenceEndV1(owner);
        xerror* saved=xrtTakeError(); xrtValueRelease(result); xrtClearError(); xrtSetErrorTake(saved);
        xl6SequenceAdmissionDropV3(&admission);
        return NULL;
    }
    xl6SequenceEndV1(owner); return result;
}
bool xl6ArrayErasedClearV1(const xvalue* source)
{
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter;
    XL6_Sequence* owner=begin(source,&layout,&converter);
    if (!owner) return false;
    xrtTypedArrayClear(xl6SequenceStorageV1(owner));
    xl6SequenceEndV1(owner);
    return !xrtGetError();
}
