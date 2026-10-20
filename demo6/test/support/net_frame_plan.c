#define main net_frame_old_plan_main
#include "executor_catalog_plan.c"
#undef main
static const char* names[]={"buffer-core","buffer","line-core","line","length-core","length","result","all"};
static XL6_ArtifactOwnershipPlan build(unsigned which)
{
    XL6_ArtifactRuntimeSet roots={0};
#define ADD(Id) assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_##Id))
    if(which==0||which==1||which==7)ADD(NET_BUFFER_CORE);
    if(which==1||which==7){ADD(NET_BUFFER_VALUE);ADD(NET_BUFFER_BUFFER);}
    if(which==2||which==3||which==7)ADD(NET_FRAME_LINE);
    if(which==3||which==7){ADD(NET_FRAME_LINE_VALUE);ADD(NET_FRAME_LINE_BUFFER);}
    if(which==4||which==5||which==7)ADD(NET_FRAME_LENGTH);
    if(which==5||which==7){ADD(NET_FRAME_LENGTH_VALUE);ADD(NET_FRAME_LENGTH_BUFFER);}
    if(which>=2){ADD(NET_FRAME_RESULT);}
    if(which==3||which==5||which==7){ADD(NET_FRAME_VALUE);ADD(NET_FRAME_BUFFER);ADD(NET_FRAME_TEXT);}
#undef ADD
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan ownership;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&ownership));return ownership;
}
int main(int argc,char** argv)
{
    if(argc==3){unsigned n=8;for(unsigned i=0;i<8;++i)if(!strcmp(argv[2],names[i]))n=i;assert(n<8);
        XL6_ArtifactOwnershipPlan plan=build(n);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<plan.domainCount;++i)printf("%s(),\n",plan.domains[i]);return 0;}return 2;
    }
    assert(argc==1);
    for(unsigned i=0;i<8;++i){XL6_ArtifactOwnershipPlan plan=build(i);
        for(size_t a=0;a<plan.domainCount;++a)for(size_t b=0;b<a;++b)assert(strcmp(plan.domains[a],plan.domains[b]));
        assert(!has_module(&plan.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&plan.runtime,"XRT_MODULE_NET_SOCKET")&&
            !has_module(&plan.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&plan.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
        if(i==0||i==2||i==4||i==6)assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_BUFFER_CORE));
        if(i<2)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_FRAME"));
        if(i==2||i==3)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_FRAME_LENGTH"));
        if(i==4||i==5)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_FRAME_LINE"));
    }
    puts("Frame plans: eight independent closures, no transport/Engine/Resolver/TLS, optional Buffer and parser families pruned");return 0;
}
