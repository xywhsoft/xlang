#include "xlir_continuation_owner_test.h"
int main(void)
{
    const char* fixtures[] = {"test/cases/cgen_optional_nullable_function_default.xl",
        "test/cases/cgen_typed_container_functions.xl", "test/cases/xlir_frame_ownership.xl",
        "test/cases/xlir_object_escape.xl"};
    for (size_t n = 0; n < 4; ++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n], &parse) && xl6StdlibExpandModule(parse.root, &parse.diags) &&
            xl6SemCheckModule(parse.root, &parse.diags, "closure_storage") &&
            xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
        verify(&ir);
        assert(ir.abi.closureStorageAbi == XL6_IR_CLOSURE_STORAGE_NONE);
        ir.abi.closureStorageAbi = XL6_IR_CLOSURE_STORAGE_RESIDENT_SCOPED_LEASE_V1;
        reject(&ir, "closure storage/code ownership ABI");
        ir.abi.closureStorageAbi = XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2;
        reject(&ir, "closure storage/code ownership ABI");
        ir.abi.closureStorageAbi = (XL6_IrClosureStorageAbi)99;
        reject(&ir, "closure storage/code ownership ABI");
        ir.abi.closureStorageAbi = XL6_IR_CLOSURE_STORAGE_NONE;
        assert(xl6IrLowerModule(&ir, &parse.diags)); verify(&ir);
        assert(ir.abi.closureStorageAbi == XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2);
        ir.abi.closureStorageAbi = XL6_IR_CLOSURE_STORAGE_NONE;
        reject(&ir, "closure storage/code ownership ABI");
        ir.abi.closureStorageAbi = (XL6_IrClosureStorageAbi)99;
        reject(&ir, "closure storage/code ownership ABI");
        ir.abi.closureStorageAbi = XL6_IR_CLOSURE_STORAGE_RESIDENT_SCOPED_LEASE_V1;
        reject(&ir, "closure storage/code ownership ABI");
        ir.abi.closureStorageAbi = XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2;
        verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &parse.diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, "__xl_runtime/xl6_closure.h") && strstr(text, "xl6ClosureRuntimeV2Get()->Create") &&
            !strstr(text, "xlir_closure_env_trace_") && !strstr(text, "xlir_closure_env_free_") &&
            !strstr(text, "static void xlir_closure_release(") && !strstr(text, "struct xlir_closure {"));
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text && strstr(text, "closure-storage: 2")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text && strstr(text, "\"closureStorageAbi\": 2")); xrtFree(text);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations == 24);
    puts("Resident closure IR: 4 source fixtures, 24 rejected C/header-gated mutations, text/JSON and resident-only control storage");
    return 0;
}
