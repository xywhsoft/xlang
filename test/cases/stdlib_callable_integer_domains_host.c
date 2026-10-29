/* Preinclude canonical generated C and symbol-derived bindings. No mirrored
 * adapter: every vector calls the actual forward/reverse generated function. */
#include <assert.h>
#include <limits.h>
#include "test/support/module_product_memory.h"
typedef bool (*Forward)(void*,const xrtcallframe*,xrtcallresult*);
typedef bool (*ReverseI64)(void*,int64_t*,int64_t);
typedef bool (*ReverseU64)(void*,uint64_t*,uint64_t);
static xrtcallresult prepared;
static uint64_t expected_identity;
static unsigned calls;
static bool raw_entry(void* data,const xrtcallframe* frame,xrtcallresult* output)
{
    (void)data; assert(frame && (frame->ArgumentCount==1 || frame->ArgumentCount==2)); ++calls;
    const xvalue* input=frame->Arguments[frame->ArgumentCount-1]; assert(input);
    assert(xrtValueTypeId(input)==expected_identity);
    return xrtCallResultMove(output,&prepared);
}
static xvalue* integer(int64_t value,uint64_t identity)
{
    xvalue* boxed=xrtValueInt(value); assert(boxed);
    if (identity) assert(xrtValueTypeIdBind(boxed,identity));
    return boxed;
}
static xvalue* unsigned_integer(uint64_t value,uint64_t identity)
{
    xvalue* boxed=xrtValueUInt(value); assert(boxed);
    if (identity) assert(xrtValueTypeIdBind(boxed,identity));
    return boxed;
}
static void forward(Forward entry,void* native,xvalue* input,bool expected,bool nominal,uint64_t identity)
{
    xvalue* arguments[]={input}; xrtcallframe frame={0}; xrtcallresult output={0};
    xvalue* sentinel=xrtValueInt(42); assert(sentinel && xrtCallResultSet(&output,0,sentinel));
    uint64_t before=xrtValueTypeId(input); frame.Arguments=arguments; frame.ArgumentCount=1;
    assert(entry(native,&frame,&output)==expected && before==xrtValueTypeId(input));
    if (expected) {
        assert(!xrtGetError() && xrtCallResultCount(&output)==1);
        const xvalue* result=xrtCallResultGet(&output,0);
        assert(xrtValueType(result)==xrtValueType(input) && xrtValueTypeId(result)==(nominal ? identity : 0));
        if (xrtValueType(input)==XVALUE_UINT) {
            uint64_t a,b; assert(xrtValueGetUInt(input,&a) && xrtValueGetUInt(result,&b) && a==b);
        } else {
            int64_t a,b; assert(xrtValueGetInt(input,&a) && xrtValueGetInt(result,&b) && a==b);
        }
    } else {
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE && xrtCallResultGet(&output,0)==sentinel);
        xrtClearError();
    }
    xrtCallResultUnit(&output); xrtValueRelease(sentinel); xrtValueRelease(input);
}
static void reverse_i64(ReverseI64 entry,xvalue* callable,xvalue* result,int64_t input,bool expected,uint64_t identity)
{
    expected_identity=identity; assert(xrtCallResultSet(&prepared,0,result));
    int64_t output=42; assert(entry(&callable,&output,input)==expected);
    if (expected) { int64_t value; assert(xrtValueGetInt(result,&value) && value==output && !xrtGetError()); }
    else { assert(output==42 && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); }
    xrtCallResultUnit(&prepared); xrtValueRelease(result);
}
static void signed_vectors(xvalue* callable,Forward f,void* native,ReverseI64 r,
    int64_t low,int64_t high,bool nominal,uint64_t identity)
{
    uint64_t bound=nominal ? identity : 0;
    const int64_t values[]={low,high,0};
    for (size_t i=0;i<3;++i) {
        forward(f,native,integer(values[i],bound),true,nominal,identity);
        reverse_i64(r,callable,integer(values[i],bound),values[i],true,bound);
    }
    if (low!=INT64_MIN) {
        forward(f,native,integer(low-1,bound),false,nominal,identity);
        reverse_i64(r,callable,integer(low-1,bound),0,false,bound);
        unsigned before=calls;
        reverse_i64(r,callable,integer(0,bound),low-1,false,bound); assert(calls==before);
    }
    if (high!=INT64_MAX) {
        forward(f,native,integer(high+1,bound),false,nominal,identity);
        reverse_i64(r,callable,integer(high+1,bound),0,false,bound);
        unsigned before=calls;
        reverse_i64(r,callable,integer(0,bound),high+1,false,bound); assert(calls==before);
    }
    forward(f,native,integer(0,identity^UINT64_C(1)),false,nominal,identity);
    reverse_i64(r,callable,integer(0,identity^UINT64_C(1)),0,false,bound);
    forward(f,native,xrtValueUInt(0),false,nominal,identity);
    reverse_i64(r,callable,xrtValueUInt(0),0,false,bound);
    if (nominal) {
        forward(f,native,integer(0,0),false,nominal,identity);
        reverse_i64(r,callable,integer(0,0),0,false,bound);
    }
}
static void unsigned_vectors(xvalue* callable,Forward f,void* native,ReverseU64 r,bool nominal,uint64_t identity)
{
    uint64_t bound=nominal ? identity : 0;
    const uint64_t values[]={0,UINT64_MAX};
    for (unsigned i=0;i<2;++i) {
        forward(f,native,unsigned_integer(values[i],bound),true,nominal,identity);
        xvalue* value=unsigned_integer(values[i],bound); assert(xrtCallResultSet(&prepared,0,value));
        uint64_t output=42; expected_identity=bound;
        assert(r(&callable,&output,values[i]) && output==values[i] && !xrtGetError()); xrtValueRelease(value);
    }
    forward(f,native,unsigned_integer(0,identity^UINT64_C(1)),false,nominal,identity);
    forward(f,native,integer(0,bound),false,nominal,identity);
    for (unsigned i=0;i<(nominal ? 3u : 2u);++i) {
        xvalue* value=i==0 ? integer(0,bound) : unsigned_integer(0,i==1 ? identity^UINT64_C(1) : 0);
        assert(xrtCallResultSet(&prepared,0,value)); uint64_t output=42; expected_identity=bound;
        assert(!r(&callable,&output,0) && output==42 && xrtErrorKind(xrtGetError())==XERR_TYPE);
        xrtClearError(); xrtValueRelease(value); xrtCallResultUnit(&prepared);
    }
    if (nominal) forward(f,native,unsigned_integer(0,0),false,nominal,identity);
}
static bool invalid_native(void* environment,int64_t* output,int64_t input)
{ (void)environment; (void)input; *output=256; return true; }
static void mixed_vectors(xvalue* callable)
{
    expected_identity=SCALAR_EI8_IDENTITY;
    for (unsigned invalid=0;invalid<5;++invalid) {
        xvalue *number=integer(invalid==4 ? 256 : 127,0),
            *code=integer(invalid==3 ? 128 : 127,invalid==1 ? 0 : invalid==2 ? SCALAR_EI8_IDENTITY^UINT64_C(1) : SCALAR_EI8_IDENTITY),
            *text=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(text);
        assert(xrtCallResultSet(&prepared,0,number) && xrtCallResultSet(&prepared,1,code) && xrtCallResultSet(&prepared,2,text));
        SCALAR_MIXED_RESULT output={.e0=7,.e1=9};
        assert(SCALAR_MIXED_REVERSE(&callable,&output,127,127)==!invalid);
        if (!invalid) assert(output.e0==127 && output.e1==127 && output.e2.Size==5 && !memcmp(output.e2.Data,"a\0\xE4\xBD\xA0",5) && !xrtGetError());
        else { assert(output.e0==7 && output.e1==9 && !output.e2.Data && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); }
        SCALAR_MIXED_DROP(&output); xrtCallResultUnit(&prepared);
        xrtValueRelease(number); xrtValueRelease(code); xrtValueRelease(text);
    }
}
static void default_and_native_result(void)
{
    xrtcallframe frame={0}; xrtcallresult output={0};
    assert(SCALAR_PI8_FORWARD(SCALAR_PI8_NATIVE,&frame,&output)); int64_t value;
    assert(xrtValueGetInt(xrtCallResultGet(&output,0),&value) && value==127); xrtCallResultUnit(&output);
    xlir_call_default defaults[]={SCALAR_PI8_NATIVE->call_parameter_defaults[0]}; defaults[0].i64_value=256;
    const char* names[]={"value"};
    XL6_Closure* native=xl6ClosureRuntimeV2Get()->Create(NULL,(void(*)(void))invalid_native,NULL,NULL,NULL,names,defaults,1); assert(native);
    assert(!SCALAR_PI8_FORWARD(native,&frame,&output) && !xrtCallResultCount(&output) && xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError();
    forward(SCALAR_PI8_FORWARD,native,integer(0,0),false,false,SCALAR_PI8_IDENTITY);
    xlir_closure_release(native); assert(!xrtGetError());
}
static void vectors(void)
{
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(raw);
    xvalue* callable=xrtValueCallableTake(&raw); assert(callable);
#define CHECK(N,L,H,B) signed_vectors(callable,SCALAR_##N##_FORWARD,SCALAR_##N##_NATIVE,SCALAR_##N##_REVERSE,L,H,B,SCALAR_##N##_IDENTITY)
    CHECK(PI8,INT8_MIN,INT8_MAX,false); CHECK(EI8,INT8_MIN,INT8_MAX,true);
    CHECK(PI16,INT16_MIN,INT16_MAX,false); CHECK(EI16,INT16_MIN,INT16_MAX,true);
    CHECK(PI32,INT32_MIN,INT32_MAX,false); CHECK(EI32,INT32_MIN,INT32_MAX,true);
    CHECK(PI64,INT64_MIN,INT64_MAX,false); CHECK(EI64,INT64_MIN,INT64_MAX,true);
    CHECK(PU8,0,UINT8_MAX,false); CHECK(EU8,0,UINT8_MAX,true);
    CHECK(PU16,0,UINT16_MAX,false); CHECK(EU16,0,UINT16_MAX,true);
    CHECK(PU32,0,UINT32_MAX,false); CHECK(EU32,0,UINT32_MAX,true);
#undef CHECK
    unsigned_vectors(callable,SCALAR_PU64_FORWARD,SCALAR_PU64_NATIVE,SCALAR_PU64_REVERSE,false,SCALAR_PU64_IDENTITY);
    unsigned_vectors(callable,SCALAR_EU64_FORWARD,SCALAR_EU64_NATIVE,SCALAR_EU64_REVERSE,true,SCALAR_EU64_IDENTITY);
    mixed_vectors(callable); default_and_native_result(); xrtValueRelease(callable); assert(!xrtGetError());
}
static void fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xvalue *a=integer(127,0),*b=integer(127,SCALAR_EI8_IDENTITY),*text=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(text);
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(raw);
    xvalue* callable=xrtValueCallableTake(&raw); assert(callable); expected_identity=SCALAR_EI8_IDENTITY;
    assert(xrtCallResultSet(&prepared,0,a) && xrtCallResultSet(&prepared,1,b) && xrtCallResultSet(&prepared,2,text));
    xvalue* arguments[]={a,b}; xrtcallframe frame={0}; frame.Arguments=arguments; frame.ArgumentCount=2;
    xrtcallresult output={0}; SCALAR_MIXED_RESULT result={.e0=7,.e1=9};
    assert(xrtMemDebugFailAfter(point)); bool is_forward=!strcmp(operation,"forward");
    bool ok=is_forward ? SCALAR_MIXED_FORWARD(SCALAR_MIXED_NATIVE,&frame,&output)
                       : SCALAR_MIXED_REVERSE(&callable,&result,127,127);
    bool hit=xl6ProductMemoryEnd(); xerror* error=xrtTakeError();
    assert(hit ? !ok && xrtErrorKind(error)==XERR_MEMORY : ok && !error);
    if (!ok) { if (is_forward) assert(!xrtCallResultCount(&output)); else assert(result.e0==7 && result.e1==9 && !result.e2.Data); }
    else if (!is_forward) assert(result.e0==127 && result.e1==127 && result.e2.Size==5);
    SCALAR_MIXED_DROP(&result); xrtCallResultUnit(&output); xrtErrorFree(error); xrtClearError();
    xrtCallResultUnit(&prepared); xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(text); xrtValueRelease(callable);
    xlir_module_fini(); assert(!xrtGetError()); xl6ProductMemoryVerify();
    printf("integer operation=%s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    if (argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); vectors();
    xlir_module_fini(); assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("integer adapters: 8 backing domains, primitive/nominal, bounds, identity, native results and defaults balanced"); return 0;
}
