/* Every added C API must resolve to the minimal TaskNet native module. */
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    const char* names[]={"xrtTaskNetAfterDataPolicyV1","xrtTaskNetCancelPolicyV1Get","xrtTaskNetDataPolicyV1","xrtTaskNetJobDataPolicyV1Get","xrtTaskNetOwnershipAdapterV1","xrtTaskNetOwnershipQuiescenceV1Get","xrtTaskNetPostPolicyV1Get","xrtTaskNetTimerPolicyV1Get","xrtTaskNetUntilDataPolicyV1"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i) {
        size_t count=0;const XL6_ArtifactNativeSymbol* entries=xl6ArtifactNativeSymbolLookup(names[i],&count);
        assert(entries&&count==1&&!strcmp(entries[0].symbol,names[i])&&
            !strcmp(entries[0].module,"XRT_MODULE_TASK_NET"));
    }
    puts("TaskNet native artifact map: 9 exact APIs; no implicit algorithm-family expansion");
    return 0;
}

