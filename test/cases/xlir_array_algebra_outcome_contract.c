#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_ARRAY_ALGEBRA",NULL)
                        : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0);
        xrtFree(text); xl6OutputDiscard(&out);
    }
}

int main(void)
{
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit,"test/cases/stdlib_array_algebra_language.xl",1) ||
        !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        for (size_t i=0;i<unit.parse.diags.count;++i) fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
        xl6CompileUnitFree(&unit); return 2;
    }
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL));
    size_t operations=0,mutations=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ARRAY_CONCAT) continue;
        assert(xl6IrCallInstructionErrorAbi(ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i+1<ir->instructionCount); XL6_IrInstruction* test=&ir->instructions[i+1];
        XL6_IrInstruction saved=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1 &&
            ir->operands[test->firstOperand]==instruction->result);
        test->falseTarget=test->trueTarget; reject(ir); *test=saved; ++mutations;
        test->kind=XL6_IR_INSTR_DISCARD; reject(ir); *test=saved; ++mutations;
        XL6_IrValueId operand=ir->operands[test->firstOperand];
        ir->operands[test->firstOperand]=ir->operands[instruction->firstOperand];
        reject(ir); ir->operands[test->firstOperand]=operand; ++mutations;
        XL6_IrNode* origin=&ir->nodes[instruction->origin-1];
        XL6_IrErrorAbi effect=origin->operatorErrorAbi; assert(effect==XL6_IR_ERROR_ABI_MAY_RAISE);
        origin->operatorErrorAbi=XL6_IR_ERROR_ABI_NO_RAISE; reject(ir);
        origin->operatorErrorAbi=effect; ++mutations; ++operations;
    }
    assert(operations>=9 && xl6IrVerifyModule(ir,NULL));
    printf("Array algebra outcome: %zu checked operations, %zu C/header-gated rejected mutations\n",operations,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
