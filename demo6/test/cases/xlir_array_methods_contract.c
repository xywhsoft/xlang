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
        assert(!(header ? xl6IrCgenEmitHeader(&output, ir, "REJECTED_METHOD", &diagnostics) :
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
    int ok = 0; unsigned methods = 0, mutations = 0;
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "array_methods_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir, NULL));
    for (size_t i = 0; i < ir.intrinsicCallCount; ++i) {
        XL6_IrIntrinsicCall* intrinsic = &ir.intrinsicCalls[i];
        if (intrinsic->kind != XL6_IR_INTRINSIC_ARRAY_REMOVE_VALUE &&
            !(intrinsic->kind == XL6_IR_INTRINSIC_TYPED_COLLECTION_MEMBER &&
              intrinsic->collectionAbi == XL6_IR_COLLECTION_ARRAY)) continue;
        assert(intrinsic->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        XL6_IrIntrinsicCall saved = *intrinsic;
#define MUTATE(Field, Value) do { intrinsic->Field = (Value); rejected(&ir); \
    *intrinsic = saved; ++mutations; } while (0)
        MUTATE(errorAbi, XL6_IR_ERROR_ABI_NO_RAISE);
        MUTATE(receiverType, XL6_IR_TYPE_INVALID);
        MUTATE(resultType, XL6_IR_TYPE_INVALID);
        MUTATE(collectionAbi, XL6_IR_COLLECTION_NONE);
#undef MUTATE
    }
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
            (instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE &&
             !(instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
               instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY))) continue;
        assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE);
        XL6_IrInstruction saved = *instruction;
#define MUTATE(Field, Value) do { instruction->Field = (Value); rejected(&ir); \
    *instruction = saved; ++mutations; } while (0)
        MUTATE(type, XL6_IR_TYPE_INVALID);
        MUTATE(result, XL6_IR_VALUE_INVALID);
        MUTATE(operandCount, 0);
        MUTATE(firstOperand, SIZE_MAX);
        MUTATE(payloadType, saved.type);
        MUTATE(opcode, XL6_IR_OPCODE_ADD);
        if (saved.typedCollectionOp==XL6_IR_TYPED_COLLECTION_RESIZE) {
            MUTATE(collectionDefaultAbi, XL6_VALUE_DEFAULT_INVALID);
            MUTATE(collectionDefaultAbi, (XL6_ValueDefaultAbiV1)UINT32_MAX);
            MUTATE(collectionDefaultAbi, saved.collectionDefaultAbi==XL6_VALUE_DEFAULT_NULL ?
                XL6_VALUE_DEFAULT_INT : XL6_VALUE_DEFAULT_NULL);
        }
#undef MUTATE
        XL6_IrValueId* receiver = &ir.operands[saved.firstOperand];
        XL6_IrValueId operand = *receiver;
        *receiver = XL6_IR_VALUE_INVALID; rejected(&ir); *receiver = operand; ++mutations;
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction outcome = ir.instructions[i + 1];
        ir.instructions[i + 1].kind = XL6_IR_INSTR_CONST;
        rejected(&ir); ir.instructions[i + 1] = outcome; ++mutations;
        assert(xl6IrVerifyModule(&ir, NULL)); ++methods;
    }
    assert(methods);
    if (!xl6OutputOpenFile(&output, argv[2])) goto done;
    ok = xl6IrCgenEmitC(&output, &ir, &parse.diags); xl6OutputClose(&output);
    if (ok && argc == 4) {
        assert(xl6OutputOpenFile(&output, argv[3])); unsigned bindings = 0;
        for (size_t i = 0; i < ir.functionCount; ++i) {
            const XL6_IrFunction* function = &ir.functions[i];
            if (!function->debugName) continue;
            const char* suffix = !strcmp(function->debugName, "coreMethods") ? "CORE" :
                !strcmp(function->debugName, "textMethods") ? "TEXT" :
                !strcmp(function->debugName, "clearMethods") ? "CLEAR" : NULL;
            if (!suffix) continue;
            const XL6_IrType* result = xl6IrTypeGet(&ir.types, function->returnType);
            assert(result && result->errorResultCName);
            xl6OutputPrintf(&output, "#define XL6_METHOD_%s xlir_fn_%u\n#define XL6_METHOD_%s_RESULT %s\n",
                suffix, (unsigned)function->symbol, suffix, result->errorResultCName); ++bindings;
        }
        for (size_t i = 0; i < ir.types.count; ++i) {
            const XL6_IrType* type = &ir.types.items[i];
            if (type->collectionAbi != XL6_IR_COLLECTION_ARRAY || type->paramCount != 1 || !type->params) continue;
            const XL6_IrType* element = xl6IrTypeGet(&ir.types, type->params[0]);
            const char* suffix = element->cAbi == XL6_IR_C_ABI_I64 ? "I64" :
                element->cAbi == XL6_IR_C_ABI_MANAGED_STRING ? "STRING" : NULL;
            if (!suffix) continue;
            xl6OutputPrintf(&output, "#define XL6_METHOD_%s_OWNER 0x%016llXULL\n#define XL6_METHOD_%s_ELEMENT 0x%016llXULL\n",
                suffix, (unsigned long long)type->typeValueId, suffix, (unsigned long long)element->typeValueId); ++bindings;
        }
        for (size_t i = 0; i < ir.storageCount; ++i) {
            const XL6_IrStorage* storage = &ir.storages[i];
            const XL6_IrNode* declaration = xl6IrNodeGet(&ir, storage->declaration);
            if (declaration && declaration->text && !strcmp(declaration->text, "methodCleanup")) {
                xl6OutputPrintf(&output, "#define XL6_METHOD_CLEANUP s%u\n", (unsigned)storage->symbol); ++bindings;
            }
        }
        assert(bindings == 6 && !xl6OutputHasFailed(&output)); xl6OutputClose(&output);
    }
    printf("Array method contract: %u operations; %u ABI/outcome mutations refused before C/header output\n", methods, mutations);
done:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n", parse.diags.items[i].line,
        parse.diags.items[i].column, parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
