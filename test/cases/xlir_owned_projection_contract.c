#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_PROJECTION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    unsigned effects = 0, projections = 0, bindingActions = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_owned_projection.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "owned_projection") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags)) goto failed;
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        if (node->projectionErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) continue;
        node->projectionErrorAbi = XL6_IR_ERROR_ABI_INVALID; reject(&ir);
        node->projectionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir);
        node->projectionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE; ++effects;
    }
    assert(effects >= 10);
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction *operation = &ir.instructions[i], *test;
        XL6_IrBlockId failure;
        if (!xl6IrInstructionIsFallibleProjection(&ir, operation)) continue;
        assert(i + 1 < ir.instructionCount);
        test = &ir.instructions[i + 1]; assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
        assert(xl6IrCallInstructionErrorAbi(&ir, operation) == XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir, operation) == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        failure = test->falseTarget; test->falseTarget = test->trueTarget;
        reject(&ir); test->falseTarget = failure;
        test->kind = XL6_IR_INSTR_TRAP_IF_FALSE; reject(&ir); test->kind = XL6_IR_INSTR_ERROR_TEST;
        if (operation->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT) {
            XL6_IrNodeId origin = test->origin;
            test->origin = operation->origin; reject(&ir); test->origin = origin;
        }
        ++projections;
    }
    for (i = 0; i < ir.cleanupActionCount; ++i) {
        XL6_IrCleanupAction* action = &ir.cleanupActions[i];
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, action->origin);
        XL6_SymbolId symbol = action->targetSymbol;
        if (origin == NULL || origin->kind != XL6_IR_NODE_PATTERN) continue;
        assert(action->kind == XL6_IR_CLEANUP_RELEASE);
        /* Two payloads have identical types; type checking alone must not
         * accept replacing one lexical owner's identity with the other. */
        {
            XL6_IrDeclarationBinding first, second;
            assert(xl6IrDeclarationBindingAt(&ir, action->origin, 0, &first));
            assert(xl6IrDeclarationBindingAt(&ir, action->origin, 1, &second));
            action->targetSymbol = symbol == first.symbol ? second.symbol : first.symbol;
            reject(&ir); action->targetSymbol = symbol;
        }
        ++bindingActions;
    }
    assert(projections >= 20 && bindingActions >= 10 && xl6IrVerifyModule(&ir, &parse.diags));
    assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "OWNED_PROJECTION", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("owned projection contract: %u typed effects, %u checked copies, %u lexical binding actions, %u rejected IR mutations\n",
        effects, projections, bindingActions, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%d:%d: %s\n",
        parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
