/* Canonical generated adapters and symbol-derived bindings are preincluded. */
#include "src/runtime/xl6_native_scalar_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <wchar.h>
typedef bool (*Forward)(void*,const xrtcallframe*,xrtcallresult*);
static xrtcallresult prepared;
static uint64_t expected_identity;
static xvaluetype expected_tag;
static unsigned calls;
static xvalue* boxed_int(int64_t v,uint64_t id)
{
    xvalue* p=xrtValueInt(v); assert(p); if(id) assert(xrtValueTypeIdBind(p,id)); return p;
}
static xvalue* boxed_uint(uint64_t v,uint64_t id)
{
    xvalue* p=xrtValueUInt(v); assert(p); if(id) assert(xrtValueTypeIdBind(p,id)); return p;
}
static xvalue* boxed_float(double v,uint64_t id)
{
    xvalue* p=xrtValueFloat(v); assert(p); if(id) assert(xrtValueTypeIdBind(p,id)); return p;
}
static bool same_float(double a,double b)
{
    return (isnan(a) && isnan(b)) || (a==b && (a!=0 || !!signbit(a)==!!signbit(b)));
}
static bool raw_entry(void* data,const xrtcallframe* frame,xrtcallresult* output)
{
    (void)data; assert(frame && (frame->ArgumentCount==1 || frame->ArgumentCount==2)); ++calls;
    const xvalue* p=frame->Arguments[frame->ArgumentCount-1]; assert(p);
    assert(xrtValueType(p)==expected_tag && xrtValueTypeId(p)==expected_identity);
    return xrtCallResultMove(output,&prepared);
}
static void forward(Forward entry,void* native,xvalue* input,bool valid,uint64_t id)
{
    xvalue* args[]={input}; xrtcallframe frame={.Arguments=args,.ArgumentCount=1}; xrtcallresult output={0};
    xvalue* sentinel=xrtValueInt(42); assert(sentinel && xrtCallResultSet(&output,0,sentinel));
    uint64_t before=xrtValueTypeId(input); assert(entry(native,&frame,&output)==valid && xrtValueTypeId(input)==before);
    if(valid) {
        const xvalue* result=xrtCallResultGet(&output,0); assert(result && xrtCallResultCount(&output)==1 && !xrtGetError());
        xvaluetype tag=xrtValueType(input); assert(xrtValueType(result)==tag && xrtValueTypeId(result)==(tag==XVALUE_BOOL ? 0 : id));
        if(tag==XVALUE_INT) { int64_t a,b; assert(xrtValueGetInt(input,&a) && xrtValueGetInt(result,&b) && a==b); }
        else if(tag==XVALUE_UINT) { uint64_t a,b; assert(xrtValueGetUInt(input,&a) && xrtValueGetUInt(result,&b) && a==b); }
        else if(tag==XVALUE_FLOAT) { double a,b; assert(xrtValueGetFloat(input,&a) && xrtValueGetFloat(result,&b) && same_float(a,b)); }
        else { bool a,b; assert(tag==XVALUE_BOOL && xrtValueGetBool(input,&a) && xrtValueGetBool(result,&b) && a==b); }
    } else {
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE && xrtCallResultGet(&output,0)==sentinel); xrtClearError();
    }
    xrtCallResultUnit(&output); xrtValueRelease(sentinel); xrtValueRelease(input);
}
/* Native slots are also deliberately unaligned. Verify the resident bytes
 * against independently constructed xvalues, not its own Box result alone. */
