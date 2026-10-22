/* Exact resident Ed25519 capability closure. Offline fixture only. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimeSet roots={0};
    const XL6_ArtifactRuntimeId selected[]={
        XL6_ARTIFACT_CRYPTO_ED25519_GENERATE,
        XL6_ARTIFACT_CRYPTO_ED25519_SIGN_BUFFER,
        XL6_ARTIFACT_CRYPTO_ED25519_SEED_SIGN_BUFFER,
        XL6_ARTIFACT_CRYPTO_ED25519_PUBLIC,
        XL6_ARTIFACT_CRYPTO_ED25519_VERIFY_BUFFER
    };
    for(size_t i=0;i<sizeof(selected)/sizeof(*selected);++i)
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,selected[i]));
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&plan));
    if(!strcmp(argv[1],"--sources")) {
        for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);
    } else {
        assert(!strcmp(argv[1],"--modules"));
        for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);
    }
    return 0;
}
