#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_UDP_CONFIG);
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&plan));
    assert(xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_UDP_CONFIG));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_UDP));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_UDP_BASE_NATIVE));
    assert(!has_module(&plan.runtime,"XRT_MODULE_NET_UDP_FUTURE"));
    assert(!has_module(&plan.runtime,"XRT_MODULE_NET_TCP")&&!has_module(&plan.runtime,"XRT_MODULE_NET_RESOLVER"));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6UdpConfigCreateV1")==XL6_ARTIFACT_UDP_CONFIG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6UdpConfigReceiveSizeV1")==XL6_ARTIFACT_UDP_CONFIG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6UdpConfigHandlePolicyV1Get")==XL6_ARTIFACT_UDP_CONFIG);
    if(argc==2){
        if(!strcmp(argv[1],"sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"domains")){for(size_t i=0;i<plan.domainCount;++i)printf("%s(),\n",plan.domains[i]);return 0;}
        return 2;
    }
    assert(argc==1);puts("UDP config: exact immutable leaf domain, no execution source, TCP or UDP Future");return 0;
}
