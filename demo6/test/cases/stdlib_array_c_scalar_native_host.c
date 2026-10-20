#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_scalar_domain.h"
#include "src/runtime/xl6_c_scalar_domain.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_array_value.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef union Slot { bool b; int64_t i; uint64_t u; double d; float f; } Slot;
typedef struct Spec { const xrttype* type; uint64_t owner,element; } Spec;
#define LAYOUT(s) (s)->owner,(s)->element,1,(s)->type->Size,(s)->type
static Spec spec(unsigned profile)
{
    const xrttype* types[]={xrtTypeBool(),xrtTypeInt8(),xrtTypeUInt8(),xrtTypeInt16(),xrtTypeUInt16(),
        xrtTypeInt32(),xrtTypeUInt32(),xrtTypeInt64(),xrtTypeUInt64(),xrtTypeFloat32(),xrtTypeFloat64(),xrtTypeType()};
    assert(profile<sizeof(types)/sizeof(*types));
    return (Spec){types[profile],1001+profile,2001+profile};
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static xvalue* create(const Spec* s)
{ return xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,s->element); }
static xvalue* encoded(const Spec* s,uint64_t identity)
{
    xvalue* result=NULL;
    switch (s->type->Kind) {
    case XRT_TYPE_BOOL: result=xrtValueBool(true); break;
    case XRT_TYPE_SIGNED_INT: result=xrtValueInt(-7); break;
    case XRT_TYPE_UNSIGNED_INT: result=xrtValueUInt(7); break;
    case XRT_TYPE_TYPE: result=xrtValueUInt(UINT64_MAX); break;
    case XRT_TYPE_FLOAT: result=xrtValueFloat(1.5); break;
    default: abort();
    }
    assert(result);
    if (s->type!=xrtTypeBool() && identity) assert(xl6ScalarIdentityBindV1(result,identity,xl6CScalarDomainTagV1(s->type)));
    return result;
}
static xvalue* element(const Spec* s) { return encoded(s,s->element); }
static void observed(const Spec* s,xvalue* owner,const xvalue* expected)
{
    xvalue* value=xl6ArrayErasedReadV1(owner,0); assert(value && xrtValueEqual(value,expected));
    assert(xrtValueTypeId(value)==(s->type==xrtTypeBool() ? 0 : s->element));
    xrtValueRelease(value);
}
static void refused(const Spec* s,xvalue* owner,xvalue* wrong)
{
    int64_t count=xl6ArrayErasedSizeV1(owner);
    assert(!xl6ArrayErasedPushV1(owner,wrong)); error(XERR_TYPE);
    assert(!xl6ArrayErasedSetV1(owner,0,wrong)); error(XERR_TYPE);
    assert(!xl6ArrayErasedInsertV1(owner,0,wrong)); error(XERR_TYPE);
    assert(xl6ArrayErasedSizeV1(owner)==count);
    (void)s;
}
static void malformed(const Spec* s)
{
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,0,s->element,s->type,s->element)); error(XERR_ARGUMENT);
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,0,s->type,s->element)); error(XERR_ARGUMENT);
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,s->element+1)); error(XERR_ARGUMENT);
    xrttype foreign=*s->type;
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,&foreign,s->element)); error(XERR_ARGUMENT);
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,NULL,s->element)); error(XERR_ARGUMENT);
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,xrtTypeTime(),s->element)); error(XERR_ARGUMENT);
    assert(!xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,(const xrttype*)(uintptr_t)1,s->element)); error(XERR_ARGUMENT);
}
static void vectors(const Spec* s)
{
    malformed(s);
    xvalue* owner=create(s),*good=element(s); assert(owner);
    Slot slot={0}; assert(xrtValueToTyped(good,s->type,&slot,NULL));
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&slot));
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),NULL); assert(alias==owner);
    xvalue* wrong=xrtValueString((xstrview){"x",1}); assert(wrong);
    refused(s,alias,wrong); xrtValueRelease(wrong);
    refused(s,alias,xrtValueNull());
    if (s->type!=xrtTypeBool()) {
        wrong=encoded(s,s->element+1);
        refused(s,alias,wrong); xrtValueRelease(wrong);
    } else {
        unsigned char invalid=2;
        assert(!xl6ArrayPushV1(owner,LAYOUT(s),&invalid)); error(XERR_TYPE);
    }
    if (s->type->Kind==XRT_TYPE_SIGNED_INT) {
        wrong=xrtValueUInt(7); refused(s,alias,wrong); xrtValueRelease(wrong);
        if (s->type->Size<8) {
            int64_t maximum=s->type->Size==1 ? INT8_MAX : s->type->Size==2 ? INT16_MAX : INT32_MAX;
            wrong=xrtValueInt(maximum+1); refused(s,alias,wrong); xrtValueRelease(wrong);
            wrong=xrtValueInt(-maximum-2); refused(s,alias,wrong); xrtValueRelease(wrong);
        }
    } else if (s->type->Kind==XRT_TYPE_UNSIGNED_INT || s->type->Kind==XRT_TYPE_TYPE) {
        wrong=xrtValueInt(7); refused(s,alias,wrong); xrtValueRelease(wrong);
        if (s->type==xrtTypeType()) {
            wrong=xrtValueUInt(UINT64_MAX); refused(s,alias,wrong); xrtValueRelease(wrong);
        } else if (s->type->Size<8) {
            uint64_t maximum=s->type->Size==1 ? UINT8_MAX : s->type->Size==2 ? UINT16_MAX : UINT32_MAX;
            wrong=xrtValueUInt(maximum+1); refused(s,alias,wrong); xrtValueRelease(wrong);
        }
    } else if (s->type==xrtTypeFloat32()) {
        const double invalid[]={0.1,DBL_MAX,DBL_MIN};
        for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
            wrong=xrtValueFloat(invalid[i]); refused(s,alias,wrong); xrtValueRelease(wrong);
        }
        const double valid[]={0.0,-0.0,FLT_MAX,-FLT_MAX,(double)FLT_TRUE_MIN,INFINITY,-INFINITY,NAN};
        for (size_t i=0;i<sizeof(valid)/sizeof(*valid);++i) {
            wrong=xrtValueFloat(valid[i]); assert(xl6ArrayErasedSetV1(alias,0,wrong)); xrtValueRelease(wrong);
        }
    }
    assert(xl6ArrayErasedSetV1(alias,0,good)); observed(s,alias,good);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError(); assert(pending);
    assert(xl6SequenceAdmitSlotV1(pin,&slot) && xl6SequenceAdmitValueV1(pin,good) && xrtGetError()==pending);
    xl6SequenceEndV1(pin); assert(xrtGetError()==pending); xrtClearError();
    xvalue* clone=xl6ArrayErasedCloneV1(alias),*snapshot=xl6ArrayToValueV1(owner,LAYOUT(s),NULL);
    xvalue* cursor=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); assert(clone && snapshot && cursor);
    assert(xrtValueEqual(xrtValueArrayAt(snapshot,0),good));
    assert(xrtValueTypeId(xrtValueArrayAt(snapshot,0))==(s->type==xrtTypeBool() ? 0 : s->element));
    xrtValueRelease(alias); xrtValueRelease(owner);
    observed(s,clone,good);
    assert(xl6ArrayIteratorAdvanceV1(cursor,999,s->element,1,s->type->Size,s->type));
    Slot read={0}; assert(xl6ArrayIteratorReadV1(cursor,999,s->element,1,s->type->Size,s->type,&read));
    assert(!memcmp(&slot,&read,s->type->Size));
    assert(xl6ArrayResizeV1(clone,LAYOUT(s),2));
    xvalue* zero=xl6ArrayErasedReadV1(clone,1); assert(zero);
    /* A union initializer may initialize only its first (bool) member and
     * leave the other bytes as padding. Define the full native zero payload. */
    Slot empty; memset(&empty,0,sizeof(empty));
    xvalue* expected=xrtValueFromTyped(s->type,&empty,NULL); assert(expected);
    if (s->type!=xrtTypeBool()) assert(xl6ScalarIdentityBindV1(expected,s->element,xl6CScalarDomainTagV1(s->type)));
    if (!xrtValueEqual(zero,expected)) {
        Slot raw={0}; assert(xl6ArrayReadV1(clone,LAYOUT(s),1,&raw));
        fprintf(stderr,"zero mismatch owner=%llu kind=%d size=%zu raw=%llu tags=%d/%d ids=%llu/%llu\n",
            (unsigned long long)s->owner,(int)s->type->Kind,s->type->Size,(unsigned long long)raw.u,
            (int)xrtValueType(zero),(int)xrtValueType(expected),
            (unsigned long long)xrtValueTypeId(zero),(unsigned long long)xrtValueTypeId(expected));
    }
    assert(xrtValueEqual(zero,expected)); xrtValueRelease(zero); xrtValueRelease(expected);
    xvalue* moved=xl6ArrayErasedTakeV1(clone,0); assert(moved && xrtValueEqual(moved,good));
    assert(xrtValueTypeId(moved)==(s->type==xrtTypeBool() ? 0 : s->element)); xrtValueRelease(moved);
    xrtValueRelease(cursor); xrtValueRelease(clone);
    assert(xrtValueEqual(xrtValueArrayAt(snapshot,0),good)); xrtValueRelease(snapshot); xrtValueRelease(good);
}
static void mappings(void)
{
    const size_t sizes[]={sizeof(char),sizeof(signed char),sizeof(unsigned char),sizeof(short),sizeof(unsigned short),
        sizeof(int),sizeof(unsigned int),sizeof(long),sizeof(unsigned long),sizeof(long long),sizeof(unsigned long long),
        sizeof(size_t),sizeof(ptrdiff_t),sizeof(ptrdiff_t),sizeof(intptr_t),sizeof(uintptr_t),sizeof(bool),sizeof(float),sizeof(double),sizeof(wchar_t),sizeof(uint64_t)};
    const unsigned profiles[]={CHAR_MIN<0 ? 1u : 2u,1,2,3,4,5,6,sizeof(long)==4 ? 5u : 7u,
        sizeof(unsigned long)==4 ? 6u : 8u,7,8,sizeof(size_t)==4 ? 6u : 8u,sizeof(ptrdiff_t)==4 ? 5u : 7u,
        sizeof(ptrdiff_t)==4 ? 5u : 7u,sizeof(intptr_t)==4 ? 5u : 7u,sizeof(uintptr_t)==4 ? 6u : 8u,0,9,10,
        sizeof(wchar_t)==2 ? (WCHAR_MIN<0 ? 3u : 4u) : (WCHAR_MIN<0 ? 5u : 6u),11};
    assert(sizeof(sizes)/sizeof(*sizes)==21 && sizeof(profiles)/sizeof(*profiles)==21);
    for (size_t i=0;i<21;++i) { Spec s=spec(profiles[i]); assert(s.type->Size==sizes[i]); }
    printf("21 native C ABI/type-id mappings: long=%zu wchar=%zu size=%zu\n",sizeof(long),sizeof(wchar_t),sizeof(size_t));
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=element(s),*owner=NULL,*alias=NULL,*result=NULL;
    Slot typed={0}; assert(xrtValueToTyped(good,s->type,&typed,NULL));
    if (strcmp(operation,"create")) {
        owner=create(s); assert(owner && xl6ArrayPushV1(owner,LAYOUT(s),&typed));
        alias=xl6ArrayEraseV1(owner,LAYOUT(s),NULL); assert(alias);
    }
    int64_t capacity=owner ? xl6ArraySizeV1(owner,LAYOUT(s),true) : 0;
    xarray previous={0};
    if (owner) {
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
        previous=xl6SequenceStorageV1(pin)->Storage; xl6SequenceEndV1(pin);
    }
    assert(xrtMemDebugFailAfter(point)); bool ok;
    if (!strcmp(operation,"create")) { result=create(s); ok=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(owner,LAYOUT(s)); ok=result!=NULL; }
    else if (!strcmp(operation,"erased-clone")) { result=xl6ArrayErasedCloneV1(alias); ok=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); ok=result!=NULL; }
    else if (!strcmp(operation,"snapshot")) { result=xl6ArrayToValueV1(owner,LAYOUT(s),NULL); ok=result!=NULL; }
    else if (!strcmp(operation,"read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6ArrayErasedPushV1(alias,good);
    else if (!strcmp(operation,"set")) ok=xl6ArrayErasedSetV1(alias,0,good);
    else if (!strcmp(operation,"insert")) ok=xl6ArrayErasedInsertV1(alias,0,good);
    else { assert(!strcmp(operation,"resize")); ok=xl6ArrayResizeV1(owner,LAYOUT(s),257); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        if (owner) {
            assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==1 && xl6ArraySizeV1(owner,LAYOUT(s),true)==capacity);
            XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
            const xarray* current=&xl6SequenceStorageV1(pin)->Storage;
            assert(current->Data==previous.Data && current->Allocation==previous.Allocation); xl6SequenceEndV1(pin);
            observed(s,alias,good);
        }
    } else assert(ok && !xrtGetError());
    xrtValueRelease(result); xrtValueRelease(alias); xrtValueRelease(owner); xrtValueRelease(good);
    balanced(&before); printf("operation=%s point=%zu hit=%d balanced\n",operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==4) { Spec s=spec((unsigned)strtoul(argv[1],NULL,10)); fault(&s,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1); mappings();
    for (unsigned profile=0;profile<12;++profile) {
        Spec s=spec(profile); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        vectors(&s); balanced(&before);
    }
    puts("12 exact native C scalar/type-id policies and identities balanced"); return 0;
}
