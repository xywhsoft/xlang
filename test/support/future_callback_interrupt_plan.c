#define main sharedCallbackPlanMain
#include "future_callback_plan.c"
#undef main
int main(int argc,char** argv)
{
    unsigned id=xl6ArtifactRuntimeSymbolOwner("xl6FutureCallbackInterruptStartV1");
    assert(id==XL6_ARTIFACT_FUTURE_CALLBACK_INTERRUPT);
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(id);
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&plan));
    assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_FUTURE_CALLBACK_INTERRUPT));
    assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_DEADLINE_SERVICE));
    for(size_t i=0;i<plan.runtime.nativeModuleCount;++i){
        assert(strncmp(plan.runtime.nativeModules[i],"XRT_MODULE_NET",14));
        assert(strncmp(plan.runtime.nativeModules[i],"XRT_MODULE_TLS",14));
    }
    if(argc==2){
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<plan.domainCount;++i)printf("%s(),\n",plan.domains[i]);return 0;}
        return 2;
    }
    assert(argc==1);puts("interruptible callbacks: real frame/Watch/deadline closure, no network/TLS");return 0;
}
