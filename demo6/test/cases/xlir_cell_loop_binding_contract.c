#include "xlir_continuation_owner_test.h"

static void suspension(const char* prefix)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char path[1024];
    size_t i, cells = 0, before = mutations;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_loop_suspension.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_loop_suspension") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        assert(0);
    }
    verify(&ir); assert(ir.asyncMachineCount && ir.generatorMachineCount);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i]; XL6_IrInstruction *test, saved;
        if (instruction->kind != XL6_IR_INSTR_CELL_NEW) continue;
        ++cells; assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
        test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget; reject(&ir, NULL); *test = saved;
    }
    assert(cells == 2); verify(&ir);
    assert(snprintf(path, sizeof(path), "%s.suspension.c", prefix) > 0 && xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.suspension.h", prefix) > 0 && xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitHeader(&out, &ir, "CELL_LOOP_SUSPENSION", &parse.diags)); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.suspension.json", prefix) > 0 && xl6OutputOpenFile(&out, path));
    xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("loop-body suspension: %llu physical cell allocations, %llu rejected mutations; async/generator smoke only\n",
        (unsigned long long)cells, (unsigned long long)(mutations - before));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}

int main(int argc, char** argv)
{
    const char* names[] = {"loopCell_range", "loopCell_array", "loopCell_string", "loopCell_list",
        "loopCell_dict", "loopCell_protocol", "loopCell_generator", "loopCellCursor", "loopCellExits", "loopCellKey", "loopCellSet"};
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId symbols[11] = {0};
    size_t i, j, n, cells = 0, prefixes = 0, acquisitions = 0; unsigned acquisitionKinds = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_loop_binding.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_loop_binding") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.functionCount; ++i) {
        XL6_IrFunction* function = &ir.functions[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, function->declaration);
        const XL6_IrNode* name = declaration && declaration->childCount
            ? xl6IrNodeGet(&ir, ir.children[declaration->firstChild]) : NULL;
        for (n = 0; n < 11; ++n) if (name && name->text && !strcmp(name->text, names[n])) {
            assert(!symbols[n]); symbols[n] = function->symbol;
        }
    }
    for (n = 0; n < 11; ++n) assert(symbols[n]);
    for (i = 0; i < ir.loopOwnerCount; ++i) {
        XL6_IrLoopOwner* owner = &ir.loopOwners[i];
        const XL6_IrStorage* storage = xl6IrStorageGet(&ir, owner->symbol);
        XL6_IrLoopOwner saved = *owner; size_t installs = 0, releases = 0;
        if (!owner->target || !storage || !storage->cellType) continue;
        ++cells; assert(owner->type == storage->cellType);
        owner->type = storage->type; reject(&ir, NULL); *owner = saved;
        owner->target = 0; reject(&ir, NULL); *owner = saved;
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* instruction = &ir.instructions[j]; XL6_IrInstruction original = *instruction;
            if (instruction->targetSymbol != owner->symbol) continue;
            if (instruction->kind == XL6_IR_INSTR_CELL_INIT) {
                ++installs;
                instruction->kind = XL6_IR_INSTR_CELL_STORE; reject(&ir, NULL); *instruction = original;
            } else if (instruction->kind == XL6_IR_INSTR_CELL_RELEASE) {
                ++releases; assert(instruction->loopOwner == i + 1);
                instruction->loopOwner = 0; reject(&ir, NULL); *instruction = original;
                instruction->kind = XL6_IR_INSTR_RELEASE; reject(&ir, NULL); *instruction = original;
            }
        }
        assert(installs == 1 && releases != 0);
    }
    for (i = 0; i < ir.exitStepCount; ++i) {
        XL6_IrExitStep* step = &ir.exitSteps[i]; size_t saved = step->loopTargetCount;
        if (step->kind != XL6_IR_EXIT_STEP_LOOP_ITERATION) continue;
        ++prefixes;
        step->loopTargetCount = saved + 1; reject(&ir, NULL); step->loopTargetCount = saved;
        if (saved) { step->loopTargetCount = saved - 1; reject(&ir, NULL); step->loopTargetCount = saved; }
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i]; XL6_IrInstruction *test, saved; unsigned kind;
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
        switch (instruction->runtimeOp) {
        case XL6_IR_RUNTIME_STRING_UTF8_AT: kind = 1; break;
        case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY: kind = 2; break;
        case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY: kind = 4; break;
        case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE: kind = 8; break;
        default: continue;
        }
        ++acquisitions; acquisitionKinds |= kind;
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, instruction) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i + 1 < ir.instructionCount); test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && ir.operands[test->firstOperand] == instruction->result);
        test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget; reject(&ir, NULL); *test = saved;
    }
    assert(cells == 17 && prefixes > cells && acquisitionKinds == 15); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CELL_LOOP_BINDING", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 11; ++n) xl6OutputPrintf(&out, "#define LOOP_BINDING_%lu xlir_fn_%u\n", (unsigned long)n, symbols[n]);
    xl6OutputClose(&out);
    printf("loop bindings: %llu physical cells, %llu explicit prefixes, %llu checked acquisitions, %llu C/header-gated rejected mutations\n",
        (unsigned long long)cells, (unsigned long long)prefixes, (unsigned long long)acquisitions, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); suspension(argv[1]); return 0;
}
