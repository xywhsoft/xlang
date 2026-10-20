#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static bool has_domain(const XL6_ArtifactOwnershipPlan* plan,const char* name)
{for(size_t i=0;i<plan->domainCount;++i)if(!strcmp(plan->domains[i],name))return true;return false;}
static XL6_ArtifactOwnershipPlan plan(const char* name)
{
    unsigned id=!strcmp(name,"owner")?XL6_ARTIFACT_NET_SOCKET_OWNER:
        !strcmp(name,"control")?XL6_ARTIFACT_NET_SOCKET_LANGUAGE:XL6_ARTIFACT_NET_SOCKET_IO;
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(id);XL6_ArtifactRuntimePlan raw;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&raw)&&xl6ArtifactOwnershipPlanBuild(&raw,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan p=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t n=0;n<p.runtime.sourceCount;++n)puts(p.runtime.sources[n]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t n=0;n<p.runtime.nativeModuleCount;++n)printf("#define %s\n",p.runtime.nativeModules[n]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t n=0;n<p.domainCount;++n)printf("%s(),\n",p.domains[n]);return 0;}return 2;}
    assert(argc==1);
    for(unsigned i=0;i<3;++i){XL6_ArtifactOwnershipPlan p=plan(i==0?"owner":i==1?"control":"io");
        assert(has_module(&p.runtime,"XRT_MODULE_NET_SOCKET"));
        assert(!has_module(&p.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&p.runtime,"XRT_MODULE_NET_RESOLVER")&&
            !has_module(&p.runtime,"XRT_MODULE_NET_DNS")&&!has_module(&p.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_TASK));
        assert(has_module(&p.runtime,"XRT_MODULE_BUFFER")== (i==2));
        assert(has_domain(&p,"xl6OwnershipDomainNetSocketOwnerV1Get"));
    }
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketSendBytesV1")==XL6_ARTIFACT_NET_SOCKET_IO);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketCreateV1")==XL6_ARTIFACT_NET_SOCKET_LANGUAGE);
    puts("Socket plan: exact physical owner/handle domains, control/IO separated, no Engine/Resolver/default async");return 0;
}
