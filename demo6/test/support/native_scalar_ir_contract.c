#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t mutations;
static void reject(XL6_IrModule* module)
{
    XL6_DiagList diagnostics={0};assert(!xl6IrVerifyModule(module,&diagnostics) && diagnostics.count);
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;size_t length;assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"INVALID_NATIVE",&diagnostics) : xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&length);assert(!length);xrtFree(text);
    }
    xl6DiagFree(&diagnostics);++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        xl6DiagPrintText(&unit.parse.diags,argv[1]);return 1;
    }
    size_t conversions=0,casts=0;
    XL6_IrRuntimeTypeMap typeMap=unit.ir.runtimeTypeMap;
    assert(typeMap.abiVersion==3 && typeMap.typeTypeValueId && typeMap.typeTypeValueId!=typeMap.u64TypeValueId);
    unit.ir.runtimeTypeMap.typeTypeValueId=0;reject(&unit.ir);unit.ir.runtimeTypeMap=typeMap;
    unit.ir.runtimeTypeMap.typeTypeValueId=typeMap.u64TypeValueId;reject(&unit.ir);unit.ir.runtimeTypeMap=typeMap;
    unit.ir.runtimeTypeMap.abiVersion=2;reject(&unit.ir);unit.ir.runtimeTypeMap=typeMap;
    for(size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* item=&unit.ir.instructions[i];
        if(item->runtimeOp!=XL6_IR_RUNTIME_NATIVE_SCALAR_CONVERT_CHECKED && item->runtimeOp!=XL6_IR_RUNTIME_NATIVE_SCALAR_AS_CHECKED)continue;
        if(item->runtimeOp==XL6_IR_RUNTIME_NATIVE_SCALAR_CONVERT_CHECKED)++conversions;else ++casts;
        XL6_IrInstruction saved=*item;
        item->payloadType=0;reject(&unit.ir);*item=saved;
        item->runtimeIdentityType=0;reject(&unit.ir);*item=saved;
        item->operandCount=0;reject(&unit.ir);*item=saved;
        item->castAbi=XL6_IR_CAST_IDENTITY;reject(&unit.ir);*item=saved;
        item->scalarConversionAbi=XL6_IR_SCALAR_CONVERSION_FLOAT_TO_INTEGER;reject(&unit.ir);*item=saved;
        assert(i+1<unit.ir.instructionCount && unit.ir.instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction status=unit.ir.instructions[i+1];
        unit.ir.instructions[i+1].operandCount=0;reject(&unit.ir);unit.ir.instructions[i+1]=status;
    }
    for(size_t i=0;i<unit.ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* item=&unit.ir.intrinsicCalls[i];
        if(item->kind!=XL6_IR_INTRINSIC_CHECKED_NATIVE_SCALAR_CONVERT)continue;
        XL6_IrErrorAbi error=item->errorAbi;item->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;reject(&unit.ir);item->errorAbi=error;
    }
    assert(conversions+casts && mutations);
    printf("native scalar conversions=%zu casts=%zu mutations=%zu rejected before output\n",conversions,casts,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
