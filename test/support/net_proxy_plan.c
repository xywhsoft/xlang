#define main net_proxy_old_plan_main
#include "executor_catalog_plan.c"
#undef main
static const char* names[]={"proxy-core","proxy","handshake-core","handshake","buffer","text","address","all"};
static XL6_ArtifactOwnershipPlan build(unsigned which)
{
    XL6_ArtifactRuntimeSet roots={0};
#define ADD(Id) assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_##Id))
    ADD(NET_PROXY);
    if(which==1||which==7){ADD(NET_PROXY_VALUE);ADD(NET_PROXY_TEXT);}
    if(which>=2)ADD(NET_PROXY_HANDSHAKE);
    if(which==3||which==5||which==6||which==7)ADD(NET_PROXY_HANDSHAKE_VALUE);
    if(which==4||which==7)ADD(NET_PROXY_HANDSHAKE_BUFFER);
    if(which==5||which==7)ADD(NET_PROXY_HANDSHAKE_TEXT);
    if(which==6||which==7)ADD(NET_PROXY_HANDSHAKE_ADDRESS);
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
        assert(!has_module(&plan.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&plan.runtime,"XRT_MODULE_NET_SOCKET")&&
            !has_module(&plan.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&plan.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
        for(size_t a=0;a<plan.domainCount;++a)for(size_t b=0;b<a;++b)assert(strcmp(plan.domains[a],plan.domains[b]));
        if(i<2)assert(!has_module(&plan.runtime,"XRT_MODULE_NET_PROXY_HANDSHAKE"));
        if(i!=4&&i!=7)assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_BUFFER_CORE));
    }
    puts("Proxy plans: eight independent closures, no transport/Engine/Resolver/TLS, optional Buffer and value/text projections pruned");return 0;
}
