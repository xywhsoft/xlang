#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan selectPlan(const char* choice)
{
    size_t id=!strcmp(choice,"source")?XL6_ARTIFACT_EXECUTION_SOURCE:!strcmp(choice,"pool")?XL6_ARTIFACT_EXECUTION_POOL:XL6_ARTIFACT_EXECUTION_EXECUTOR;
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(id);
    if(!strcmp(choice,"both")){
        XL6_ArtifactRuntimeSet pool=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_EXECUTION_POOL);
        for(size_t i=0;i<XL6_ARTIFACT_RUNTIME_WORD_COUNT;++i)roots.words[i]|=pool.words[i];
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){
        XL6_ArtifactOwnershipPlan selected=selectPlan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<selected.runtime.sourceCount;++i)puts(selected.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<selected.runtime.nativeModuleCount;++i)printf("#define %s\n",selected.runtime.nativeModules[i]);return 0;}
        return 2;
    }
    assert(argc==1&&executor_catalog_plan_main()==0);
    XL6_ArtifactOwnershipPlan source=selectPlan("source"),executor=selectPlan("executor"),pool=selectPlan("pool");
    assert(!has_module(&source.runtime,"XRT_MODULE_EXECUTOR")&&!has_module(&source.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!xl6ArtifactRuntimeSetHas(&source.runtime.components,XL6_ARTIFACT_CLOSURE));
    assert(has_module(&executor.runtime,"XRT_MODULE_EXECUTOR")&&!has_module(&executor.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!xl6ArtifactRuntimeSetHas(&executor.runtime.components,XL6_ARTIFACT_TASK_POOL));
    assert(has_module(&pool.runtime,"XRT_MODULE_TASK_POOL")&&!has_module(&pool.runtime,"XRT_MODULE_EXECUTOR"));
    assert(!xl6ArtifactRuntimeSetHas(&pool.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
    assert(!xl6ArtifactRuntimeSetHas(&pool.runtime.components,XL6_ARTIFACT_CLOSURE));
    assert(!xl6ArtifactRuntimeSetHas(&pool.runtime.components,XL6_ARTIFACT_TASK));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionExecutorCreateV1")==XL6_ARTIFACT_EXECUTION_EXECUTOR);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionTaskPoolCreateV1")==XL6_ARTIFACT_EXECUTION_POOL);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TaskPoolReleaseClearedOwnerV1")==XL6_ARTIFACT_TASK_POOL);
    puts("Execution resources plan: source/Executor/TaskPool independent; exact owned family domains and symbol providers");return 0;
}
