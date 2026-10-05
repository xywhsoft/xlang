/* Preinclude the actual emitted C and its source-derived binding header.
 * Both adapters are executed, not mirrored by this native test. */
#include <assert.h>
#include <float.h>
#include <math.h>
#include "test/support/module_product_memory.h"
static xrtcallresult prepared;
static bool raw_entry(void* data,const xrtcallframe* frame,xrtcallresult* output)
{
    (void)data; assert(frame && frame->ArgumentCount==2);
    return xrtCallResultMove(output,&prepared);
}
static bool same_float(double a,double b)
{
    return isnan(a) ? isnan(b) : a==b && (!a ? !!signbit(a)==!!signbit(b) : true);
}
static void forward_f32(xvalue* input,bool expected)
{
    xvalue* arguments[]={input}; xrtcallframe frame={0}; xrtcallresult output={0};
    frame.Arguments=arguments; frame.ArgumentCount=1;
    xvalue* sentinel=xrtValueInt(42); assert(sentinel && xrtCallResultSet(&output,0,sentinel));
    assert(SCALAR_F32_FORWARD(SCALAR_F32_NATIVE,&frame,&output)==expected);
    if (expected) {
        double a,b; assert(xrtValueGetFloat(input,&a) && xrtValueGetFloat(xrtCallResultGet(&output,0),&b));
        assert(same_float(a,b) && !xrtGetError());
    } else {
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE && xrtCallResultCount(&output)==1 && xrtCallResultGet(&output,0)==sentinel);
        xrtClearError();
    }
    xrtCallResultUnit(&output); xrtValueRelease(sentinel); xrtValueRelease(input);
}
static void reverse_mixed(xvalue* callable,xvalue* number,xvalue* stamp,bool expected)
{
    xvalue* text=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5}); assert(text);
    assert(xrtCallResultSet(&prepared,0,number) && xrtCallResultSet(&prepared,1,stamp) &&
        xrtCallResultSet(&prepared,2,text));
    SCALAR_MIXED_RESULT output={.e0=7.5f,.e1=99};
    assert(SCALAR_MIXED_REVERSE(&callable,&output,1.5f,123)==expected);
    if (expected) {
        double widened; int64_t time;
        assert(xrtValueGetFloat(number,&widened) && xrtValueGetTime(stamp,&time));
        assert(same_float(widened,output.e0) && output.e1==time && output.e2.Size==5 &&
            !memcmp(output.e2.Data,"a\0\xE4\xBD\xA0",5) && !xrtGetError());
    } else {
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE && output.e0==7.5f && output.e1==99 && !output.e2.Data); xrtClearError();
    }
    SCALAR_MIXED_DROP(&output); xrtCallResultUnit(&prepared);
    xrtValueRelease(text); xrtValueRelease(number); xrtValueRelease(stamp);
}
static void vectors(void)
{
    const double numbers[]={0.0,-0.0,1.5,-1.5,FLT_MAX,-FLT_MAX,(double)0x1p-149f,INFINITY,-INFINITY,NAN};
    for (size_t i=0;i<sizeof(numbers)/sizeof(numbers[0]);++i) forward_f32(xrtValueFloat(numbers[i]),true);
    forward_f32(xrtValueFloat(0.1),false); forward_f32(xrtValueFloat(DBL_MAX),false);
    forward_f32(xrtValueInt(1),false); forward_f32(xrtValueTime(1),false);
    xvalue* foreign=xrtValueFloat(1.5); assert(foreign && xrtValueTypeIdBind(foreign,UINT64_C(0x123456789ABCDEF)));
    forward_f32(foreign,false);
    xvalue* wrong=xrtValueInt(123); xvalue* arguments[]={wrong}; xrtcallframe frame={0}; xrtcallresult output={0};
    frame.ArgumentCount=1; frame.Arguments=arguments;
    assert(!SCALAR_TIME_FORWARD(SCALAR_TIME_NATIVE,&frame,&output) && xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError(); xrtValueRelease(wrong); xrtCallResultUnit(&output);
    frame.ArgumentCount=0; frame.Arguments=NULL;
    assert(SCALAR_TIME_FORWARD(SCALAR_TIME_NATIVE,&frame,&output));
    assert(xrtValueType(xrtCallResultGet(&output,0))==XVALUE_TIME); xrtCallResultUnit(&output);
    for (unsigned i=0;i<2;++i) {
        int64_t stamp=i ? INT64_MAX : INT64_MIN; xvalue* v=xrtValueTime(stamp); assert(v);
        arguments[0]=v; frame.ArgumentCount=1; frame.Arguments=arguments;
        assert(SCALAR_TIME_FORWARD(SCALAR_TIME_NATIVE,&frame,&output)); int64_t actual;
        assert(xrtValueGetTime(xrtCallResultGet(&output,0),&actual) && actual==stamp);
        xrtCallResultUnit(&output); xrtValueRelease(v);
    }
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(raw);
    xvalue* boxed=xrtValueCallableTake(&raw); assert(boxed && !raw);
    reverse_mixed(boxed,xrtValueFloat(1.5),xrtValueTime(INT64_MIN),true);
    reverse_mixed(boxed,xrtValueFloat(-0.0),xrtValueTime(INT64_MAX),true);
    reverse_mixed(boxed,xrtValueFloat(0.1),xrtValueTime(123),false);
    reverse_mixed(boxed,xrtValueFloat(DBL_MAX),xrtValueTime(123),false);
    reverse_mixed(boxed,xrtValueInt(1),xrtValueTime(123),false);
    reverse_mixed(boxed,xrtValueFloat(1.5),xrtValueInt(123),false);
    xrtValueRelease(boxed); assert(!xrtGetError());
}
static void fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xlir_module_init());
    xvalue *a=xrtValueFloat(1.5),*b=xrtValueTime(INT64_MIN),*text=xrtValueString((xstrview){"a\0\xE4\xBD\xA0",5});
    xrtcallresult output={0}; SCALAR_MIXED_RESULT result={.e0=7.5f,.e1=99};
    xrtcallable* raw=xrtCallableCreate(NULL,raw_entry,NULL,NULL); assert(a && b && text && raw);
    xvalue* callable=xrtValueCallableTake(&raw); assert(callable);
    assert(xrtCallResultSet(&prepared,0,a) && xrtCallResultSet(&prepared,1,b) && xrtCallResultSet(&prepared,2,text));
    xvalue* arguments[]={a,b}; xrtcallframe frame={0}; frame.Arguments=arguments; frame.ArgumentCount=2;
    assert(xrtMemDebugFailAfter(point));
    bool forward=!strcmp(operation,"forward");
    bool ok=forward ? SCALAR_MIXED_FORWARD(SCALAR_MIXED_NATIVE,&frame,&output)
                    : SCALAR_MIXED_REVERSE(&callable,&result,1.5f,INT64_MIN);
    bool hit=xl6ProductMemoryEnd(); xerror* error=xrtTakeError();
    assert(hit ? !ok && xrtErrorKind(error)==XERR_MEMORY : ok && !error);
    if (!ok) {
        if (forward) assert(!xrtCallResultCount(&output));
        else assert(result.e0==7.5f && result.e1==99 && !result.e2.Data);
    }
    SCALAR_MIXED_DROP(&result); xrtCallResultUnit(&output); xrtErrorFree(error); xrtClearError();
    xrtCallResultUnit(&prepared); xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(text); xrtValueRelease(callable);
    xlir_module_fini(); assert(!xrtGetError());
    xl6ProductMemoryVerify();
    printf("scalar operation=%s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    if (argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); vectors();
    xlir_module_fini(); assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("scalar adapters: exact f32, time, default binding, mixed result atomicity and errors balanced"); return 0;
}
