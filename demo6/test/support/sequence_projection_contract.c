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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,ir,"BAD_SEQUENCE",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags) && xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t projections=0;
    for(size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* row=&unit.ir.instructions[i],saved=*row;
        if(row->runtimeOp!=XL6_IR_RUNTIME_NATIVE_CONTAINER_PROJECT_CHECKED)continue;
        ++projections;
        assert(xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,row)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
        assert(xl6IrRuntimeOperandOwnership(row->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        assert(xl6IrRuntimeOperandOwnership(row->runtimeOp,1)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        row->castAbi=XL6_IR_CAST_XVALUE_TO_F64_CHECKED; reject(&unit.ir); *row=saved;
        row->operandCount=1; reject(&unit.ir); *row=saved;
        row->payloadType=row->type; reject(&unit.ir); *row=saved;
        row->runtimeIdentityType=XL6_IR_TYPE_INVALID; reject(&unit.ir); *row=saved;
        row->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_64; reject(&unit.ir); *row=saved;
        row->collectionAbi=XL6_IR_COLLECTION_ARRAY; reject(&unit.ir); *row=saved;
        row->runtimeResourcePayloadAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN; reject(&unit.ir); *row=saved;
        row->targetValue=unit.ir.operands[row->firstOperand]; reject(&unit.ir); *row=saved;
        XL6_IrValueId old=unit.ir.operands[row->firstOperand+1];
        unit.ir.operands[row->firstOperand+1]=unit.ir.operands[row->firstOperand]; reject(&unit.ir);
        unit.ir.operands[row->firstOperand+1]=old;
        XL6_IrInstruction* test=&unit.ir.instructions[i+1], testSaved=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST);
        test->kind=XL6_IR_INSTR_INVALID; reject(&unit.ir); *test=testSaved;
        test->firstOperand=unit.ir.operandCount; reject(&unit.ir); *test=testSaved;
        XL6_IrNode* origin=&unit.ir.nodes[row->origin-1]; XL6_IrErrorAbi error=origin->castErrorAbi;
        origin->castErrorAbi=XL6_IR_ERROR_ABI_NO_RAISE; reject(&unit.ir); origin->castErrorAbi=error;
    }
    assert(projections && rejected==projections*12);
    printf("native container projections=%zu mutations=%zu rejected before output\n",projections,rejected);
    xl6CompileUnitFree(&unit); return 0;
}
