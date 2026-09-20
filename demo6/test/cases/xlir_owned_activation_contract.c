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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_CAPTURE", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void frame_contract(XL6_IrModule* ir, XL6_IrContinuationFrameSlot* frames, size_t frameCount,
    XL6_IrContinuationFrameAction* actions, size_t first, size_t count)
{
    size_t i; int borrowed = 0; size_t owned = 0;
    for (i = 0; i < count; ++i) {
        XL6_IrContinuationFrameSlot* frame = &frames[actions[first + i].frameSlot];
        if (frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER) {
            assert(!borrowed && frame->source == XL6_IR_ASYNC_SPILL_SOURCE_STORAGE &&
                actions[first + i].kind == XL6_IR_ASYNC_FRAME_ACTION_INIT_PARAMETER &&
                actions[first + i].value == 0 && actions[first + i].instruction == SIZE_MAX); ++owned;
        } else borrowed = 1;
    }
    assert(owned == 2 && borrowed);
    {
        XL6_IrContinuationFrameAction tmp = actions[first];
        actions[first] = actions[first + count - 1]; actions[first + count - 1] = tmp;
        reject(ir);
        actions[first + count - 1] = actions[first]; actions[first] = tmp;
    }
    for (i = 0; i < frameCount; ++i) {
        XL6_IrContinuationFrameSlot* frame = &frames[i];
        XL6_IrContinuationFrameSlot saved = *frame;
        if (frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER) continue;
        frame->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE; reject(ir); *frame = saved;
        frame->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED; reject(ir); *frame = saved;
        frame->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(ir); *frame = saved;
        frame->initBit = SIZE_MAX; reject(ir); *frame = saved;
    }
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, p, captures = 0, releases = 0, ready = 0, pool = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(argc == 4 && xl6ParseFile("test/cases/xlir_owned_activation.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    if (!xl6SemCheckModule(parse.root, &parse.diags, "owned_activation") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR &&
            target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC) continue;
        if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) {
            ready += target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE;
            pool += target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL;
        }
        for (p = 0; p < target->parameterCount; ++p) {
            if (target->parameterAbis[p] != XL6_IR_PARAMETER_ABI_OWNED) continue;
            ++captures;
            if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) {
                assert(target->asyncCaptureAbis[p] == XL6_IR_ASYNC_CAPTURE_TAKE_PARAMETER);
                target->asyncCaptureAbis[p] = XL6_IR_ASYNC_CAPTURE_RETAIN_XVALUE; reject(&ir);
                target->asyncCaptureAbis[p] = XL6_IR_ASYNC_CAPTURE_TAKE_PARAMETER;
            } else {
                assert(target->generatorCaptureAbis[p] == XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER);
                target->generatorCaptureAbis[p] = XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE; reject(&ir);
                target->generatorCaptureAbis[p] = XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER;
            }
            target->parameterAbis[p] = XL6_IR_PARAMETER_ABI_BORROW; reject(&ir);
            target->parameterAbis[p] = XL6_IR_PARAMETER_ABI_OWNED;
        }
    }
    assert(captures == 8 && ready == 1 && pool == 2 && ir.asyncMachineCount == 1 && ir.generatorMachineCount == 1);
    frame_contract(&ir, ir.asyncMachineFrameSlots, ir.asyncMachineFrameSlotCount, ir.asyncFrameActions,
        ir.asyncMachines[0].firstPrologueAction, ir.asyncMachines[0].prologueActionCount);
    frame_contract(&ir, ir.generatorFrameSlots, ir.generatorFrameSlotCount, ir.generatorFrameActions,
        ir.generatorMachines[0].firstPrologueAction, ir.generatorMachines[0].prologueActionCount);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        const XL6_IrStorage* storage = xl6IrStorageGet(&ir, instruction->targetSymbol);
        if (instruction->kind != XL6_IR_INSTR_RELEASE || storage == NULL || storage->kind != XL6_IR_STORAGE_PARAMETER) continue;
        {
            const XL6_IrNode* owner = xl6IrNodeGet(&ir, storage->owner);
            const XL6_IrCallTarget* target = owner ? xl6IrCallTargetGet(&ir, owner->symbol) : NULL;
            if (!target || (target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
                target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)) continue;
        }
        instruction->kind = XL6_IR_INSTR_DISCARD; reject(&ir); instruction->kind = XL6_IR_INSTR_RELEASE; ++releases;
    }
    assert(releases >= 8 && xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed; xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_OWNED_CAPTURE", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    for (i = 0; i < ir.callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &ir.callTargets[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, target->declaration);
        const XL6_IrNode* name = declaration && declaration->childCount
            ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        if (!name || !name->text) continue;
        if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC || target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)
            xl6OutputPrintf(&out, "#define CAPTURE_%s xlir_%s_start_%u\n", name->text,
                target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ? "async" : "generator", (unsigned int)target->symbol);
    }
    xl6OutputClose(&out);
    printf("owned activation: %llu consuming captures, %llu explicit worker releases, %llu rejected mutations, ready/pool/async/generator\n",
        (unsigned long long)captures, (unsigned long long)releases, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n", parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    return 1;
}
