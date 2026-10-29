#include "xlir_continuation_owner_test.h"

static int hasYield(const XL6_IrModule* ir, XL6_IrNodeId id)
{
    const XL6_IrNode* node = xl6IrNodeGet(ir, id); size_t i;
    if (node == NULL || node->kind == XL6_IR_NODE_FUNC_DECL) return 0;
    if (node->kind == XL6_IR_NODE_YIELD) return 1;
    for (i = 0; i < node->childCount; ++i)
        if (hasYield(ir, ir->children[node->firstChild + i])) return 1;
    return 0;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, bindings = 0, filters = 0, completedHandlers = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_generator_close_catch_dispatch.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "generator_close_catch_dispatch") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d %s\n",
            parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
        return 2;
    }
    verify(&ir); assert(ir.generatorMachineCount == 8);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrNodeId saved = instruction->origin, ownerId = saved;
        const XL6_IrNode* owner = xl6IrNodeGet(&ir, ownerId);
        int filter = instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            instruction->runtimeOp == XL6_IR_RUNTIME_ERROR_KIND_IS;
        int binding = instruction->kind == XL6_IR_INSTR_STORE;
        if (!filter && !binding) continue;
        while (owner != NULL && owner->kind != XL6_IR_NODE_TRY && owner->kind != XL6_IR_NODE_FUNC_DECL) {
            ownerId = owner->parent; owner = xl6IrNodeGet(&ir, ownerId);
        }
        if (owner == NULL || owner->kind != XL6_IR_NODE_TRY || ownerId == saved) continue;
        /* Reverting handler dispatch to the coarse try origin must fail even
         * after rebuilding every continuation cache. Both emitted artifacts
         * must remain empty for IR without a valid closing-region contract. */
        instruction->origin = ownerId;
        assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags)); reject(&ir, NULL);
        instruction->origin = saved;
        assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags)); verify(&ir);
        filters += filter; bindings += binding;
    }
    assert(filters == 12 && bindings >= 16);
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i], saved = *instruction;
        const XL6_IrNode* node = xl6IrNodeGet(&ir, instruction->origin);
        XL6_IrNodeId catches;
        char* constant;
        if (instruction->kind != XL6_IR_INSTR_GENERATOR_CLOSE_TEST ||
            instruction->generatorClosePointAbi != XL6_IR_GENERATOR_CLOSE_POINT_AFTER_TRY) continue;
        assert(node && node->kind == XL6_IR_NODE_TRY && node->childCount == 3);
        catches = ir.children[node->firstChild + 1];
        if (hasYield(&ir, catches)) continue;
        /* Handler completion needs a boundary even with no handler yield.
         * Removing it cannot be hidden by synchronizing the machine cache. */
        constant = xrtStrDup("false"); assert(constant);
        instruction->kind = XL6_IR_INSTR_CONST; instruction->immediateText = constant;
        instruction->generatorClosePointAbi = XL6_IR_GENERATOR_CLOSE_POINT_INVALID;
        instruction->generatorClosePlan = 0;
        assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags));
        reject(&ir, "closing path escapes its cleanup boundary");
        *instruction = saved; xrtFree(constant);
        assert(xl6IrBuildGeneratorMachines(&ir, &parse.diags)); verify(&ir); ++completedHandlers;
    }
    assert(completedHandlers == 4);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "GENERATOR_CLOSE_CATCH", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir);
    assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    printf("catch close: %llu handler stores, %llu typed filters, %llu nonsuspending handler boundaries, %llu C/header-gated mutations\n",
        (unsigned long long)bindings, (unsigned long long)filters,
        (unsigned long long)completedHandlers, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
