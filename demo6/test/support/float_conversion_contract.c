#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t rejected;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; XL6_Output out; size_t length;
    assert(!xl6IrVerifyModule(ir,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,ir,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,ir,"BAD_FLOAT",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags) && xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t calls=0,instructions=0;
    for(size_t i=0;i<unit.ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* row=&unit.ir.intrinsicCalls[i], saved=*row;
        if(row->kind!=XL6_IR_INTRINSIC_CHECKED_FLOAT_CONVERT ||
            xl6IrTypeGet(&unit.ir.types,row->resultType)->cAbi!=XL6_IR_C_ABI_F32)continue;
        ++calls;
        row->castAbi=XL6_IR_CAST_XVALUE_TO_F64_CHECKED; reject(&unit.ir); *row=saved;
        row->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE; reject(&unit.ir); *row=saved;
        row->resultAbi=XL6_IR_INTRINSIC_RESULT_OWNED; reject(&unit.ir); *row=saved;
        XL6_IrIntrinsicOperand* operand=&unit.ir.intrinsicOperands[row->firstOperand];
        XL6_IrIntrinsicOperandAbi previous=operand->abi;
        operand->abi=previous==XL6_IR_INTRINSIC_OPERAND_BORROW
            ? XL6_IR_INTRINSIC_OPERAND_VALUE : XL6_IR_INTRINSIC_OPERAND_BORROW;
        reject(&unit.ir); operand->abi=previous;
    }
    for(size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* row=&unit.ir.instructions[i], saved=*row;
        if(row->runtimeOp!=XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT &&
            row->runtimeOp!=XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT &&
            row->runtimeOp!=XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT)continue;
        ++instructions;
        assert(xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,row)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        assert(xl6IrRuntimeOperandOwnership(row->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        row->runtimeOp=row->runtimeOp==XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT
            ? XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT : XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT;
        reject(&unit.ir); *row=saved;
        row->castAbi=XL6_IR_CAST_XVALUE_TO_F64_CHECKED; reject(&unit.ir); *row=saved;
        row->operandCount=0; reject(&unit.ir); *row=saved;
        row->payloadType=row->type; reject(&unit.ir); *row=saved;
        row->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_64; reject(&unit.ir); *row=saved;
    }
    assert(calls && calls==instructions && rejected==calls*9);
    printf("float conversions=%zu mutations=%zu rejected before output\n",calls,rejected);
    xl6CompileUnitFree(&unit); return 0;
}
