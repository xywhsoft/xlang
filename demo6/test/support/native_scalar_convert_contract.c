#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_native_scalar_value.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define TARGETS(X) \
 X(CHAR,char) X(SCHAR,signed char) X(UCHAR,unsigned char) X(SHORT,short) X(USHORT,unsigned short) \
 X(INT,int) X(UINT,unsigned int) X(LONG,long) X(ULONG,unsigned long) X(LONGLONG,long long) \
 X(ULONGLONG,unsigned long long) X(SIZE,size_t) X(SSIZE,ptrdiff_t) X(PTRDIFF,ptrdiff_t) \
 X(INTPTR,intptr_t) X(UINTPTR,uintptr_t) X(BOOL,bool) X(FLOAT,float) X(DOUBLE,double) X(WCHAR,wchar_t)
static size_t pairs,ranges;
static void target(unsigned profile,const void* expected,size_t size,const void* input,unsigned source,const xvalue* dynamic)
{
    unsigned char result[32];memset(result,0xa5,sizeof(result));
    xl6ProductMemoryBegin(0);
    assert(dynamic ? xl6ValueConvertNativeScalarV1(dynamic,profile,result+1) :
        xl6NativeScalarConvertV1(input,source,profile,result+1));
    assert(!xl6ProductMemoryEnd() && !xrtGetError());xl6ProductMemoryVerify();
    assert(!memcmp(result+1,expected,size) && result[0]==0xa5 && result[size+1]==0xa5);++pairs;
}
static void matrix(const void* input,unsigned source,const xvalue* dynamic)
{
#define CHECK(Name,Type) { Type expected=(Type)1;target(XL6_NATIVE_SCALAR_##Name,&expected,sizeof(expected),input,source,dynamic); }
    TARGETS(CHECK)
#undef CHECK
}
static void range(unsigned targetProfile,const void* input,unsigned inputProfile,const xvalue* dynamic)
{
    unsigned char result[32],saved[32];memset(result,0xa5,sizeof(result));memcpy(saved,result,sizeof(saved));
    assert(!(dynamic ? xl6ValueConvertNativeScalarV1(dynamic,targetProfile,result+1) :
        xl6NativeScalarConvertV1(input,inputProfile,targetProfile,result+1)));
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE && !memcmp(result,saved,sizeof(result)));
    xrtClearError();++ranges;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();
#define SOURCE(Name,Type) { Type input=(Type)1;matrix(&input,XL6_SCALAR_INPUT_NATIVE_BASE+XL6_NATIVE_SCALAR_##Name,NULL); }
    TARGETS(SOURCE)
#undef SOURCE
    bool boolean=true;int64_t integer=1;uint64_t unsignedValue=1;float single=1;double real=1;uint32_t character=1;
    matrix(&boolean,XL6_SCALAR_INPUT_BOOL,NULL);matrix(&integer,XL6_SCALAR_INPUT_I64,NULL);
    matrix(&unsignedValue,XL6_SCALAR_INPUT_U64,NULL);matrix(&single,XL6_SCALAR_INPUT_F32,NULL);
    matrix(&real,XL6_SCALAR_INPUT_F64,NULL);matrix(&integer,XL6_SCALAR_INPUT_TIME,NULL);matrix(&character,XL6_SCALAR_INPUT_CHAR,NULL);
    xvalue* values[]={xrtValueBool(true),xrtValueInt(1),xrtValueUInt(1),xrtValueFloat(1),xrtValueTime(1),xrtValueChar(1)};
    for(size_t i=0;i<sizeof(values)/sizeof(*values);++i) { assert(values[i]);matrix(NULL,0,values[i]);xrtValueRelease(values[i]); }
    double nan=NAN,infinity=INFINITY;int64_t negative=-1;uint64_t huge=UINT64_MAX;
    xvalue* dynamicNan=xrtValueFloat(NAN);assert(dynamicNan);
#define RANGE(Name,Type) if(XL6_NATIVE_SCALAR_##Name!=XL6_NATIVE_SCALAR_BOOL && XL6_NATIVE_SCALAR_##Name!=XL6_NATIVE_SCALAR_FLOAT && XL6_NATIVE_SCALAR_##Name!=XL6_NATIVE_SCALAR_DOUBLE) { \
    range(XL6_NATIVE_SCALAR_##Name,&nan,XL6_SCALAR_INPUT_F64,NULL); \
    range(XL6_NATIVE_SCALAR_##Name,&infinity,XL6_SCALAR_INPUT_F64,NULL); \
    range(XL6_NATIVE_SCALAR_##Name,NULL,0,dynamicNan); \
    if((Type)-1>(Type)0)range(XL6_NATIVE_SCALAR_##Name,&negative,XL6_SCALAR_INPUT_I64,NULL); \
    else range(XL6_NATIVE_SCALAR_##Name,&huge,XL6_SCALAR_INPUT_U64,NULL); }
    TARGETS(RANGE)
#undef RANGE
    xrtValueRelease(dynamicNan);
    double over=DBL_MAX;range(XL6_NATIVE_SCALAR_FLOAT,&over,XL6_SCALAR_INPUT_F64,NULL);
    float result=0;int64_t doubleRound=INT64_C(4611686293305294849);
    assert(xl6NativeScalarConvertV1(&doubleRound,XL6_SCALAR_INPUT_I64,XL6_NATIVE_SCALAR_FLOAT,&result) && result==0x1.000002p62f);
    xvalue* boxed=xrtValueInt(doubleRound);assert(boxed);
    assert(xl6ValueConvertNativeScalarV1(boxed,XL6_NATIVE_SCALAR_FLOAT,&result) && result==0x1.000002p62f);xrtValueRelease(boxed);
    assert(xl6NativeScalarConvertV1(&infinity,XL6_SCALAR_INPUT_F64,XL6_NATIVE_SCALAR_FLOAT,&result) && isinf(result));
    double fraction=-1.9;int truncated=0;
    assert(xl6NativeScalarConvertV1(&fraction,XL6_SCALAR_INPUT_F64,XL6_NATIVE_SCALAR_INT,&truncated) && truncated==-1);
    xvalue* mismatch=xrtValueString(xrtStrView("wrong"));assert(mismatch);
    xrtSetErrorKind(XERR_STATE);const xerror* original=xrtGetError();assert(original);bool present=true;int unchanged=91;
    xl6ProductMemoryBegin(0);
    assert(xl6ValueTryReadNativeScalarV1(mismatch,XL6_NATIVE_SCALAR_INT,123,&present,&unchanged) && !present && unchanged==91);
    assert(!xl6ProductMemoryEnd() && xrtGetError()==original);xl6ProductMemoryVerify();xrtClearError();xrtValueRelease(mismatch);
    assert(pairs==660 && ranges==69 && !xrtGetError());
    printf("native conversion profiles=20 pairs=%zu range-rejections=%zu zero-allocation unaligned transactional balanced\n",pairs,ranges);
    return 0;
}
