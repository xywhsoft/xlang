/* Optional resident component. Native graph callbacks never live in a JIT
 * module; custom language callbacks remain ordinary traced xvalue owners. */
#include "release/extlibs/xacme/native/xacme_impl.c"
#define XRUNTIME_MODULE_ALL
#include "include/xruntime_decl.h"
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "xl6_execution_resource.h"
#include "xl6_ownership_catalog.h"
#include "src/api/xl_foreign_error.h"

typedef enum AcmeKind { AC_CONFIG=1, AC_CLIENT, AC_PROVIDER, AC_GRANT } AcmeKind;
typedef struct AcmeOwner {
    volatile int32 refs;
    xatomic32 busy;
    AcmeKind kind;
    const void* claim;
    bool closing,cleared,queued;
    struct AcmeOwner* next;
    XL6_ExecutionResourceV1* borrowed;
    char* text[7];
    char** resolvers;
    size_t resolverCount;
    int64 timeout,issueTimeout;
    uint32 propagateTimeout;
    xacmeclient* client;
    xacmednsprovider provider;
    int providerKind;
    xvalue* callbacks[3];
    xacmeissuegrant grant;
    bool renewed;
} AcmeOwner;
static xatomic32 pendingLock;
static AcmeOwner* pending;
static void failed(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }
static void mutationBegin(xrtownershipscope* s)
{ if(!xrtOwnershipMutationBegin(s))abort(); }
static void mutationEnd(xrtownershipscope* s)
{ if(!xrtOwnershipScopeEnd(s))abort(); }
static AcmeOwner* newOwner(AcmeKind kind)
{
    AcmeOwner* o=xrtCalloc(1,sizeof(*o));
    if(o){o->refs=1;o->kind=kind;}return o;
}
static void pendingEnter(void)
{
    uint32 expected=0;
    while(!xrtAtomic32CompareExchange(&pendingLock,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))
    {expected=0;xrtThreadYield();}
}
static void pendingLeave(void){xrtAtomic32Store(&pendingLock,0,XMEMORY_RELEASE);}
static void enqueue(AcmeOwner* o)
{
    xrtownershipscope s={0};mutationBegin(&s);pendingEnter();
    if(!o->queued){o->queued=true;o->next=pending;pending=o;}
    pendingLeave();mutationEnd(&s);
}
static xacmehttp* http(AcmeOwner* o)
{
    if(o->kind==AC_CLIENT)return o->client?&o->client->Http:NULL;
    /* Every audited built-in provider context has Http as its first field.
     * Custom callbacks are identified separately, never cast to this layout. */
    return o->kind==AC_PROVIDER&&o->providerKind&&o->provider.pContext?
        (xacmehttp*)o->provider.pContext:NULL;
}
static bool cleanup(AcmeOwner* o)
{
    o->closing=true;
    if(o->client){
        if(!xrtAcmeClientCleanup(o->client))return false;
        xrtAcmeClientDestroy(o->client);o->client=NULL;
    }
    if(o->kind==AC_PROVIDER&&o->providerKind&&o->provider.pContext){
        switch(o->providerKind){
        case 1:xrtAcmeDnsAliProviderUnit(&o->provider);break;
        case 2:xrtAcmeDnsCfProviderUnit(&o->provider);break;
        case 3:xrtAcmeDnsTencentProviderUnit(&o->provider);break;
        case 4:xrtAcmeDnsAwsProviderUnit(&o->provider);break;
        case 5:xrtAcmeDnsHuaweiProviderUnit(&o->provider);break;
        default:abort();
        }
        if(o->provider.pContext)return false;
    }
    return true;
}
static bool cleanupProgress(AcmeOwner* o)
{
    /* Readiness is an explicit bool, not a diagnostic-only language failure.
     * Keep an incoming diagnostic intact and consume this cleanup attempt's
     * native timeout/error. Semantic callback failures keep their scope bit. */
    xerror* previous=xrtTakeError();bool ready=cleanup(o);
    xrtClearError();xrtSetErrorTake(previous);return ready;
}
static void freeOwner(AcmeOwner* o)
{
    for(size_t i=0;i<7;++i){if(o->text[i]){xrtSecureZero(o->text[i],strlen(o->text[i]));xrtFree(o->text[i]);}}
    for(size_t i=0;i<o->resolverCount;++i)xrtFree(o->resolvers[i]);
    xrtFree(o->resolvers);
    for(size_t i=0;i<3;++i)xrtValueRelease(o->callbacks[i]);
    xl6ExecutionResourceReleaseV1(o->borrowed);
    xrtAcmeGrantUnit(&o->grant);xrtFree(o);
}
void xl6AcmeDropV1(void* raw)
{
    AcmeOwner* o=raw;if(!o)return;
    int32 remaining=xrtOwnershipRefRelease(&o->refs);
    if(remaining<0)abort();
    if(remaining)return;
    /* No language owner remains. On incomplete native cleanup an explicit
     * resident pending owner retains every dependency until CleanupPending. */
    if(!cleanup(o)){
        xrtownershipscope s={0};mutationBegin(&s);o->refs=1;mutationEnd(&s);
        enqueue(o);return;
    }
    freeOwner(o);
}
static bool enter(AcmeOwner* o,AcmeKind kind,bool closing,XL6_ExecutionScopeV1* engine)
{
    uint32 expected=0;xrtownershipscope s={0};mutationBegin(&s);
    /* Acquire the instance gate BEFORE reading mutable lifecycle fields.
     * Mutation scopes exclude graph Freeze, but do not serialize two callers. */
    bool acquired=o&&o->kind==kind&&
        xrtAtomic32CompareExchange(&o->busy,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED);
    bool ok=acquired&&!o->cleared&&!o->claim&&(closing||!o->closing);
    if(acquired&&!ok)xrtAtomic32Store(&o->busy,0,XMEMORY_RELEASE);
    mutationEnd(&s);
    if(!ok){xrtSetErrorKind(XERR_STATE);return false;}
    if(!closing&&o->borrowed&&(!xl6ExecutionResourceEnterV1(o->borrowed,true,engine)||!engine->engine)){
        if(engine->resource)xl6ExecutionResourceLeaveV1(engine);
        xrtownershipscope rollback={0};mutationBegin(&rollback);
        xrtAtomic32Store(&o->busy,0,XMEMORY_RELEASE);mutationEnd(&rollback);return false;
    }
    return true;
}
static void leave(AcmeOwner* o,XL6_ExecutionScopeV1* engine)
{
    if(engine->resource)xl6ExecutionResourceLeaveV1(engine);
    xrtownershipscope s={0};mutationBegin(&s);
    xrtAtomic32Store(&o->busy,0,XMEMORY_RELEASE);mutationEnd(&s);
}
static bool validText(xstrview v)
{
    if((!v.Data&&v.Size)||v.Size==SIZE_MAX||(v.Size&&memchr(v.Data,0,v.Size))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return true;
}
static char* copyText(xstrview v)
{
    if(!validText(v))return NULL;
    char* p=xrtMalloc(v.Size+1);if(p){if(v.Size)memcpy(p,v.Data,v.Size);p[v.Size]=0;}return p;
}
static cstr optionalText(cstr text){return text&&text[0]?text:NULL;}
static bool arrayText(const xvalue* a,size_t i,xstrview* v)
{return xrtValueGetString(xrtValueArrayGet(a,i),v)&&validText(*v);}
typedef struct AcmeDomains {xstrview local[16];xstrview* data;size_t count;} AcmeDomains;
static void domainsUnit(AcmeDomains* views)
{if(views->data!=views->local)xrtFree(views->data);views->data=NULL;views->count=0;}
static bool domains(const xvalue* a,AcmeDomains* views)
{
    if(!a||xrtValueType(a)!=XVALUE_ARRAY||!(views->count=xrtValueCount(a))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(views->count>SIZE_MAX/sizeof(xstrview)){xrtSetErrorKind(XERR_RANGE);return false;}
    /* Upstream limits DISTINCT domains, not input entries. Preserve order and
     * duplicates and let its own flow/store validation decide their meaning. */
    views->data=views->count<=16?views->local:xrtMalloc(views->count*sizeof(xstrview));
    if(!views->data)return false;
    for(size_t i=0;i<views->count;++i)if(!arrayText(a,i,&views->data[i]))return false;
    return true;
}
void* xl6AcmeConfigCreateV1(const xvalue* text,const xvalue* resolvers,
    int64_t timeout,int64_t propagate,int64_t issue)
{
    if(!text||xrtValueType(text)!=XVALUE_ARRAY||xrtValueCount(text)!=7||
       !resolvers||xrtValueType(resolvers)!=XVALUE_ARRAY||timeout< -1||issue< -1||
       propagate<0||(uint64)propagate>UINT32_MAX){xrtSetErrorKind(XERR_ARGUMENT);failed();return NULL;}
    AcmeOwner* o=newOwner(AC_CONFIG);if(!o){failed();return NULL;}
    o->timeout=timeout;o->issueTimeout=issue;o->propagateTimeout=(uint32)propagate;
    for(size_t i=0;i<7;++i){xstrview v;if(!arrayText(text,i,&v)||!(o->text[i]=copyText(v)))goto failure;}
    o->resolverCount=xrtValueCount(resolvers);
    if(o->resolverCount>SIZE_MAX/sizeof(char*)){xrtSetErrorKind(XERR_RANGE);goto failure;}
    if(o->resolverCount){
        o->resolvers=xrtCalloc(o->resolverCount,sizeof(char*));if(!o->resolvers){o->resolverCount=0;goto failure;}
        for(size_t i=0;i<o->resolverCount;++i){xstrview v;if(!arrayText(resolvers,i,&v)||!(o->resolvers[i]=copyText(v)))goto failure;}
    }
    return o;
failure:xl6AcmeDropV1(o);failed();return NULL;
}
void xl6AcmeConfigEngineV1(void* raw,void* borrowed)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 scope={0};
    if(!o||o->kind!=AC_CONFIG||o->borrowed||!borrowed||
       !xl6ExecutionResourceEnterV1(borrowed,true,&scope)){xrtSetErrorKind(XERR_ARGUMENT);failed();return;}
    if(!scope.engine){xl6ExecutionResourceLeaveV1(&scope);xrtSetErrorKind(XERR_TYPE);failed();return;}
    o->borrowed=xl6ExecutionResourceRetainV1(borrowed);xl6ExecutionResourceLeaveV1(&scope);
    if(!o->borrowed)failed();
}
static void config(AcmeOwner* o,xacmeaccountconfig* account,xacmeclientconfig* client,xnetengine* engine)
{
    xrtAcmeAccountConfigInit(account);xrtAcmeClientConfigInit(client);
    account->sDirectoryUrl=o->text[0];account->sAccountKeyPem=o->text[1];
    account->Eab=(xacmeeab){o->text[2],o->text[3]};account->sContactEmail=o->text[4];
    client->pAccount=account;client->sCaPem=o->text[5];client->sCertKeyPem=o->text[6];
    client->pBorrowedEngine=engine;client->uTimeoutMs=o->timeout;
    client->uPropagateTimeoutMs=o->propagateTimeout;client->uIssueTimeoutMs=o->issueTimeout;
    client->sPropagateResolvers=(const cstr*)o->resolvers;client->iPropagateResolverCount=o->resolverCount;
}
void* xl6AcmeClientCreateV1(void* raw)
{
    AcmeOwner* c=raw;XL6_ExecutionScopeV1 scope={0};
    if(!enter(c,AC_CONFIG,false,&scope)){failed();return NULL;}
    AcmeOwner* o=newOwner(AC_CLIENT);xacmeaccountconfig a;xacmeclientconfig v;
    if(o&&(!c->borrowed||(o->borrowed=xl6ExecutionResourceRetainV1(c->borrowed)))){
        config(c,&a,&v,scope.engine);o->client=xrtAcmeClientCreate(&v);}
    leave(c,&scope);
    if(!o||!o->client){xl6AcmeDropV1(o);failed();return NULL;}return o;
}
bool xl6AcmeCleanupV1(void* raw)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 scope={0};
    if(!o||!enter(o,o->kind,true,&scope)){failed();return false;}
    bool ok=cleanupProgress(o);leave(o,&scope);return ok;
}
void* xl6AcmeAccountPemV1(void* raw)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 scope={0};
    if(!enter(o,AC_CLIENT,false,&scope)){failed();return NULL;}
    char* result=xrtAcmeClientAccountPem(o->client);leave(o,&scope);if(!result)failed();return result;
}
static bool callback(AcmeOwner* o,size_t index,xstrview fqdn,xstrview txt)
{
    xvalue* args[]={xrtValueString(fqdn),xrtValueString(txt)};
    xrtcallresult result=XRT_CALL_RESULT_INIT;bool answer=false;
    bool ok=args[0]&&args[1];
    if(ok){xrtcallframe frame={.ArgumentCount=2,.Arguments=args};
        ok=xrtValueInvoke(o->callbacks[index],&frame,&result)&&xrtCallResultCount(&result)==1&&
            xrtValueGetBool(xrtCallResultGet(&result,0),&answer);}
    xrtCallResultUnit(&result);xrtValueRelease(args[0]);xrtValueRelease(args[1]);
    if(!ok)xl6ForeignCallbackFail();
    return ok&&answer;
}
static bool customAdd(xacmednsprovider* p,xstrview f,xstrview t){return callback(p->pContext,0,f,t);}
static bool customRemove(xacmednsprovider* p,xstrview f,xstrview t){return callback(p->pContext,1,f,t);}
static bool customPropagate(xacmednsprovider* p,xstrview f,xstrview t){return callback(p->pContext,2,f,t);}
void* xl6AcmeProviderCustomV1(xstrview id,int64_t caps,const xvalue* add,const xvalue* remove,const xvalue* propagate)
{
    if(caps<0||(uint64)caps>UINT32_MAX||!xrtValueIsCallable(add)||!xrtValueIsCallable(remove)||
       ((caps&XACME_DNS_CAP_PROPAGATE)&&!xrtValueIsCallable(propagate))){xrtSetErrorKind(XERR_ARGUMENT);failed();return NULL;}
    AcmeOwner* o=newOwner(AC_PROVIDER);if(!o){failed();return NULL;}
    o->text[0]=copyText(id);o->callbacks[0]=xrtValueRetain((xvalue*)add);o->callbacks[1]=xrtValueRetain((xvalue*)remove);
    if(caps&XACME_DNS_CAP_PROPAGATE)o->callbacks[2]=xrtValueRetain((xvalue*)propagate);
    o->provider=(xacmednsprovider){o->text[0],(uint32)caps,o,customAdd,customRemove,o->callbacks[2]?customPropagate:NULL};
    if(!o->text[0]||!xrtAcmeDnsProviderValidate(&o->provider)){xl6AcmeDropV1(o);failed();return NULL;}return o;
}
void* xl6AcmeProviderCreateV1(int64_t kind,const xvalue* fields,void* raw)
{
    AcmeOwner* c=raw;XL6_ExecutionScopeV1 scope={0};xstrview views[4];char* values[4]={0};
    if(!c||!enter(c,AC_CONFIG,false,&scope)){failed();return NULL;}
    AcmeOwner* o=NULL;size_t count=xrtValueCount(fields);bool ok=false;
    if(!fields||xrtValueType(fields)!=XVALUE_ARRAY||count>4||kind<1||kind>5){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    for(size_t i=0;i<count;++i)if(!arrayText(fields,i,&views[i])||!(values[i]=copyText(views[i])))goto done;
    o=newOwner(AC_PROVIDER);if(!o)goto done;o->providerKind=(int)kind;
    if(c->borrowed&&!(o->borrowed=xl6ExecutionResourceRetainV1(c->borrowed)))goto done;
    if(kind==1&&count==3){xacmednaliconfig v;xrtAcmeDnsAliConfigInit(&v);v.sAccessKeyId=values[0];v.sAccessKeySecret=values[1];v.sEndpoint=optionalText(values[2]);ok=xrtAcmeDnsAli(&v,scope.engine,&o->provider);}
    else if(kind==2&&count==2){xacmednscfconfig v;xrtAcmeDnsCfConfigInit(&v);v.sApiToken=values[0];v.sEndpoint=optionalText(values[1]);ok=xrtAcmeDnsCf(&v,scope.engine,&o->provider);}
    else if(kind==3&&count==3){xacmednstencentconfig v;xrtAcmeDnsTencentConfigInit(&v);v.sSecretId=values[0];v.sSecretKey=values[1];v.sEndpoint=optionalText(values[2]);ok=xrtAcmeDnsTencent(&v,scope.engine,&o->provider);}
    else if(kind==4&&count==4){xacmednsawsconfig v;xrtAcmeDnsAwsConfigInit(&v);v.sAccessKeyId=values[0];v.sSecretAccessKey=values[1];v.sRegion=values[2];v.sEndpoint=optionalText(values[3]);ok=xrtAcmeDnsAws(&v,scope.engine,&o->provider);}
    else if(kind==5&&count==3){xacmednshuaaweiconfig v;xrtAcmeDnsHuaweiConfigInit(&v);v.sAccessKey=values[0];v.sSecretKey=values[1];v.sEndpoint=optionalText(values[2]);ok=xrtAcmeDnsHuawei(&v,scope.engine,&o->provider);}
    else xrtSetErrorKind(XERR_ARGUMENT);
done:
    for(size_t i=0;i<4;++i){if(values[i])xrtSecureZero(values[i],strlen(values[i]));xrtFree(values[i]);}
    leave(c,&scope);if(!ok){xl6AcmeDropV1(o);failed();return NULL;}return o;
}
bool xl6AcmeProviderValidateV1(void* raw)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 s={0};if(!enter(o,AC_PROVIDER,false,&s)){failed();return false;}
    bool ok=xrtAcmeDnsProviderValidate(&o->provider);leave(o,&s);if(!ok)failed();return ok;
}
bool xl6AcmeProviderCallV1(void* raw,int64_t operation,xstrview fqdn,xstrview txt)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 s={0};
    if(!validText(fqdn)||!validText(txt)||!enter(o,AC_PROVIDER,false,&s)){failed();return false;}
    bool ok=operation==0?o->provider.Add(&o->provider,fqdn,txt):operation==1?
        o->provider.Remove(&o->provider,fqdn,txt):operation==2&&o->provider.Propagate?
        o->provider.Propagate(&o->provider,fqdn,txt):false;
    leave(o,&s);if(!ok)failed();return ok;
}
xstrview xl6AcmeProviderIdV1(void* raw)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 scope={0};
    if(!enter(o,AC_PROVIDER,false,&scope)){failed();return (xstrview){0};}
    const char* id=o->provider.sId;
    size_t n=strlen(id);char* copy=xrtStrDupView((xstrview){id,n});
    leave(o,&scope);
    if(!copy)failed();
    return (xstrview){copy,copy?n:0};
}
int64_t xl6AcmeProviderCapsV1(void* raw)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 scope={0};
    if(!enter(o,AC_PROVIDER,false,&scope)){failed();return 0;}
    int64_t caps=o->provider.iCaps;leave(o,&scope);return caps;
}
int64_t xl6AcmeProviderCountV1(void){return (int64_t)xrtAcmeDnsProviderCount();}
xstrview xl6AcmeProviderRegistryIdV1(int64_t index)
{
    const char* id=index<0?NULL:xrtAcmeDnsProviderId((size_t)index);
    if(!id){xrtSetErrorKind(XERR_RANGE);failed();return (xstrview){0};}
    size_t n=strlen(id);char* copy=xrtStrDupView((xstrview){id,n});
    if(!copy)failed();
    return (xstrview){copy,copy?n:0};
}
void* xl6AcmeIssueV1(void* raw,const xvalue* names,void* dns,bool alternate,xstrview root,int64_t days,bool stored)
{
    AcmeOwner* o=raw;AcmeOwner* p=dns;XL6_ExecutionScopeV1 a={0},b={0};AcmeDomains list={0};
    if(days<INT_MIN||days>INT_MAX||!validText(root)||!domains(names,&list)||!enter(o,AC_CLIENT,false,&a)){domainsUnit(&list);failed();return NULL;}
    if(!enter(p,AC_PROVIDER,false,&b)){leave(o,&a);domainsUnit(&list);failed();return NULL;}
    AcmeOwner* g=newOwner(AC_GRANT);const char* path=root.Data?root.Data:"";bool ok=false;
    if(g)ok=stored?xrtAcmeClientIssueStored(o->client,list.data,list.count,&p->provider,path,(int)days,&g->grant,&g->renewed):
        xrtAcmeClientIssueEx(o->client,list.data,list.count,&p->provider,alternate,&g->grant);
    domainsUnit(&list);leave(p,&b);leave(o,&a);if(!ok){xl6AcmeDropV1(g);failed();return NULL;}return g;
}
void* xl6AcmeGrantTakeV1(void* raw,bool key)
{
    AcmeOwner* o=raw;if(!o||o->kind!=AC_GRANT){xrtSetErrorKind(XERR_TYPE);failed();return NULL;}
    char** slot=key?&o->grant.sKeyPem:&o->grant.sFullchainPem;char* result=*slot;*slot=NULL;
    if(!result){xrtSetErrorKind(XERR_STATE);failed();}return result;
}
bool xl6AcmeGrantRenewedV1(void* raw){AcmeOwner* o=raw;return o&&o->kind==AC_GRANT&&o->renewed;}
bool xl6AcmeClientControlV1(void* raw,int64_t operation,xstrview text,xstrview root,int64_t reason)
{
    AcmeOwner* o=raw;XL6_ExecutionScopeV1 scope={0};
    if(reason<INT_MIN||reason>INT_MAX||!validText(text)||!validText(root)||!enter(o,AC_CLIENT,false,&scope)){failed();return false;}
    /* S inputs are language-managed UTF-8 with Data[Size] == NUL. After the
     * embedded-NUL check, all C read paths borrow these exact bytes. */
    const char* t=text.Data?text.Data:"";const char* r=root.Data?root.Data:"";bool ok=false;
    ok=operation==0?xrtAcmeClientRevoke(o->client,t,(int)reason):operation==1?
        xrtAcmeClientRollover(o->client,t,*r?r:NULL):operation==2?xrtAcmeClientDeactivate(o->client):false;
    leave(o,&scope);if(!ok)failed();return ok;
}
void* xl6AcmeStoreLoadV1(xstrview root,xstrview domain,int64_t what)
{
    const char* r=root.Data?root.Data:"";const char* d=domain.Data?domain.Data:"";void* result=NULL;
    if(validText(root)&&validText(domain)){
        if(what==0)result=xrtAcmeStoreLoadAccount(r,d);
        else if(what==1)result=xrtAcmeStoreLoadCert(r,d);
        else if(what==2)result=xrtAcmeStoreLoadCertCa(r,d);
        else if(what==3){AcmeOwner* o=newOwner(AC_GRANT);if(o&&xrtAcmeStoreLoadGrant(r,d,&o->grant))result=o;else xl6AcmeDropV1(o);}
        else xrtSetErrorKind(XERR_ARGUMENT);
    }
    if(!result)failed();
    return result;
}
bool xl6AcmeStoreSaveV1(xstrview root,xstrview domain,xstrview chain,xstrview key,xstrview directory,int64_t what)
{
    const char* values[5]={0};xstrview views[]={root,domain,chain,key,directory};bool ok=false;
    for(size_t i=0;i<5;++i){if(!validText(views[i]))goto done;values[i]=views[i].Data?views[i].Data:"";}
    xacmeissuegrant grant={(str)values[2],(str)values[3]};
    if(what==0)ok=xrtAcmeStoreSaveAccount(values[0],values[1],values[2]);
    else if(what==1)ok=xrtAcmeStoreSaveCert(values[0],values[1],values[2],values[4]);
    else if(what==2)ok=xrtAcmeStoreSaveGrant(values[0],values[1],&grant,values[4]);
    else xrtSetErrorKind(XERR_ARGUMENT);
done:
    if(!ok)failed();
    return ok;
}
bool xl6AcmeStoreNeedRenewV1(xstrview root,xstrview domain,int64_t days)
{
    const char* r=root.Data?root.Data:"";const char* d=domain.Data?domain.Data:"";bool need=false;
    bool ok=validText(root)&&validText(domain)&&days>=INT_MIN&&days<=INT_MAX&&xrtAcmeStoreNeedRenew(r,d,(int)days,&need);
    if(!ok)failed();
    return need;
}
xvalue* xl6AcmeStoreListDomainsV1(xstrview root,int64_t capacity)
{
    const char* r=root.Data?root.Data:"";char (*items)[256]=NULL;xvalue* result=NULL;size_t count=0;
    if(capacity<0||(uint64)capacity>SIZE_MAX/256){xrtSetErrorKind(XERR_RANGE);goto done;}
    if(capacity)items=xrtCalloc((size_t)capacity,256);
    if(!validText(root)||(capacity&&!items)||!xrtAcmeStoreListDomains(r,items,(size_t)capacity,&count))goto done;
    result=xrtValueArray();if(!result)goto done;
    for(size_t i=0;i<count;++i){xvalue* v=xrtValueString((xstrview){items[i],strlen(items[i])});bool ok=v&&xrtValueArrayAppend(result,v);xrtValueRelease(v);if(!ok){xrtValueRelease(result);result=NULL;break;}}
done:xrtFree(items);if(!result)failed();return result;
}
void* xl6AcmeObtainV1(void* raw,const xvalue* names,void* dns,xstrview root,int64_t days)
{
    AcmeOwner* c=raw;AcmeOwner* p=dns;XL6_ExecutionScopeV1 a={0},b={0};AcmeDomains list={0};
    if(days<INT_MIN||days>INT_MAX||!domains(names,&list)||!enter(c,AC_CONFIG,false,&a)){domainsUnit(&list);failed();return NULL;}
    if(!enter(p,AC_PROVIDER,false,&b)){leave(c,&a);domainsUnit(&list);failed();return NULL;}
    xacmeaccountconfig account;xacmeclientconfig client;config(c,&account,&client,a.engine);
    xacmeobtainconfig v;xrtAcmeObtainConfigInit(&v);
    v.pAccount=&account;v.sCaPem=client.sCaPem;v.pBorrowedEngine=client.pBorrowedEngine;
    v.uTimeoutMs=client.uTimeoutMs;v.sPropagateResolvers=client.sPropagateResolvers;v.iPropagateResolverCount=client.iPropagateResolverCount;
    v.uPropagateTimeoutMs=client.uPropagateTimeoutMs;v.uIssueTimeoutMs=client.uIssueTimeoutMs;v.sCertKeyPem=client.sCertKeyPem;
    v.sStoreRoot=root.Data?root.Data:"";v.iRenewalDays=(int)days;
    AcmeOwner* g=newOwner(AC_GRANT);bool ok=g&&validText(root)&&xrtAcmeObtain(&v,list.data,list.count,&p->provider,&g->grant,&g->renewed);
    domainsUnit(&list);leave(p,&b);leave(c,&a);if(!ok){xl6AcmeDropV1(g);failed();return NULL;}return g;
}
int64_t xl6AcmeCleanupPendingV1(int64_t timeout)
{
    if(timeout< -1){xrtSetErrorKind(XERR_ARGUMENT);failed();return -1;}
    double deadline=__xrtWaitAfter(timeout);size_t remaining=0;AcmeOwner* batch;
    do{
        xrtownershipscope s={0};mutationBegin(&s);pendingEnter();batch=pending;pending=NULL;pendingLeave();mutationEnd(&s);
        remaining=0;
        while(batch){AcmeOwner* o=batch;batch=o->next;
            mutationBegin(&s);o->next=NULL;o->queued=false;mutationEnd(&s);
            xacmehttp* h=http(o);int64 saved=h?h->uTimeoutMs:0;
            if(h)h->uTimeoutMs=timeout==0?0:__xrtWaitRemaining(deadline);
            if(cleanupProgress(o)){
                mutationBegin(&s);o->refs=0;mutationEnd(&s);freeOwner(o);
            }else{
                h=http(o);if(h)h->uTimeoutMs=saved;
                enqueue(o);++remaining;
            }}
        size_t upstream=0;xerror* previous=xrtTakeError();
        (void)xrtAcmeCleanupPending(0,&upstream);xrtClearError();xrtSetErrorTake(previous);
        remaining+=upstream;
        if(!remaining||timeout==0||__xrtWaitExpired(deadline))break;
        xrtSleep(1);
    }while(true);
    return remaining>INT64_MAX?INT64_MAX:(int64_t)remaining;
}

