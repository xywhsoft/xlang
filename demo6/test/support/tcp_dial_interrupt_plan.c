#define main dialBasicPlanMain
#include "tcp_dial_language_plan.c"
#undef main
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TCP_DIAL_INTERRUPT_FUTURE);
    if(argc==3&&!strcmp(argv[2],"all")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_DIAL_OPTIONS));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_DIAL_SYNC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SYNC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_STATS));}
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&plan));
    if(argc==3){
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<plan.domainCount;++i)printf("%s(),\n",plan.domains[i]);return 0;}return 2;}
    assert(argc==1&&dialBasicPlanMain(1,argv)==0);
    XL6_ArtifactOwnershipPlan basic=dialPlan("async");
    assert(!xl6ArtifactRuntimeSetHas(&basic.runtime.components,XL6_ARTIFACT_DEADLINE_SERVICE));
    assert(!xl6ArtifactRuntimeSetHas(&basic.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_INTERRUPT));
    assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_DEADLINE_SERVICE));
    assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_FUTURE_CALLBACK_INTERRUPT));
    puts("Dial interruption plan: real timer/parent owned closure; basic Dial prunes interruption");return 0;
}
