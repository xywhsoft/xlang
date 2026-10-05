#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "REJECTED_READ", &diagnostics) :
            xl6IrCgenEmitC(&output, ir, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
}
int main(int argc, char** argv)
{
    assert(argc == 3 || argc == 4);
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok = 0; unsigned reads = 0, mutations = 0;
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "scalar_read_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            !xl6IrArrayScalarReadRuntimeOpIs(instruction->runtimeOp)) continue;
        assert(xl6IrArrayScalarReadRuntimeInstructionCompatible(&ir, instruction));
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrRuntimeArrayScalarReadErrorPhysicalAbi(instruction->runtimeOp) ==
            XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE);
        const XL6_IrType* receiver = xl6IrArrayScalarReadReceiverType(&ir, instruction);
        assert(receiver && receiver->params[0] == instruction->type);
        XL6_IrInstruction saved = *instruction;
#define MUTATE(Field, Value) do { instruction->Field = (Value); \
    assert(!xl6IrArrayScalarReadRuntimeInstructionCompatible(&ir, instruction)); \
    rejected(&ir); *instruction = saved; ++mutations; } while (0)
        MUTATE(type, XL6_IR_TYPE_INVALID);
        MUTATE(result, XL6_IR_VALUE_INVALID);
        MUTATE(firstOperand, SIZE_MAX);
        MUTATE(operandCount, 0);
        MUTATE(operandCount, 1);
        MUTATE(operandCount, 3);
        MUTATE(immediateText, (char*)"foreign");
        MUTATE(immediateData, (unsigned char*)"foreign");
        MUTATE(immediateSize, 1);
        MUTATE(payloadType, saved.type);
        MUTATE(calleeType, saved.type);
        MUTATE(runtimeIdentityType, saved.type);
        MUTATE(targetValue, saved.result);
        MUTATE(targetSymbol, UINT32_MAX);
        MUTATE(collectionAbi, XL6_IR_COLLECTION_ARRAY);
        MUTATE(typedCollectionOp, XL6_IR_TYPED_COLLECTION_CLEAR);
        MUTATE(dynamicCollectionOp, XL6_IR_DYNAMIC_COLLECTION_CLEAR);
        MUTATE(opcode, XL6_IR_OPCODE_ADD);
        MUTATE(runtimeOp, saved.runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL ?
            XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER : XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL);
#undef MUTATE
        XL6_IrValueId* operands = ir.operands + saved.firstOperand;
        XL6_IrValueId first = operands[0], second = operands[1];
        operands[0] = XL6_IR_VALUE_INVALID;
        rejected(&ir); operands[0] = first; ++mutations;
        operands[1] = XL6_IR_VALUE_INVALID;
        rejected(&ir); operands[1] = second; ++mutations;
        operands[0] = second; operands[1] = first;
        rejected(&ir); operands[0] = first; operands[1] = second; ++mutations;
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction outcome = ir.instructions[i + 1];
        ir.instructions[i + 1].kind = XL6_IR_INSTR_CONST;
        rejected(&ir); ir.instructions[i + 1] = outcome; ++mutations;
        assert(xl6IrVerifyModule(&ir, NULL)); ++reads;
    }
    assert(reads);
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags); xl6OutputClose(&output);
    if (ok && argc == 4) {
        assert(xl6OutputOpenFile(&output, argv[3])); unsigned bindings = 0;
        for (size_t i = 0; i < ir.functionCount; ++i) {
            const XL6_IrFunction* function = &ir.functions[i];
            if (!function->debugName) continue;
            const char* suffix = strcmp(function->debugName, "scalarI64") == 0 ? "I64" :
                strcmp(function->debugName, "scalarU64") == 0 ? "U64" :
                strcmp(function->debugName, "scalarBool") == 0 ? "BOOL" :
                strcmp(function->debugName, "scalarF64") == 0 ? "F64" :
                strcmp(function->debugName, "scalarSum") == 0 ? "SUM" : NULL;
            if (!suffix) continue;
            const XL6_IrType* result = xl6IrTypeGet(&ir.types, function->returnType);
            assert(result && result->errorResultCName);
            xl6OutputPrintf(&output, "#define XL6_READ_%s xlir_fn_%u\n#define XL6_READ_%s_RESULT %s\n",
                suffix, (unsigned)function->symbol, suffix, result->errorResultCName);
            ++bindings;
        }
        for (size_t i = 0; i < ir.types.count; ++i) {
            const XL6_IrType* type = &ir.types.items[i];
            if (type->kind != XL6_IR_TYPE_GENERIC_INSTANCE || type->collectionAbi != XL6_IR_COLLECTION_ARRAY ||
                type->paramCount != 1 || !type->params) continue;
            const XL6_IrType* element = xl6IrTypeGet(&ir.types, type->params[0]);
            const char* suffix = element->cAbi == XL6_IR_C_ABI_I64 ? "I64" :
                element->cAbi == XL6_IR_C_ABI_U64 ? "U64" :
                element->cAbi == XL6_IR_C_ABI_BOOL ? "BOOL" :
                element->cAbi == XL6_IR_C_ABI_F64 ? "F64" : NULL;
            if (!suffix) continue;
            xl6OutputPrintf(&output, "#define XL6_READ_%s_OWNER 0x%016llXULL\n#define XL6_READ_%s_ELEMENT 0x%016llXULL\n",
                suffix, (unsigned long long)type->typeValueId, suffix, (unsigned long long)element->typeValueId);
            ++bindings;
        }
        assert(bindings == 9 && !xl6OutputHasFailed(&output)); xl6OutputClose(&output);
    }
    printf("scalar read contract: %u operations; %u ABI/outcome mutations refused before C/header output\n", reads, mutations);
done:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n", parse.diags.items[i].line,
        parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
