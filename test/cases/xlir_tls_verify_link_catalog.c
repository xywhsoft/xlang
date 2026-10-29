/* Exercise the compiler's real binary lookup, not a second implementation. */
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    static const XL6_ArtifactNativeSymbol expected[]={
#define XL6_ARTIFACT_NATIVE_SYMBOL(Symbol,Module) {Symbol,Module},
#include "src/compile/xl6_artifact_native_symbols.inc"
#undef XL6_ARTIFACT_NATIVE_SYMBOL
    };
    size_t unique=0;
    for(size_t i=0;i<sizeof(expected)/sizeof(*expected);){
        size_t end=i+1,count=0;while(end<sizeof(expected)/sizeof(*expected) && !strcmp(expected[i].symbol,expected[end].symbol))end++;
        const XL6_ArtifactNativeSymbol* found=xl6ArtifactNativeSymbolLookup(expected[i].symbol,&count);
        assert(found && count==end-i);
        for(size_t j=0;j<count;j++)assert(!strcmp(found[j].symbol,expected[i+j].symbol) &&
            ((!found[j].module && !expected[i+j].module) ||
             (found[j].module && expected[i+j].module && !strcmp(found[j].module,expected[i+j].module))));
        unique++;i=end;
    }
    static const XL6_ArtifactNativeSymbol repaired[]={
        {"xrtTlsPeerVerifyLimited","XRT_MODULE_TLS_VERIFY"},
        {"xrtTlsVerifierCreateLimited","XRT_MODULE_TLS_VERIFY"},
        {"xrtTlsVerifyLimitsInit","XRT_MODULE_TLS_VERIFY"},
        {"xrtX509PathBuildCheck","XRT_MODULE_X509_PATH_BUILD"},
        {"xrtX509PathBuildCheckLimit","XRT_MODULE_X509_PATH_BUILD"},
        {"xrtX509PathCheck","XRT_MODULE_X509_PATH"},
        {"xrtX509StoreAddData","XRT_MODULE_X509_STORE"},
        {"xrtX509StoreClone","XRT_MODULE_X509_STORE"},
        {"xrtX509StoreMerge","XRT_MODULE_X509_STORE"}
    };
    for(size_t i=0;i<sizeof(repaired)/sizeof(*repaired);i++){
        size_t count=0;const XL6_ArtifactNativeSymbol* found=xl6ArtifactNativeSymbolLookup(repaired[i].symbol,&count);
        assert(found && count==1 && !strcmp(found->module,repaired[i].module));
    }
    size_t count=73;
    assert(!xl6ArtifactNativeSymbolLookup(NULL,&count) && !count);
    assert(!xl6ArtifactNativeSymbolLookup("unknown-not-a-native-api",&count) && !count);
    assert(!xl6ArtifactNativeSymbolLookup("xrtTlsVerifyLimitsInit",NULL));
    /* New independent runtime families may add symbols to this shared catalog. */
    assert(unique>=3844);
    printf("native link catalog: %zu exact symbols; 9 TLS/X509 roots; unknowns rejected\n",unique);
    return 0;
}
