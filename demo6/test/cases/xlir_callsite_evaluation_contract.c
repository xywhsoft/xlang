#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
static size_t mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_EVALUATION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, a, reordered = 0, protocol = 0, rhsFirst = 0, keywordInterleaving = 0;
    size_t membership = 0, indexedWrites = 0, mutableArguments = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_callsite_evaluation.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "callsite_evaluation"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.callSiteCount; ++i) {
        XL6_IrCallSite* site = &ir.callSites[i];
        XL6_IrCallSite saved = *site;
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, site->origin);
        const XL6_IrCallArgument* first = xl6IrCallSiteEvaluationArgument(&ir, site, 0);
        if (site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL) {
            assert(site->receiver != XL6_IR_NODE_INVALID && site->dispatchSlot != 0);
            ++protocol;
        }
        if (origin->kind == XL6_IR_NODE_ASSIGN && site->receiverEvaluationIndex == 1) ++rhsFirst;
        if (origin->kind == XL6_IR_NODE_BINARY && origin->opcode == XL6_IR_OPCODE_CONTAINS) {
            assert(site->receiverEvaluationIndex == 1); ++membership;
        }
        if (origin->kind == XL6_IR_NODE_ASSIGN && site->argumentCount >= 2) {
            assert(first && first->parameter + 1 == site->argumentCount); ++indexedWrites;
        }
        if (first && site->variadicAbi == XL6_IR_CALL_VARIADIC_DICT &&
            first->source == XL6_IR_CALL_ARGUMENT_DICT_VARARG_ITEM) ++keywordInterleaving;
        site->dispatchKind = (XL6_IrCallDispatchKind)99; reject(&ir); *site = saved;
        site->receiverEvaluationIndex = saved.receiverEvaluationIndex == SIZE_MAX ? 0 : SIZE_MAX;
        reject(&ir); *site = saved;
        if (site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL) {
            site->receiver = XL6_IR_NODE_INVALID; reject(&ir); *site = saved;
            site->dispatchSlot = 0; reject(&ir); *site = saved;
        }
        for (a = 0; a < site->argumentCount; ++a) {
            XL6_IrCallArgument* argument = &ir.callArguments[site->firstArgument + a];
            XL6_IrCallArgument savedArgument = *argument;
            if (argument->evaluationIndex != a) ++reordered;
            argument->evaluationIndex = site->argumentCount;
            reject(&ir); *argument = savedArgument;
            if (origin->kind == XL6_IR_NODE_CALL && argument->source != XL6_IR_CALL_ARGUMENT_DEFAULT) {
                const XL6_IrNode* arguments = xl6IrNodeGet(&ir, ir.children[origin->firstChild + 1]);
                XL6_IrNode* wrapper = &ir.nodes[ir.children[arguments->firstChild + argument->evaluationIndex] - 1];
                unsigned int flags = wrapper->flags;
                if (flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) ++mutableArguments;
                wrapper->flags ^= XL6_AST_FLAG_BYREF;
                reject(&ir); wrapper->flags = flags;
            }
        }
        if (site->argumentCount >= 2) {
            XL6_IrCallArgument* left = &ir.callArguments[site->firstArgument];
            XL6_IrCallArgument* right = left + 1;
            XL6_IrCallArgument savedLeft = *left, savedRight = *right;
            left->expression = savedRight.expression; right->expression = savedLeft.expression;
            left->evaluationIndex = savedRight.evaluationIndex; right->evaluationIndex = savedLeft.evaluationIndex;
            left->source = savedRight.source; right->source = savedLeft.source;
            reject(&ir); *left = savedLeft; *right = savedRight;
        }
    }
    assert(reordered >= 40 && protocol >= 6 && rhsFirst >= 10 && keywordInterleaving >= 3);
    assert(membership >= 2 && indexedWrites >= 9 && mutableArguments >= 8);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_CALLSITE_EVALUATION", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define CALLSITE_PROTOCOL_COUNT %llu\n", (unsigned long long)protocol);
    xl6OutputClose(&out);
    printf("callsite evaluation: %llu reordered bindings, %llu protocol sites, %llu RHS-first sites, %llu interleaved keyword calls, %llu rejected mutations\n",
        (unsigned long long)reordered, (unsigned long long)protocol, (unsigned long long)rhsFirst,
        (unsigned long long)keywordInterleaving, (unsigned long long)mutations);
    printf("callsite evaluation: %llu membership sites, %llu indexed writes, %llu mutable arguments\n",
        (unsigned long long)membership, (unsigned long long)indexedWrites, (unsigned long long)mutableArguments);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
