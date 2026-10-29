#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const XL6_ArtifactRuntimeId roots[]={
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_STREAM_NATIVE,
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_STREAM_FUTURE_NATIVE,
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_LISTENER_NATIVE,
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_LISTENER_FUTURE_NATIVE,
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_DIAL_NATIVE,
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_DIAL_FUTURE_NATIVE,
    XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_DIAL_PROXY_NATIVE
};
static XL6_ArtifactOwnershipPlan plan(unsigned kind)
{
    assert(kind<8);
    XL6_ArtifactRuntimeSet selected=xl6ArtifactRuntimeSetSingleton(roots[kind==7?5:kind]);
    if(kind==7)assert(xl6ComponentSetAdd(selected.words,XL6_ARTIFACT_RUNTIME_COUNT,roots[6]));
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&selected,&runtime));
    assert(xl6ArtifactOwnershipPlanBuild(&runtime,&result));return result;
}
static bool has(const XL6_ArtifactOwnershipPlan* p,XL6_ArtifactRuntimeId id)
{return xl6ArtifactRuntimeSetHas(&p->runtime.components,id);}
int main(int argc,char** argv)
{
    if(argc==3){
        unsigned kind=(unsigned)strtoul(argv[2],NULL,10);XL6_ArtifactOwnershipPlan p=plan(kind);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;i++)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;i++)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;i++)printf("%s(),\n",p.domains[i]);return 0;}
        return 2;
    }
    assert(argc==1);
    for(unsigned i=0;i<8;i++){
        XL6_ArtifactOwnershipPlan p=plan(i);
        assert(has(&p,roots[0]));
        assert(has(&p,roots[1])==(i==1));
        assert(has(&p,roots[2])==(i==2||i==3));
        assert(has(&p,roots[3])==(i==3));
        assert(has(&p,roots[4])==(i>=4));
        assert(has(&p,roots[5])==(i==5||i==7));
        assert(has(&p,roots[6])==(i==6||i==7));
        assert(!has(&p,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_DIAL_NATIVE));
        assert(!has(&p,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_PROXY_DIAL_NATIVE));
        assert(!has(&p,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_NATIVE));
        assert(!has(&p,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_SERVER_NATIVE));
        assert(!has(&p,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_RESOLVER_NATIVE));
    }
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddTlsStreamEventsV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddTlsListenerEventsV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddTlsDialDoneV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    puts("TLS native domains: 8 independent exact closures; typed TLS policy groups; no unrelated Future/Listener/Proxy providers");
    return 0;
}
