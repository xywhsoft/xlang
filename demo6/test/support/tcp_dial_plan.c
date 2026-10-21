#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(bool dial)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(
        dial?XL6_ARTIFACT_EXECUTION_TCP_DIAL:XL6_ARTIFACT_EXECUTION_TCP);
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&result));
    return result;
}
int main(int argc,char** argv)
{
    if(argc==2){XL6_ArtifactOwnershipPlan p=plan(true);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;
    }
    assert(argc==1);XL6_ArtifactOwnershipPlan plain=plan(false),dial=plan(true);
    assert(!xl6ArtifactRuntimeSetHas(&plain.runtime.components,XL6_ARTIFACT_EXECUTION_TCP_DIAL));
    assert(!xl6ArtifactRuntimeSetHas(&plain.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_DIAL_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&plain.runtime.components,XL6_ARTIFACT_EXECUTION_NET_RESOLVER));
    assert(xl6ArtifactRuntimeSetHas(&dial.runtime.components,XL6_ARTIFACT_EXECUTION_NET_RESOLVER));
    assert(xl6ArtifactRuntimeSetHas(&dial.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_DIAL_NATIVE));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionTcpDialAddressesSourceV1")==XL6_ARTIFACT_EXECUTION_TCP_DIAL);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddDialDoneV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    puts("Dial plan: exact native domains, real Resolver parent, TCP-only pruning");return 0;
}