static void codec(const void* slot,size_t size,unsigned profile,uint64_t id,xvalue* input,bool valid)
{
    unsigned char output[32],before[32]; memset(output,0xa5,sizeof(output)); memcpy(before,output,sizeof(before));
    assert(xl6ValueReadNativeScalarV1(input,profile,id,output+1)==valid);
    if(valid) {
        assert(!memcmp(output+1,slot,size) && output[0]==0xa5 && output[size+1]==0xa5 && !xrtGetError());
        unsigned char raw[32]; memcpy(raw+1,slot,size);
        xvalue* boxed=xl6ValueBoxNativeScalarV1(raw+1,profile,id); assert(boxed && !xrtGetError());
        assert(xrtValueType(boxed)==xrtValueType(input) && xrtValueTypeId(boxed)==(xrtValueType(input)==XVALUE_BOOL ? 0 : id));
        memset(output,0xa5,sizeof(output)); assert(xl6ValueReadNativeScalarV1(boxed,profile,id,output+1) && !memcmp(output+1,slot,size));
        xrtValueRelease(boxed);
    } else { assert(!memcmp(output,before,sizeof(output)) && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); }
    xrtValueRelease(input);
}
#define REVERSE(N,T,R,I,V) do { \
    xvalue* raw=(R); assert(raw && xrtCallResultSet(&prepared,0,raw)); \
    T value=(T)42; expected_identity=SCALAR_##N##_IDENTITY; \
    assert(SCALAR_##N##_REVERSE(&callable,&value,(T)(I))==(V)); \
    if(V) { assert(value==(T)(I) && !xrtGetError()); } \
    else { assert(value==(T)42 && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); } \
    xrtCallResultUnit(&prepared); xrtValueRelease(raw); \
} while(0)
#define ARRAY(N,T,I) do { \
    xvalue* array=xl6SequenceCallableCreateV1(XL6_SEQUENCE_ARRAY_V1,SCALAR_##N##_ARRAY,SCALAR_##N##_SIGNATURE,SCALAR_##N##_SIGNATURE,false); assert(array); \
    xvalue* boxed=xlir_closure_to_xvalue_scalar(SCALAR_##N##_NATIVE,SCALAR_##N##_FORWARD,SCALAR_##N##_SIGNATURE); assert(boxed); \
    assert(!xl6ArrayErasedPushV1(array,boxed) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError(); \
    xvalue* erased=xl6ArrayEraseV1(array,SCALAR_##N##_ARRAY,SCALAR_##N##_SIGNATURE,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(erased==array); xrtValueRelease(erased); \
    assert(xl6ArrayErasedPushV1(array,boxed) && xl6ArrayErasedSizeV1(array)==1); \
    xvalue* projected=xl6ArrayErasedReadV1(array,0); assert(projected && xrtValueTypeId(projected)==SCALAR_##N##_SIGNATURE); \
    T output=(T)0; assert(SCALAR_##N##_REVERSE(&projected,&output,(T)(I)) && output==(T)(I) && !xrtGetError()); \
    xrtValueRelease(projected); xrtValueRelease(boxed); xrtValueRelease(array); \
} while(0)
#define SIGNED(N,T,L,H) do { \
    const int64_t numbers[]={L,H,0}; expected_tag=XVALUE_INT; \
    for(size_t j=0;j<3;++j) { T slot=(T)numbers[j]; \
        forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(numbers[j],0),true,SCALAR_##N##_IDENTITY); \
        REVERSE(N,T,boxed_int(numbers[j],0),numbers[j],true); \
        codec(&slot,sizeof(slot),SCALAR_##N##_PROFILE,SCALAR_##N##_IDENTITY,boxed_int(numbers[j],0),true); \
    } \
    if((int64_t)(L)!=INT64_MIN) { int64_t n=(int64_t)(L); --n; \
        forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(n,0),false,SCALAR_##N##_IDENTITY); \
        REVERSE(N,T,boxed_int(n,0),0,false); \
        T slot=0; codec(&slot,sizeof(slot),SCALAR_##N##_PROFILE,SCALAR_##N##_IDENTITY,boxed_int(n,0),false); \
    } \
    if((int64_t)(H)!=INT64_MAX) { int64_t n=(int64_t)(H); ++n; \
        forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(n,0),false,SCALAR_##N##_IDENTITY); \
        REVERSE(N,T,boxed_int(n,0),0,false); \
    } \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_uint(0,0),false,SCALAR_##N##_IDENTITY); \
    REVERSE(N,T,boxed_uint(0,0),0,false); \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(0,SCALAR_##N##_IDENTITY^1),false,SCALAR_##N##_IDENTITY); \
    REVERSE(N,T,boxed_int(0,SCALAR_##N##_IDENTITY^1),0,false); \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(0,SCALAR_##N##_IDENTITY),true,SCALAR_##N##_IDENTITY); \
    ARRAY(N,T,H); \
} while(0)
#define UNSIGNED(N,T,H) do { \
    const uint64_t numbers[]={0,H}; expected_tag=XVALUE_UINT; \
    for(size_t j=0;j<2;++j) { T slot=(T)numbers[j]; \
        forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_uint(numbers[j],0),true,SCALAR_##N##_IDENTITY); \
        REVERSE(N,T,boxed_uint(numbers[j],0),numbers[j],true); \
        codec(&slot,sizeof(slot),SCALAR_##N##_PROFILE,SCALAR_##N##_IDENTITY,boxed_uint(numbers[j],0),true); \
    } \
    if((uint64_t)(H)!=UINT64_MAX) { uint64_t n=(uint64_t)(H); ++n; \
        forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_uint(n,0),false,SCALAR_##N##_IDENTITY); \
        REVERSE(N,T,boxed_uint(n,0),0,false); \
        T slot=0; codec(&slot,sizeof(slot),SCALAR_##N##_PROFILE,SCALAR_##N##_IDENTITY,boxed_uint(n,0),false); \
    } \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(0,0),false,SCALAR_##N##_IDENTITY); \
    REVERSE(N,T,boxed_int(0,0),0,false); \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_uint(0,SCALAR_##N##_IDENTITY^1),false,SCALAR_##N##_IDENTITY); \
    REVERSE(N,T,boxed_uint(0,SCALAR_##N##_IDENTITY^1),0,false); \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_uint(0,SCALAR_##N##_IDENTITY),true,SCALAR_##N##_IDENTITY); \
    ARRAY(N,T,H); \
} while(0)
#define FLOATS(N,T) do { \
    const double numbers[]={0.0,-0.0,1.5,FLT_TRUE_MIN,FLT_MAX,INFINITY,-INFINITY,NAN}; expected_tag=XVALUE_FLOAT; \
    for(size_t j=0;j<sizeof(numbers)/sizeof(numbers[0]);++j) { \
        forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_float(numbers[j],0),true,SCALAR_##N##_IDENTITY); \
        xvalue* raw=boxed_float(numbers[j],0); assert(xrtCallResultSet(&prepared,0,raw)); \
        T value=42; expected_identity=SCALAR_##N##_IDENTITY; assert(SCALAR_##N##_REVERSE(&callable,&value,(T)numbers[j])); \
        assert(same_float(value,numbers[j]) && !xrtGetError()); xrtCallResultUnit(&prepared); xrtValueRelease(raw); \
        if(!isnan(numbers[j])) { T slot=(T)numbers[j]; codec(&slot,sizeof(slot),SCALAR_##N##_PROFILE,SCALAR_##N##_IDENTITY,boxed_float(numbers[j],0),true); } \
    } \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_int(0,0),false,SCALAR_##N##_IDENTITY); \
    forward(SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,boxed_float(0,SCALAR_##N##_IDENTITY^1),false,SCALAR_##N##_IDENTITY); \
    ARRAY(N,T,1.5); \
} while(0)
static void vectors(void)
{
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(raw);
    xvalue* callable=xrtValueCallableTake(&raw); assert(callable);
    if(CHAR_MIN<0) { SIGNED(Char,char,CHAR_MIN,CHAR_MAX); } else { UNSIGNED(Char,char,CHAR_MAX); }
    SIGNED(SChar,signed char,SCHAR_MIN,SCHAR_MAX); UNSIGNED(UChar,unsigned char,UCHAR_MAX);
    SIGNED(Short,short,SHRT_MIN,SHRT_MAX); UNSIGNED(UShort,unsigned short,USHRT_MAX);
    SIGNED(Int,int,INT_MIN,INT_MAX); UNSIGNED(UInt,unsigned int,UINT_MAX);
    SIGNED(Long,long,LONG_MIN,LONG_MAX); UNSIGNED(ULong,unsigned long,ULONG_MAX);
    SIGNED(LongLong,long long,LLONG_MIN,LLONG_MAX); UNSIGNED(ULongLong,unsigned long long,ULLONG_MAX);
    UNSIGNED(Size,size_t,SIZE_MAX); SIGNED(SSize,ptrdiff_t,PTRDIFF_MIN,PTRDIFF_MAX);
    SIGNED(PtrDiff,ptrdiff_t,PTRDIFF_MIN,PTRDIFF_MAX); SIGNED(IntPtr,intptr_t,INTPTR_MIN,INTPTR_MAX);
    UNSIGNED(UIntPtr,uintptr_t,UINTPTR_MAX);
    if(WCHAR_MIN<0) { SIGNED(WChar,wchar_t,WCHAR_MIN,WCHAR_MAX); } else { UNSIGNED(WChar,wchar_t,WCHAR_MAX); }
    FLOATS(Float,float); FLOATS(Double,double);
    forward(SCALAR_Float_FORWARD,SCALAR_Float_NATIVE,boxed_float(0.1,0),false,SCALAR_Float_IDENTITY);
    xvalue* rounded=boxed_float(0.1,0); assert(xrtCallResultSet(&prepared,0,rounded)); xrtValueRelease(rounded);
    float number=42; expected_identity=SCALAR_Float_IDENTITY; expected_tag=XVALUE_FLOAT;
    assert(!SCALAR_Float_REVERSE(&callable,&number,0) && number==42 && xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError(); xrtCallResultUnit(&prepared);
    expected_tag=XVALUE_BOOL; expected_identity=0;
    for(unsigned j=0;j<2;++j) {
        bool value=j!=0; forward(SCALAR_Bool_FORWARD,SCALAR_Bool_NATIVE,xrtValueRetain(xrtValueBool(value)),true,SCALAR_Bool_IDENTITY);
        assert(xrtCallResultSet(&prepared,0,xrtValueBool(value))); bool result=!value;
        assert(SCALAR_Bool_REVERSE(&callable,&result,value) && result==value && !xrtGetError()); xrtCallResultUnit(&prepared);
        codec(&value,sizeof(value),SCALAR_Bool_PROFILE,SCALAR_Bool_IDENTITY,xrtValueRetain(xrtValueBool(value)),true);
    }
    unsigned char invalid_bool=2; assert(!xl6ValueBoxNativeScalarV1(&invalid_bool,SCALAR_Bool_PROFILE,SCALAR_Bool_IDENTITY));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE && !xrtValueTypeId(xrtValueBool(false)) && !xrtValueTypeId(xrtValueBool(true))); xrtClearError();
    forward(SCALAR_Bool_FORWARD,SCALAR_Bool_NATIVE,boxed_int(1,0),false,SCALAR_Bool_IDENTITY);
    ARRAY(Bool,bool,true);
    uint64_t type=UINT64_C(0x123456789abcdef0); expected_tag=XVALUE_UINT;
    forward(SCALAR_Type_FORWARD,SCALAR_Type_NATIVE,boxed_uint(type,SCALAR_Type_IDENTITY),true,SCALAR_Type_IDENTITY);
    REVERSE(Type,uint64_t,boxed_uint(type,SCALAR_Type_IDENTITY),type,true);
    codec(&type,sizeof(type),SCALAR_Type_PROFILE,SCALAR_Type_IDENTITY,boxed_uint(type,SCALAR_Type_IDENTITY),true);
    forward(SCALAR_Type_FORWARD,SCALAR_Type_NATIVE,boxed_uint(type,0),false,SCALAR_Type_IDENTITY);
    REVERSE(Type,uint64_t,boxed_uint(type,0),type,false);
    forward(SCALAR_Type_FORWARD,SCALAR_Type_NATIVE,boxed_int(0,SCALAR_Type_IDENTITY),false,SCALAR_Type_IDENTITY);
    REVERSE(Type,uint64_t,boxed_uint(type,SCALAR_Type_IDENTITY^1),type,false);
    ARRAY(Type,uint64_t,type);
    unsigned char output[16],before[16]; memset(output,0xa5,sizeof(output)); memcpy(before,output,sizeof(before));
    assert(!xl6ValueReadNativeScalarV1(xrtValueNull(),0,1,output) && !memcmp(output,before,sizeof(output)) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6ValueBoxNativeScalarV1(output,UINT_MAX,1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6ValueReadNativeScalarV1(xrtValueNull(),SCALAR_Int_PROFILE,0,output) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6ValueBoxNativeScalarV1(NULL,SCALAR_Int_PROFILE,1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    unsigned count=calls; xrtcallframe empty={0}; xrtcallresult result={0};
    assert(!SCALAR_Int_FORWARD(SCALAR_Int_NATIVE,&empty,&result) && !xrtCallResultCount(&result) && calls==count && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    xrtValueRelease(callable); assert(!xrtGetError());
}
static void mixed(xvalue* callable,xvalue* a,xvalue* b,xvalue* text,bool valid)
{
    xvalue* args[]={a,b}; xrtcallframe frame={.Arguments=args,.ArgumentCount=2}; xrtcallresult boxed={0};
    bool ok=SCALAR_MIXED_FORWARD(SCALAR_MIXED_NATIVE,&frame,&boxed);
    assert(ok==valid && (ok ? xrtCallResultCount(&boxed)==3 && !xrtGetError() : !xrtCallResultCount(&boxed) && xrtErrorKind(xrtGetError())==XERR_TYPE));
    xrtClearError(); xrtCallResultUnit(&boxed);
    assert(xrtCallResultSet(&prepared,0,a) && xrtCallResultSet(&prepared,1,b) && xrtCallResultSet(&prepared,2,text));
    SCALAR_MIXED_RESULT output={.e0=7,.e1=9}; expected_tag=XVALUE_UINT; expected_identity=SCALAR_MIXED_TYPE_IDENTITY;
    ok=SCALAR_MIXED_REVERSE(&callable,&output,7,9); assert(ok==valid);
    if(ok) assert(output.e0==7 && output.e1==9 && output.e2.Size==5 && !memcmp(output.e2.Data,"a\0\xE4\xBD\xA0",5) && !xrtGetError());
    else { assert(output.e0==7 && output.e1==9 && !output.e2.Data && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); }
    SCALAR_MIXED_DROP(&output); xrtCallResultUnit(&prepared);
}
static void fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue *a=boxed_uint(7,0),*b=boxed_uint(9,SCALAR_MIXED_TYPE_IDENTITY),*text=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(text);
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(raw); xvalue* callable=xrtValueCallableTake(&raw); assert(callable);
    assert(xrtCallResultSet(&prepared,0,a) && xrtCallResultSet(&prepared,1,b) && xrtCallResultSet(&prepared,2,text));
    xvalue* args[]={a,b}; xrtcallframe frame={.Arguments=args,.ArgumentCount=2}; xrtcallresult output={0};
    SCALAR_MIXED_RESULT result={.e0=7,.e1=9}; expected_tag=XVALUE_UINT; expected_identity=SCALAR_MIXED_TYPE_IDENTITY;
    assert(xrtMemDebugFailAfter(point)); bool f=!strcmp(operation,"forward");
    bool ok=f ? SCALAR_MIXED_FORWARD(SCALAR_MIXED_NATIVE,&frame,&output) : SCALAR_MIXED_REVERSE(&callable,&result,7,9);
    bool hit=xl6ProductMemoryEnd(); xerror* error=xrtTakeError();
    assert(hit ? !ok && xrtErrorKind(error)==XERR_MEMORY : ok && !error);
    if(!ok) { if(f) assert(!xrtCallResultCount(&output)); else assert(result.e0==7 && result.e1==9 && !result.e2.Data); }
    else if(!f) assert(result.e0==7 && result.e1==9 && result.e2.Size==5);
    SCALAR_MIXED_DROP(&result); xrtCallResultUnit(&output); xrtErrorFree(error); xrtClearError();
    xrtCallResultUnit(&prepared); xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(text); xrtValueRelease(callable);
    xlir_module_fini(); assert(!xrtGetError()); xl6ProductMemoryVerify();
    printf("native operation=%s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    if(argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); vectors();
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(raw); xvalue* callable=xrtValueCallableTake(&raw); assert(callable);
    xvalue* text=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(text);
    xvalue* a=boxed_uint(7,0),*b=boxed_uint(9,SCALAR_MIXED_TYPE_IDENTITY); mixed(callable,a,b,text,true);
    xrtValueRelease(b); b=boxed_uint(9,0); mixed(callable,a,b,text,false);
    xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(text); xrtValueRelease(callable);
    xlir_module_fini(); assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("native adapters: 21 profiles and native Array recipes, long=%zu wchar=%zu, exact tags/identity/bounds/atomic output and NUL balanced\n",sizeof(long),sizeof(wchar_t));
    return 0;
}
