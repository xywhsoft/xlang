#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan build(int selected)
{
    static const size_t roots[]={XL6_ARTIFACT_TLS_STREAM_CONNECT,XL6_ARTIFACT_TLS_LISTENER_ASYNC,XL6_ARTIFACT_TLS_STREAM_BLOCKING,XL6_ARTIFACT_TLS_VECTOR,XL6_ARTIFACT_TLS_VECTOR_SYNC,XL6_ARTIFACT_TLS_RECV_OWNED,XL6_ARTIFACT_TLS_RECV_OWNED_FUTURE,XL6_ARTIFACT_TLS_WORKER,XL6_ARTIFACT_TLS_WORKER_ASYNC,XL6_ARTIFACT_TLS_WORKER_BLOCKING,XL6_ARTIFACT_NET_FRAME_REF,XL6_ARTIFACT_TLS_DATA_RECV_OWNED_BASIC,XL6_ARTIFACT_TLS_DATA_RECV_OWNED_INTERRUPT,XL6_ARTIFACT_TLS_DATA_RECV_OWNED_BLOCKING,XL6_ARTIFACT_TLS_DATA_VECTOR_TEXT_BASIC,XL6_ARTIFACT_TLS_DATA_VECTOR_TEXT_INTERRUPT,XL6_ARTIFACT_TLS_DATA_VECTOR_TEXT_BLOCKING,XL6_ARTIFACT_TLS_DATA_VECTOR_BYTES_BASIC,XL6_ARTIFACT_TLS_DATA_VECTOR_BYTES_INTERRUPT,XL6_ARTIFACT_TLS_DATA_VECTOR_BYTES_BLOCKING,XL6_ARTIFACT_TLS_DATA_VECTOR_BUFFER_BASIC,XL6_ARTIFACT_TLS_DATA_VECTOR_BUFFER_INTERRUPT,XL6_ARTIFACT_TLS_DATA_VECTOR_BUFFER_BLOCKING};
    XL6_ArtifactRuntimeSet set={{0}};
    if(selected<0){
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_VERIFIER_CREATE));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_FRAME_LINE));
    }
    for(size_t i=0;i<sizeof(roots)/sizeof(*roots);i++)if(selected<0||(size_t)selected==i)
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,roots[i]));
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&set,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==2){XL6_ArtifactOwnershipPlan p=build(-1);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;i++)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;i++)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;i++)printf("%s(),\n",p.domains[i]);return 0;}return 2;}
    assert(argc==1);
    for(int i=0;i<23;i++){
        XL6_ArtifactOwnershipPlan p=build(i);
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_TLS_SESSION_CORE));
        if(i>=3)assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_TLS_STREAM_CONNECT));
    }
    XL6_ArtifactOwnershipPlan frame=build(10);
    assert(!xl6ArtifactRuntimeSetHas(&frame.runtime.components,XL6_ARTIFACT_TLS_STREAM_IO));
    assert(!xl6ArtifactRuntimeSetHas(&frame.runtime.components,XL6_ARTIFACT_NET_FRAME_LINE));
    assert(!xl6ArtifactRuntimeSetHas(&frame.runtime.components,XL6_ARTIFACT_NET_FRAME_LENGTH));
    puts("TLS data plan: 23 exact closures; independent data families and framer reference append");return 0;
}
