#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(!strcmp(choice,"config")?XL6_ARTIFACT_NET_ENGINE_CONFIG:
        !strcmp(choice,"stats")?XL6_ARTIFACT_NET_ENGINE_STATS:XL6_ARTIFACT_NET_ENGINE_LANGUAGE);
    if(!strcmp(choice,"all")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_ENGINE_CONFIG));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_ENGINE_STATS));
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){
        XL6_ArtifactOwnershipPlan selected=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<selected.runtime.sourceCount;++i)puts(selected.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<selected.runtime.nativeModuleCount;++i)printf("#define %s\n",selected.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<selected.domainCount;++i)printf("%s(),\n",selected.domains[i]);return 0;}
        return 2;
    }
    assert(argc==1);
    XL6_ArtifactOwnershipPlan config=plan("config"),engine=plan("engine"),stats=plan("stats");
    assert(!xl6ArtifactRuntimeSetHas(&config.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!xl6ArtifactRuntimeSetHas(&config.runtime.components,XL6_ARTIFACT_CLOSURE));
    assert(!xl6ArtifactRuntimeSetHas(&stats.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!has_module(&stats.runtime,"XRT_MODULE_NET_ENGINE"));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_DETACHED_WORK));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_TASK));
    assert(!has_module(&engine.runtime,"XRT_MODULE_EXECUTOR")&&!has_module(&engine.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!has_module(&engine.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&engine.runtime,"XRT_MODULE_NET_TCP"));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetEngineCreateV1")==XL6_ARTIFACT_NET_ENGINE_LANGUAGE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetEngineConfigWithWorkersV1")==XL6_ARTIFACT_NET_ENGINE_CONFIG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetEngineSnapshotPostsAcceptedV1")==XL6_ARTIFACT_NET_ENGINE_STATS);
    puts("Engine plan: typed source/native/work domains, immutable leaves, no Executor/TaskPool/Resolver/TCP");return 0;
}
