#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void verifyClassGeneratorAdmission(void)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/sem_generator_class_param.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "class_generator_admission"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}
static void rejectMutableGeneratorParameter(const char* mode)
{
    XL6_ParseResult parse; char source[256]; size_t i; int exact = 0;
    snprintf(source, sizeof(source), "class Box\n int value\nend\nfunc words(%s Box value) as generator<int>\n yield 1\nend\n", mode);
    xl6ParseResultInit(&parse);
    assert(xl6ParseSource(source, "virtual_generator_negative.xl", &parse));
    assert(!xl6SemCheckModule(parse.root, &parse.diags, "virtual_generator_negative"));
    for (i = 0; i < parse.diags.count; ++i)
        if (strstr(parse.diags.items[i].message, "generator") && strstr(parse.diags.items[i].message, "byref") &&
            strstr(parse.diags.items[i].message, "out")) exact = 1;
    assert(exact); xl6ParseResultFree(&parse);
}
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_VIRTUAL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, p, j, mixed = 0, calls = 0, detached = 0, frames = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    rejectMutableGeneratorParameter("byref"); rejectMutableGeneratorParameter("out");
    verifyClassGeneratorAdmission();
    assert(xl6ParseFile("test/cases/xlir_virtual_call_outcome.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "virtual_outcome"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(ir.classDispatchSlotCount == 3);
    for (i = 0; i < ir.classDispatchSlotCount; ++i) {
        XL6_IrClassDispatchSlot* slot = &ir.classDispatchSlots[i];
        const XL6_IrCallTarget* base = xl6IrCallTargetGet(&ir, slot->declarationSymbol);
        XL6_IrErrorAbi saved = slot->errorAbi;
        assert(base && saved == XL6_IR_ERROR_ABI_MAY_RAISE);
        if (base->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE) ++mixed;
        slot->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); slot->errorAbi = saved;
        for (p = 0; p < slot->parameterCount; ++p) {
            XL6_IrParameterAbi abi = slot->parameterAbis[p];
            slot->parameterAbis[p] = XL6_IR_PARAMETER_ABI_INVALID; reject(&ir); slot->parameterAbis[p] = abi;
        }
    }
    /* The multi-result change slot now owns a result carrier and can raise;
     * the scalar text/accept slot still proves no-raise base lifting. */
    assert(mixed >= 1);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i];
        const XL6_IrClassDispatchSlot* slot;
        XL6_IrInstruction saved;
        if (call->kind != XL6_IR_INSTR_VIRTUAL_CALL) continue;
        slot = &ir.classDispatchSlots[call->dispatchSlot - 1]; ++calls;
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        saved = ir.instructions[i + 1]; ir.instructions[i + 1].kind = XL6_IR_INSTR_DISCARD;
        reject(&ir); ir.instructions[i + 1] = saved;
        for (p = 0; p < slot->parameterCount; ++p) {
            XL6_IrValueId value = ir.operands[call->firstOperand + p + 1];
            if (slot->parameterAbis[p] != XL6_IR_PARAMETER_ABI_OWNED) continue;
            assert(xl6IrInstructionOperandOwnership(&ir, call, p + 1) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
            for (j = 0; j < i; ++j) {
                XL6_IrInstruction* move = &ir.instructions[j]; XL6_DiagList diags = {0};
                if (move->kind != XL6_IR_INSTR_MOVE || move->operandCount != 1 || ir.operands[move->firstOperand] != value) continue;
                saved = *move; move->kind = XL6_IR_INSTR_DISCARD; move->targetSymbol = 0;
                assert(!xl6IrVerifyOwnerConsumption(&ir, &diags) && diags.count); xl6DiagFree(&diags);
                reject(&ir); *move = saved; ++detached; break;
            }
        }
    }
    assert(calls >= 10 && detached >= 3);
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        XL6_IrContinuationFrameTransferAbi abi;
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) continue;
        assert(target->parameterCount == 1 && target->generatorCaptureAbis);
        abi = target->generatorCaptureAbis[0];
        assert(abi == XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE);
        target->generatorCaptureAbis[0] = XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE;
        reject(&ir); target->generatorCaptureAbis[0] = abi; ++frames;
    }
    assert(frames == 2 && xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_VIRTUAL_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define XLIR_VIRTUAL_SLOT_COUNT %llu\n", (unsigned long long)ir.classDispatchSlotCount);
    xl6OutputClose(&out);
    printf("virtual outcome: %llu calls, %llu mixed-effect slots, %llu consuming transfers, %llu retained generator receivers, %llu rejected mutations\n",
        (unsigned long long)calls, (unsigned long long)mixed, (unsigned long long)detached,
        (unsigned long long)frames, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
