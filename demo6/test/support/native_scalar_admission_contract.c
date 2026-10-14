#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_native_scalar_value.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
static size_t profiles;
static void check(unsigned profile,const void* data,size_t size)
{
    xvalue* value=xl6ValueBoxNativeScalarV1(data,profile,123);assert(value && !xrtGetError());
    xrtSetErrorKind(XERR_STATE);const xerror* original=xrtGetError();assert(original);
    xl6ProductMemoryBegin(0);
    assert(xl6ValueAdmitsNativeScalarV1(value,profile,123,false));
    assert(!xl6ValueAdmitsNativeScalarV1(value,0,123,false));
    assert(!xl6ValueAdmitsNativeScalarV1(value,profile,0,false));
    assert(!xl6ValueAdmitsNativeScalarV1(NULL,profile,123,false));
    assert(!xl6ValueAdmitsNativeScalarV1(xrtValueNull(),profile,123,false));
    assert(xl6ValueAdmitsNativeScalarV1(xrtValueNull(),profile,123,true));
    if(profile!=XL6_NATIVE_SCALAR_BOOL)assert(!xl6ValueAdmitsNativeScalarV1(value,profile,124,false));
    assert(!xl6ProductMemoryEnd() && xrtGetError()==original);xl6ProductMemoryVerify();xrtClearError();
    unsigned char unaligned[32];memset(unaligned,0xa5,sizeof(unaligned));
    assert(size<sizeof(unaligned)-1 && xl6ValueReadNativeScalarV1(value,profile,123,unaligned+1));
    assert(!memcmp(unaligned+1,data,size));xrtValueRelease(value);++profiles;
}
#define PROFILE(id,Type) do { Type value=(Type)0;check(XL6_NATIVE_SCALAR_##id,&value,sizeof(value)); } while(0)
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    PROFILE(CHAR,char);PROFILE(SCHAR,signed char);PROFILE(UCHAR,unsigned char);
    PROFILE(SHORT,short);PROFILE(USHORT,unsigned short);PROFILE(INT,int);PROFILE(UINT,unsigned int);
    PROFILE(LONG,long);PROFILE(ULONG,unsigned long);PROFILE(LONGLONG,long long);PROFILE(ULONGLONG,unsigned long long);
    PROFILE(SIZE,size_t);PROFILE(SSIZE,ptrdiff_t);PROFILE(PTRDIFF,ptrdiff_t);
    PROFILE(INTPTR,intptr_t);PROFILE(UINTPTR,uintptr_t);PROFILE(BOOL,bool);
    PROFILE(FLOAT,float);PROFILE(DOUBLE,double);PROFILE(WCHAR,wchar_t);PROFILE(TYPE_ID,uint64_t);
    xvalue* wide=xrtValueUInt(256);assert(wide && !xl6ValueAdmitsNativeScalarV1(wide,XL6_NATIVE_SCALAR_UCHAR,123,false));xrtValueRelease(wide);
    xvalue* lossy=xrtValueFloat(1.1);assert(lossy && !xl6ValueAdmitsNativeScalarV1(lossy,XL6_NATIVE_SCALAR_FLOAT,123,false));xrtValueRelease(lossy);
    xvalue* rawType=xrtValueUInt(7);assert(rawType && !xl6ValueAdmitsNativeScalarV1(rawType,XL6_NATIVE_SCALAR_TYPE_ID,123,false));xrtValueRelease(rawType);
    assert(profiles==21 && !xrtGetError());printf("native scalar admission profiles=%zu zero-allocation original-error-preserved balanced\n",profiles);
    return 0;
}
