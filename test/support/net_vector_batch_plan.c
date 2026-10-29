/* Selected API closures are proved separately from extra test-setup roots. */
#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static const char* names[]={"progress","packet","text","bytes","buffer","receive","batch","all"};
static const unsigned ids[]={XL6_ARTIFACT_NET_BATCH_PROGRESS,XL6_ARTIFACT_NET_DATAGRAM_PACKET,
    XL6_ARTIFACT_NET_VECTOR_TEXT,XL6_ARTIFACT_NET_VECTOR_BYTES,XL6_ARTIFACT_NET_VECTOR_BUFFER,
    XL6_ARTIFACT_NET_VECTOR_RECEIVE,XL6_ARTIFACT_NET_SOCKET_BATCH};
static unsigned selected(const char* name)
{for(unsigned i=0;i<8;++i)if(!strcmp(name,names[i]))return i;assert(0);return 0;}
static XL6_ArtifactOwnershipPlan plan(unsigned which,bool setup)
{
    XL6_ArtifactRuntimeSet roots={0};
    if(which==7){for(unsigned i=0;i<7;++i)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[i]));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_DATAGRAM_PACKET_INFO));}
    else assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[which]));
    if(setup&&which>=2){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_SOCKET_IO));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_SOCKET_DATAGRAM_IO));
        if(which==2||which==7)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_STRING_TYPE));
        if(which>=4)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION));
    }
    XL6_ArtifactRuntimePlan raw;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&raw)&&xl6ArtifactOwnershipPlanBuild(&raw,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){unsigned which=selected(argv[2]);bool api=!strncmp(argv[1],"--api-",6);
        XL6_ArtifactOwnershipPlan p=plan(which,!api);const char* action=argv[1]+(api?6:2);
        if(!strcmp(action,"sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(action,"modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(action,"domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;
    }
    assert(argc==1);
    for(unsigned i=0;i<8;++i){XL6_ArtifactOwnershipPlan p=plan(i,false);
        assert(has_module(&p.runtime,"XRT_MODULE_NET_SOCKET")== (i>=2));
        assert(has_module(&p.runtime,"XRT_MODULE_BUFFER")== (i==1||i>=4));
        assert(!has_module(&p.runtime,"XRT_MODULE_NET_ENGINE")&&!has_module(&p.runtime,"XRT_MODULE_NET_DNS")&&
            !has_module(&p.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&p.runtime,"XRT_MODULE_TLS"));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE)&&
            !xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_TASK));
        if(i<7){
            assert(xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_NET_VECTOR_TEXT)==(i==2));
            assert(xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_NET_VECTOR_BYTES)==(i==3));
            assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_NET_DATAGRAM_PACKET_INFO));
        }
    }
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketSendVectorTextV1")==XL6_ARTIFACT_NET_VECTOR_TEXT);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketSendVectorBytesV1")==XL6_ARTIFACT_NET_VECTOR_BYTES);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketSendVectorBufferV1")==XL6_ARTIFACT_NET_VECTOR_BUFFER);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketReceiveFromVectorIntoV1")==XL6_ARTIFACT_NET_VECTOR_RECEIVE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetSocketSendBatchV1")==XL6_ARTIFACT_NET_SOCKET_BATCH);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetDatagramPacketInfoV1")==XL6_ARTIFACT_NET_DATAGRAM_PACKET_INFO);
    puts("Vector/batch plan: eight independent API closures; test setup roots separately identified");return 0;
}
