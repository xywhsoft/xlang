#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(
        !strcmp(choice,"sync")?XL6_ARTIFACT_TCP_PROXY_DIAL_SYNC:
        !strcmp(choice,"async")?XL6_ARTIFACT_TCP_PROXY_DIAL_INTERRUPT_FUTURE:XL6_ARTIFACT_EXECUTION_TCP_PROXY_DIAL);
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan p=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;
    }
    assert(argc==1);XL6_ArtifactOwnershipPlan sync=plan("sync"),async=plan("async"),resource=plan("resource");
    assert(!xl6ArtifactRuntimeSetHas(&sync.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(!xl6ArtifactRuntimeSetHas(&sync.runtime.components,XL6_ARTIFACT_TCP_PROXY_DIAL_FUTURE));
    assert(!xl6ArtifactRuntimeSetHas(&async.runtime.components,XL6_ARTIFACT_TCP_PROXY_DIAL_SYNC));
    assert(!xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_EXECUTION_TCP_DIAL));
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_EXECUTION_TCP_DIAL_COMMON));
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_EXECUTION_NET_RESOLVER));
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_PROXY_DIAL_NATIVE));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpProxyDialStartV1")==XL6_ARTIFACT_TCP_PROXY_DIAL_FUTURE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpProxyDialV1")==XL6_ARTIFACT_TCP_PROXY_DIAL_SYNC);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionTcpPrepareDialV1")==XL6_ARTIFACT_EXECUTION_TCP_DIAL_COMMON);
    puts("Proxy plan: real Resolver parent, exact protocols/domains, sync prunes queue, Proxy prunes regular Dial provider");return 0;
}
