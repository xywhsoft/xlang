#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan dialPlan(const char* choice)
{
    bool sync=!strcmp(choice,"sync");
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(sync?XL6_ARTIFACT_TCP_DIAL_SYNC:XL6_ARTIFACT_TCP_DIAL_FUTURE);
    if(!strcmp(choice,"all")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_DIAL_SYNC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SYNC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_STATS));
    }
    XL6_ArtifactRuntimePlan runtime;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&runtime)&&xl6ArtifactOwnershipPlanBuild(&runtime,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan p=dialPlan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;++i)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;++i)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;++i)printf("%s(),\n",p.domains[i]);return 0;}return 2;}
    assert(argc==1);XL6_ArtifactOwnershipPlan sync=dialPlan("sync"),async=dialPlan("async");
    assert(!xl6ArtifactRuntimeSetHas(&sync.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(!xl6ArtifactRuntimeSetHas(&sync.runtime.components,XL6_ARTIFACT_TCP_DIAL_FUTURE));
    assert(!xl6ArtifactRuntimeSetHas(&async.runtime.components,XL6_ARTIFACT_TCP_DIAL_SYNC));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpDialStartV1")==XL6_ARTIFACT_TCP_DIAL_FUTURE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TcpDialAddressesV1")==XL6_ARTIFACT_TCP_DIAL_SYNC);
    puts("Dial language plan: exact class contracts, shared projection, sync prunes language queue");return 0;
}
