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
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_ARRAY_IMPORT",NULL) :
            xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0);
        xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(void)
{
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit,"test/cases/stdlib_array_import_language.xl",1) ||
        !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        for (size_t i=0;i<unit.parse.diags.count;++i) fprintf(stderr,"%s\n",unit.parse.diags.items[i].message);
        xl6CompileUnitFree(&unit); return 2;
    }
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL));
    size_t operations=0,mutations=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            instruction->runtimeOp!=XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN) continue;
        assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        assert(xl6IrCallInstructionErrorAbi(ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        XL6_IrInstruction saved=*instruction,*test=&ir->instructions[i+1],outcome=*test;
        assert(xl6IrArrayImportRuntimeInstructionCompatible(ir,instruction));
        test->falseTarget=test->trueTarget; reject(ir); *test=outcome; ++mutations;
        test->kind=XL6_IR_INSTR_DISCARD; reject(ir); *test=outcome; ++mutations;
        XL6_IrValueId operand=ir->operands[test->firstOperand];
        ir->operands[test->firstOperand]=ir->operands[instruction->firstOperand];
        reject(ir); ir->operands[test->firstOperand]=operand; ++mutations;
        instruction->payloadType=instruction->type; reject(ir); *instruction=saved; ++mutations;
        instruction->collectionAbi=XL6_IR_COLLECTION_LIST; reject(ir); *instruction=saved; ++mutations;
        instruction->sequenceAdmissionIdentitySet=ir->nominalIdentitySetCount+1;
        reject(ir); *instruction=saved; ++mutations;
        const XL6_IrType* target=xl6IrTypeGet(&ir->types,instruction->type);
        XL6_IrType* physical=&ir->types.items[instruction->type-1];
        XL6_IrSequenceLayout layout=target->sequenceLayout;
        physical->sequenceLayout.elementType=instruction->type;
        reject(ir); physical->sequenceLayout=layout; ++mutations; ++operations;
    }
    size_t projections=0,futures=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        const XL6_IrType* type=xl6IrTypeGet(&ir->types,instruction->type);
        bool future=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_FUTURE_REF;
        bool scalar=type && instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY &&
            xl6IrNativeScalarProfileForCAbi(type->cAbi)!=XL6_NATIVE_SCALAR_NONE;
        bool nullable=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY &&
            xl6IrNativeScalarProfileForCAbi(xl6IrNullableScalarElementAbi(ir,instruction->type))!=
                XL6_NATIVE_SCALAR_NONE;
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL || !(future || scalar || nullable)) continue;
        assert(xl6IrCallInstructionErrorAbi(ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,instruction)==(future ?
            XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT));
        XL6_IrInstruction saved=*instruction,*test=&ir->instructions[i+1],outcome=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST &&
            ir->operands[test->firstOperand]==instruction->result);
        test->falseTarget=test->trueTarget; reject(ir); *test=outcome; ++mutations;
        test->kind=XL6_IR_INSTR_DISCARD; reject(ir); *test=outcome; ++mutations;
        if (future) instruction->runtimeIdentityType=XL6_IR_TYPE_INVALID;
        else instruction->payloadType=XL6_IR_TYPE_INVALID;
        reject(ir); *instruction=saved; ++mutations;
        if (future) ++futures; else ++projections;
    }
    assert(operations>=15 && projections>=5 && futures>=1 && xl6IrVerifyModule(ir,NULL));
    printf("Array import outcome: %zu checked imports, %zu C scalar projections, %zu exact Future boxes, %zu C/header-gated rejected mutations\n",operations,projections,futures,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
