/* Actual source closure; no source-name inference or shipping acceptance claim. */
#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
int main(int argc,char** argv)
{
    if(argc==2&&(!strcmp(argv[1],"--pool-sources")||!strcmp(argv[1],"--pool-modules"))){
        XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK_POOL);
        XL6_ArtifactRuntimePlan plan;XL6_ArtifactOwnershipPlan ownership;
        assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
        if(!strcmp(argv[1],"--pool-sources"))
            for(size_t i=0;i<ownership.runtime.sourceCount;++i)puts(ownership.runtime.sources[i]);
        else
            for(size_t i=0;i<ownership.runtime.nativeModuleCount;++i)printf("#define %s\n",ownership.runtime.nativeModules[i]);
        return 0;
    }
    if(argc==2&&!strcmp(argv[1],"--task-sources")){
        XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK);
        XL6_ArtifactRuntimePlan plan;assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
        for(size_t i=0;i<plan.sourceCount;++i)puts(plan.sources[i]);
        return 0;
    }
    assert(argc==1);
    assert(executor_catalog_plan_main()==0);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TaskPoolControlV1Get")==XL6_ARTIFACT_TASK_POOL);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TaskRuntimeV1Get")==XL6_ARTIFACT_TASK);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TaskNativeBeginV1")==XL6_ARTIFACT_TASK_POOL);
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK_POOL);
    XL6_ArtifactRuntimePlan plan;XL6_ArtifactOwnershipPlan ownership;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    assert(has_module(&ownership.runtime,"XRT_MODULE_TASK_POOL"));
    assert(has_module(&ownership.runtime,"XRT_MODULE_ATOMIC"));
    assert(!has_module(&ownership.runtime,"XRT_MODULE_EXECUTOR"));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_TASK));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_CAPTURE));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_CAPTURE_V2));
    assert(!xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_EXECUTOR));
    assert(ownership.domainCount==1&&!strcmp(ownership.domains[0],"xl6OwnershipDomainTaskPoolV1Get"));
    roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TASK);
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    unsigned task=0,pool=0;
    for(size_t i=0;i<ownership.domainCount;++i){
        task+=!strcmp(ownership.domains[i],"xl6OwnershipDomainTaskV1Get");
        pool+=!strcmp(ownership.domains[i],"xl6OwnershipDomainTaskPoolV1Get");
    }
    assert(task==1&&pool==1&&xl6ArtifactRuntimeSetHas(&ownership.runtime.components,XL6_ARTIFACT_TASK_POOL));
    puts("Task pool control plan: management excludes worker frames/captures/default async; legacy tasks include both exact domains");
    return 0;
}
