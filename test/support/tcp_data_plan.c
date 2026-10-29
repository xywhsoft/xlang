#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(bool all)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TCP_RECEIVE_INTO_NET_BUFFER_FUTURE);
    if(all){const unsigned ids[]={XL6_ARTIFACT_TCP_FUTURE,XL6_ARTIFACT_TCP_SYNC,XL6_ARTIFACT_TCP_STATS,
        XL6_ARTIFACT_TCP_NET_BUFFER,XL6_ARTIFACT_TCP_VECTOR_TEXT,XL6_ARTIFACT_TCP_VECTOR_BYTES,
        XL6_ARTIFACT_TCP_VECTOR_BUFFER,XL6_ARTIFACT_NET_BUFFER_VALUE,XL6_ARTIFACT_TCP_FILE,XL6_ARTIFACT_TCP_RECEIVE_INTO_LINE_FUTURE,
        XL6_ARTIFACT_TCP_RECEIVE_INTO_LENGTH_FUTURE};
        for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);++i)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[i]));}
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan p=plan(true);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;
    }
    assert(argc==1);XL6_ArtifactOwnershipPlan plain=plan(false),all=plan(true);
    assert(!xl6ArtifactRuntimeSetHas(&plain.runtime.components,XL6_ARTIFACT_NET_FRAME_LINE));
    assert(!xl6ArtifactRuntimeSetHas(&plain.runtime.components,XL6_ARTIFACT_NET_FRAME_LENGTH));
    assert(!xl6ArtifactRuntimeSetHas(&plain.runtime.components,XL6_ARTIFACT_TCP_FILE));
    assert(!xl6ArtifactRuntimeSetHas(&all.runtime.components,XL6_ARTIFACT_NET_SOCKET_LANGUAGE));
    assert(!xl6ArtifactRuntimeSetHas(&all.runtime.components,XL6_ARTIFACT_EXECUTION_TCP_DIAL));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpStreamSendVectorTextV1")==XL6_ARTIFACT_TCP_VECTOR_TEXT);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpStreamRecvIntoLineAsyncV1")==XL6_ARTIFACT_TCP_RECEIVE_INTO_LINE_FUTURE);
    puts("TCP data plan: exact input families; no raw Socket, Dial, File or framer hitchhiking");return 0;
}
