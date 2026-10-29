#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan plan(const char* name)
{
    assert(!strcmp(name,"values")||!strcmp(name,"control")||!strcmp(name,"io")||!strcmp(name,"integration"));
    unsigned id=!strcmp(name,"values")?XL6_ARTIFACT_NET_DATAGRAM_ERROR_VALUES:
        !strcmp(name,"control")?XL6_ARTIFACT_NET_SOCKET_DATAGRAM_CONTROL:XL6_ARTIFACT_NET_SOCKET_DATAGRAM_ERROR_IO;
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(id);XL6_ArtifactRuntimePlan raw;XL6_ArtifactOwnershipPlan result;
    if(!strcmp(name,"integration"))assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_SOCKET_DATAGRAM_CONTROL));
    assert(xl6ArtifactRuntimeClosePlan(&roots,&raw)&&xl6ArtifactOwnershipPlanBuild(&raw,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan p=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;}
    assert(argc==1);const char* names[]={"values","control","io","integration"};
    for(unsigned i=0;i<4;++i){XL6_ArtifactOwnershipPlan p=plan(names[i]);
        assert(has_module(&p.runtime,"XRT_MODULE_NET_SOCKET")== (i!=0));
        assert(has_module(&p.runtime,"XRT_MODULE_BUFFER")== (i>=2));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_NET_DATAGRAM_VALUES)&&
            !xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_NET_SOCKET_DATAGRAM_IO));
        assert(!has_module(&p.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&p.runtime,"XRT_MODULE_NET_RESOLVER")&&
            !has_module(&p.runtime,"XRT_MODULE_NET_DNS")&&!has_module(&p.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    }
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketReceiveErrorIntoV1")==XL6_ARTIFACT_NET_SOCKET_DATAGRAM_ERROR_IO);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetDatagramErrorInfoCreateV1")==XL6_ARTIFACT_NET_DATAGRAM_ERROR_VALUES);
    puts("Datagram error plan: independent values/control/IO, no ordinary-datagram providers or async roots");return 0;
}
