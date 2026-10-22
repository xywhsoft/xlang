#ifndef UDP_TRANSFER_TEST_H
#define UDP_TRANSFER_TEST_H
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_native_class_borrow.h"
static XL6_NativeClassArrayContextV1 testContext(unsigned kind,const XL6_NativeHandlePolicyV1* policy)
{return (XL6_NativeClassArrayContextV1){{700+kind,800+kind,3,sizeof(xvalue*),xrtTypeValue()},"handle",6,policy};}
static xvalue* testObject(void* raw,const XL6_NativeHandlePolicyV1* policy,unsigned kind)
{
    xvalue* h=xl6NativeHandleTakeV1(&raw,policy);CHECK(h&&!raw);xvalue* object=xrtValueObject();CHECK(object);
    CHECK(xrtValueObjectSetNewPostCommit(object,(xstrview){"handle",6},h));
    CHECK(xrtValueObjectSetNewPostCommit(object,(xstrview){"$xlir.class.runtime-type",24},xrtValueInt(900+kind)));return object;
}
static xvalue* testArray(unsigned kind,xvalue* const* objects,size_t count)
{
    uint64_t tag=900+kind;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&tag,1};
    xvalue* value=xl6SequenceNominalCreateV1(1,700+kind,800+kind,&schema,false,800+kind);CHECK(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,1,700+kind,800+kind,3,sizeof(xvalue*),xrtTypeValue());CHECK(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i)CHECK(xl6SequenceAdmitSlotV1(pin,&objects[i])&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&objects[i]));
    xl6SequenceEndV1(pin);return value;
}
static xvalue* testTexts(size_t count)
{
    xvalue* value=xl6SequencePrimitiveCreateV1(1,301,302,2,sizeof(xstrview),xl6StringTypeV1(),XL6_VALUE_ADMIT_STRING,false,302);CHECK(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,1,301,302,2,sizeof(xstrview),xl6StringTypeV1());CHECK(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i){xstrview text=i%2?(xstrview){"B\xe4\xbd\xa0",4}:(xstrview){"A\0",2};
        CHECK(xl6SequenceAdmitSlotV1(pin,&text)&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&text));}
    xl6SequenceEndV1(pin);return value;
}
static xvalue* testBytes(void)
{
    xvalue* value=xl6SequenceReferenceCreateV1(1,401,402,XL6_VALUE_ADMIT_BYTES,false,402);CHECK(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,1,401,402,3,sizeof(xvalue*),xrtTypeValue());CHECK(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<2;++i){xvalue* item=xrtValueBytes((xbytesview){(const uint8_t*)"b\0",2});CHECK(item);
        CHECK(xl6SequenceAdmitSlotV1(pin,&item)&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&item));xrtValueRelease(item);}
    xl6SequenceEndV1(pin);return value;
}
#endif
