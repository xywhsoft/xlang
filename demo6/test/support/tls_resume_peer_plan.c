#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimeSet roots={0};
    const XL6_ArtifactRuntimeId ids[]={XL6_ARTIFACT_TLS_RESUME_PEER,XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION};
    for(size_t i=0;i<sizeof(ids)/sizeof(*ids);i++)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[i]));
    XL6_ArtifactRuntimePlan closed;XL6_ArtifactOwnershipPlan owned;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&closed)&&xl6ArtifactOwnershipPlanBuild(&closed,&owned));
    if(!strcmp(argv[1],"--sources"))for(size_t i=0;i<owned.runtime.sourceCount;i++)puts(owned.runtime.sources[i]);
    else{assert(!strcmp(argv[1],"--modules"));for(size_t i=0;i<owned.runtime.nativeModuleCount;i++)printf("#define %s\n",owned.runtime.nativeModules[i]);}
    return 0;
}
