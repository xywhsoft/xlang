#include "xlir_continuation_owner_test.h"

static void suspension(const char* prefix)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, cells = 0, before = mutations; char path[1024];
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_allocation_suspension.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_suspension") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        assert(0);
    }
    verify(&ir); assert(ir.asyncMachineCount && ir.generatorMachineCount);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* allocation = &ir.instructions[i];
        XL6_IrInstruction *test, saved;
        if (allocation->kind != XL6_IR_INSTR_CELL_NEW) continue;
        ++cells; test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
        test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget;
        reject(&ir, NULL); *test = saved;
    }
    assert(cells == 3); verify(&ir);
    assert(snprintf(path, sizeof(path), "%s.suspension.c", prefix) > 0 && xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.suspension.h", prefix) > 0 && xl6OutputOpenFile(&out, path));
    assert(xl6IrCgenEmitHeader(&out, &ir, "CELL_SUSPENSION", &parse.diags)); xl6OutputClose(&out);
    assert(snprintf(path, sizeof(path), "%s.suspension.json", prefix) > 0 && xl6OutputOpenFile(&out, path));
    xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("cell suspension: %llu checked cell allocations, %llu rejected mutations\n",
        (unsigned long long)cells, (unsigned long long)(mutations - before));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}

int main(int argc, char** argv)
{
    const char* names[] = {"cellInt", "cellString", "cellVar", "cellNullable", "cellParameter",
        "cellStringParameter", "cellVarParameter", "cellParameters", "cellNullableStringParameter",
        "cellPattern", "cellCaught"};
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId symbols[11] = {0};
    size_t i, j, n, allocations = 0, parameters = 0, installations = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_allocation_outcome.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_allocation_outcome") ||
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
            assert(function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        }
        for (j = 0; function->parameterCells && j < function->parameterCount; ++j) {
            XL6_SymbolId saved = function->parameterCells[j];
            if (!saved) continue;
            ++parameters;
            function->parameterCells[j] = 0; reject(&ir, NULL); function->parameterCells[j] = saved;
        }
    }
    for (n = 0; n < 11; ++n) assert(symbols[n]);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* allocation = &ir.instructions[i];
        XL6_IrInstruction *test, *init = NULL, saved;
        const XL6_IrStorage* storage;
        XL6_IrErrorAbi effect; XL6_IrValueId initial;
        if (allocation->kind != XL6_IR_INSTR_CELL_NEW) continue;
        ++allocations; storage = xl6IrStorageGet(&ir, allocation->targetSymbol); assert(storage);
        assert(allocation->operandCount == 0 && allocation->result && allocation->type == storage->cellType);
        assert(xl6IrValueOwnershipGet(&ir, allocation->result) == XL6_IR_OWNERSHIP_MOVE);
        assert(xl6IrCallInstructionErrorAbi(&ir, allocation) == XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(&ir, allocation) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        effect = ir.nodes[allocation->origin - 1].constructionErrorAbi;
        assert(effect == XL6_IR_ERROR_ABI_MAY_RAISE);
        ir.nodes[allocation->origin - 1].constructionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&ir, NULL); ir.nodes[allocation->origin - 1].constructionErrorAbi = effect;
        test = &ir.instructions[i + 1]; saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST && test->operandCount == 1 &&
            ir.operands[test->firstOperand] == allocation->result);
        test->targetSymbol = allocation->targetSymbol; reject(&ir, NULL); *test = saved;
        test->falseTarget = test->trueTarget; reject(&ir, NULL); *test = saved;
        test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget;
        reject(&ir, NULL); *test = saved;
        test->kind = XL6_IR_INSTR_JUMP; test->operandCount = 0; test->falseTarget = 0;
        reject(&ir, NULL); *test = saved;
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* candidate = &ir.instructions[j];
            if (candidate->kind == XL6_IR_INSTR_CELL_INIT && candidate->operandCount == 2 &&
                ir.operands[candidate->firstOperand] == allocation->result) { assert(!init); init = candidate; }
        }
        assert(init && init->targetSymbol == allocation->targetSymbol && !init->result);
        ++installations; initial = ir.operands[init->firstOperand + 1];
        assert(xl6IrInstructionOperandOwnership(&ir, init, 0) == XL6_IR_OPERAND_OWNERSHIP_CONSUME &&
            xl6IrInstructionOperandOwnership(&ir, init, 1) == XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        saved = *allocation;
        allocation->operandCount = 1; reject(&ir, NULL); *allocation = saved;
        allocation->result = 0; reject(&ir, NULL); *allocation = saved;
        allocation->targetSymbol = 0; reject(&ir, NULL); *allocation = saved;
        allocation->type = storage->type; reject(&ir, NULL); *allocation = saved;
        saved = *init;
        init->operandCount = 1; reject(&ir, NULL); *init = saved;
        init->targetSymbol = 0; reject(&ir, NULL); *init = saved;
        init->type = storage->type; reject(&ir, NULL); *init = saved;
        ir.operands[init->firstOperand] = initial;
        ir.operands[init->firstOperand + 1] = allocation->result;
        reject(&ir, NULL);
        ir.operands[init->firstOperand] = allocation->result; ir.operands[init->firstOperand + 1] = initial;
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* release = &ir.instructions[j];
            if (release->kind != XL6_IR_INSTR_CELL_RELEASE || release->targetSymbol != storage->symbol) continue;
            saved = *release;
            /* A second installation is not a release, even with the same
             * physical cell type and a structurally valid operand slice. */
            *release = *init; reject(&ir, NULL); *release = saved;
        }
    }
    assert(allocations == 14 && installations == allocations && parameters == 7);
    verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CELL_OUTCOME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    for (n = 0; n < 11; ++n) xl6OutputPrintf(&out, "#define CELL_OUTCOME_%lu xlir_fn_%u\n", (unsigned long)n, symbols[n]);
    xl6OutputClose(&out);
    printf("cell allocation entries: %llu checked allocations/installations, %llu parameter cells, %llu rejected C/header-gated mutations\n",
        (unsigned long long)allocations, (unsigned long long)parameters, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); suspension(argv[1]); return 0;
}
