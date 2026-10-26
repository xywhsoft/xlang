/* Declaration reachability must not imply implementation reachability. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2);bool check=!strncmp(argv[1],"--check-",8);
    assert(check||!strncmp(argv[1],"--peer-",7));
    XL6_ArtifactRuntimeSet roots={0};
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,
        check?XL6_ARTIFACT_X509_REVOCATION_CHECK:XL6_ARTIFACT_TLS_PEER_POLICY));
    XL6_ArtifactRuntimePlan plan;XL6_ArtifactOwnershipPlan owned;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan)&&xl6ArtifactOwnershipPlanBuild(&plan,&owned));
    const char* mode=argv[1]+(check?8:7);
    if(!strcmp(mode,"sources"))for(size_t i=0;i<owned.runtime.sourceCount;i++)puts(owned.runtime.sources[i]);
    else{assert(!strcmp(mode,"modules"));for(size_t i=0;i<owned.runtime.nativeModuleCount;i++)puts(owned.runtime.nativeModules[i]);}
    return 0;
}
