#define main tcp_options_old_plan_main
#include "executor_catalog_plan.c"
#undef main
static const char* names[]={"stream","dial","listen","address","all"};
static XL6_ArtifactOwnershipPlan build(unsigned which)
{
    XL6_ArtifactRuntimeSet roots={0};
#define ADD(Id) assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_##Id))
    ADD(TCP_STREAM_OPTIONS);
    if(which==1||which==4)ADD(TCP_DIAL_OPTIONS);
    if(which>=2)ADD(TCP_LISTEN_OPTIONS);
    if(which==3||which==4)ADD(TCP_LISTEN_OPTIONS_ADDRESS);
#undef ADD
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan ownership;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&ownership));return ownership;
}
int main(int argc,char** argv)
{
    if(argc==3){unsigned n=5;for(unsigned i=0;i<5;++i)if(!strcmp(argv[2],names[i]))n=i;assert(n<5);
        XL6_ArtifactOwnershipPlan plan=build(n);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<plan.domainCount;++i)printf("%s(),\n",plan.domains[i]);return 0;}return 2;
    }
    assert(argc==1);
    for(unsigned i=0;i<5;++i){XL6_ArtifactOwnershipPlan plan=build(i);
        assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
        assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_BUFFER_CORE));
        assert(!has_module(&plan.runtime,"XRT_MODULE_TLS")&&!has_module(&plan.runtime,"XRT_MODULE_NET_UDP"));
        if(i==0||i==2||i==3)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_TCP_DIAL"));
        if(i<2)assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_TCP_LISTEN_OPTIONS));
        if(i!=3&&i!=4)assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_NET_ADDRESS));
        for(size_t a=0;a<plan.domainCount;++a)for(size_t b=0;b<a;++b)assert(strcmp(plan.domains[a],plan.domains[b]));
    }
    puts("TCP options plans: five independent closures, no execution source, optional Dial/listener/address projections pruned");return 0;
}
