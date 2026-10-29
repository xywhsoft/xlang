#include "xlir_continuation_owner_test.h"

static size_t checkCursors(XL6_IrModule* ir, XL6_IrContinuationFrameSlot* slots, size_t count)
{
    size_t i, cursors = 0;
    for (i = 0; i < count; ++i) {
        XL6_IrContinuationFrameSlot* slot = &slots[i], saved = *slot;
        if (slot->destroyAbi != XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_VALUE_ITERATOR) continue;
        ++cursors; assert(slot->initBit != SIZE_MAX);
        slot->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(ir, NULL); *slot = saved;
        slot->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE; reject(ir, NULL); *slot = saved;
        slot->initBit = SIZE_MAX; reject(ir, NULL); *slot = saved;
    }
    return cursors;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    const char* names[] = {"projectionAwait", "projectionYield"};
    XL6_SymbolId targets[2] = {0};
    size_t i, j, keys = 0, logical = 0, prefixes = 0, effects = 0, cells = 0;
    char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_dynamic_iteration_allocation.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "dynamic_iteration_allocation") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 1 && ir.generatorMachineCount == 1);
    assert(checkCursors(&ir, ir.asyncMachineFrameSlots, ir.asyncMachineFrameSlotCount) == 1);
    assert(checkCursors(&ir, ir.generatorFrameSlots, ir.generatorFrameSlotCount) == 1);
    for (i = 0; i < ir.functionCount; ++i) {
        const XL6_IrFunction* function = &ir.functions[i];
        const XL6_IrNode* decl = xl6IrNodeGet(&ir, function->declaration);
        const XL6_IrNode* name = decl && decl->childCount ? xl6IrNodeGet(&ir, ir.children[decl->firstChild]) : NULL;
        for (j = 0; j < 2; ++j) if (name && name->text && !strcmp(name->text, names[j])) {
            assert(!targets[j]); targets[j] = function->symbol;
        }
    }
    assert(targets[0] && targets[1]);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i], *test, saved;
        int projection = instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE ||
             instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE);
        if (!projection && instruction->kind != XL6_IR_INSTR_CELL_NEW) continue;
        if (projection) {
            if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE) ++keys;
            else ++logical;
            assert(xl6IrCallInstructionErrorAbi(&ir, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE);
            assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        } else ++cells;
        assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            ir.operands[test->firstOperand] == instruction->result);
        test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget;
        reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_JUMP; test->operandCount = 0;
        reject(&ir, NULL); *test = saved;
    }
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrNode* parent = xl6IrNodeGet(&ir, node->parent);
        const XL6_IrNode* loop = parent ? xl6IrNodeGet(&ir, parent->parent) : NULL;
        if (node->kind != XL6_IR_NODE_NAME || !loop || loop->kind != XL6_IR_NODE_LOOP ||
            node->constructionErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        ++effects; node->constructionErrorAbi = XL6_IR_ERROR_ABI_INVALID;
        reject(&ir, NULL); node->constructionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    }
    for (i = 0; i < ir.exitStepCount; ++i) {
        XL6_IrExitStep* step = &ir.exitSteps[i]; size_t saved = step->loopTargetCount;
        if (step->kind != XL6_IR_EXIT_STEP_LOOP_ITERATION) continue;
        ++prefixes; step->loopTargetCount = saved + 1; reject(&ir, NULL); step->loopTargetCount = saved;
        if (saved) { step->loopTargetCount = saved - 1; reject(&ir, NULL); step->loopTargetCount = saved; }
    }
    assert(keys == 4 && logical == 1 && cells == 2 && effects == 6 && prefixes > 5); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "DYNAMIC_ITERATION_ALLOCATION", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define ITERATION_PROJECTION_AWAIT xlir_async_start_%u\n", targets[0]);
    xl6OutputPrintf(&out, "#define ITERATION_PROJECTION_YIELD xlir_generator_start_%u\n", targets[1]);
    xl6OutputClose(&out);
    printf("dynamic iteration: %llu key + %llu logical checked projections, %llu cells, %llu construction effects, %llu exit prefixes, two resident cursors, %llu C/header-gated mutations\n",
        (unsigned long long)keys, (unsigned long long)logical, (unsigned long long)cells,
        (unsigned long long)effects, (unsigned long long)prefixes, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
