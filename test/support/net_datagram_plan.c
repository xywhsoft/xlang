#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static bool has_domain(const XL6_ArtifactOwnershipPlan* plan,const char* name)
{for(size_t i=0;i<plan->domainCount;++i)if(!strcmp(plan->domains[i],name))return true;return false;}
static XL6_ArtifactOwnershipPlan datagramPlan(const char* name)
{
    assert(!strcmp(name,"values")||!strcmp(name,"control")||!strcmp(name,"io")||!strcmp(name,"integration"));
    unsigned id=!strcmp(name,"values")?XL6_ARTIFACT_NET_DATAGRAM_VALUES:
        !strcmp(name,"control")?XL6_ARTIFACT_NET_SOCKET_DATAGRAM_CONTROL:XL6_ARTIFACT_NET_SOCKET_DATAGRAM_IO;
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(id);XL6_ArtifactRuntimePlan raw;XL6_ArtifactOwnershipPlan result;
    if(!strcmp(name,"integration"))assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_SOCKET_DATAGRAM_CONTROL));
    assert(xl6ArtifactRuntimeClosePlan(&roots,&raw)&&xl6ArtifactOwnershipPlanBuild(&raw,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan p=datagramPlan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t n=0;n<p.runtime.sourceCount;++n)puts(p.runtime.sources[n]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t n=0;n<p.runtime.nativeModuleCount;++n)printf("#define %s\n",p.runtime.nativeModules[n]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t n=0;n<p.domainCount;++n)printf("%s(),\n",p.domains[n]);return 0;}return 2;}
    assert(argc==1);
    const char* names[]={"values","control","io","integration"};
    for(unsigned i=0;i<4;++i){XL6_ArtifactOwnershipPlan p=datagramPlan(names[i]);
        assert(has_module(&p.runtime,"XRT_MODULE_NET_SOCKET")== (i!=0));
        assert(has_module(&p.runtime,"XRT_MODULE_BUFFER")== (i>=2));
        assert(!has_module(&p.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&p.runtime,"XRT_MODULE_NET_RESOLVER")&&
            !has_module(&p.runtime,"XRT_MODULE_NET_DNS")&&!has_module(&p.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE)&&
            !xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_TASK));
        if(i!=1){assert(has_domain(&p,"xl6OwnershipDomainNetDatagramInfoV1Get"));
            assert(has_domain(&p,"xl6OwnershipDomainNetDatagramControlV1Get"));}
    }
    assert(!strcmp(xl6ArtifactRuntimeComponentGet(XL6_ARTIFACT_NET_DATAGRAM_VALUES)->source,"src/runtime/xl6_net_datagram_values.c"));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketMetadataSetV1")==XL6_ARTIFACT_NET_SOCKET_DATAGRAM_CONTROL);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketReceiveFromIntoV1")==XL6_ARTIFACT_NET_SOCKET_DATAGRAM_IO);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketSendConnectedMessageBytesV1")==XL6_ARTIFACT_NET_SOCKET_DATAGRAM_IO);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetDatagramInfoCreateV1")==XL6_ARTIFACT_NET_DATAGRAM_VALUES);
    puts("Datagram plan: exact value/control/IO component identities and ownership, no unnecessary Socket/Buffer/async roots");return 0;
}
