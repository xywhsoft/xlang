#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    const char* name=!strcmp(choice,"core")?"xl6TcpStreamCreateV1":!strcmp(choice,"sync")?"xl6TcpStreamRecvV1":
        !strcmp(choice,"stats")?"xl6TcpStreamSnapshotV1":"xl6TcpAcceptStartV1";
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(xl6ArtifactRuntimeSymbolOwner(name));
    if(!strcmp(choice,"all")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SYNC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_STATS));
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan result=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<result.runtime.sourceCount;++i)puts(result.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<result.runtime.nativeModuleCount;++i)printf("#define %s\n",result.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<result.domainCount;++i)printf("%s(),\n",result.domains[i]);return 0;}return 2;
    }
    assert(argc==1);XL6_ArtifactOwnershipPlan core=plan("core"),future=plan("future"),sync=plan("sync"),stats=plan("stats");
    assert(!xl6ArtifactRuntimeSetHas(&core.runtime.components,XL6_ARTIFACT_TCP_FUTURE));
    assert(!xl6ArtifactRuntimeSetHas(&core.runtime.components,XL6_ARTIFACT_TCP_SYNC));
    assert(!xl6ArtifactRuntimeSetHas(&core.runtime.components,XL6_ARTIFACT_TCP_STATS));
    assert(!xl6ArtifactRuntimeSetHas(&future.runtime.components,XL6_ARTIFACT_TCP_SYNC));
    assert(!xl6ArtifactRuntimeSetHas(&sync.runtime.components,XL6_ARTIFACT_TCP_FUTURE));
    assert(!xl6ArtifactRuntimeSetHas(&stats.runtime.components,XL6_ARTIFACT_TCP_LANGUAGE));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpConnectedStartV1")==XL6_ARTIFACT_TCP_FUTURE);
    puts("TCP language plan: independent core/Future/sync/stats, exact class policies and service ownership");return 0;
}
