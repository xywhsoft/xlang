#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out; size_t bytes = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out, module, "BAD_FACTORY", NULL) : xl6IrCgenEmitC(&out, module, NULL)));
        char* text = xl6OutputTakeMemoryText(&out, &bytes);
        assert(text && !bytes); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1) && xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    unsigned factories = 0;
    for (size_t i = 0; i < module->instructionCount; ++i) {
        XL6_IrInstruction* call = &module->instructions[i];
        if (call->kind != XL6_IR_INSTR_NATIVE_STATUS_OUT) continue;
        ++factories;
        XL6_IrInstruction saved = *call;
#define BAD(F,V) do { call->F = (V); refused(module); *call = saved; } while (0)
        BAD(operandCount, 0); BAD(type, 0); BAD(targetSymbol, 0); BAD(opcode, XL6_IR_OPCODE_ADD);
        BAD(kind, XL6_IR_INSTR_CALL); BAD(elementIndex, 0);
#undef BAD
        XL6_IrInstruction *status = call + 1, *raw = call + 2, *code = call + 3,
            *equal = call + 4, *null = call + 5, *branch = call + 6;
        size_t element = raw->elementIndex; raw->elementIndex = 0; refused(module); raw->elementIndex = element;
        XL6_IrOpcode op = equal->opcode; equal->opcode = XL6_IR_OPCODE_NE; refused(module); equal->opcode = op;
        char* literal = code->immediateText; code->immediateText = "1LL"; refused(module); code->immediateText = literal;
        XL6_IrValueId operand = module->operands[null->firstOperand];
        module->operands[null->firstOperand] = status->result; refused(module); module->operands[null->firstOperand] = operand;
        XL6_IrBlockId edge = branch->trueTarget;
        branch->trueTarget = branch->falseTarget; refused(module); branch->trueTarget = edge;
        XL6_IrInstruction* present = &module->instructions[module->blocks[edge - 1].firstInstruction];
        edge = present->falseTarget;
        present->falseTarget = present->trueTarget; refused(module); present->falseTarget = edge;
        XL6_IrBlock* failure = &module->blocks[branch->falseTarget - 1];
        XL6_IrInstruction* disposeBranch = &module->instructions[failure->firstInstruction + 3];
        XL6_IrInstruction* dispose = &module->instructions[module->blocks[disposeBranch->falseTarget - 1].firstInstruction];
        XL6_SymbolId symbol = dispose->targetSymbol;
        dispose->targetSymbol = call->targetSymbol; refused(module); dispose->targetSymbol = symbol;
        edge = disposeBranch->falseTarget;
        disposeBranch->falseTarget = disposeBranch->trueTarget; refused(module); disposeBranch->falseTarget = edge;
        for (size_t f = 0; f < module->functionCount; ++f) {
            XL6_IrFunction* function = &module->functions[f];
            if (i < function->firstInstruction || i >= function->firstInstruction + function->instructionCount) continue;
            XL6_IrBlockId entry = function->entryBlock;
            function->entryBlock = present->falseTarget; refused(module); function->entryBlock = entry;
        }
        assert(xl6IrVerifyModule(module, NULL));
    }
    unsigned guarded=0;
    for(size_t i=0;i<module->instructionCount;++i) {
        XL6_IrInstruction* begin=&module->instructions[i];
        if(!begin->evaluationNativeResult)continue;
        ++guarded;XL6_IrInstruction saved=*begin;
        begin->evaluationNativeResult=0;refused(module);*begin=saved;
        begin->evaluationNativeDestroy=0;refused(module);*begin=saved;
        begin->evaluationNativeResult=begin->evaluationNativeResult+1;refused(module);*begin=saved;
        for(size_t j=i+1;j<module->instructionCount;++j) {
            XL6_IrInstruction* rollback=&module->instructions[j];
            if(rollback->kind!=XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK || rollback->evaluationScope!=i+1)continue;
            XL6_IrInstruction original=*rollback;
            rollback->targetSymbol=0;refused(module);*rollback=original;
            rollback->evaluationScope=0;refused(module);*rollback=original;
            rollback->kind=XL6_IR_INSTR_DROP;refused(module);*rollback=original;
        }
    }
    assert(guarded);
    unsigned addresses = 0;
    for (size_t i = 0; i < module->callTargetCount; ++i) {
        XL6_IrCallTarget* target = &module->callTargets[i];
        for (size_t p = 0; p < target->parameterCount; ++p) {
            if (target->parameterPhysicalAbis[p] != XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_C_AGGREGATE_BORROW) continue;
            ++addresses;
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT; refused(module);
            target->parameterPhysicalAbis[p] = XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_C_AGGREGATE_BORROW;
        }
    }
    assert(factories >= 3 && addresses >= 2 && xl6IrVerifyModule(module, NULL));
    printf("native factories=%u const addresses=%u mutations=%u zero-output=%u\n", factories, addresses, mutations, mutations * 2);
    xl6CompileUnitFree(&unit);
    return 0;
}
