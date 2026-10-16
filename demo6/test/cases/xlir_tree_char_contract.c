#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned mutations;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_CHAR_TREE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    bool plan=argc==4 && !strcmp(argv[1],"--plan");
    assert(argc==2 || plan);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[plan?2:1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned calls=0;
    if(plan) {
        XL6_ArtifactRuntimePlan result;
        assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&result,&unit.parse.diags));
        bool expected=!strcmp(argv[3],"1"),hasChar=false,hasTree=false;
        assert(expected || !strcmp(argv[3],"0"));
        for(size_t i=0;i<result.sourceCount;++i) {
            if(!strcmp(result.sources[i],"src/runtime/xl6_char_type.c")) hasChar=true;
            if(!strcmp(result.sources[i],"src/runtime/xl6_tree.c")) hasTree=true;
        }
        assert(hasTree && hasChar==expected);
        assert(xl6ComponentSetHas(result.components.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_CHAR_TYPE)==expected);
        printf("char-tree plan-char=%d sources=%zu\n",hasChar,result.sourceCount);
        xl6CompileUnitFree(&unit);return 0;
    }
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(ins->runtimeResourceKeyOrderAbi!=XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_CHAR) continue;
        ++calls;assert(ins->runtimeResourceKeyPayloadAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY);
        XL6_IrInstruction saved=*ins;
        ins->runtimeResourceKeyOrderAbi=XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_UNSIGNED_U64;refused(ir);*ins=saved;
        ins->runtimeResourceKeyOrderAbi=XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID;refused(ir);*ins=saved;
        ins->runtimeResourceKeyOrderAbi=(XL6_IrRuntimeResourceKeyOrderAbi)UINT32_MAX;refused(ir);*ins=saved;
        ins->runtimeResourceKeyPayloadAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN;refused(ir);*ins=saved;
    }
    assert(calls && xl6IrVerifyModule(ir,NULL));
    printf("char-tree calls=%u mutations=%u\n",calls,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
