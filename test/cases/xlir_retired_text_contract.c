#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc>2);for(int i=2;i<argc;++i)assert(xl6IrRuntimeStringBridgeKind(argv[i])==XL6_IR_RUNTIME_BRIDGE_NONE);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    unsigned targets=0;
    for(size_t i=0;i<unit.ir.callTargetCount;++i) {
        const XL6_IrCallTarget* target=&unit.ir.callTargets[i];
        if(target->cName && !strncmp(target->cName,"xl6Text",7)) {
            assert(target->runtimeBridge==XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE);++targets;
        }
    }
    assert(targets>=26);XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    bool value=false,query=false;
    for(size_t i=0;i<plan.sourceCount;++i) {
        if(!strcmp(plan.sources[i],"src/runtime/xl6_text_value.c"))value=true;
        if(!strcmp(plan.sources[i],"src/runtime/xl6_text_query.c"))query=true;
    }
    assert(value && query);printf("resident text targets=%u retired=%d exact native plan\n",targets,argc-2);
    xl6CompileUnitFree(&unit);return 0;
}
