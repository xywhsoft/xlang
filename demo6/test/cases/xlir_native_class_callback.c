/* Resident unary class callback: real independent code owners, Close,
 * detached arguments, first failures and allocation-prefix rollback. No I/O. */
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_native_class_callback.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_tls_server_request.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Owner {unsigned pins,acquired,released;bool closed;} Owner;
static int acquire(void* raw)
{Owner* p=raw;if(p->closed)return 0;++p->pins;++p->acquired;return 1;}
static void release(void* raw)
{Owner* p=raw;assert(p->pins);--p->pins;++p->released;}
typedef struct State {unsigned calls,mode;xvalue* saved;} State;
typedef struct Environment {State* state;} Environment;
static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_TRIVIAL,0,sizeof(State*)};
static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(Environment),1,&slot};
static const char* const names[]={"request"};
static const XL6_ClassIdentitySchemaV3 schema={sizeof(schema),UINT64_C(0x6243616c6c626163),601,
    XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}};
static bool callback(void* raw,xvalue** output,xvalue* argument)
{
    State* state=((Environment*)raw)->state;++state->calls;
    assert(!state->saved);state->saved=xrtValueRetain(argument);
    if(state->mode==1)return true; /* Intentional optional rejection. */
    if(state->mode==2)return false; /* Checked failure without a diagnostic. */
    if(state->mode==3){xrtSetErrorKind(XERR_RANGE);return false;}
    if(state->mode==4){*output=xrtValueRetain(argument);xrtSetErrorKind(XERR_RANGE);return true;}
    if(state->mode==5){*output=xrtValueInt(12);return *output!=NULL;}
    *output=xrtValueRetain(argument);return true;
}
static XL6_Closure* target(const XL6_CodeOwner* owner,State* state)
{
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->state=state;
    XL6_Closure* result=xl6ClosureRuntimeV2Get()->Create(owner,(void(*)(void))callback,NULL,env,&layout,names,NULL,1);
    assert(result);return result;
}
static XL6_NativeClassCallbackContextV1 context(const XL6_CodeOwner* owner,const char* argument,const char* result)
{
    const XL6_NativeHandlePolicyV1* policy=xl6TlsServerRequestHandlePolicyV1Get();
    return (XL6_NativeClassCallbackContextV1){{{argument,7,policy},owner,&schema},{result,7,policy},schema.identity};
}
static void* request(void)
{
    static const uint8 name[]={'a',0,'b'};
    return xl6TlsServerRequestCopyV1((xbytesview){name,sizeof(name)},(xbytesview){0});
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static void retainedAfterClose(unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner a={0},b={0};XL6_CodeOwner ownerA={sizeof(ownerA),&a,acquire,release},ownerB={sizeof(ownerB),&b,acquire,release};
    State state={0};state.mode=mode;XL6_Closure* original=target(&ownerB,&state);
    char argumentName[]="payload",resultName[]="payload";
    XL6_Closure* driver=xl6NativeClassCallbackCreateV1(context(&ownerA,argumentName,resultName),original);assert(driver);
    memset(argumentName,'x',7);memset(resultName,'y',7);
    xl6ClosureRuntimeV1Get()->Release(original);a.closed=b.closed=true;
    assert(a.pins==1&&b.pins==1&&a.acquired==1&&b.acquired==1);
    void* raw=request();assert(raw);xvalue* output=NULL;
    bool ok=xl6NativeClassCallbackInvokeTakeV1(driver,&raw,&output);
    assert(!raw&&state.calls==1&&state.saved);
    assert(ok==(mode<2));assert((output!=NULL)==(mode==0));
    if(mode==2)assert(!xrtGetError());
    if(mode==3||mode==4)assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_RANGE);
    if(mode==5)assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError();xrtValueRelease(output);
    xl6ClosureRuntimeV1Get()->Release(driver);
    assert(a.pins==1&&!b.pins&&a.acquired==1&&b.acquired==1);
    xvalue* handle=xrtValueObjectGet(state.saved,(xstrview){"payload",7});void* native=NULL;
    assert(handle&&xrtValueGetHandle(handle,&native,NULL,NULL)&&native);
    xvalue* name=xl6TlsServerRequestNameV1(native);xbytesview bytes={0};
    assert(name&&xrtValueGetBytes(name,&bytes)&&bytes.Size==3&&!memcmp(bytes.Data,"a\0b",3));
    xrtValueRelease(name);xrtValueRelease(state.saved);
    assert(!a.pins&&a.released==1&&b.released==1&&!xrtGetError());balanced(&before);
}
static void prefix(bool invoking,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner a={0},b={0};XL6_CodeOwner ownerA={sizeof(ownerA),&a,acquire,release},ownerB={sizeof(ownerB),&b,acquire,release};
    State state={0};XL6_Closure* original=target(&ownerB,&state),*driver=NULL;
    XL6_NativeClassCallbackContextV1 recipe=context(&ownerA,"payload","payload");
    void* raw=NULL;xvalue* output=NULL;bool ok=false;
    if(invoking){driver=xl6NativeClassCallbackCreateV1(recipe,original);assert(driver);raw=request();assert(raw);}
    assert(xrtMemDebugFailAfter(point));
    if(invoking)ok=xl6NativeClassCallbackInvokeTakeV1(driver,&raw,&output);
    else driver=xl6NativeClassCallbackCreateV1(recipe,original);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(!point)assert(hit);
    if(hit)assert(invoking?(!ok&&!output&&!raw):!driver);
    else assert(invoking?(ok&&output&&!raw):driver!=NULL);
    xrtClearError();xrtValueRelease(output);xrtValueRelease(state.saved);
    xl6ClosureRuntimeV1Get()->Release(driver);xl6ClosureRuntimeV1Get()->Release(original);
    assert(!a.pins&&!b.pins&&a.acquired==a.released&&b.acquired==b.released&&!xrtGetError());balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(unsigned mode=0;mode<6;++mode)retainedAfterClose(mode);
    for(unsigned operation=0;operation<2;++operation)for(size_t point=0;point<128;++point)prefix(operation!=0,point);
    puts("native class callback: independent Close leases, owned arguments, first failures, 256 OOM prefixes: ok");
    return 0;
}
