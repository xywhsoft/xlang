#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan plan(void)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_EXECUTOR_JOBS);
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    XL6_ArtifactOwnershipPlan closed=plan();
    if(argc==2&&!strcmp(argv[1],"--sources")){
        for(size_t i=0;i<closed.runtime.sourceCount;++i)puts(closed.runtime.sources[i]);
        return 0;
    }
    if(argc==2&&!strcmp(argv[1],"--modules")){
        for(size_t i=0;i<closed.runtime.nativeModuleCount;++i)printf("#define %s\n",closed.runtime.nativeModules[i]);
        return 0;
    }
    assert(argc==1&&executor_catalog_plan_main()==0);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutorSubmitClosureV1")==XL6_ARTIFACT_EXECUTOR_JOBS);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutorThrowIfFailedV1")==XL6_ARTIFACT_EXECUTOR_JOBS);
    assert(has_module(&closed.runtime,"XRT_MODULE_EXECUTOR"));
    assert(xl6ArtifactRuntimeSetHas(&closed.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTOR_JOBS));
    assert(xl6ArtifactRuntimeSetHas(&closed.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_EXECUTOR));
    assert(!xl6ArtifactRuntimeSetHas(&closed.runtime.components,XL6_ARTIFACT_EXECUTOR));
    assert(!xl6ArtifactRuntimeSetHas(&closed.runtime.components,XL6_ARTIFACT_TASK));
    assert(!xl6ArtifactRuntimeSetHas(&closed.runtime.components,XL6_ARTIFACT_TASK_POOL));
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_EXECUTOR);
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan async;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&async));
    assert(!xl6ArtifactRuntimeSetHas(&async.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
    puts("Executor jobs plan: exact producer/native domains, no TaskPool/default async dependency or implicit producer");
    return 0;
}
