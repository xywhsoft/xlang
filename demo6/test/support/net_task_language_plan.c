#define main old_executor_plan_main
#include "executor_catalog_plan.c"
#undef main
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK_NET_LANGUAGE);
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&plan));
    assert(has_module(&plan.runtime,"XRT_MODULE_TASK_NET"));
    assert(!has_module(&plan.runtime,"XRT_MODULE_TASK_POOL")&&!has_module(&plan.runtime,"XRT_MODULE_EXECUTOR"));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_TASK));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_TASK_SUBMIT_CLOSURE));
    bool prepared=false,network=false;
    for(size_t i=0;i<plan.runtime.sourceCount;++i){
        prepared|=!strcmp(plan.runtime.sources[i],"src/runtime/xl6_task_result_closure.c");
        network|=!strcmp(plan.runtime.sources[i],"src/runtime/xl6_task_net_language.c");
        assert(strcmp(plan.runtime.sources[i],"src/runtime/xl6_task_pool.c"));
    }
    assert(prepared&&network);
    if(argc==1){puts("NetTask language plan: actual typed closure/owned network/catalog providers; no pool/executor");return 0;}
    assert(argc==2);
    if(!strcmp(argv[1],"--sources"))for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);
    else {assert(!strcmp(argv[1],"--modules"));for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);}
    return 0;
}
