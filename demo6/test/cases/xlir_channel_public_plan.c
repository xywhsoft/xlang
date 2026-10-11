#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==3); bool required=!strcmp(argv[2],"required");
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TYPED_CHANNEL)==required);
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CHANNEL));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TYPED_QUEUE_MPMC));
    unsigned native=0;
    for(size_t i=0;i<plan.sourceCount;++i) {
        assert(strcmp(plan.sources[i],"src/api/xl_channel_bridge.c"));
        if(!strcmp(plan.sources[i],"src/runtime/xl6_channel.c")) ++native;
    }
    assert(native==(required ? 1u : 0u));
    printf("Public source native Channel required=%d providers=%u sources=%zu\n",required,native,plan.sourceCount);
    xl6CompileUnitFree(&unit); return 0;
}
