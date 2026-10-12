#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE);
    (void)xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TASK);
    (void)xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_FUTURE_CALLBACK);
    (void)xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NATIVE_FUTURE_PAYLOAD);
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_VALUE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_NATIVE));
    FILE* out=fopen(argv[1],"wb");assert(out);
    for(size_t i=0;i<plan.nativeModuleCount;++i)fprintf(out,"#define %s\n",plan.nativeModules[i]);
    fputs("#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_MEMORY_STATS\n#define XRUNTIME_IMPLEMENTATION\n#include \"lib/xruntime/xruntime.h\"\n",out);
    assert(!fclose(out));for(size_t i=0;i<plan.sourceCount;++i)puts(plan.sources[i]);return 0;
}
