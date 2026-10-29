#define main udpBaselinePlanMain
#define plan udpBaselinePlan
#include "udp_language_plan.c"
#undef plan
#undef main
static XL6_ArtifactOwnershipPlan transferPlan(const char* kind)
{
    XL6_ArtifactRuntimeSet roots={{0}};
    if(!strcmp(kind,"all")){XL6_ArtifactOwnershipPlan old=udpBaselinePlan("all");roots=old.runtime.roots;}
    XL6_ArtifactRuntimeId id=!strcmp(kind,"message")?XL6_ARTIFACT_UDP_MESSAGE:
        !strcmp(kind,"text")?XL6_ARTIFACT_UDP_VECTOR_TEXT:
        !strcmp(kind,"refs")?XL6_ARTIFACT_UDP_REFS:
        !strcmp(kind,"mixed")?XL6_ARTIFACT_UDP_TRANSFER_OWNED:
        !strcmp(kind,"copy")?XL6_ARTIFACT_UDP_TRANSFER_COPY:XL6_ARTIFACT_UDP_MULTICAST;
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,id));
    if(!strcmp(kind,"all")){
        const XL6_ArtifactRuntimeId add[]={XL6_ARTIFACT_UDP_MESSAGE_BUFFER,XL6_ARTIFACT_UDP_MESSAGE_ARRAY,
            XL6_ARTIFACT_UDP_VECTOR_TEXT,XL6_ARTIFACT_UDP_VECTOR_BYTES,XL6_ARTIFACT_UDP_VECTOR_BUFFER,
            XL6_ARTIFACT_UDP_REFS,XL6_ARTIFACT_UDP_TRANSFER_COPY,XL6_ARTIFACT_UDP_TRANSFER_OWNED,
            XL6_ARTIFACT_UDP_MESSAGE_SEND,XL6_ARTIFACT_UDP_MULTICAST,
            /* Fixture producers, not dependencies of the transfer adapters. */
            XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION,XL6_ARTIFACT_STRING_TYPE};
        for(size_t i=0;i<sizeof(add)/sizeof(*add);++i)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,add[i]));
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    const char* kinds[]={"message","text","refs","mixed","copy","control","all"};
    if(argc==3){XL6_ArtifactOwnershipPlan p=transferPlan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;}
    assert(argc==1);
    for(size_t i=0;i<sizeof(kinds)/sizeof(*kinds);++i){XL6_ArtifactOwnershipPlan p=transferPlan(kinds[i]);
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
        if(!i){assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
            assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_BUFFER_CORE));
            assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_SEQUENCE));}
        if(i&&i<5)assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_UDP_MULTICAST));
    }
    puts("UDP transfer plan: independent immutable messages, vectors, refs, copy/mixed batches and Worker controls");return 0;
}
