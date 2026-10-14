#include "src/runtime/xl6_float_convert.h"
#include "release/include/xruntime_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
static bool failWithoutDiagnostic;
extern bool __real_xrtNumParse(xstrview,uint32_t,double*);
bool __wrap_xrtNumParse(xstrview text,uint32_t flags,double* output)
{
    return failWithoutDiagnostic ? false : __real_xrtNumParse(text,flags,output);
}

static void value(xvalue* source, float expected)
{
    assert(source); float result=99;
    assert(xl6Float32ValueConvertV1(source,&result));
    assert(isnan(expected) ? isnan(result) : result==expected);
    if(expected==0)assert(!!signbit(expected)==!!signbit(result));
    assert(!xrtGetError()); xrtValueRelease(source);
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    value(xrtValueNull(),0); value(xrtValueBool(true),1);
    value(xrtValueInt(INT64_MIN),(float)INT64_MIN); value(xrtValueUInt(UINT64_MAX),(float)UINT64_MAX);
    value(xrtValueInt(INT64_C(4611686293305294849)),0x1.000002p62f);
    value(xrtValueUInt(UINT64_C(9223372586610589697)),0x1.000002p63f);
    value(xrtValueFloat(16777217),16777216); value(xrtValueFloat(-0.0),-0.0f);
    value(xrtValueFloat(NAN),NAN); value(xrtValueFloat(INFINITY),INFINITY);
    value(xrtValueFloat(-INFINITY),-INFINITY); value(xrtValueFloat(FLT_MAX),FLT_MAX);
    value(xrtValueFloat(0x1p-149),0x1p-149f); value(xrtValueFloat(0x1p-151),0);
    float output=77;
    assert(xl6Float32ConvertV1(INFINITY,&output) && isinf(output) && output>0);
    assert(xl6Float32ConvertV1(NAN,&output) && isnan(output));
    output=77;
    assert(!xl6Float32ConvertV1(DBL_MAX,&output) && output==77);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    assert(xl6Float32TextConvertV1((xstrview){"1.5",3},&output) && output==1.5f);
    const char exact[]={'2','.','5','x'};
    assert(xl6Float32TextConvertV1((xstrview){exact,3},&output) && output==2.5f);
    const char zero[]={'1',0,'.','5'};
    output=77;
    assert(!xl6Float32TextConvertV1((xstrview){zero,sizeof(zero)},&output) && output==77);
    assert(xrtErrorKind(xrtGetError())==XERR_VALUE); xrtClearError();
    xvalue* tooLarge=xrtValueFloat(DBL_MAX); assert(tooLarge);
    assert(!xl6Float32ValueConvertV1(tooLarge,&output) && output==77);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError(); xrtValueRelease(tooLarge);
    xvalue* bytes=xrtValueBytes((xbytesview){(const unsigned char*)"x",1}); assert(bytes);
    assert(!xl6Float32ValueConvertV1(bytes,&output) && output==77);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); xrtValueRelease(bytes);
    assert(!xl6Float32TextConvertV1((xstrview){NULL,1},&output) && output==77); xrtClearError();
    assert(!xl6Float32ValueConvertV1(NULL,&output) && output==77); xrtClearError();
    failWithoutDiagnostic=true;
    assert(!xl6Float32TextConvertV1((xstrview){"1.5",3},&output) && output==77 && !xrtGetError());
    failWithoutDiagnostic=false;
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    /* Exhaust every diagnostic-allocation prefix. Failure is not inferred
     * from error presence; output remains untouched even when error is NULL. */
    size_t runs=0;
    for(size_t point=0;point<1000;++point) {
        xl6ProductMemoryBegin(point); output=77;
        assert(!xl6Float32TextConvertV1((xstrview){zero,sizeof(zero)},&output) && output==77);
        bool hit=xl6ProductMemoryEnd();
        xrtClearError(); xl6ProductMemoryVerify(); ++runs;
        if(!hit)break;
    }
    assert(runs>1 && runs<1000);
    printf("native float conversions balanced; diagnostic prefixes=%zu null diagnostic preserved\n",runs);
    return 0;
}
