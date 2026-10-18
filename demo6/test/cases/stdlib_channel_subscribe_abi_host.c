/* Ordinary native ABI/ownership smoke against an actual shipping DLL/SO.
 * No implementation is compiled into this host; every operation is resolved
 * from the selected product. Keep the product resident for the process lifetime. */
#define XRT_MODULE_CHANNEL_SUBSCRIBE
#define XRT_MODULE_MEMORY_DEBUG
#ifndef XL6_SUBSCRIBE_DECL_HEADER
#error "Select the frozen product's declaration header"
#endif
#include XL6_SUBSCRIBE_DECL_HEADER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static struct {
    __typeof__(&xrtChannelInit) Init;
    __typeof__(&xrtChannelUnit) Unit;
    __typeof__(&xrtChannelCount) Count;
    __typeof__(&xrtChannelTrySend) Send;
    __typeof__(&xrtChannelSubscribeCreateV1) Create;
    __typeof__(&xrtChannelSubscribeStart) Start;
    __typeof__(&xrtChannelSubscribeRef) Ref;
    __typeof__(&xrtChannelSubscribeDestroy) Destroy;
    __typeof__(&xrtChannelSubscribeStop) Stop;
    __typeof__(&xrtChannelSubscribeResult) Result;
    __typeof__(&xrtMemDebugFailAfter) FailAfter;
    __typeof__(&xrtMemDebugFailClear) FailClear;
    __typeof__(&xrtMemDebugFailTriggered) FailTriggered;
    __typeof__(&xrtMemDebugSnapshot) Snapshot;
    __typeof__(&xrtClearError) ClearError;
} api;

typedef struct Context {
    xchannel* channel;
    int notified, dropped;
    bool active, release;
    xchannelselectresult selected;
    ptr value;
} Context;

static void require(bool condition,const char* message)
{
    if (!condition) { fprintf(stderr,"[FAIL] %s\n",message); exit(1); }
}

static void notify(xchannelsubscription* subscription,ptr data)
{
    Context* context=data;
    context->active=true;
    require(++context->notified==1,"duplicate notification");
    require(api.Result(subscription,&context->selected,&context->value),"notification preceded Ready");
    (void)api.Count(context->channel); /* Reenter the originating Channel. */
    if (context->release) {
        api.Destroy(subscription);
        require(!context->dropped,"context dropped before callback return");
    }
    context->active=false;
}

static void drop(const void* data)
{
    Context* context=(Context*)data;
    require(!context->active,"context freed during callback");
    require(++context->dropped==1,"duplicate Data release");
}

static const xchannelsubscribepolicyv1 policy={sizeof(policy),notify,drop};

static void probe(void)
{
    xchannel channel;
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    xmemdebugsnapshot before,after;
    require(api.Init(&channel,0u),"channel initialization failed");
    api.Snapshot(&before);
    for (uint64 i=0;i<3u;i++) {
        Context context={.channel=&channel,.release=true};
        require(api.FailAfter(i),"allocation injection unavailable");
        xchannelsubscription* subscription=api.Create(&item,1u,&context,&policy);
        if (i<2u) {
            require(!subscription && api.FailTriggered(),"Create allocation prefix incomplete");
            require(!context.notified && !context.dropped,"failure consumed context");
        } else {
            require(subscription!=NULL,"Create failed after complete allocation prefix");
            require(api.Start(subscription),"Start failed");
            require(api.Send(&channel,(ptr)(uintptr_t)71u)==XCHANNEL_OK,"rendezvous failed");
            require(!api.FailTriggered(),"wait/wake/cleanup attempted an allocation");
            require(context.notified==1 && context.dropped==1 && context.selected.Wait==XWAIT_OK &&
                context.selected.Index==0u && context.selected.Result==XCHANNEL_OK &&
                (uintptr_t)context.value==71u,"completion ownership/result mismatch");
        }
        api.FailClear();
        api.ClearError();
        api.Snapshot(&after);
        require(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,
            "Create/completion retained live storage");
    }
    {
        Context context={.channel=&channel};
        xchannelsubscription* subscription=api.Create(&item,1u,&context,&policy);
        require(subscription && api.Start(subscription),"stop setup failed");
        require(api.Ref(subscription)==subscription,"Ref failed");
        api.Destroy(subscription);
        require(!context.notified && !context.dropped,"nonlast release stopped subscription");
        require(api.Stop(subscription,XWAIT_TIMEOUT),"timeout request failed");
        require(context.notified==1 && context.selected.Wait==XWAIT_TIMEOUT &&
            context.selected.Index==XCHANNEL_SELECT_NONE,"timeout result mismatch");
        api.Destroy(subscription);
        require(context.dropped==1,"timeout context leaked");
    }
    require(api.Unit(&channel),"physical waiter unlink incomplete");
    api.Snapshot(&after);
    require(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,"subscription leaked");
}

int main(int argc,char** argv)
{
    require(argc==2,"expected a frozen product path");
    #if defined(_WIN32)
    HMODULE library=LoadLibraryA(argv[1]);
    #define IMPORT(member,name) do { FARPROC address=GetProcAddress(library,#name); \
        _Static_assert(sizeof(address)==sizeof(api.member),"function pointer ABI mismatch"); \
        memcpy(&api.member,&address,sizeof(api.member)); require(api.member!=NULL,#name " missing"); } while(0)
    #else
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    #define IMPORT(member,name) do { void* address=dlsym(library,#name); \
        _Static_assert(sizeof(address)==sizeof(api.member),"function pointer ABI mismatch"); \
        memcpy(&api.member,&address,sizeof(api.member)); require(api.member!=NULL,#name " missing"); } while(0)
    #endif
    require(library!=NULL,"unable to load ordinary product");
    IMPORT(Init,xrtChannelInit); IMPORT(Unit,xrtChannelUnit);
    IMPORT(Count,xrtChannelCount); IMPORT(Send,xrtChannelTrySend);
    IMPORT(Create,xrtChannelSubscribeCreateV1); IMPORT(Start,xrtChannelSubscribeStart);
    IMPORT(Ref,xrtChannelSubscribeRef); IMPORT(Destroy,xrtChannelSubscribeDestroy);
    IMPORT(Stop,xrtChannelSubscribeStop); IMPORT(Result,xrtChannelSubscribeResult);
    IMPORT(FailAfter,xrtMemDebugFailAfter); IMPORT(FailClear,xrtMemDebugFailClear);
    IMPORT(FailTriggered,xrtMemDebugFailTriggered); IMPORT(Snapshot,xrtMemDebugSnapshot);
    IMPORT(ClearError,xrtClearError);
    probe();
    puts("[PASS] actual product subscription ABI, allocation prefix, reentry and physical retirement");
    return 0;
}
