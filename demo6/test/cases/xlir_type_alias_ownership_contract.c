/* Aliases must not erase the owned temporary used by canonical iteration. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/cgen/xl6_ir_cgen_native_header.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations;
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned view = 0; view != 3; ++view) {
        XL6_Output out; size_t size = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        int emitted = view == 0 ? xl6IrCgenEmitC(&out, ir, NULL) :
            view == 1 ? xl6IrCgenEmitHeader(&out, ir, "BAD_ALIAS_OWNER", NULL) :
            xl6IrCgenEmitNativeContractHeader(&out, ir, "BAD_ALIAS_OWNER", NULL);
        assert(!emitted);
        char* text = xl6OutputTakeMemoryText(&out, &size);
        assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    size_t loops = 0, calls = 0, causes = 0;
    assert(argc == 2); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "type_alias_ownership") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (size_t i = 0; i < ir.nodeCount; ++i) {
        const XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, node->storageType);
        if (node->kind == XL6_IR_NODE_CALL && type && type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED) {
            assert(node->ownership == XL6_IR_OWNERSHIP_MOVE || node->ownership == XL6_IR_OWNERSHIP_SHARED);
            ++calls;
        }
        if (node->kind == XL6_IR_NODE_MEMBER && node->text && !strcmp(node->text, "cause")) {
            assert(type && type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR && node->ownership == XL6_IR_OWNERSHIP_MOVE);
            ++causes;
        }
        if (node->kind == XL6_IR_NODE_LOOP) {
            const XL6_IrNode* source = xl6IrNodeGet(&ir, ir.children[node->firstChild+1]);
            assert(node->loopAbi != XL6_IR_LOOP_ABI_NONE && source && source->ownership == XL6_IR_OWNERSHIP_MOVE);
            assert(xl6IrLoopSourceAcquisition(&ir, source) == XL6_IR_LOOP_SOURCE_OWNED_TEMPORARY);
            ++loops;
        }
    }
    assert(loops == 6 && calls >= 15 && causes == 1);
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (size_t i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* loop = &ir.nodes[i];
        if (loop->kind != XL6_IR_NODE_LOOP) continue;
        XL6_IrNode* source = &ir.nodes[ir.children[loop->firstChild+1]-1];
        XL6_IrNode savedSource = *source, savedLoop = *loop;
        source->ownership = XL6_IR_OWNERSHIP_COPY; reject(&ir); *source = savedSource;
        source->storageType = XL6_IR_TYPE_INVALID; reject(&ir); *source = savedSource;
        loop->loopAbi = XL6_IR_LOOP_ABI_NONE; reject(&ir); *loop = savedLoop;
        loop->loopItemAbi = XL6_IR_LOOP_ITEM_NONE; reject(&ir); *loop = savedLoop;
        loop->loopResourceType = XL6_IR_TYPE_INVALID; reject(&ir); *loop = savedLoop;
    }
    assert(xl6IrVerifyModule(&ir, NULL) && mutations == 30);
    printf("type alias ownership: %zu owned loops, %zu managed calls, %zu owned cause; %zu mutations refused before C/public/native output\n",
        loops, calls, causes, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
