#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TYPED_CHANNEL);
    const unsigned extra[]={XL6_ARTIFACT_ARRAY_PRIMITIVE_ADMISSION,XL6_ARTIFACT_ARRAY_REFERENCE_ADMISSION,
        XL6_ARTIFACT_STRING_TYPE,XL6_ARTIFACT_OWNERSHIP_PLAN,XL6_ARTIFACT_ARRAY_FUTURE_ADMISSION};
    for (size_t i=0;i<sizeof(extra)/sizeof(*extra);++i)
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,extra[i]));
    XL6_ArtifactRuntimePlan plan; assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ChannelReceiveV1")==XL6_ARTIFACT_TYPED_CHANNEL);
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CHANNEL));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TYPED_QUEUE_MPMC));
    printf("{\"Sources\":[");
    for (size_t i=0;i<plan.sourceCount;++i) printf("%s\"%s\"",i ? "," : "",plan.sources[i]);
    printf("],\"NativeModules\":[");
    for (size_t i=0;i<plan.nativeModuleCount;++i) printf("%s\"%s\"",i ? "," : "",plan.nativeModules[i]);
    puts("],\"NativeOnly\":true,\"PublicLanguageCutover\":false}"); return 0;
}
