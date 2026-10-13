#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static void reject(XL6_IrModule* module)
{
    XL6_DiagList diags={0}; XL6_Output out; size_t length;
    assert(!xl6IrVerifyModule(module,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,module,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,module,"BAD_STACK",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text); xl6DiagFree(&diags);
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t count=0,mutations=0;
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&unit.ir.instructions[i];
        if (instruction->runtimeOp!=XL6_IR_RUNTIME_STACK_ERASE) continue;
        ++count; XL6_IrInstruction saved=*instruction;
        assert(xl6IrStackErasureInstructionCompatible(&unit.ir,instruction));
        assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        instruction->runtimeIdentityType=XL6_IR_TYPE_INVALID; reject(&unit.ir); ++mutations; *instruction=saved;
        instruction->payloadType=XL6_IR_TYPE_INVALID; reject(&unit.ir); ++mutations; *instruction=saved;
        instruction->operandCount=0; reject(&unit.ir); ++mutations; *instruction=saved;
        instruction->type=instruction->runtimeIdentityType; reject(&unit.ir); ++mutations; *instruction=saved;
        instruction->kind=XL6_IR_INSTR_VALUE; instruction->runtimeOp=XL6_IR_RUNTIME_NONE;
        instruction->payloadType=instruction->runtimeIdentityType=XL6_IR_TYPE_INVALID;
        reject(&unit.ir); ++mutations; *instruction=saved;
        XL6_IrType* stack=(XL6_IrType*)xl6IrTypeGet(&unit.ir.types,instruction->runtimeIdentityType);
        XL6_IrSequenceRoleAbi role=stack->sequenceLayout.role;
        stack->sequenceLayout.role=XL6_IR_SEQUENCE_ROLE_ARRAY; reject(&unit.ir); ++mutations; stack->sequenceLayout.role=role;
    }
    assert(count>=6 && mutations==count*6);
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    printf("Stack erasure boundaries=%zu mutations=%zu rejected before output\n",count,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
