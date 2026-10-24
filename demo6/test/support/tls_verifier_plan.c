#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimeSet roots={0};
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TlsVerificationPolicyHandlePolicyV1Get")==XL6_ARTIFACT_TLS_VERIFICATION_POLICY);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TlsVerifierHandlePolicyV1Get")==XL6_ARTIFACT_TLS_VERIFIER);
    const XL6_ArtifactRuntimeId ids[]={XL6_ARTIFACT_TLS_VERIFIER_CREATE,XL6_ARTIFACT_TLS_VERIFIER_VERIFY,
        XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_VERIFICATION_POLICY,XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_VERIFIER,
        XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION,XL6_ARTIFACT_X509_CERTIFICATE};
    for(size_t i=0;i<sizeof(ids)/sizeof(*ids);i++)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids[i]));
    XL6_ArtifactRuntimePlan closed;XL6_ArtifactOwnershipPlan owned;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&closed) && xl6ArtifactOwnershipPlanBuild(&closed,&owned));
    if(!strcmp(argv[1],"--sources"))for(size_t i=0;i<owned.runtime.sourceCount;i++)puts(owned.runtime.sources[i]);
    else{assert(!strcmp(argv[1],"--modules"));for(size_t i=0;i<owned.runtime.nativeModuleCount;i++)printf("#define %s\n",owned.runtime.nativeModules[i]);}
    return 0;
}
