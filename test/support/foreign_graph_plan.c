#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_FOREIGN_RESOURCE);
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_OWNERSHIP_PLAN));
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NATIVE_HANDLE));
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    XL6_ArtifactRuntimePlan extended=plan;
    extended.entry=XL6_ARTIFACT_ENTRY_EXECUTABLE_V1;
    extended.entryOwnershipAbi=XL6_ARTIFACT_ENTRY_OWNERSHIP_DIRECT;
    extended.nativeModules[extended.nativeModuleCount++]="XRT_MODULE_MATH";
    XL6_ArtifactRuntimePlan prior=extended;
    const char* unknown[]={"unregistered_foreign_runtime"};
    assert(!xl6ArtifactRuntimeRequireSymbols(&extended,unknown,1)&&!memcmp(&extended,&prior,sizeof(prior)));
    const char* symbols[]={"xl6ForeignNetClientVisitV1"};
    assert(xl6ArtifactRuntimeRequireSymbols(&extended,symbols,1));
    assert(extended.entryOwnershipAbi==XL6_ARTIFACT_ENTRY_OWNERSHIP_GRAPH);
    bool math=false,domain=false;
    for(size_t i=0;i<extended.nativeModuleCount;++i)math|=!strcmp(extended.nativeModules[i],"XRT_MODULE_MATH");
    for(size_t i=0;i<extended.sourceCount;++i)domain|=!strcmp(extended.sources[i],"src/runtime/xl6_tls_client_context_domain.c");
    assert(math&&domain);
    if(argc==2&&!strcmp(argv[1],"--sources")){
        for(size_t i=0;i<plan.sourceCount;++i)puts(plan.sources[i]);
        return 0;
    }
    assert(argc==2&&!strcmp(argv[1],"--modules"));
    for(size_t i=0;i<plan.nativeModuleCount;++i)printf("#define %s\n",plan.nativeModules[i]);
    return 0;
}
