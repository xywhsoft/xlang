/* Capture graph emission must consume verified capture facts; malformed
 * managed/trivial and cell identities must fail before C or headers escape. */
#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i, phase, captures = 0;
    if (argc == 5) return generate(argc, argv);
    assert(argc == 1);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_capture_ownership.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "capture_ownership"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    for (phase = 0; phase < 2; ++phase) {
        if (phase) assert(xl6IrLowerModule(&ir, &parse.diags));
        assert(xl6IrVerifyModule(&ir, &parse.diags));
        for (i = 0; i < ir.closureCaptureCount; ++i) {
            XL6_IrClosureCapture* capture = &ir.closureCaptures[i];
            XL6_IrClosureCaptureAbi saved = capture->abi;
            capture->abi = (XL6_IrClosureCaptureAbi)999;
            reject(&ir, "unknown capture graph layout", 0);
            capture->abi = saved;
            if (saved == XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE ||
                saved == XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE ||
                saved == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL) {
                capture->abi = XL6_IR_CLOSURE_CAPTURE_COPY_VALUE;
                reject(&ir, "erase real owning edge as trivial", 0);
                capture->abi = saved;
            }
            if (saved == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL) {
                XL6_IrTypeId cell = capture->cellType;
                capture->cellType = capture->type;
                reject(&ir, "replace shared cell identity with payload", 0);
                capture->cellType = cell;
            }
            ++captures;
        }
        assert(xl6IrVerifyModule(&ir, &parse.diags));
    }
    assert(captures && rejected);
    printf("capture ownership: %zu typed/lowered captures, %zu mutations rejected before C/header emission\n", captures, rejected);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
