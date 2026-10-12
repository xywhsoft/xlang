#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv)
{
    assert(argc==3); unsigned mode=0; assert(sscanf(argv[1],"%u",&mode)==1 && mode<4);
    const XL6_ArtifactRuntimeId ids[]={XL6_ARTIFACT_SLOT_MAP_ITERATOR_COUNT,XL6_ARTIFACT_SLOT_MAP_ITERATOR_KEYS,
        XL6_ARTIFACT_SLOT_MAP_ITERATOR_VALUES,XL6_ARTIFACT_SLOT_MAP_ITERATOR_PAIRS};
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(ids[mode]);
    roots.words[XL6_ARTIFACT_ARRAY_PRIMITIVE_ADMISSION/64]|=UINT64_C(1)<<(XL6_ARTIFACT_ARRAY_PRIMITIVE_ADMISSION%64);
    XL6_ArtifactRuntimePlan plan={0}; assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_ITERATOR_KEY)==((mode&1)!=0));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_PAIRS)==(mode==3));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEYS)==(mode==1));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_VALUE)==((mode&1)!=0));
    FILE* file=fopen(argv[2],"wb"); assert(file); fputs("#define XRT_MODULE_MEMORY_DEBUG\n",file);
    for(size_t i=0;i<plan.nativeModuleCount;++i) fprintf(file,"#define %s\n",plan.nativeModules[i]);
    fputs("#define XRUNTIME_IMPLEMENTATION\n#include \"lib/xruntime/xruntime.h\"\n",file); assert(!fclose(file));
    for(size_t i=0;i<plan.sourceCount;++i) printf("%s\n",plan.sources[i]);
    return 0;
}
