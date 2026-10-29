#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_TREE_KEY",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==3);bool bytes=!strcmp(argv[2],"bytes");assert(bytes || !strcmp(argv[2],"scalar"));
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned calls=0;
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        XL6_IrRuntimeResourceKeyOrderAbi expected=bytes ? XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_BYTES : XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_C_SCALAR;
        if(ins->runtimeResourceKeyOrderAbi!=expected) continue;
        ++calls;assert(ins->runtimeResourceKeyPayloadAbi==(bytes ? XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN : XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY));
        XL6_IrInstruction saved=*ins;
        ins->runtimeResourceKeyOrderAbi=XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_BOOL;refused(ir);*ins=saved;
        ins->runtimeResourceKeyOrderAbi=XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID;refused(ir);*ins=saved;
        ins->runtimeResourceKeyType=0;refused(ir);*ins=saved;
        ins->runtimeResourceKeyPayloadAbi=bytes ? XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY : XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY;refused(ir);*ins=saved;
        if(calls%16==0) {printf("tree-key %s verified calls=%u mutations=%u\n",argv[2],calls,mutations);fflush(stdout);}
    }
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    bool selected=false;
    for(size_t i=0;i<plan.sourceCount;++i)if(!strcmp(plan.sources[i],"src/runtime/xl6_tree_bytes_key.c"))selected=true;
    assert(selected==bytes && calls && xl6IrVerifyModule(ir,NULL));
    printf("tree-key %s calls=%u mutations=%u codec=%d\n",argv[2],calls,mutations,selected);
    xl6CompileUnitFree(&unit);return 0;
}
