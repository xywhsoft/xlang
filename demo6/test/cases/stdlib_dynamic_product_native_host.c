#include "src/runtime/xl6_dynamic_product.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Context { size_t count,calls,drops; unsigned failure; } Context;
static Context* dropping;
static void drop(ptr data,ptr user)
{
    (void)user; unsigned order=(unsigned)(uintptr_t)data; ++dropping->drops;
    if(dropping->failure==5 && order==1)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else if(dropping->failure==4 || dropping->failure==5)xrtSetErrorKind(order==1?XERR_IO:XERR_STATE);
}
static const xvaluehandleops operations={.Drop=drop};
static xvalue* handle(unsigned order)
{ ptr raw=(ptr)(uintptr_t)order; return xrtValueHandleTake(&raw,&operations,NULL); }
static bool invoke(ptr data,const xrtcallframe* frame,xrtcallresult* result)
{
    Context* ctx=data; ++ctx->calls; assert(frame);
    for(size_t i=0;i<ctx->count;++i) {
        xvalue* item=(ctx->failure==4 || ctx->failure==5) ? handle((unsigned)i+1) :
            i==0 ? xrtValueBool(false) : i==1 ? xrtValueString((xstrview){"a\0b",3}) : xrtValueInt((int64_t)i);
        if(!item || !xrtCallResultSet(result,i,item)) { xrtValueRelease(item); return false; }
        xrtValueRelease(item);
    }
    if(ctx->failure==1 || ctx->failure==3)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(ctx->failure==2 || ctx->failure==3)xrtSetErrorKind(XERR_IO);
    /* A checked language callback records its failure independently. XRT's
     * successful raw C entry otherwise restores the ambient diagnostic. */
    if(ctx->failure==2)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return ctx->failure!=1;
}
extern bool __real_xrtValueInvoke(const xvalue*,const xrtcallframe*,xrtcallresult*);
bool __wrap_xrtValueInvoke(const xvalue* callable,const xrtcallframe* frame,xrtcallresult* result)
{
    bool ok=__real_xrtValueInvoke(callable,frame,result);
    if(ok && dropping->failure==6)xrtSetErrorKind(XERR_IO);
    if(ok && dropping->failure==7)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return ok;
}
static xvalue* callable(Context* ctx)
{
    xrtcallable* native=xrtCallableCreate(NULL,invoke,ctx,NULL); assert(native);
    xvalue* result=xrtValueCallable(native); xrtCallableUnref(native); assert(result); return result;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void callCase(size_t count,unsigned mode)
{
    Context ctx={.count=count,.failure=mode}; dropping=&ctx;
    xvalue* function=callable(&ctx); xvalue* a=NULL; xvalue* b=NULL;
    xvalue** output[]={&a,&b};
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    bool ok=xl6DynamicFunctionProductTakeV1(function,0,NULL,0,NULL,NULL,2,output);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(count==2 && !mode) {
        assert(ok && outcome.ok && !outcome.error && a && b);
        bool flag=true; xstrview text={0}; assert(xrtValueGetBool(a,&flag) && !flag);
        assert(xrtValueGetString(b,&text) && text.Size==3 && !memcmp(text.Data,"a\0b",3));
    } else {
        assert(!ok && !outcome.ok && !a && !b);
        if(mode==1 || mode==3 || mode==7)assert(!outcome.error);
        else assert(outcome.error && xrtErrorKind(outcome.error)==(mode==2 || mode==6?XERR_IO:XERR_TYPE));
    }
    if(mode==4 || mode==5)assert(ctx.drops==count);
    xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(function);
}
static void argumentFailure(void)
{
    Context ctx={.count=2,.failure=4}; dropping=&ctx;
    xvalue* function=callable(&ctx); xvalue* a=NULL; xvalue* b=NULL; xvalue** output[]={&a,&b};
    xvalue* owned=handle(1); xvalue* second=handle(2); assert(owned && second);
    /* Result values stay retained until argument cleanup: they cannot hide a
     * failure from the consumed input's destructor. */
    ctx.failure=0;
    xvalue* arguments[]={owned,second}; dropping=&ctx;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    ctx.failure=4;
    bool ok=xl6DynamicFunctionProductTakeV1(function,2,arguments,0,NULL,NULL,2,output);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!ok && !outcome.ok && !a && !b && ctx.drops==4);
    assert(outcome.error && xrtErrorKind(outcome.error)==XERR_IO);
    xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(function);
}
static void invalid(void)
{
    Context ctx={.count=2}; dropping=&ctx; xvalue* function=callable(&ctx);
    xvalue* a=xrtValueInt(99); xvalue* original=a; xvalue* b=NULL; xvalue** output[]={&a,&b};
    assert(!xl6DynamicFunctionProductTakeV1(function,0,NULL,0,NULL,NULL,2,output));
    assert(a==original && !b && !ctx.calls); xrtClearError(); xrtValueRelease(a); a=NULL;
    output[1]=&a;
    assert(!xl6DynamicFunctionProductTakeV1(function,0,NULL,0,NULL,NULL,2,output));
    assert(!a && !ctx.calls); xrtClearError(); xrtValueRelease(function);
}
static void fault(size_t expected,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    Context ctx={.count=expected}; dropping=&ctx; xvalue* function=callable(&ctx);
    xvalue* values[6]={0}; xvalue** outputs[6]={0}; assert(expected<=6);
    for(size_t i=0;i<expected;++i)outputs[i]=&values[i];
    xvalue* arg=xrtValueInt(7); assert(arg); xvalue* arguments[]={arg};
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    bool ok=xl6DynamicFunctionProductTakeV1(function,1,arguments,0,NULL,NULL,expected,outputs);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !ok && !outcome.ok : ok && outcome.ok && !outcome.error);
    for(size_t i=0;i<expected;++i) { assert(hit ? !values[i] : values[i]!=NULL); xrtValueRelease(values[i]); }
    xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(function); balanced(&before);
    printf("product expected=%zu point=%zu hit=%d balanced\n",expected,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if(argc==3) { fault((size_t)strtoull(argv[1],NULL,10),(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    callCase(2,0); callCase(1,0); callCase(3,0);
    for(unsigned mode=1;mode<=3;++mode)callCase(2,mode);
    callCase(2,6); callCase(2,7);
    callCase(3,4); callCase(3,5); argumentFailure(); invalid(); balanced(&before);
    puts("dynamic product exact-count/no-diagnostic/cleanup balanced"); return 0;
}