/* Whole-domain graph: each wrapper's real references and every owning slot.
 * Native children have their own preparation/Finish and remain pinned until
 * the wrapper consumes their creator slots. No native destructor under Freeze. */
static bool count(const void* raw,size_t* n)
{const AcmeOwner* o=raw;if(!o||!n||o->refs<=0||o->cleared||xrtAtomic32Load(&o->busy,XMEMORY_ACQUIRE))return false;*n=(size_t)o->refs;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{
    AcmeOwner* o=(AcmeOwner*)raw;size_t n;if(!visit||!count(o,&n))return false;
    if(o->borrowed&&!visit(xl6ExecutionResourceOwnershipV1(o->borrowed),context))return false;
    for(size_t i=0;i<3;++i)if(o->callbacks[i]&&!visit(xrtValueOwnership(o->callbacks[i]),context))return false;
    xacmehttp* h=http(o);
    return !h||((!h->pResolver||visit(xrtNetResolverOwnership(h->pResolver),context))&&
        (!h->bEngineOwned||!h->pEngine||visit(xrtNetEngineOwnership(h->pEngine),context)));
}
static const xrtownershipops ownerOps={count,trace};
static bool hold(const void* raw){AcmeOwner* o=(AcmeOwner*)raw;return xrtOwnershipRefRetain(&o->refs)>0;}
static void drop(const void* raw){xl6AcmeDropV1((void*)raw);}
static bool claim(const void* raw,const void* token)
{AcmeOwner* o=(AcmeOwner*)raw;if(!token||o->cleared||(o->claim&&o->claim!=token))return false;o->claim=token;return true;}
static void restore(const void* raw,const void* token)
{AcmeOwner* o=(AcmeOwner*)raw;if(o->claim!=token||o->cleared)abort();o->claim=NULL;}
static bool ready(const void* raw){return ((const AcmeOwner*)raw)->closing;}
static xrtownershipprepareresult prepare(const void* raw,const void* token)
{AcmeOwner* o=(AcmeOwner*)raw;xrtownershipscope s={0};mutationBegin(&s);if(o->claim!=token)abort();o->closing=true;mutationEnd(&s);return XRT_OWNERSHIP_PREPARE_READY;}
static void clear(const void* raw,const void* token)
{AcmeOwner* o=(AcmeOwner*)raw;if(o->claim!=token||!o->closing||o->cleared)abort();o->cleared=true;}
static bool finish(const void* raw,const void* token)
{AcmeOwner* o=(AcmeOwner*)raw;if(o->claim!=token||!o->cleared)abort();return cleanup(o);}
static bool busy(const void* raw){return raw&&xrtAtomic32Load(&((const AcmeOwner*)raw)->busy,XMEMORY_ACQUIRE)!=0;}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6AcmeHandlePolicyV1Get()->Ops&&
        visit((xrtownershipref){raw,&ownerOps},context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6AcmeHandlePolicyV1Get,xl6AcmeDropV1,handleTrace)
static bool load(XL6_OwnershipCatalog* catalog)
{
    static const xrtownershipquiescencev1 q={sizeof(q),&ownerOps,busy};
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6AcmeHandlePolicyV1Get())&&xl6OwnershipCatalogAddQuiescenceV1(catalog,&q);
}
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)policies;(void)completion;
    static const xrtownershipadapterv1 a={sizeof(a),hold,drop,claim,restore,NULL,clear,finish};
    static const xrtownershippreparationv1 p={sizeof(p),&a,ready,prepare};
    if(ref.Ops!=&ownerOps||!ref.Data)return false;
    *adapter=&a;*preparation=&p;return true;
}
const XL6_OwnershipDomainV1* xl6OwnershipDomainXacmeV1Get(void)
{static const XL6_OwnershipDomainV1 d={sizeof(d),load,resolve,NULL};return &d;}

