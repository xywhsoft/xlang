#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_ArtifactOwnershipPlan plan(unsigned mode)
{
    static const size_t roots[]={XL6_ARTIFACT_TLS_STREAM_LANGUAGE,XL6_ARTIFACT_TLS_STREAM_ASYNC,
        XL6_ARTIFACT_TLS_STREAM_ASYNC_INTERRUPT,XL6_ARTIFACT_TLS_STREAM_BLOCKING,XL6_ARTIFACT_TLS_LISTENER_ASYNC,
        XL6_ARTIFACT_TLS_LISTENER_ASYNC_INTERRUPT,XL6_ARTIFACT_TLS_LISTENER_BLOCKING,XL6_ARTIFACT_TLS_STREAM_CONNECT};
    XL6_ArtifactRuntimeSet set=xl6ArtifactRuntimeSetSingleton(roots[mode]);
    if(mode==7){
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_STREAM_ASYNC_INTERRUPT));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_LISTENER_ASYNC_INTERRUPT));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_STREAM_BLOCKING));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_LISTENER_BLOCKING));
        assert(xl6ComponentSetAdd(set.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TLS_VERIFIER_CREATE));
    }
    XL6_ArtifactRuntimePlan native;XL6_ArtifactOwnershipPlan owned;
    assert(xl6ArtifactRuntimeClosePlan(&set,&native)&&xl6ArtifactOwnershipPlanBuild(&native,&owned));return owned;
}
int main(int argc,char** argv)
{
    if(argc==2){XL6_ArtifactOwnershipPlan p=plan(7);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<p.runtime.sourceCount;i++)puts(p.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<p.runtime.nativeModuleCount;i++)printf("#define %s\n",p.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<p.domainCount;i++)printf("%s(),\n",p.domains[i]);return 0;}return 2;}
    assert(argc==1);
    for(unsigned mode=0;mode<8;mode++){
        XL6_ArtifactOwnershipPlan p=plan(mode);const XL6_ArtifactRuntimeSet* set=&p.runtime.components;
        assert(!xl6ArtifactRuntimeSetHas(set,XL6_ARTIFACT_TLS_SESSION_CORE));
        assert(!xl6ArtifactRuntimeSetHas(set,XL6_ARTIFACT_EXECUTION_TCP));
        assert(xl6ArtifactRuntimeSetHas(set,XL6_ARTIFACT_EXECUTION_TLS_CLIENT)==(mode==7));
        assert(xl6ArtifactRuntimeSetHas(set,XL6_ARTIFACT_FUTURE_CALLBACK_INTERRUPT)==(mode==2||mode==5||mode==7));
        assert(xl6ArtifactRuntimeSetHas(set,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE)==(mode==1||mode==2||mode==4||mode==5||mode==7));
        assert(xl6ArtifactRuntimeSetHas(set,XL6_ARTIFACT_EXECUTION_TLS_LISTENER)==(mode>=4));
    }
    puts("TLS IO plan: eight exact closures; no client role for server/IO, no interrupt for plain async, no callback executor for sync");return 0;
}
