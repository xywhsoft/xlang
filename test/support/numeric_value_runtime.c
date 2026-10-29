#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_value_numeric.h"
#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_native_scalar_value.h"
#include "src/runtime/xl6_scalar_domain.h"
#include "src/runtime/xl6_error_text.h"
#include "src/runtime/xl6_sequence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void balance(const xmemdebugsnapshot* before)
{xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
 assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
 after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
 before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);}
static uint64_t bits(const xvalue* value)
{uint64_t u=0;int64_t i=0;if(xrtValueType(value)==XVALUE_UINT)assert(xrtValueGetUInt(value,&u));
 else{assert(xrtValueGetInt(value,&i));u=(uint64_t)i;}return u;}
static void error(xvalue* value,const char* kind)
{assert(!value&&xrtGetError()&&!strcmp(xl6ErrorKindNameV1(xrtGetError()),kind));xrtClearError();}
static xvalue* box(unsigned rule,int64_t number,uint64_t identity)
{uint64_t u=(uint64_t)number;return xl6ValueBoxIntegerV1(rule==XL6_VALUE_ADMIT_U64?(const void*)&u:&number,rule,identity,true);}
static void enums(void)
{
    static const unsigned rules[]={XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I16,XL6_VALUE_ADMIT_I32,XL6_VALUE_ADMIT_I64,
        XL6_VALUE_ADMIT_U8,XL6_VALUE_ADMIT_U16,XL6_VALUE_ADMIT_U32,XL6_VALUE_ADMIT_U64};
    static const unsigned widths[]={8,16,32,64,8,16,32,64};
    for(size_t at=0;at<8;++at){unsigned rule=rules[at],width=widths[at];uint64_t identity=101+at;
        xvalue* a=box(rule,1,identity),*b=box(rule,2,identity);assert(a&&b);
        assert(xrtValueScalarDomainV1(a)==xl6ScalarEnumDomainV1(rule));
        for(unsigned op=5;op<=7;++op){xvalue* out=xl6ValueNumericBinaryV1(a,b,op);assert(out);
            assert(bits(out)==(op==5?0:3)&&xrtValueTypeId(out)==identity&&xrtValueScalarDomainV1(out)==xrtValueScalarDomainV1(a));xrtValueRelease(out);}
        xvalue* out=xl6ValueNumericUnaryV1(a,2);assert(out);
        uint64_t expected=at<4||width==64?UINT64_MAX-1:(UINT64_C(1)<<width)-2;
        assert(bits(out)==expected&&xrtValueTypeId(out)==identity&&xrtValueScalarDomainV1(out)==xrtValueScalarDomainV1(a));xrtValueRelease(out);
        xvalue* different=box(rule,2,identity+100),*primitive=xrtValueInt(1);assert(different&&primitive);
        for(unsigned op=0;op<=9;++op){if(op<5||op>7)error(xl6ValueNumericBinaryV1(a,b,op),"TypeError");
            error(xl6ValueNumericBinaryV1(a,different,op),"TypeError");error(xl6ValueNumericBinaryV1(a,primitive,op),"TypeError");}
        error(xl6ValueNumericUnaryV1(a,0),"TypeError");error(xl6ValueNumericUnaryV1(a,1),"TypeError");
        xvalue* missing=xrtValueInt(1);assert(missing&&xrtValueTypeIdBind(missing,identity));
        error(xl6ValueNumericBinaryV1(missing,missing,6),"TypeError");xrtValueRelease(missing);
        xvalue* malformed=xrtValueInt(1000);assert(malformed&&xrtValueScalarDomainBindV1(malformed,identity,xl6ScalarEnumDomainV1(XL6_VALUE_ADMIT_U8)));
        error(xl6ValueNumericUnaryV1(malformed,2),"TypeError");xrtValueRelease(malformed);
        assert(xrtMemDebugFailAfter(0));out=xl6ValueNumericBinaryV1(a,b,6);assert(!out&&xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();
        assert(xrtMemDebugFailAfter(1));out=xl6ValueNumericBinaryV1(a,b,6);assert(out&&!xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();xrtValueRelease(out);
        xrtValueRelease(primitive);xrtValueRelease(different);xrtValueRelease(a);xrtValueRelease(b);
    }
}
static void consuming(void)
{
    xvalue* value=xrtValueInt(255);assert(value);value=xl6ValueEnumBindTakeV1(value,XL6_VALUE_ADMIT_U8,600);
    assert(value&&xrtValueScalarDomainV1(value)==xl6ScalarEnumDomainV1(XL6_VALUE_ADMIT_U8));xrtValueRelease(value);
    value=xrtValueInt(256);assert(value);error(xl6ValueEnumBindTakeV1(value,XL6_VALUE_ADMIT_U8,600),"TypeError");
    value=xrtValueInt(1);assert(value);xvalue* shared=xrtValueRetain(value);assert(shared==value);
    error(xl6ValueEnumBindTakeV1(value,XL6_VALUE_ADMIT_U8,600),"RuntimeError");
    assert(bits(shared)==1&&!xrtValueTypeId(shared)&&!xrtValueScalarDomainV1(shared));xrtValueRelease(shared);
}
static void primitive(void)
{
    xvalue* a=xrtValueInt(100),*b=xrtValueInt(3);assert(a&&b);
    static const uint64_t expected[]={103,97,300,33,1,0,103,103,800,12};
    for(unsigned op=0;op<10;++op){xvalue* out=xl6ValueNumericBinaryV1(a,b,op);assert(out&&bits(out)==expected[op]&&!xrtValueTypeId(out));xrtValueRelease(out);}
    xvalue* zero=xrtValueInt(0);assert(zero);error(xl6ValueNumericBinaryV1(a,zero,3),"ZeroDivisionError");
    xrtValueRelease(zero);xrtValueRelease(b);xrtValueRelease(a);
    a=xrtValueUInt(UINT64_MAX);
    for(unsigned op=0;op<3;++op){b=xl6ValueNumericUnaryV1(a,op);assert(b&&xrtValueType(b)==XVALUE_UINT&&bits(b)==(op==0?UINT64_MAX:op==1?1:0));xrtValueRelease(b);}xrtValueRelease(a);
    unsigned char c=255;a=xl6ValueBoxNativeScalarV1(&c,XL6_NATIVE_SCALAR_UCHAR,701);assert(a&&xrtValueScalarDomainV1(a)==(XL6_SCALAR_DOMAIN_NATIVE_V1|XL6_VALUE_ADMIT_U8));
    b=xl6ValueNumericUnaryV1(a,2);assert(b&&xrtValueType(b)==XVALUE_UINT&&bits(b)==(UINT64_MAX^255));xrtValueRelease(b);xrtValueRelease(a);
    float f=1.25f;a=xl6ValueBoxNativeScalarV1(&f,XL6_NATIVE_SCALAR_FLOAT,702);assert(a);
    b=xl6ValueNumericUnaryV1(a,1);double number=0;assert(b&&xrtValueGetFloat(b,&number)&&number==-1.25);xrtValueRelease(b);xrtValueRelease(a);
}
int main(void)
{
    if(sizeof(void*)==8) assert(sizeof(XL6_SequenceAdmissionV3)==56 &&
        offsetof(XL6_SequenceAdmissionV3,transfer)==24&&offsetof(XL6_SequenceAdmissionV3,scalarDomain)==28&&
        offsetof(XL6_SequenceAdmissionV3,size)==32&&offsetof(XL6_SequenceAdmissionV3,boxedIdentity)==48);
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    enums();consuming();primitive();balance(&before);
    puts("Numeric values: eight exact enum domains, primitive/native separation, consuming bind, OOM and zero live delta");return 0;
}
