#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static size_t mutations;
/* Change every redundant Future item contract together. Agreement between
 * cached tables is insufficient: the canonical item TypeId must reject this. */
static size_t future_contracts(XL6_IrModule* ir, XL6_IrGeneratorYieldAbi abi)
{
    size_t i, changed = 0;
    for (i = 0; i < ir->callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir->callTargets[i];
        const XL6_IrType* item = xl6IrTypeGet(&ir->types, target->generatorItemType);
        if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR && item &&
            item->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            target->generatorYieldAbi = abi; ++changed;
        }
    }
    for (i = 0; i < ir->generatorMachineCount; ++i) {
        XL6_IrGeneratorMachine* machine = &ir->generatorMachines[i];
        const XL6_IrType* item = xl6IrTypeGet(&ir->types, machine->itemType);
        if (item && item->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) machine->yieldAbi = abi;
    }
    for (i = 0; i < ir->generatorSuspensionCount; ++i) {
        XL6_IrGeneratorSuspension* suspension = &ir->generatorSuspensions[i];
        const XL6_IrType* item = xl6IrTypeGet(&ir->types, suspension->itemType);
        if (item && item->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            suspension->yieldAbi = abi;
            ir->instructions[suspension->yieldInstruction].generatorYieldAbi = abi;
        }
    }
    return changed;
}
static void reject(XL6_IrModule* ir, const char* label)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    if (xl6IrVerifyModule(ir, &diags)) { fprintf(stderr, "generator payload mutation escaped: %s\n", label); assert(0); }
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_PAYLOAD", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, p, generators = 0, asyncLaunches = 0, futures = 0, retained = 0;
    assert(argc == 5); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argv[1], &parse) && xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "generator_payload"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) && xl6IrLowerModule(&ir, &parse.diags));
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        XL6_IrGeneratorYieldAbi saved;
        const XL6_IrType* item;
        if (target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR &&
            target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC) continue;
        assert(target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER);
        target->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT;
        reject(&ir, "launch without isolated outcome carrier");
        target->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER;
        if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) { ++asyncLaunches; continue; }
        ++generators; item = xl6IrTypeGet(&ir.types, target->generatorItemType); assert(item);
        saved = target->generatorYieldAbi;
        assert(saved == xl6IrGeneratorYieldAbiForType(&ir.types, target->generatorItemType));
        if (item->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            ++futures; assert(saved == XL6_IR_GENERATOR_YIELD_MOVE_FUTURE);
        }
        target->generatorYieldAbi = XL6_IR_GENERATOR_YIELD_INVALID;
        reject(&ir, "missing item transfer"); target->generatorYieldAbi = saved;
        for (p = 0; p < target->parameterCount; ++p) {
            XL6_IrContinuationFrameTransferAbi capture = target->generatorCaptureAbis[p];
            assert(capture == xl6IrParameterFrameTransfer(&ir, target->parameterTypes[p], target->parameterAbis[p]));
            retained += capture == XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE;
            target->generatorCaptureAbis[p] = XL6_IR_ASYNC_FRAME_TRANSFER_INVALID;
            reject(&ir, "missing capture transfer"); target->generatorCaptureAbis[p] = capture;
        }
    }
    assert(generators || asyncLaunches);
    if (futures) {
        assert(future_contracts(&ir, XL6_IR_GENERATOR_YIELD_COPY_RAW_POINTER) == futures);
        reject(&ir, "coherent Future item ownership erasure");
        assert(future_contracts(&ir, XL6_IR_GENERATOR_YIELD_MOVE_FUTURE) == futures);
    }
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        if (node->kind != XL6_IR_NODE_YIELD ||
            node->implicitConversionErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        node->implicitConversionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&ir, "yield boxing without failure effect");
        node->implicitConversionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind == XL6_IR_INSTR_YIELD) {
            XL6_IrGeneratorYieldAbi saved = instruction->generatorYieldAbi;
            instruction->generatorYieldAbi = XL6_IR_GENERATOR_YIELD_INVALID;
            reject(&ir, "yield without transfer"); instruction->generatorYieldAbi = saved;
        }
    }
    for (i = 0; i < ir.generatorFrameSlotCount; ++i) {
        XL6_IrContinuationFrameSlot saved = ir.generatorFrameSlots[i];
        if (saved.residence == XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER) {
            ir.generatorFrameSlots[i].transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_INVALID;
            reject(&ir, "frame without capture transfer"); ir.generatorFrameSlots[i] = saved;
            ir.generatorFrameSlots[i].destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_INVALID;
            reject(&ir, "frame without capture destruction"); ir.generatorFrameSlots[i] = saved;
        }
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]) && xl6IrCgenEmitHeader(&out, &ir, "GENERATOR_PAYLOAD", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[4])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    {
        char path[1024]; int length = snprintf(path, sizeof(path), "%s.launch.h", argv[3]);
        assert(length > 0 && (size_t)length < sizeof(path) && xl6OutputOpenFile(&out, path));
        for (i = 0; i < ir.callTargetCount; ++i) {
            const XL6_IrCallTarget* target = &ir.callTargets[i];
            if (target->parameterCount) continue;
            if (asyncLaunches == 1 && target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC)
                xl6OutputPrintf(&out, "#define XLIR_PAYLOAD_ASYNC_LAUNCH xlir_async_start_%u\n", target->symbol);
            if (generators == 1 && target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)
                xl6OutputPrintf(&out, "#define XLIR_PAYLOAD_GENERATOR_LAUNCH xlir_generator_start_%u\n", target->symbol);
        }
        xl6OutputClose(&out);
    }
    printf("generator payload: %zu generators, %zu async launches, %zu Future item ABIs, %zu retained xvalue captures, %zu mutations rejected\n",
        generators, asyncLaunches, futures, retained, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
