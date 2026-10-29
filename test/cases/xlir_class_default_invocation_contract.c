#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned mutations;
static void rejected(const XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output output; XL6_DiagList diags; size_t size = SIZE_MAX;
        xl6DiagInit(&diags); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "REJECTED_DEFAULT", &diags) :
            xl6IrCgenEmitC(&output, module, &diags)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diags);
    }
    ++mutations;
}
int main(int argc, char** argv)
{
    bool emitOnly = argc == 6;
    assert(argc == 5 || (emitOnly && !strcmp(argv[5], "--emit-only")));
    XL6_ParseResult parse; XL6_IrModule module; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&module); unsigned invocations = 0, unreachableInvocations = 0;
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "root_factory") ||
        !xl6IrBuildModuleFromAst(&module, parse.root, &parse.diags) || !xl6IrLowerModule(&module, &parse.diags)) {
        for (size_t i = 0; i < parse.diags.count; ++i)
            fprintf(stderr, "%d:%d %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 1;
    }
    assert(xl6IrVerifyModule(&module, NULL));
    for (size_t i = 0; i < module.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &module.instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL || instruction->runtimeOp != XL6_IR_RUNTIME_CLASS_DEFAULT_INVOKE) continue;
        uint32 factory = xl6IrClassDefaultInvocationFunction(&module, instruction); assert(factory);
        const XL6_IrFunction* caller = NULL;
        for (size_t f = 0; f < module.functionCount; ++f) {
            const XL6_IrFunction* candidate = &module.functions[f];
            if (i >= candidate->firstInstruction && i - candidate->firstInstruction < candidate->instructionCount) {
                assert(!caller); caller = candidate;
            }
        }
        assert(caller);
        /* Dead source bodies remain verifiable IR, but must not retain a
         * factory simply because they contain a valid invocation. */
        if (caller->reachability != XL6_IR_REACHABILITY_UNREACHABLE)
            assert(module.functions[factory - 1].reachability != XL6_IR_REACHABILITY_UNREACHABLE);
        else ++unreachableInvocations;
        assert(xl6IrCallInstructionErrorAbi(&module, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&module, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
        assert(i + 1 < module.instructionCount && module.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        XL6_IrInstruction saved = *instruction;
        for (size_t n = 0; n < module.instructionCount; ++n)
            assert(module.instructions[n].kind != XL6_IR_INSTR_RUNTIME_CALL ||
                module.instructions[n].runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_NEW ||
                module.instructions[n].origin != instruction->origin);
        if (!emitOnly) {
#define MUTATE(Field, Value) do { instruction->Field = (Value); rejected(&module); *instruction = saved; } while (0)
        MUTATE(type, XL6_IR_TYPE_INVALID);
        MUTATE(payloadType, XL6_IR_TYPE_INVALID);
        MUTATE(result, XL6_IR_VALUE_INVALID);
        MUTATE(operandCount, 1);
        MUTATE(targetSymbol, module.functions[factory - 1].symbol);
        MUTATE(targetValue, saved.result);
        MUTATE(calleeType, saved.type);
        MUTATE(runtimeIdentityType, saved.type);
        MUTATE(immediateText, "constructor");
        MUTATE(opcode, XL6_IR_OPCODE_ADD);
        MUTATE(collectionAbi, XL6_IR_COLLECTION_ARRAY);
        MUTATE(sequenceAdmissionIdentitySet, UINT32_MAX);
#undef MUTATE
        XL6_IrInstruction outcome = module.instructions[i + 1];
        module.instructions[i + 1].kind = XL6_IR_INSTR_DISCARD; rejected(&module);
        module.instructions[i + 1] = outcome;
        module.instructions[i + 1].falseTarget = outcome.trueTarget; rejected(&module);
        module.instructions[i + 1] = outcome;
        module.instructions[i + 1].falseTarget = XL6_IR_BLOCK_INVALID; rejected(&module);
        module.instructions[i + 1] = outcome;
        XL6_IrFunction function = module.functions[factory - 1];
        module.functions[factory - 1].errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; rejected(&module);
        module.functions[factory - 1] = function;
        module.functions[factory - 1].returnType = XL6_IR_TYPE_INVALID; rejected(&module);
        module.functions[factory - 1] = function;
        }
        ++invocations;
    }
    assert(invocations && mutations == (emitOnly ? 0 : 17 * invocations) && xl6IrVerifyModule(&module, NULL));
    assert(xl6OutputOpenFile(&output, argv[2]) && xl6IrCgenEmitC(&output, &module, &parse.diags)); xl6OutputClose(&output);
    assert(xl6OutputOpenFile(&output, argv[3]) && xl6IrCgenEmitHeader(&output, &module, "ROOT_DEFAULT", &parse.diags)); xl6OutputClose(&output);
    assert(xl6OutputOpenFile(&output, argv[4])); xl6IrDumpModuleJson(&output, &module); xl6OutputClose(&output);
    printf("Class default: %u checked invocations, %u rejected C/header mutations\n", invocations, mutations);
    printf("Class default: %u unreachable caller invocations\n", unreachableInvocations);
    xl6IrModuleFree(&module); xl6ParseResultFree(&parse); return 0;
}
