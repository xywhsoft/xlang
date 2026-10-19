/* Exact resident closure, without JIT or shipping-product acceptance claims. */
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool has_module(const XL6_ArtifactRuntimePlan* plan,const char* module)
{
    for(size_t i=0;i<plan->nativeModuleCount;++i)if(!strcmp(plan->nativeModules[i],module))return true;
    return false;
}
int main(void)
{
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddExecutorV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainNativeExecutorV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_EXECUTOR);
    XL6_ArtifactRuntimePlan plan;XL6_ArtifactOwnershipPlan ownership;
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_EXECUTOR);
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_EXECUTOR));
    assert(!has_module(&ownership.runtime,"XRT_MODULE_EXECUTOR"));
    for(size_t i=0;i<ownership.domainCount;++i)assert(strcmp(ownership.domains[i],"xl6OwnershipDomainNativeExecutorV1Get"));
    roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_EXECUTOR);
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    assert(has_module(&ownership.runtime,"XRT_MODULE_EXECUTOR"));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_EXECUTOR));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_TASK));
    assert(ownership.domainCount==1&&!strcmp(ownership.domains[0],"xl6OwnershipDomainNativeExecutorV1Get"));
    puts("Executor catalog plan: independent native domain, default async pruning, exact symbol owners");
    return 0;
}
