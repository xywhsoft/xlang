#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool contains(const char* const* names,size_t count,const char* value)
{for(size_t i=0;i<count;++i)if(!strcmp(names[i],value))return true;return false;}
static void selection(const char* symbol,bool grouped)
{
    XL6_ArtifactRuntimeId id=xl6ArtifactRuntimeSymbolOwner(symbol);
    assert(id==(grouped ? XL6_ARTIFACT_NATIVE_CALLBACK_GROUP:XL6_ARTIFACT_NATIVE_CALLBACK));
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(id);XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_CALLBACK));
    assert(!!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_CALLBACK_GROUP)==grouped);
    assert(contains(plan.sources,plan.sourceCount,"src/runtime/xl6_native_callback.c"));
    assert(contains(plan.sources,plan.sourceCount,"src/runtime/xl6_native_callback_group.c")==grouped);
    XL6_ArtifactOwnershipPlan owned;assert(xl6ArtifactOwnershipPlanBuild(&plan,&owned));
    assert(contains(owned.domains,owned.domainCount,"xl6OwnershipDomainNativeCallbackV1Get"));
    assert(contains(owned.domains,owned.domainCount,"xl6OwnershipDomainNativeCallbackGroupV1Get")==grouped);
    assert(contains(owned.runtime.sources,owned.runtime.sourceCount,"src/runtime/xl6_native_callback_group.c")==grouped);
    printf("GROUP_COMPONENT %s grouped=%d sources=%zu domains=%zu\n",symbol,(int)grouped,plan.sourceCount,owned.domainCount);
}
int main(void)
{
    selection("xl6NativeCallbackRuntimeV1Get",false);
    selection("xl6NativeCallbackReportFailureV1",false);
    selection("xl6OwnershipDomainNativeCallbackV1Get",false);
    selection("xl6NativeCallbackGroupRuntimeV1Get",true);
    selection("xl6NativeCallbackCreateSharedAdapterV1",true);
    selection("xl6OwnershipDomainNativeCallbackGroupV1Get",true);
    assert(xl6ArtifactRuntimeSymbolOwner("not_a_native_callback_symbol")==XL6_ARTIFACT_RUNTIME_COUNT);
    XL6_ArtifactRuntimeSet empty={{0}};XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&empty,&plan) && !plan.sourceCount);
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_CALLBACK_GROUP));
    puts("GROUP_COMPONENTS_OK");return 0;
}
