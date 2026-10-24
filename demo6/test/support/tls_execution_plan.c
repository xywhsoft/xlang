#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(unsigned mode)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(mode==0?XL6_ARTIFACT_EXECUTION_TLS_STREAM:
        mode==1?XL6_ARTIFACT_EXECUTION_TLS_LISTENER:mode==2?XL6_ARTIFACT_EXECUTION_TLS_CLIENT:XL6_ARTIFACT_EXECUTION_TLS_SERVER);
    if(mode==4){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_EXECUTION_TLS_CLIENT));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_VERIFIER_CREATE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_STREAM_FUTURE_NATIVE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_LISTENER_FUTURE_NATIVE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_STREAM_LANGUAGE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_STREAM_CONNECT));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_LISTENER_LANGUAGE));
    }
    XL6_ArtifactRuntimePlan native;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&native)&&xl6ArtifactOwnershipPlanBuild(&native,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==2){XL6_ArtifactOwnershipPlan p=plan(4);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;i++)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;i++)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;i++)printf("%s(),\n",p.domains[i]);return 0;}return 2;
    }
    assert(argc==1);
    for(unsigned i=0;i<5;i++){
        XL6_ArtifactOwnershipPlan p=plan(i);
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_TLS_SESSION_CORE));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_TCP));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_DIAL_NATIVE));
        assert(!xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_DIAL_PROXY_NATIVE));
        assert(xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_TLS_CLIENT)==(i==2||i==4));
        assert(xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_TLS_SERVER)==(i==3||i==4));
        assert(xl6ArtifactRuntimeSetHas(&p.runtime.components,XL6_ARTIFACT_EXECUTION_TLS_LISTENER)==(i==1||i>=3));
    }
    puts("TLS execution: five exact role/resource closures; no old TCP, Dial, Proxy or sans-I/O wrapper dependency");return 0;
}
