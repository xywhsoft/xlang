#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_array_index.h"
#include "src/runtime/xl6_sequence_projection.h"
#include <limits.h>

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
static bool write(const xvalue* source, int64_t index, const xvalue* item, Write operation)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter;
    XL6_Sequence* owner=begin(source,&layout,&converter);
    if (!owner) return false;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    bool ok=false;
    if (operation==PUSH) ok=xl6SequenceAdmitValueV1(owner,item) && allocation(xrtTypedArrayPushValue(storage,item,converter));
    else if (xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),operation==INSERT,&at))
        ok=xl6SequenceAdmitValueV1(owner,item) && allocation(operation==SET ? xrtTypedArraySetValue(storage,at,item,converter) :
                                     xrtTypedArrayInsertValue(storage,at,item,converter));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayErasedSetV1(const xvalue* source, int64_t index, const xvalue* item) { return write(source,index,item,SET); }
bool xl6ArrayErasedPushV1(const xvalue* source, const xvalue* item) { return write(source,0,item,PUSH); }
bool xl6ArrayErasedInsertV1(const xvalue* source, int64_t index, const xvalue* item) { return write(source,index,item,INSERT); }
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
    XL6_SequenceAdmissionV2 admission={0};
    if (storage && !xl6SequenceAdmissionCopyV2(owner,&admission)) {
        xerror* error=xrtTakeError(); xrtTypedArrayDestroy(storage); storage=NULL;
        xrtClearError(); xrtSetErrorTake(error);
    }
    xl6SequenceEndV1(owner);
    xvalue* result=xl6SequenceWrapV1(storage,layout.identity,layout.elementIdentity,
        layout.transfer,layout.size,layout.type);
    if (!result) { xl6SequenceAdmissionDropV2(&admission); return NULL; }
    owner=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,layout.identity,
        layout.elementIdentity,layout.transfer,layout.size,layout.type);
    if (!owner || !xl6SequenceAdmissionTakeV2(owner,&admission) || !xl6SequenceBindProjectionV1(owner,converter)) {
        if (owner) xl6SequenceEndV1(owner);
        xerror* saved=xrtTakeError(); xrtValueRelease(result); xrtClearError(); xrtSetErrorTake(saved);
        xl6SequenceAdmissionDropV2(&admission);
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
