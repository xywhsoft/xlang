#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK_SUBMIT_CLOSURE);
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan ownership;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&ownership));
    if(argc==2&&!strcmp(argv[1],"--sources")){
        for(size_t i=0;i<ownership.runtime.sourceCount;++i)puts(ownership.runtime.sources[i]);
        return 0;}
    if(argc==2&&!strcmp(argv[1],"--modules")){
        for(size_t i=0;i<ownership.runtime.nativeModuleCount;++i)printf("#define %s\n",ownership.runtime.nativeModules[i]);
        return 0;}
    assert(argc==1&&has_module(&ownership.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
    puts("Task submit plan: actual selected closure/Future-value/task providers and native selectors");
    return 0;
}
