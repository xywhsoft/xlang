#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_closure.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
typedef struct Owner {unsigned holds,acquires,releases;} Owner;
static int acquire(void* raw) {Owner* state=raw;++state->holds;++state->acquires;return 1;}
static void release(void* raw) {Owner* state=raw;assert(state->holds==1);--state->holds;++state->releases;}
static void dropEnvironment(void* raw) { (void)raw; }
static bool traceEnvironment(const void* raw,XL6_ModuleOwnershipVisit visit,void* data)
{ (void)raw;(void)data;return visit!=NULL; } /* Only borrowed fixture coordination pointers. */
typedef struct Call {
    xl6_task_group* group;xfuture* source;XL6_Closure* self;XL6_CodeOwner owner;
    xerror* error;unsigned calls,mode;bool dropCreator;
} Call;
static const XL6_FutureValueProfileV1 profile={sizeof(profile),XL6_FUTURE_VALUE_VOID,0,UINT64_C(17),NULL};
static void balanced(const xmemdebugsnapshot* before)
{
    assert(!xrtGetError());xmemdebugsnapshot after={0};xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool invoke(void* raw,xfuture** output)
{
    Call* call=raw;++call->calls;XL6_TaskGroupStatsV1 stats={0};
    assert(xl6TaskGroupGetV1(call->group,&stats)&&stats.active>=1);
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref ref=xl6TaskGroupOwnershipV1(call->group);size_t count=0;
    assert(!ref.Ops->Count(ref.Data,&count)); /* Real in-flight group, not a fake graph leaf. */
    assert(xl6CodeOwnerScopeActive(&call->owner));assert(xrtOwnershipScopeEnd(&freeze));
    if(call->dropCreator){xl6ClosureRuntimeV1Get()->Release(call->self);call->self=NULL;}
    if(call->mode==1){xrtSetErrorTake(xrtErrorRef(call->error));return false;}
    if(call->mode==2)return false; /* Authoritative failure without a diagnostic. */
    if(call->mode==3)return true; /* Invalid non-null Future result must fail, too. */
    if(call->mode==4){
        xpromise* producer=xrtPromiseCreate(output,NULL);if(!producer)return false;
        bool ok=xrtPromiseResolve(producer,NULL);xrtPromiseDestroy(producer);
        return ok;
    }
    if(call->mode==5)assert(xl6TaskGroupClose(call->group));
    if(call->mode>=6){
        *output=xrtFutureRef(call->source);
        if(call->mode==7)xrtSetErrorTake(xrtErrorRef(call->error));
        return call->mode==7;
    }
    *output=xrtFutureRef(call->source);return *output!=NULL;
}
static XL6_Closure* make(Call* call,Owner* owner)
{
    call->owner=(XL6_CodeOwner){sizeof(XL6_CodeOwner),owner,acquire,release};
    call->self=xl6ClosureRuntimeV1Get()->Create(&call->owner,(void (*)(void))invoke,NULL,
        call,dropEnvironment,NULL,NULL,0,traceEnvironment);assert(call->self&&owner->holds==1);return call->self;
}
static void cases(void)
{
    for(unsigned mode=0;mode<=7;++mode){
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Owner owner={0};Call call={.mode=mode,.dropCreator=true};
        call.group=xl6TaskGroupCreate();xpromise* producer=xrtPromiseCreate(&call.source,NULL);assert(call.group&&producer);
        if(mode==1||mode==7){xrtSetErrorKind(XERR_RANGE);call.error=xrtTakeError();assert(call.error);}
        XL6_Closure* starter=make(&call,&owner);xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        xfuture* output=xl6TaskGroupStartLanguageV1(profile,call.group,starter);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(call.calls==1&&!call.self&&!owner.holds&&owner.acquires==1&&owner.releases==1);
        if((mode>=1&&mode<=3)||mode>=6){
            assert(!output&&!outcome.ok);
            if(mode==3)assert(outcome.error&&xrtErrorKind(outcome.error)==XERR_STATE);
            else assert(outcome.error==call.error);
            XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(call.group,&stats)&&!stats.active&&stats.accepting);
        }else assert(output&&outcome.ok&&(mode==4||output==call.source));
        xrtErrorFree(outcome.error);xrtClearError();
        /* A failed starter must not cancel another holder's source Future. */
        xcancel* cancel=xrtFutureCancelToken(call.source);assert(cancel&&!xrtCancelRequested(cancel));
        xrtCancelDestroy(cancel);assert(xrtPromiseResolve(producer,NULL));
        if(output){assert(xl6TaskGroupJoin(call.group));xrtFutureDestroy(output);}
        xrtPromiseDestroy(producer);xrtFutureDestroy(call.source);xrtErrorFree(call.error);
        xl6TaskGroupDestroy(call.group);balanced(&before);
    }
}
static void rejection(void)
{
    xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Owner owner={0};Call call={0};
    XL6_TaskGroupConfigV1 config={sizeof(config),NULL,1,0};call.group=xl6TaskGroupCreateConfiguredV1(&config);
    xpromise* producer=xrtPromiseCreate(&call.source,NULL);assert(call.group&&producer);make(&call,&owner);
    xfuture* output=xl6TaskGroupStartLanguageV1(profile,call.group,call.self);assert(output&&call.calls==1);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6TaskGroupStartLanguageV1(profile,call.group,call.self));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(!outcome.ok&&call.calls==1);xrtErrorFree(outcome.error);xrtClearError();
    assert(xl6TaskGroupClose(call.group)&&xrtPromiseResolve(producer,NULL));
    assert(xl6ErrorScopeBegin(&scope));assert(!xl6TaskGroupStartLanguageV1(profile,call.group,call.self));
    outcome=xl6ErrorScopeEnd(&scope);assert(!outcome.ok&&call.calls==1);xrtErrorFree(outcome.error);xrtClearError();
    xl6ClosureRuntimeV1Get()->Release(call.self);assert(!owner.holds);
    xrtFutureDestroy(output);xrtPromiseDestroy(producer);xrtFutureDestroy(call.source);xl6TaskGroupDestroy(call.group);balanced(&before);
}
static void prefixes(void)
{
    for(unsigned kind=0;kind<2;++kind)for(size_t point=0;point<128;++point){
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Owner owner={0};Call call={.mode=kind?4:0};
        call.group=xl6TaskGroupCreate();xpromise* producer=xrtPromiseCreate(&call.source,NULL);assert(call.group&&producer);make(&call,&owner);
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(point));
        xfuture* output=xl6TaskGroupStartLanguageV1(profile,call.group,call.self);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(outcome.ok==!hit&&(hit?!output:output!=NULL));xrtErrorFree(outcome.error);xrtClearError();
        if(hit){XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(call.group,&stats)&&!stats.active);}
        assert(xrtPromiseResolve(producer,NULL));if(output)xrtFutureDestroy(output);
        xl6ClosureRuntimeV1Get()->Release(call.self);assert(!owner.holds&&owner.acquires==owner.releases);
        xrtPromiseDestroy(producer);xrtFutureDestroy(call.source);xl6TaskGroupDestroy(call.group);balanced(&before);
        if(!hit){printf("TaskGroup starter kind=%u allocation-prefix=%zu balanced\n",kind,point);break;}
        assert(point<127);
    }
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    cases();rejection();prefixes();
    puts("TaskGroup starter: actual code scope, no retained callback, exact Future owner, NULL failure precedence and rollback balanced");return 0;
}
