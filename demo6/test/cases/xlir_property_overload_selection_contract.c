#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static size_t selectors, containerGetters, mutations;
static void checkSemanticOwner(const XL6_AstNode* node, const XL6_AstNode* parent)
{
    if (!node) return;
    if (node->kind == XL6_AST_INDEX &&
        (node->semanticCallKind == XL6_SEM_CALL_PROPERTY_GET ||
         node->semanticCallKind == XL6_SEM_CALL_WITNESS)) {
        const XL6_AstNode* selector = node->children[0];
        assert(node->semanticTargetSymbolId && node->inferredType);
        assert(selector->kind == XL6_AST_MEMBER && !selector->inferredType &&
            selector->semanticCallKind == XL6_SEM_CALL_NONE && !selector->semanticTargetSymbolId);
        ++selectors;
    }
    if (parent && parent->kind == XL6_AST_INDEX && parent->children[0] == node &&
        parent->semanticCallKind == XL6_SEM_CALL_NONE &&
        (node->semanticCallKind == XL6_SEM_CALL_PROPERTY_GET ||
         node->semanticCallKind == XL6_SEM_CALL_WITNESS)) {
        assert(node->inferredType && node->semanticTargetSymbolId);
        ++containerGetters;
    }
    for (size_t i = 0; i < node->childCount; ++i) checkSemanticOwner(node->children[i], node);
}
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out; size_t size = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out, ir, "BAD_PROPERTY", NULL) :
            xl6IrCgenEmitC(&out, ir, NULL)));
        char* text = xl6OutputTakeMemoryText(&out, &size);
        assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    size_t classes = 0, protocols = 0, slots = 0;
    assert(argc == 2); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "property_selection")) goto failed;
    checkSemanticOwner(parse.root, NULL);
    assert(selectors >= 5 && containerGetters >= 1);
    if (!xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (size_t i = 0; i < ir.callSiteCount; ++i) {
        XL6_IrCallSite* site = &ir.callSites[i];
        const XL6_IrNode* call = xl6IrNodeGet(&ir, site->origin);
        if (call->kind != XL6_IR_NODE_INDEX ||
            (call->callKind != XL6_SEM_CALL_PROPERTY_GET && call->callKind != XL6_SEM_CALL_WITNESS)) continue;
        XL6_IrNode* selector = &ir.nodes[ir.children[call->firstChild] - 1];
        XL6_IrNode savedSelector = *selector; XL6_IrCallSite savedSite = *site;
        assert(selector->kind == XL6_IR_NODE_MEMBER && !selector->type &&
            !selector->storageType && !selector->targetSymbol && selector->callKind == XL6_SEM_CALL_NONE);
        assert(!xl6IrCallSiteGet(&ir, ir.children[call->firstChild]));
        if (site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL) ++protocols; else ++classes;
        site->receiver = ir.children[call->firstChild]; reject(&ir); *site = savedSite;
        site->targetSymbol = 0; reject(&ir); *site = savedSite;
        selector->callKind = XL6_SEM_CALL_PROPERTY_GET; reject(&ir); *selector = savedSelector;
        selector->targetSymbol = site->targetSymbol; reject(&ir); *selector = savedSelector;
        selector->type = selector->storageType = site->resultType; reject(&ir); *selector = savedSelector;
        XL6_IrCallArgument* argument = &ir.callArguments[site->firstArgument];
        XL6_IrCallArgument savedArgument = *argument;
        argument->expression = site->receiver; reject(&ir); *argument = savedArgument;
    }
    for (size_t i = 0; i < ir.classDispatchSlotCount; ++i) {
        XL6_IrClassDispatchSlot* slot = &ir.classDispatchSlots[i];
        const XL6_IrCallTarget* base = xl6IrCallTargetGet(&ir, slot->declarationSymbol);
        assert(base && base->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE &&
            slot->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
        XL6_IrErrorAbi saved = slot->errorAbi;
        slot->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir); slot->errorAbi = saved;
        ++slots;
    }
    assert(classes >= 3 && protocols >= 2 && slots == 2 && xl6IrVerifyModule(&ir, NULL));
    printf("property overload selection: %zu semantic selectors, %zu container getters, %zu class / %zu protocol sites, %zu mixed-effect scalar slots, %zu rejected mutations\n",
        selectors, containerGetters, classes, protocols, slots, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
