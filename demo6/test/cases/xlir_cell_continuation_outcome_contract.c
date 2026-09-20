#include "xlir_continuation_owner_test.h"

static size_t checkCells(XL6_IrModule* ir, XL6_IrAsyncMachineFrameSlot* slots, size_t count)
{
    size_t i, cells = 0;
    for (i = 0; i < count; ++i) {
        XL6_IrAsyncMachineFrameSlot* slot = &slots[i], saved = *slot;
        if (slot->destroyAbi != XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL) continue;
        ++cells;
        assert(slot->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE);
        assert(slot->initBit != SIZE_MAX);
        slot->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE; reject(ir, NULL); *slot = saved;
        slot->transferAbi = XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE; reject(ir, NULL); *slot = saved;
        slot->initBit = SIZE_MAX; reject(ir, NULL); *slot = saved;
    }
    assert(cells >= 2); return cells;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    const char* names[] = {"cellContinuationWords", "cellContinuationAwait", "cellContinuationManaged"};
    XL6_SymbolId targets[3] = {0};
    size_t i, j, cells = 0, asyncCells, generatorCells; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_continuation_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_continuation_outcome") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.asyncMachineCount == 2 && ir.generatorMachineCount == 1);
    for (i = 0; i < ir.functionCount; ++i) {
        XL6_IrFunction* function = &ir.functions[i];
        const XL6_IrNode* decl = xl6IrNodeGet(&ir, function->declaration);
        const XL6_IrNode* name = decl && decl->childCount ? xl6IrNodeGet(&ir, ir.children[decl->firstChild]) : NULL;
        for (j = 0; j < 3; ++j) if (name && name->text && !strcmp(name->text, names[j])) {
            assert(!targets[j]); targets[j] = function->symbol;
        }
    }
    assert(targets[0] && targets[1] && targets[2]);
    asyncCells = checkCells(&ir, ir.asyncMachineFrameSlots, ir.asyncMachineFrameSlotCount);
    generatorCells = checkCells(&ir, ir.generatorFrameSlots, ir.generatorFrameSlotCount);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind == XL6_IR_INSTR_CELL_NEW) {
            XL6_IrInstruction* test = &ir.instructions[i + 1], saved = *test;
            assert(i + 1 < ir.instructionCount && test->kind == XL6_IR_INSTR_ERROR_TEST);
            test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget;
            reject(&ir, NULL); *test = saved; ++cells;
        } else if (instruction->kind == XL6_IR_INSTR_CELL_INIT) {
            XL6_IrInstruction saved = *instruction;
            instruction->kind = XL6_IR_INSTR_CELL_STORE; reject(&ir, NULL); *instruction = saved;
        }
    }
    assert(cells == 5); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CELL_CONTINUATION_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define CELL_CONTINUATION_WORDS xlir_generator_start_%u\n", targets[0]);
    xl6OutputPrintf(&out, "#define CELL_CONTINUATION_AWAIT xlir_async_start_%u\n", targets[1]);
    xl6OutputPrintf(&out, "#define CELL_CONTINUATION_MANAGED xlir_async_start_%u\n", targets[2]);
    xl6OutputClose(&out);
    printf("cell continuation: %llu async + %llu generator cell residents, %llu source cell constructions, %llu C/header-gated mutations\n",
        (unsigned long long)asyncCells, (unsigned long long)generatorCells,
        (unsigned long long)cells, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
