#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv)
{
    assert(argc==2); XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SLOT_MAP_KEY_CAPTURE);
    XL6_ArtifactRuntimePlan plan={0}; assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CAPTURE_V2));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_VALUE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_LEAF_VALUE));
    FILE* file=fopen(argv[1],"wb"); assert(file); fputs("#define XRT_MODULE_MEMORY_DEBUG\n",file);
    for(size_t i=0;i<plan.nativeModuleCount;++i) fprintf(file,"#define %s\n",plan.nativeModules[i]);
    fputs("#define XRUNTIME_IMPLEMENTATION\n#include \"lib/xruntime/xruntime.h\"\n",file); assert(!fclose(file));
    for(size_t i=0;i<plan.sourceCount;++i) printf("%s\n",plan.sources[i]);
    return 0;
}
