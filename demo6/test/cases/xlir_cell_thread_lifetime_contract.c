#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId factory = 0; size_t i, cells = 0, captures = 0; char binding[1024];
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_cell_thread_lifetime.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "cell_thread_lifetime") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.functionCount; ++i) {
        XL6_IrFunction* function = &ir.functions[i];
        const XL6_IrNode* decl = xl6IrNodeGet(&ir, function->declaration);
        const XL6_IrNode* name = decl && decl->childCount ? xl6IrNodeGet(&ir, ir.children[decl->firstChild]) : NULL;
        if (name && name->text && !strcmp(name->text, "cellThreadFactory")) { assert(!factory); factory = function->symbol; }
    }
    assert(factory);
    for (i = 0; i < ir.closureCaptureCount; ++i) {
        XL6_IrClosureCapture* capture = &ir.closureCaptures[i], saved = *capture;
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, capture->type);
        assert(capture->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL && capture->cellType);
        capture->abi = type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
            ? XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE : XL6_IR_CLOSURE_CAPTURE_COPY_VALUE;
        capture->cellType = XL6_IR_TYPE_INVALID;
        reject(&ir, "captures of one source binding must share the same cell identity"); *capture = saved;
        ++captures;
    }
    for (i = 0; i < ir.instructionCount; ++i) if (ir.instructions[i].kind == XL6_IR_INSTR_CELL_NEW) {
        XL6_IrInstruction* test = &ir.instructions[i + 1], saved = *test;
        assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
        test->trueTarget = saved.falseTarget; test->falseTarget = saved.trueTarget; reject(&ir, NULL); *test = saved; ++cells;
    }
    assert(captures == 6 && cells == 3); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CELL_THREAD_LIFETIME", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0 && xl6OutputOpenFile(&out, binding));
    xl6OutputPrintf(&out, "#define CELL_THREAD_FACTORY xlir_fn_%u\n", factory); xl6OutputClose(&out);
    printf("cell thread lifetime: %llu shared captures, %llu source cells, %llu C/header-gated mutations\n",
        (unsigned long long)captures, (unsigned long long)cells, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
