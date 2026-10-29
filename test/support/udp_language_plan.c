#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(
        !strcmp(choice,"values")?XL6_ARTIFACT_UDP_VALUES:XL6_ARTIFACT_UDP_LANGUAGE);
    if(!strcmp(choice,"all")){
        const XL6_ArtifactRuntimeId more[]={XL6_ARTIFACT_UDP_ADDRESS,XL6_ARTIFACT_UDP_STATS,
            XL6_ARTIFACT_UDP_PACKET_INFO,XL6_ARTIFACT_UDP_SEND,XL6_ARTIFACT_UDP_SEND_BUFFER,
            XL6_ARTIFACT_UDP_SEND_REF,XL6_ARTIFACT_UDP_POLL,XL6_ARTIFACT_UDP_SYNC,
            XL6_ARTIFACT_NET_ENGINE_LANGUAGE,XL6_ARTIFACT_NET_DATAGRAM_VALUES,
            XL6_ARTIFACT_UDP_CONFIG};
        for(size_t i=0;i<sizeof(more)/sizeof(*more);++i)
            assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,more[i]));
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
    assert(argc==1);XL6_ArtifactOwnershipPlan values=plan("values"),core=plan("core"),all=plan("all");
    assert(!xl6ArtifactRuntimeSetHas(&values.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!xl6ArtifactRuntimeSetHas(&values.runtime.components,XL6_ARTIFACT_BUFFER_CORE));
    assert(!xl6ArtifactRuntimeSetHas(&core.runtime.components,XL6_ARTIFACT_UDP_SYNC));
    assert(!xl6ArtifactRuntimeSetHas(&all.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    for(size_t i=0;i<all.runtime.sourceCount;++i)
        assert(!strstr(all.runtime.sources[i],"tcp")&&!strstr(all.runtime.sources[i],"resolver")&&!strstr(all.runtime.sources[i],"task_pool"));
    puts("UDP language plan: exact independent values, socket, poll, sync and native domains");return 0;
}
