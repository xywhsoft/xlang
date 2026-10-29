#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_PROTOCOL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, p, j, mixed = 0, calls = 0, owned = 0, mutable = 0, async = 0;
    XL6_SymbolId pending = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_protocol_call_outcome.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "protocol_outcome"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(ir.protocolDispatchSlotCount == 11);
    for (i = 0; i < ir.protocolDispatchSlotCount; ++i) {
        XL6_IrProtocolDispatchSlot* slot = &ir.protocolDispatchSlots[i];
        XL6_IrErrorAbi error = slot->errorAbi;
        XL6_IrCallErrorPhysicalAbi physical = slot->errorPhysicalAbi;
        assert(error == XL6_IR_ERROR_ABI_MAY_RAISE && physical == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
        slot->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); slot->errorAbi = error;
        slot->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE; reject(&ir); slot->errorPhysicalAbi = physical;
        for (j = 0; j < ir.protocolDispatchImplCount; ++j) {
            const XL6_IrProtocolDispatchImpl* impl = &ir.protocolDispatchImpls[j];
            const XL6_IrCallTarget* target = xl6IrCallTargetGet(&ir, impl->implementationSymbol);
            if (impl->slot != i) continue;
            assert(target);
            if (target->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE) ++mixed;
            if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) ++async;
        }
        for (p = 0; p < slot->parameterCount; ++p) {
            XL6_IrParameterAbi abi = slot->parameterAbis[p];
            slot->parameterAbis[p] = XL6_IR_PARAMETER_ABI_INVALID; reject(&ir); slot->parameterAbis[p] = abi;
        }
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i]; XL6_IrInstruction saved;
        const XL6_IrProtocolDispatchSlot* slot;
        if (call->kind != XL6_IR_INSTR_PROTOCOL_CALL) continue;
        slot = &ir.protocolDispatchSlots[call->dispatchSlot - 1]; ++calls;
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        saved = ir.instructions[i + 1]; ir.instructions[i + 1].kind = XL6_IR_INSTR_DISCARD;
        reject(&ir); ir.instructions[i + 1] = saved;
        saved = *call; call->dispatchSlot = 0; reject(&ir); *call = saved;
        call->result = XL6_IR_VALUE_INVALID; reject(&ir); *call = saved;
        for (p = 0; p < slot->parameterCount; ++p) {
            if (slot->parameterAbis[p] == XL6_IR_PARAMETER_ABI_OWNED) {
                assert(xl6IrInstructionOperandOwnership(&ir, call, p + 1) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
                ++owned;
            } else if (slot->parameterAbis[p] == XL6_IR_PARAMETER_ABI_INOUT ||
                       slot->parameterAbis[p] == XL6_IR_PARAMETER_ABI_OUT) ++mutable;
        }
    }
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    /* Managed parameter binding/cleanup can raise independently of an empty
     * method body. Explicit scalar-only witnesses keep no-raise lifting
     * coverage stable without misclassifying those ownership effects. */
    assert(calls >= 36 && mixed >= 4 && owned >= 5 && mutable == 0 && async == 1 && pending != 0);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_PROTOCOL_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define PROTOCOL_PENDING s%u\n", (unsigned int)pending);
    xl6OutputClose(&out);
    printf("protocol outcome: %llu calls, %llu no-raise witnesses in joined slots, %llu consuming arguments, %llu legacy mutable arguments, %llu async witnesses, %llu rejected mutations\n",
        (unsigned long long)calls, (unsigned long long)mixed, (unsigned long long)owned,
        (unsigned long long)mutable, (unsigned long long)async, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
