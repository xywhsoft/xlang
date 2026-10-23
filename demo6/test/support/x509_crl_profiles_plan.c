#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimeSet roots={0};
    const XL6_ArtifactRuntimeId selected[]={
        #define XL6_TEXT_EXPORT(Symbol,Provider) XL6_ARTIFACT_##Provider,
        #include "src/runtime/xl6_x509_crl_profiles_symbols.inc"
        #undef XL6_TEXT_EXPORT
        XL6_ARTIFACT_OWNERSHIP_DOMAIN_X509_CRL_ENTRY,XL6_ARTIFACT_OWNERSHIP_DOMAIN_X509_CRL_ENTRIES,
        XL6_ARTIFACT_OWNERSHIP_DOMAIN_X509_ISSUING_DISTRIBUTION_POINT
    };
    bool seen[XL6_ARTIFACT_RUNTIME_COUNT]={0};
    for(size_t i=0;i<sizeof(selected)/sizeof(*selected);++i)if(!seen[selected[i]]) {
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,selected[i]));seen[selected[i]]=true;
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan plan;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base) && xl6ArtifactOwnershipPlanBuild(&base,&plan));
    if(!strcmp(argv[1],"--sources"))for(size_t i=0;i<plan.runtime.sourceCount;++i)puts(plan.runtime.sources[i]);
    else {
        assert(!strcmp(argv[1],"--modules"));
        for(size_t i=0;i<plan.runtime.nativeModuleCount;++i)printf("#define %s\n",plan.runtime.nativeModules[i]);
    }
    return 0;
}
