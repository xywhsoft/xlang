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
    __typeof__(&xrtChannelSubscribeCreateOwnedV1) CreateOwned;
    __typeof__(&xrtChannelSubscribeOwnership) Ownership;
    __typeof__(&xrtChannelSubscribeOwnershipQuiescenceV1Get) Quiescence;
    __typeof__(&xrtChannelSubscribeOwnershipAdapterV1) Adapter;
    __typeof__(&xrtOwnershipFreezeTryBegin) Freeze;
    __typeof__(&xrtOwnershipMutationBegin) Mutation;
    __typeof__(&xrtOwnershipScopeEnd) ScopeEnd;
    __typeof__(&xrtOwnershipInspectReachable) Inspect;
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

typedef struct OwnedContext {
    Context base;
    xchannelsubscription* subscription;
    int refs;
} OwnedContext;

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

static bool owned_count(const void* data,size_t* count)
{
    const OwnedContext* context=data;
    if(!context || context->refs<=0) return false;
    *count=(size_t)context->refs;
    return true;
}
static bool owned_trace(const void* data,xrtownershipvisitor visit,ptr opaque)
{
    const OwnedContext* context=data;
    return !context->subscription || visit(api.Ownership(context->subscription),opaque);
}
static const xrtownershipops owned_ops={owned_count,owned_trace};
static void owned_notify(xchannelsubscription* subscription,ptr data)
{
    OwnedContext* context=data;
    xrtownershipscope freeze={0};
    const xrtownershipquiescencev1* quiescence=api.Quiescence();
    require(api.Freeze(&freeze),"owned callback inherited API mutation");
    require(quiescence->Busy(subscription),"active callback not reported busy");
    require(api.ScopeEnd(&freeze),"owned callback freeze end failed");
    notify(subscription,&context->base);
}
static void owned_drop(const void* data)
{
    OwnedContext* context=(OwnedContext*)data;
    xrtownershipscope freeze={0},mutation={0};
    require(api.Freeze(&freeze) && api.ScopeEnd(&freeze),"owned Drop inherited API mutation");
    require(api.Mutation(&mutation) && context->refs>0,"owned Data reference invalid");
    --context->refs;
    ++context->base.dropped;
    require(api.ScopeEnd(&mutation),"owned Drop scope end failed");
}
static const xchannelsubscribeownershipv1 owned_policy={sizeof(owned_policy),owned_notify,owned_drop,&owned_ops};

static void inspect_owned(OwnedContext* context,bool rooted)
{
    xrtownershipscope freeze={0};
    xrtownershipref anchor=api.Ownership(context->subscription);
    const xchannelsubscribeownershipv1* allowed[]={&owned_policy};
    xrtownershipresult result={0};
    bool reachable=false;
    require(api.Freeze(&freeze),"owned graph freeze failed");
    require(api.Adapter(anchor,allowed,1u)!=NULL,"resident owned policy not admitted");
    require(api.Inspect(&anchor,1u,NULL,0u,&reachable,&result,NULL,NULL) &&
        result.NodeCount==2u && result.EdgeCount==2u && result.ExternalRootCount==(rooted?1u:0u) &&
        reachable==rooted,"actual product graph does not match real Data/registration rights");
    require(api.ScopeEnd(&freeze),"owned graph freeze end failed");
}

static void probe_owned(void)
{
    xchannel channel;
    OwnedContext context={.base.channel=&channel,.refs=1};
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    xmemdebugsnapshot before,after;
    require(api.Init(&channel,0u),"owned channel init failed");
    api.Snapshot(&before);
    for(uint64 i=0;i<2u;i++) {
        require(api.FailAfter(i),"owned constructor allocation injection unavailable");
        require(!api.CreateOwned(&item,1u,&context,&owned_policy) && api.FailTriggered() &&
            context.refs==1 && !context.base.dropped,"owned allocation failure consumed Data");
        api.FailClear(); api.ClearError();
    }
    context.subscription=api.CreateOwned(&item,1u,&context,&owned_policy);
    require(context.subscription!=NULL,"owned create failed");
    inspect_owned(&context,false);
    require(api.Start(context.subscription),"owned Start failed");
    inspect_owned(&context,true);
    {
        xrtownershipscope freeze={0};
        const xchannelsubscribeownershipv1* allowed[]={&owned_policy};
        int token=0;
        require(api.Freeze(&freeze),"pending graph claim probe failed");
        const xrtownershipadapterv1* adapter=api.Adapter(api.Ownership(context.subscription),allowed,1u);
        require(adapter && !adapter->Claim(context.subscription,&token),"collector cancelled accepted pending work");
        require(api.ScopeEnd(&freeze),"pending graph probe end failed");
    }
    require(api.Send(&channel,(ptr)(uintptr_t)91u)==XCHANNEL_OK && context.base.notified==1 &&
        context.base.value==(ptr)(uintptr_t)91u,"owned completion mismatch");
    inspect_owned(&context,false);
    {
        xrtownershipscope freeze={0};
        const xchannelsubscribeownershipv1* allowed[]={&owned_policy};
        xrtownershipref ref=api.Ownership(context.subscription);
        int token=0;
        require(api.Freeze(&freeze),"settled cycle freeze failed");
        const xrtownershipadapterv1* adapter=api.Adapter(ref,allowed,1u);
        require(adapter && adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token),"settled native cycle claim failed");
        ++context.refs; /* The actual independent planner pin on the context. */
        adapter->Clear(ref.Data,&token);
        require(context.refs==2 && !context.base.dropped,"Clear ran Data Drop early");
        require(api.ScopeEnd(&freeze),"settled clear end failed");
        require(adapter->Finish(ref.Data,&token) && adapter->Finish(ref.Data,&token) &&
            context.refs==1 && context.base.dropped==1,"settled Finish not exact/idempotent");
        xchannelsubscription* subscription=context.subscription;
        context.subscription=NULL;
        api.Destroy(subscription);
        adapter->Drop(ref.Data);
        --context.refs; /* Return the planner's context pin; no extra Data Drop. */
    }
    require(!context.refs && context.base.dropped==1 && api.Unit(&channel),"settled native cycle leaked");
    api.Snapshot(&after);
    require(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,"owned provider retained live storage");
}

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
    IMPORT(CreateOwned,xrtChannelSubscribeCreateOwnedV1); IMPORT(Ownership,xrtChannelSubscribeOwnership);
    IMPORT(Quiescence,xrtChannelSubscribeOwnershipQuiescenceV1Get); IMPORT(Adapter,xrtChannelSubscribeOwnershipAdapterV1);
    IMPORT(Freeze,xrtOwnershipFreezeTryBegin); IMPORT(Mutation,xrtOwnershipMutationBegin);
    IMPORT(ScopeEnd,xrtOwnershipScopeEnd); IMPORT(Inspect,xrtOwnershipInspectReachable);
    IMPORT(FailAfter,xrtMemDebugFailAfter); IMPORT(FailClear,xrtMemDebugFailClear);
    IMPORT(FailTriggered,xrtMemDebugFailTriggered); IMPORT(Snapshot,xrtMemDebugSnapshot);
    IMPORT(ClearError,xrtClearError);
    probe();
    probe_owned();
    puts("[PASS] actual product subscription ABI, allocation prefix, reentry and physical retirement");
    return 0;
}
