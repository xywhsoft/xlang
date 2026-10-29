#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(!strcmp(choice,"native")?XL6_ARTIFACT_NATIVE_CLASS_RESULT:
        !strcmp(choice,"base")?XL6_ARTIFACT_UDP_FUTURE:XL6_ARTIFACT_UDP_FUTURE_INTERRUPT);
    const XL6_ArtifactRuntimeId more[]={XL6_ARTIFACT_UDP_PACKET_INFO,XL6_ARTIFACT_UDP_STATS,XL6_ARTIFACT_UDP_ADDRESS,
        XL6_ARTIFACT_UDP_SEND,XL6_ARTIFACT_UDP_SYNC,XL6_ARTIFACT_NET_ENGINE_LANGUAGE,XL6_ARTIFACT_UDP_CONFIG};
    if(!strcmp(choice,"native"))assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_UDP_BATCH));
    if(!strcmp(choice,"all"))for(size_t i=0;i<sizeof(more)/sizeof(*more);++i)
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,more[i]));
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){XL6_ArtifactOwnershipPlan r=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<r.runtime.sourceCount;++i)puts(r.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<r.runtime.nativeModuleCount;++i)printf("#define %s\n",r.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<r.domainCount;++i)printf("%s(),\n",r.domains[i]);return 0;}return 2;
    }
    assert(argc==1);XL6_ArtifactOwnershipPlan n=plan("native"),b=plan("base"),f=plan("all");
    assert(!xl6ArtifactRuntimeSetHas(&n.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!xl6ArtifactRuntimeSetHas(&n.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(!xl6ArtifactRuntimeSetHas(&b.runtime.components,XL6_ARTIFACT_UDP_SYNC));
    assert(!xl6ArtifactRuntimeSetHas(&b.runtime.components,XL6_ARTIFACT_DEADLINE_SERVICE));
    assert(xl6ArtifactRuntimeSetHas(&f.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    for(size_t i=0;i<f.runtime.sourceCount;++i)assert(!strstr(f.runtime.sources[i],"tcp")&&!strstr(f.runtime.sources[i],"resolver")&&!strstr(f.runtime.sources[i],"task_pool"));
    puts("UDP async plan: native result recipe, fixed callback queue, explicit finite services, no TCP/Resolver/TaskPool");return 0;
}
