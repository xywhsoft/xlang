/* Offline: corrupt the exact enum binding, checked edge and count contracts. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;

static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList diagnostics;
        size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "BAD_DOMAIN", &diagnostics)
                        : xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&diagnostics);
    }
    ++refusals;
}

static XL6_IrInstruction* definition(XL6_IrModule* ir, XL6_IrValueId value)
{
    for (size_t i = 0; i < ir->instructionCount; ++i)
        if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}

int main(int argc, char** argv)
{
    XL6_CompileUnit unit;
    assert(argc == 2);
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    XL6_IrModule* ir = &unit.ir;
    XL6_IrTypeId other = XL6_IR_TYPE_INVALID, integer = XL6_IR_TYPE_INVALID;
    for (size_t i = 0; i < ir->types.count; ++i) {
        const XL6_IrType* type = &ir->types.items[i];
        if (type->kind == XL6_IR_TYPE_NUMERIC_ENUM && !strcmp(type->name, "Other"))
            other = (XL6_IrTypeId)i + 1;
        if (type->kind == XL6_IR_TYPE_BUILTIN && type->cAbi == XL6_IR_C_ABI_I64 &&
            type->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64)
            integer = (XL6_IrTypeId)i + 1;
    }
    assert(other && integer);
    assert(xl6IrLowerModule(ir, &unit.parse.diags));
    assert(xl6IrVerifyModule(ir, NULL));
    size_t binds = 0, counts = 0;
    for (size_t i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir->instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_COUNT) {
            const XL6_IrType* type = xl6IrTypeGet(&ir->types, instruction->type);
            assert(type && type->kind == XL6_IR_TYPE_BUILTIN &&
                type->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64);
            XL6_IrTypeId saved = instruction->type;
            instruction->type = other;
            refused(ir);
            instruction->type = saved;
            ++counts;
            continue;
        }
        if (instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ENUM_BIND) continue;
        assert(xl6IrEnumBindAdmission(ir, instruction) == XL6_VALUE_ADMIT_U8);
        XL6_IrInstruction saved = *instruction;
#define BAD(field, value) do { instruction->field = (value); refused(ir); *instruction = saved; } while (0)
        BAD(kind, XL6_IR_INSTR_UNARY);
        BAD(operandCount, 0);
        BAD(operandCount, 2);
        BAD(firstOperand, ir->operandCount);
        BAD(integerTarget, XL6_IR_INTEGER_TARGET_INVALID);
        BAD(integerTarget, XL6_IR_INTEGER_TARGET_UNSIGNED_16);
        BAD(integerSourceAbi, XL6_IR_INTEGER_SOURCE_I64);
        BAD(runtimeIdentityType, integer);
        if (saved.runtimeIdentityType != other) BAD(runtimeIdentityType, other);
        BAD(type, integer);
        BAD(payloadType, other);
        BAD(calleeType, other);
        BAD(immediateText, (char*)"1");
        BAD(opcode, XL6_IR_OPCODE_BIT_OR);
        BAD(targetValue, saved.result);
        BAD(targetSymbol, 1);
        /* A legacy identity-only bind is not a substitute for a domain bind,
         * even after removing the field that is exclusive to the new op. */
        instruction->runtimeOp = XL6_IR_RUNTIME_XVALUE_TYPE_BIND;
        instruction->integerTarget = XL6_IR_INTEGER_TARGET_INVALID;
        refused(ir);
        *instruction = saved;
        XL6_IrInstruction* box = definition(ir, ir->operands[saved.firstOperand]);
        assert(box && box->runtimeOp == XL6_IR_RUNTIME_XVALUE_I64);
        XL6_IrInstruction boxSaved = *box;
        box->runtimeOp = XL6_IR_RUNTIME_XVALUE_U64;
        refused(ir);
        *box = boxSaved;
        box->operandCount = 0;
        refused(ir);
        *box = boxSaved;
        XL6_IrInstruction* scalar = definition(ir, ir->operands[box->firstOperand]);
        assert(scalar);
        XL6_IrTypeId scalarType = scalar->type;
        scalar->type = integer;
        refused(ir);
        scalar->type = scalarType;
        assert(i + 1 < ir->instructionCount);
        XL6_IrInstruction* edge = instruction + 1;
        assert(edge->kind == XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction edgeSaved = *edge;
        edge->kind = XL6_IR_INSTR_RELEASE;
        refused(ir);
        *edge = edgeSaved;
        XL6_IrValueId status = ir->operands[edge->firstOperand];
        ir->operands[edge->firstOperand] = box->result;
        refused(ir);
        ir->operands[edge->firstOperand] = status;
        assert(xl6IrVerifyModule(ir, NULL));
        ++binds;
#undef BAD
    }
    assert(binds >= 2 && counts == 1 && refusals >= 40);
    xl6CompileUnitFree(&unit);
    printf("XLIR: enum scalar domain: %zu binds, %zu canonical count, %zu refusals\n",
        binds, counts, refusals);
    return 0;
}
