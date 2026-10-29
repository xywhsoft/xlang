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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_ACTIVATION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void negative(const char* source, const char* expected)
{
    XL6_ParseResult parse; size_t i; int found = 0;
    xl6ParseResultInit(&parse); assert(xl6ParseSource(source, "activation_negative.xl", &parse));
    assert(!xl6SemCheckModule(parse.root, &parse.diags, "activation_negative"));
    for (i = 0; i < parse.diags.count; ++i) if (strstr(parse.diags.items[i].message, expected)) found = 1;
    assert(found); xl6ParseResultFree(&parse);
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, calls = 0, generators = 0, retained = 0, slotIndex = SIZE_MAX;
    negative("class A\n static virtual async func f() as int\n return 1\n end\nend\n", "static class member");
    negative("class A\n private virtual async func f() as int\n return 1\n end\nend\n", "private class member");
    negative("class A\n override async func f() as int\n return 1\n end\nend\n", "exact base member");
    negative("class A\n virtual async func f() as int\n return 1\n end\nend\nclass B extends A\n async func f() as int\n return 2\n end\nend\n", "explicit override");
    negative("class A\n final virtual async func f() as int\n return 1\n end\nend\nclass B extends A\n override async func f() as int\n return 2\n end\nend\n", "final class member");
    negative("class A\n virtual async func f() as int\n return 1\n end\nend\nclass B extends A\n override func f() as int\n return 2\n end\nend\n", "exact base member");
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_virtual_activation.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    if (!xl6SemCheckModule(parse.root, &parse.diags, "virtual_activation") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    assert(ir.classDispatchSlotCount == 3 && ir.protocolDispatchSlotCount == 3);
    for (i = 0; i < ir.classDispatchSlotCount; ++i) {
        XL6_IrClassDispatchSlot* slot = &ir.classDispatchSlots[i];
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(&ir, slot->declarationSymbol);
        XL6_IrTypeId type = slot->returnType;
        assert(target && type == target->returnType && slot->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(slot->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
        slot->returnType = target->workerReturnType; reject(&ir); slot->returnType = type;
        slot->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); slot->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        slot->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT; reject(&ir);
        slot->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER;
        if (strcmp(slot->name, "numbers") == 0) slotIndex = i;
    }
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) {
            XL6_IrErrorAbi saved = target->errorAbi;
            assert(target->futureOutcomeAbi == XL6_IR_FUTURE_OUTCOME_INVALID);
            target->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND; reject(&ir);
            target->futureOutcomeAbi = XL6_IR_FUTURE_OUTCOME_INVALID;
            assert(saved == XL6_IR_ERROR_ABI_MAY_RAISE && target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
            target->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); target->errorAbi = saved;
            ++generators;
            if (target->parameterCount && target->parameterAbis[0] == XL6_IR_PARAMETER_ABI_BORROW &&
                target->generatorCaptureAbis[0] == XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE) {
                target->generatorCaptureAbis[0] = XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE; reject(&ir);
                target->generatorCaptureAbis[0] = XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE; ++retained;
            }
        }
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* call = &ir.instructions[i]; XL6_IrInstruction saved;
        if (call->kind != XL6_IR_INSTR_VIRTUAL_CALL && call->kind != XL6_IR_INSTR_PROTOCOL_CALL &&
            call->kind != XL6_IR_INSTR_GENERATOR_START) continue;
        assert(xl6IrCallInstructionErrorAbi(&ir, call) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        saved = ir.instructions[i + 1]; ir.instructions[i + 1].kind = XL6_IR_INSTR_DISCARD;
        reject(&ir); ir.instructions[i + 1] = saved; ++calls;
    }
    assert(generators == 4 && retained == 4 && calls == 10 && slotIndex != SIZE_MAX);
    assert(xl6IrVerifyModule(&ir, &parse.diags) && argc == 4);
    assert(xl6OutputOpenFile(&out, argv[1])); if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed; xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_ACTIVATION", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define ACTIVATION_DISPATCH xlir_vdispatch_%lu\n", (unsigned long)(slotIndex + 1));
    xl6OutputPrintf(&out, "#define ACTIVATION_FAULT_RESULT %s\n",
        xl6IrTypeGet(&ir.types, ir.classDispatchSlots[slotIndex].returnType)->errorResultCName);
    for (i = 0; i < ir.protocolDispatchSlotCount; ++i)
        if (strcmp(ir.protocolDispatchSlots[i].name, "numbers") == 0)
            xl6OutputPrintf(&out, "#define ACTIVATION_PROTOCOL xlir_pdispatch_%lu\n", (unsigned long)(i + 1));
    for (i = 0; i < ir.classDispatchImplCount; ++i) {
        const XL6_IrClassDispatchImpl* impl = &ir.classDispatchImpls[i];
        const XL6_IrNode* owner = xl6IrNodeGet(&ir, impl->ownerDeclaration);
        if (impl->slot == slotIndex && owner && owner->text && strcmp(owner->text, "ActivationDerived") == 0)
            xl6OutputPrintf(&out, "#define ACTIVATION_DIRECT xlir_generator_start_%u\n", (unsigned int)impl->implementationSymbol);
    }
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, storage->declaration);
        if (storage->kind != XL6_IR_STORAGE_GLOBAL || !declaration || !declaration->text) continue;
        if (strcmp(declaration->text, "activationPending") == 0) xl6OutputPrintf(&out, "#define ACTIVATION_PENDING s%u\n", (unsigned int)storage->symbol);
        if (strcmp(declaration->text, "activationReceiver") == 0) xl6OutputPrintf(&out, "#define ACTIVATION_RECEIVER s%u\n", (unsigned int)storage->symbol);
        if (strcmp(declaration->text, "activationDrops") == 0) xl6OutputPrintf(&out, "#define ACTIVATION_DROPS s%u\n", (unsigned int)storage->symbol);
        if (strcmp(declaration->text, "activationBodies") == 0) xl6OutputPrintf(&out, "#define ACTIVATION_BODIES s%u\n", (unsigned int)storage->symbol);
    }
    xl6OutputClose(&out);
    printf("virtual activation: %llu checked calls, %llu generators, %llu retained receivers, %llu rejected mutations, 6 frontend negatives\n",
        (unsigned long long)calls, (unsigned long long)generators, (unsigned long long)retained, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    return 1;
}
