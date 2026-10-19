#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan taskPlan(void)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK);
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan ownership;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&ownership));
    return ownership;
}
int main(int argc,char** argv)
{
    XL6_ArtifactOwnershipPlan plan=taskPlan();
    if(argc==2&&!strcmp(argv[1],"--sources")){
        for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);
        return 0;}
    if(argc==2&&!strcmp(argv[1],"--modules")){
        for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);
        return 0;}
    assert(argc==1&&executor_catalog_plan_main()==0);
    assert(has_module(&plan.runtime,"XRT_MODULE_TASK_POOL"));
    assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_CAPTURE_V2));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
    puts("Task checked plan: actual task/pool/capture sources and native selectors, no detached error-ledger dependency");
    return 0;
}
