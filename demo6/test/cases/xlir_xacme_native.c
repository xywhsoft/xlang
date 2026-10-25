/* Real upstream + host ABI boundary. No network credentials or public CA. */
#include "src/runtime/xl6_xacme.c"
#include "src/runtime/xl6_execution_net_engine.h"
#include <assert.h>
#include <stdio.h>
static xvalue* strings(const char* const* items,size_t n)
{
    xvalue* a=xrtValueArray();assert(a);
    for(size_t i=0;i<n;++i)assert(xrtValueArrayAppendNew(a,xrtValueString(xrtStrView(items[i]))));
    return a;
}
static size_t calls;
static bool onDns(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    xstrview f,t;assert(frame->ArgumentCount==2);
    assert(xrtValueGetString(frame->Arguments[0],&f)&&xrtValueGetString(frame->Arguments[1],&t));
    assert(f.Size==26&&t.Size==3);++calls;
    if(context){
        AcmeOwner* self=*(AcmeOwner**)context;xl6_error_scope nested={0};assert(xl6ErrorScopeBegin(&nested));
        (void)xl6AcmeProviderCapsV1(self);
        xl6_error_result rejected=xl6ErrorScopeEnd(&nested);
        assert(!rejected.ok&&xrtErrorKind(rejected.error)==XERR_STATE);xrtErrorFree(rejected.error);
    }
    xvalue* answer=xrtValueBool(true);assert(answer);
    return xrtCallResultSetTake(result,0,&answer);
}
static bool edges(xrtownershipref ref,void* context)
{assert(ref.Data&&ref.Ops);++*(size_t*)context;return true;}
typedef struct MockDns {xacmehttp http;const char* url;bool add;} MockDns;
static bool onMockDns(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    MockDns* dns=context;xstrview fqdn,txt;assert(frame->ArgumentCount==2);
    assert(xrtValueGetString(frame->Arguments[0],&fqdn)&&xrtValueGetString(frame->Arguments[1],&txt));
    char body[2048],url[512];xacmehttpresponse response={0};
    int n=snprintf(body,sizeof(body),"{\"host\":\"%.*s\",\"value\":\"%.*s\"}",(int)fqdn.Size,fqdn.Data,(int)txt.Size,txt.Data);
    assert(n>0&&(size_t)n<sizeof(body));
    snprintf(url,sizeof(url),"%s/%s-txt",dns->url,dns->add?"set":"clear");
    bool ok=xacmeHttpExchange(&dns->http,"POST",url,"application/json",(xstrview){body,(size_t)n},&response)&&response.iStatus==200;
    xacmeHttpResponseUnit(&response);if(!ok)return false;
    xvalue* answer=xrtValueBool(true);return answer&&xrtCallResultSetTake(result,0,&answer);
}
static char* readPem(const char* path)
{
    FILE* file=fopen(path,"rb");assert(file);assert(!fseek(file,0,SEEK_END));long size=ftell(file);assert(size>0);
    rewind(file);char* text=xrtMalloc((size_t)size+1);assert(text);
    assert(fread(text,1,(size_t)size,file)==(size_t)size);fclose(file);text[size]=0;return text;
}
static void mockFlow(void)
{
    const char* directory=getenv("XACME_MOCK_DIRECTORY");if(!directory)return;
    char* pem=readPem(getenv("XACME_MOCK_CA"));
    const char* fields[]={directory,"","","","xacme@example.test",pem,""};
    const char* dnsServer[]={getenv("XACME_MOCK_RESOLVER")};
    xvalue* values=strings(fields,7);xvalue* resolvers=strings(dnsServer,1);
    AcmeOwner* c=xl6AcmeConfigCreateV1(values,resolvers,3000,3000,10000);assert(c);
    MockDns contexts[2]={0};xvalue* callbacks[2]={0};
    xrtparamdesc parameters[2]={{.Type=xrtTypeString()},{.Type=xrtTypeString()}};
    const xrttype* returns[]={xrtTypeBool()};xrtfunctionsig signature={.ParamCount=2,.Params=parameters,.ReturnCount=1,.ReturnTypes=returns};
    for(size_t i=0;i<2;++i){
        contexts[i].url=getenv("XACME_MOCK_CHALL");contexts[i].add=i==0;
        assert(xacmeHttpInit(&contexts[i].http,NULL,NULL,3000));
        xrtcallable* callable=xrtCallableCreate(&signature,onMockDns,&contexts[i],NULL);assert(callable);
        callbacks[i]=xrtValueCallableTake(&callable);assert(callbacks[i]);
    }
    AcmeOwner* provider=xl6AcmeProviderCustomV1(xrtStrView("mock"),0,callbacks[0],callbacks[1],NULL);assert(provider);
    AcmeOwner* client=xl6AcmeClientCreateV1(c);assert(client);
    char* account=xl6AcmeAccountPemV1(client);assert(account);
    xstrview root=xrtStrView(getenv("XACME_MOCK_ROOT"));
    assert(xl6AcmeStoreSaveV1(root,xrtStrView(directory),xrtStrView(account),xrtStrView(""),xrtStrView(""),0));
    xrtFree(account);
    const char* names[]={"example.test","*.example.test"};xvalue* domainsValue=strings(names,2);
    AcmeOwner* issued=xl6AcmeIssueV1(client,domainsValue,provider,true,xrtStrView(""),0,false);assert(issued);
    assert(issued->grant.sFullchainPem&&issued->grant.sKeyPem);xl6AcmeDropV1(issued);
    AcmeOwner* first=xl6AcmeIssueV1(client,domainsValue,provider,false,root,30,true);assert(first&&first->renewed);
    AcmeOwner* again=xl6AcmeIssueV1(client,domainsValue,provider,false,root,30,true);assert(again&&!again->renewed);
    AcmeOwner* loaded=xl6AcmeStoreLoadV1(root,xrtStrView(names[0]),3);assert(loaded);
    assert(!strcmp(loaded->grant.sFullchainPem,first->grant.sFullchainPem));
    assert(!strcmp(loaded->grant.sKeyPem,first->grant.sKeyPem));
    AcmeOwner* obtained=xl6AcmeObtainV1(c,domainsValue,provider,root,30);assert(obtained&&!obtained->renewed);
    assert(xl6AcmeClientControlV1(client,0,xrtStrView(first->grant.sFullchainPem),xrtStrView(""),-1));
    assert(xl6AcmeClientControlV1(client,2,xrtStrView(""),xrtStrView(""),0));
    xl6AcmeDropV1(obtained);xl6AcmeDropV1(loaded);xl6AcmeDropV1(again);xl6AcmeDropV1(first);
    assert(xl6AcmeCleanupV1(client));xl6AcmeDropV1(client);
    assert(xl6AcmeCleanupV1(provider));xl6AcmeDropV1(provider);xl6AcmeDropV1(c);
    for(size_t i=0;i<2;++i){xrtValueRelease(callbacks[i]);assert(xacmeHttpUnit(&contexts[i].http));}
    xrtValueRelease(domainsValue);xrtValueRelease(values);xrtValueRelease(resolvers);xrtFree(pem);
    assert(xl6AcmeCleanupPendingV1(5000)==0);
    puts("xacme mock: TLS, JWS, TXT propagation, IssueEx, IssueStored, Store, Obtain, Revoke, Deactivate passed");
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    const char* fields[]={"","","","","","",""};
    xvalue* texts=strings(fields,7);xvalue* resolvers=strings(NULL,0);
    AcmeOwner* c=xl6AcmeConfigCreateV1(texts,resolvers,0,0,0);assert(c);
    size_t faults=0;
    for(size_t i=0;i<12;++i){
        xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
        xl6_error_scope failureScope={0};assert(xl6ErrorScopeBegin(&failureScope));
        assert(xrtMemDebugFailAfter(i));AcmeOwner* trial=xl6AcmeConfigCreateV1(texts,resolvers,0,0,0);
        bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&failureScope);
        if(injected){assert(!trial&&!outcome.ok);++faults;}else assert(trial&&outcome.ok);
        xl6AcmeDropV1(trial);xrtErrorFree(outcome.error);xrtClearError();xrtMemDebugSnapshot(&after);
        assert(after.LiveCount==before.LiveCount&&after.LiveBytes==before.LiveBytes);
    }
    assert(faults>=8);
    size_t references=0;assert(count(c,&references)&&references==1);
    size_t outgoing=0;assert(trace(c,edges,&outgoing)&&outgoing==0);
    uint64 us=99;assert(xlacmeTimeout(2,&us)&&us==2000);
    assert(xlacmeTimeout(-1,&us)&&us==0);
    assert(xlacmeTimeout(INT64_MAX,&us)&&us==UINT64_MAX);
    assert(!xlacmeTimeout(-2,&us));xrtClearError();
    const char* credentials[][4]={{"id","secret",""},{"token",""},{"id","secret",""},{"id","secret","region",""},{"id","secret",""}};
    const size_t counts[]={3,2,3,4,3};
    for(int i=0;i<5;++i){
        xvalue* v=strings(credentials[i],counts[i]);AcmeOwner* p=xl6AcmeProviderCreateV1(i+1,v,c);assert(p);
        assert(xl6AcmeProviderValidateV1(p));outgoing=0;assert(trace(p,edges,&outgoing)&&outgoing==2);
        assert(xl6AcmeCleanupV1(p)&&xl6AcmeCleanupV1(p));xl6AcmeDropV1(p);xrtValueRelease(v);
    }
    assert(xl6AcmeProviderCountV1()==5);
    for(int i=0;i<5;++i){xstrview id=xl6AcmeProviderRegistryIdV1(i);assert(id.Size>0);xrtFree((void*)id.Data);}
    /* Incomplete destruction must preserve the actual outer owner, even
     * after its last language/native user reference is gone. */
    xvalue* cfPending=strings(credentials[1],2);
    AcmeOwner* delayed=xl6AcmeProviderCreateV1(2,cfPending,c);assert(delayed);
    xacmehttp* transport=http(delayed);assert(transport&&transport->pEngine);
    transport->uTimeoutMs=1;
    xnetaddr local;assert(xrtNetAddrLoopback(&local,XNET_FAMILY_IPV4,0));
    xnetudp* blocker=xrtNetUdpBind(transport->pEngine,&local,0,NULL,NULL,NULL);assert(blocker);
    xl6_error_scope readinessScope={0};assert(xl6ErrorScopeBegin(&readinessScope));
    assert(!xl6AcmeCleanupV1(delayed)&&delayed->provider.pContext);
    xl6_error_result readiness=xl6ErrorScopeEnd(&readinessScope);assert(readiness.ok&&!readiness.error);
    xl6AcmeDropV1(delayed);
    double started=xrtTimer();assert(xl6ErrorScopeBegin(&readinessScope));
    assert(xl6AcmeCleanupPendingV1(0)==1);
    readiness=xl6ErrorScopeEnd(&readinessScope);assert(readiness.ok&&!readiness.error);
    assert(xrtTimer()-started<0.5);
    assert(xrtNetUdpClose(blocker));xrtNetUdpDestroy(blocker);xrtClearError();
    assert(xl6AcmeCleanupPendingV1(3000)==0);xrtValueRelease(cfPending);xrtClearError();
    xrtparamdesc params[2]={{.Name={"fqdn",4},.Type=xrtTypeString()},{.Name={"txt",3},.Type=xrtTypeString()}};
    const xrttype* returns[]={xrtTypeBool()};
    xrtfunctionsig sig={.Name={"dns",3},.ParamCount=2,.Params=params,.ReturnCount=1,.ReturnTypes=returns};
    AcmeOwner* callbackSelf=NULL;
    xrtcallable* call=xrtCallableCreate(&sig,onDns,&callbackSelf,NULL);assert(call);
    xvalue* cb=xrtValueCallableTake(&call);assert(cb);
    AcmeOwner* custom=xl6AcmeProviderCustomV1(xrtStrView("custom"),1,cb,cb,cb);assert(custom);
    callbackSelf=custom;
    for(int i=0;i<3;++i)assert(xl6AcmeProviderCallV1(custom,i,xrtStrView("_acme-challenge.test.local"),xrtStrView("abc")));
    assert(calls==3);outgoing=0;assert(trace(custom,edges,&outgoing)&&outgoing==3);
    assert(xl6AcmeCleanupV1(custom));xl6AcmeDropV1(custom);xrtValueRelease(cb);
    /* Taking one result leaves the other owned by the rollback shell. */
    AcmeOwner* grant=newOwner(AC_GRANT);assert(grant);
    grant->grant.sFullchainPem=copyText(xrtStrView("chain"));grant->grant.sKeyPem=copyText(xrtStrView("key"));
    void* original=grant->grant.sFullchainPem;void* taken=xl6AcmeGrantTakeV1(grant,false);
    assert(taken==original&&!grant->grant.sFullchainPem&&grant->grant.sKeyPem);
    xl6AcmeDropV1(grant);xrtFree(taken);
    /* Preserve the native diagnostic rather than rebuilding a text error. */
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6AcmeStoreLoadV1(xrtStrView("build/xacme/no-such-store"),xrtStrView("test.local"),1));
    xl6_error_result failure=xl6ErrorScopeEnd(&scope);assert(!failure.ok&&xrtErrorKind(failure.error)==XERR_NOT_FOUND);
    xrtErrorFree(failure.error);
    assert(xl6AcmeCleanupPendingV1(0)==0);
    /* The borrowed engine is a real retained runtime owner. Every call enters
     * it; explicit host Close cannot leave a usable dangling native pointer. */
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* engine=xl6ExecutionNetEngineCreateV1(source,NULL);assert(engine);
    assert(xl6ExecutionNetEngineStartV1(engine));
    xl6AcmeConfigEngineV1(c,engine);assert(c->borrowed==engine);
    xvalue* cf=strings(credentials[1],2);AcmeOwner* borrowed=xl6AcmeProviderCreateV1(2,cf,c);assert(borrowed);
    outgoing=0;assert(trace(borrowed,edges,&outgoing)&&outgoing==2);
    assert(xl6ExecutionResourceCloseV1(engine));
    assert(xl6AcmeCleanupV1(borrowed));xl6AcmeDropV1(borrowed);xrtValueRelease(cf);
    xl6AcmeDropV1(c);xrtValueRelease(texts);xrtValueRelease(resolvers);
    xl6ExecutionResourceReleaseV1(engine);assert(xl6ExecutionSourceCloseV1(source));
    XL6_ExecutionRetireResultV1 retired={0};
    for(size_t i=0;i<5000&&!retired.ready;++i){retired=xl6ExecutionSourceDestroyV1(&source);if(!retired.ready)xrtSleep(1);}
    assert(retired.ready&&retired.ok&&!source);
    mockFlow();
    puts("xacme native: five providers, callback, ownership, timeout, transfer and errors passed");
    return 0;
}
