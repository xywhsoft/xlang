#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(unsigned mode)
{
    static const size_t keys[]={XL6_ARTIFACT_TLS_DIAL_OPTIONS,XL6_ARTIFACT_EXECUTION_TLS_DIAL,
        XL6_ARTIFACT_EXECUTION_TLS_PROXY_DIAL,XL6_ARTIFACT_TLS_LISTENER_ASYNC,XL6_ARTIFACT_TLS_WORKER_ASYNC};
    XL6_ArtifactRuntimeSet set={{0}};
    for(size_t i=0;i<sizeof(keys)/sizeof(*keys);i++)if(mode==5||mode==i)assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,keys[i]));
    if(mode==5){
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_STREAM_CONNECT));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_VERIFIER_CREATE));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_STREAM_BLOCKING));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_WORKER_BLOCKING));
    }
    XL6_ArtifactRuntimePlan native;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&set,&native)&&xl6ArtifactOwnershipPlanBuild(&native,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==2){XL6_ArtifactOwnershipPlan p=plan(5);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;i++)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;i++)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;i++)printf("%s(),\n",p.domains[i]);return 0;}return 2;}
    assert(argc==1);
    for(unsigned i=0;i<6;i++){
        XL6_ArtifactOwnershipPlan p=plan(i);const XL6_ArtifactRuntimeSet* s=&p.runtime.components;
        assert(!xl6ArtifactRuntimeSetHas(s,XL6_ARTIFACT_TLS_SESSION_CORE));
        if(i!=5)assert(!xl6ArtifactRuntimeSetHas(s,XL6_ARTIFACT_EXECUTION_TLS_CLIENT));
        if(i==0)for(size_t n=0;n<p.runtime.nativeModuleCount;n++)assert(!strstr(p.runtime.nativeModules[n],"XRT_MODULE_TLS_"));
        assert(xl6ArtifactRuntimeSetHas(s,XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_DIAL_PROXY_NATIVE)==(i==2||i==5));
        if(i==1||i==2)assert(!xl6ArtifactRuntimeSetHas(s,XL6_ARTIFACT_TLS_LISTENER_LANGUAGE));
    }
    puts("TLS dial plan: six exact closures; options without TLS algorithms, direct without Proxy, Dial without Listener");return 0;
}
