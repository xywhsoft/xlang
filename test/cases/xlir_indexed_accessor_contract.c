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
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_ACCESSOR", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, getters = 0, protocols = 0, classes = 0, containerReads = 0, genericMembers = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_indexed_accessor.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "indexed_accessor"));
    if (!xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (i = 0; i < ir.callSiteCount; ++i) {
        XL6_IrCallSite* site = &ir.callSites[i];
        XL6_IrCallSite savedSite = *site;
        const XL6_IrNode* call = xl6IrNodeGet(&ir, site->origin);
        if (call->kind == XL6_IR_NODE_INDEX && (call->callKind == XL6_SEM_CALL_PROPERTY_GET ||
            call->callKind == XL6_SEM_CALL_WITNESS)) {
            XL6_IrNodeId selectorId = ir.children[call->firstChild];
            XL6_IrNode* selector = &ir.nodes[selectorId - 1];
            XL6_IrNode savedSelector = *selector;
            XL6_IrCallArgument* argument = &ir.callArguments[site->firstArgument];
            XL6_IrCallArgument savedArgument = *argument;
            assert(selector->kind == XL6_IR_NODE_MEMBER && selector->callKind == XL6_SEM_CALL_NONE &&
                selector->valueCategory == XL6_IR_VALUE_NONE && !selector->type && !selector->storageType &&
                !xl6IrCallSiteGet(&ir, selectorId));
            assert(site->receiver == ir.children[selector->firstChild] && site->receiverEvaluationIndex == 0);
            assert(site->argumentCount == 2 && argument->evaluationIndex == 0);
            ++getters;
            if (site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL) ++protocols; else ++classes;
            site->receiver = selectorId; reject(&ir); *site = savedSite;
            site->targetSymbol = 0; reject(&ir); *site = savedSite;
            selector->callKind = XL6_SEM_CALL_PROPERTY_GET; reject(&ir); *selector = savedSelector;
            selector->valueCategory = XL6_IR_VALUE_RVALUE; reject(&ir); *selector = savedSelector;
            selector->type = selector->storageType = site->resultType; reject(&ir); *selector = savedSelector;
            argument->expression = site->receiver; reject(&ir); *argument = savedArgument;
        } else if (call->kind == XL6_IR_NODE_MEMBER && site->argumentCount == 0 &&
            site->dispatchKind == XL6_IR_CALL_DISPATCH_PROTOCOL) {
            const XL6_IrNode* parent = xl6IrNodeGet(&ir, call->parent);
            if (parent && parent->kind == XL6_IR_NODE_INDEX) {
                assert(call->type && call->storageType && parent->callKind == XL6_SEM_CALL_NONE);
                ++containerReads;
            }
        }
    }
    for (i = 0; i < ir.genericSpecializationCount; ++i) {
        XL6_IrGenericSpecialization* instance = &ir.genericSpecializations[i];
        XL6_IrNode* declaration = &ir.nodes[instance->declaration - 1];
        XL6_IrNode* source = &ir.nodes[instance->sourceDeclaration - 1];
        XL6_IrTypeId savedOwner = declaration->declarationOwnerType;
        XL6_IrTypeId savedSourceOwner = source->declarationOwnerType;
        const XL6_IrType* owner = xl6IrTypeGet(&ir.types, savedOwner);
        size_t j;
        if (!savedOwner) continue;
        assert(owner && owner->paramCount == instance->typeArgCount);
        ++genericMembers;
        declaration->declarationOwnerType = 0; reject(&ir);
        declaration->declarationOwnerType = savedSourceOwner; reject(&ir);
        declaration->declarationOwnerType = savedOwner;
        source->declarationOwnerType = savedOwner; reject(&ir);
        source->declarationOwnerType = savedSourceOwner;
        for (j = 0; j < ir.genericSpecializationCount; ++j) {
            const XL6_IrGenericSpecialization* other = &ir.genericSpecializations[j];
            if (other->sourceDeclaration == instance->sourceDeclaration &&
                other->declaration != instance->declaration) {
                declaration->declarationOwnerType =
                    ir.nodes[other->declaration - 1].declarationOwnerType;
                reject(&ir); declaration->declarationOwnerType = savedOwner;
            }
        }
    }
    assert(genericMembers >= 8);
    printf("indexed accessor: %llu exact generic member owners\n", (unsigned long long)genericMembers);
    assert(getters >= 10 && protocols >= 6 && classes >= 4 && containerReads >= 1);
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    if (!xl6IrCgenEmitC(&out, &ir, &parse.diags)) goto failed;
    xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_INDEXED_ACCESSOR", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define INDEXED_ACCESSOR_COUNT %llu\n", (unsigned long long)getters); xl6OutputClose(&out);
    printf("indexed accessor: %llu getters, %llu protocol / %llu class, %llu separate container reads, %llu rejected mutations\n",
        (unsigned long long)getters, (unsigned long long)protocols, (unsigned long long)classes,
        (unsigned long long)containerReads, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    return 1;
}
