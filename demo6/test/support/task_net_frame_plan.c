/* Real artifact dependency closure: a shared frame must not select a pool. */
#define main task_net_parent_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan plan(const char* which)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(!strcmp(which,"frame")?
        XL6_ARTIFACT_TASK_FRAME:XL6_ARTIFACT_TASK_NET);
    XL6_ArtifactRuntimePlan initial;XL6_ArtifactOwnershipPlan owned;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&initial)&&xl6ArtifactOwnershipPlanBuild(&initial,&owned));
    assert(!has_module(&owned.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!has_module(&owned.runtime,"XRT_MODULE_EXECUTOR"));
    assert(!xl6ArtifactRuntimeSetHas(&owned.runtime.components,XL6_ARTIFACT_TASK));
    assert(!xl6ArtifactRuntimeSetHas(&owned.runtime.components,XL6_ARTIFACT_TASK_POOL));
    bool frame=false;
    for(size_t i=0;i<owned.runtime.sourceCount;++i){
        const char* s=owned.runtime.sources[i];
        assert(strcmp(s,"src/runtime/xl6_task.c")&&strcmp(s,"src/runtime/xl6_task_pool.c"));
        frame|=!strcmp(s,"src/runtime/xl6_task_frame.c");
    }
    assert(frame);
    unsigned task=0,net=0,engine=0;
    for(size_t i=0;i<owned.domainCount;++i){
        task+=!strcmp(owned.domains[i],"xl6OwnershipDomainTaskV1Get");
        net+=!strcmp(owned.domains[i],"xl6OwnershipDomainTaskNetV1Get");
        engine+=!strcmp(owned.domains[i],"xl6OwnershipDomainNetEngineNativeV1Get");
    }
    assert(task==1);
    if(!strcmp(which,"net"))assert(net==1&&engine==1&&has_module(&owned.runtime,"XRT_MODULE_TASK_NET"));
    else assert(!net&&!engine&&!has_module(&owned.runtime,"XRT_MODULE_TASK_NET"));
    return owned;
}
int main(int argc,char** argv)
{
    if(argc==1){plan("frame");plan("net");puts("Task frame plan: shared lifecycle, exact sealed network family, no computation pool");return 0;}
    assert(argc==3);XL6_ArtifactOwnershipPlan p=plan(argv[2]);
    if(!strcmp(argv[1],"--sources"))for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);
    else if(!strcmp(argv[1],"--modules"))for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);
    else {assert(!strcmp(argv[1],"--domains"));for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);}
    return 0;
}
