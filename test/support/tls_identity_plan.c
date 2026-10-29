#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimeSet roots={0};
    assert(xl6ArtifactRuntimeSymbolOwner("xl6TlsIdentityHandlePolicyV1Get")==XL6_ARTIFACT_TLS_IDENTITY);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainTlsIdentityV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_IDENTITY);
    const XL6_ArtifactRuntimeId selected[]={
        #define XL6_TEXT_EXPORT(Symbol,Provider) XL6_ARTIFACT_##Provider,
        #include "src/runtime/xl6_tls_identity_symbols.inc"
        #undef XL6_TEXT_EXPORT
        XL6_ARTIFACT_OWNERSHIP_DOMAIN_TLS_IDENTITY,XL6_ARTIFACT_CRYPTO_P256,
        XL6_ARTIFACT_CRYPTO_P384,XL6_ARTIFACT_CRYPTO_ED25519,XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION
    };
    for(size_t i=0;i<sizeof(selected)/sizeof(*selected);++i)assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,selected[i]));
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base) && xl6ArtifactOwnershipPlanBuild(&base,&plan));
    if(!strcmp(argv[1],"--sources"))for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);
    else{assert(!strcmp(argv[1],"--modules"));for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);}
    return 0;
}
