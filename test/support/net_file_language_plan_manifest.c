#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_NET_FILE_BYTES);
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_FILE_WRITE));
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&plan));
    bool file=false,family=false,language=false,input=false;
    for(size_t i=0;i<plan.domainCount;++i){
        file|=!strcmp(plan.domains[i],"xl6OwnershipDomainNetFileNativeV1Get");
        family|=!strcmp(plan.domains[i],"xl6OwnershipDomainExecutionNetFileV1Get");
        language|=!strcmp(plan.domains[i],"xl6OwnershipDomainNetFileLanguageV1Get");
        input|=!strcmp(plan.domains[i],"xl6OwnershipDomainNetFileBytesV1Get");
    }
    assert(file&&family&&language&&input);
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_POOL));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_EXECUTOR));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_TCP));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_EXECUTION_UDP));
    if(argc==2&&!strcmp(argv[1],"--sources")){for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);return 0;}
    if(argc==2&&!strcmp(argv[1],"--modules")){for(size_t i=0;i<plan.runtime.nativeModuleCount;++i){assert(!strstr(plan.runtime.nativeModules[i],"_MODULE_ALL"));printf("#define %s\n",plan.runtime.nativeModules[i]);}return 0;}
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetFileReadV1")==XL6_ARTIFACT_NET_FILE_FUTURE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetFileWriteBytesV1")==XL6_ARTIFACT_NET_FILE_BYTES);
    puts("NetFile language plan: exact handle/input/native/family domains; no default pool/TCP/UDP");return 0;
}
