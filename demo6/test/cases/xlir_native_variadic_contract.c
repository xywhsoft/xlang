/* Native ... values must retain an exact C entry and its admission profile.
 * Corrupted bindings must fail before either C or a public header escapes. */
#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main

int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_c_variadic_value.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "native_variadic"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags));
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    size_t native = SIZE_MAX, other = SIZE_MAX, call = SIZE_MAX;
    for (size_t i = 0; i < ir.closureTargetCount; ++i) {
        if (ir.closureTargets[i].kind != XL6_IR_CLOSURE_TARGET_C_VARIADIC_NATIVE) continue;
        if (native == SIZE_MAX) native = i;
        else if (ir.closureTargets[i].signatureType != ir.closureTargets[native].signatureType) other = i;
    }
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        const XL6_IrType* signature = xl6IrTypeGet(&ir.types, ir.instructions[i].calleeType);
        if (ir.instructions[i].kind == XL6_IR_INSTR_CLOSURE_CALL && signature &&
            signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_C_VARIADIC) { call = i; break; }
    }
    assert(native != SIZE_MAX && other != SIZE_MAX && call != SIZE_MAX);
    XL6_IrClosureTarget* closure = &ir.closureTargets[native];
    XL6_IrTypeId signature = closure->signatureType;
    closure->signatureType = ir.closureTargets[other].signatureType;
    reject(&ir, "native entry signature changed", 0);
    closure->signatureType = signature;
    XL6_SymbolId entry = closure->adaptedTargetSymbol;
    closure->adaptedTargetSymbol = ir.closureTargets[other].adaptedTargetSymbol;
    reject(&ir, "native entry origin changed", 0);
    closure->adaptedTargetSymbol = entry;
    ++closure->callParameterCount;
    reject(&ir, "native profile offset changed", 0);
    --closure->callParameterCount;
    closure->captureCount = 1;
    reject(&ir, "native entry acquired an unverified environment", 0);
    closure->captureCount = 0;
    closure->rawCallbackAbi = XL6_IR_CLOSURE_RAW_CALLBACK_DIRECT_THUNK;
    reject(&ir, "native variadic entry erased as a fixed C callback", 0);
    closure->rawCallbackAbi = XL6_IR_CLOSURE_RAW_CALLBACK_NONE;
    size_t operands = ir.instructions[call].operandCount;
    ir.instructions[call].operandCount = 0;
    reject(&ir, "native call lost its callable operand", 0);
    ir.instructions[call].operandCount = operands;
    assert(rejected == 6 && xl6IrVerifyModule(&ir, &parse.diags));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    puts("native variadic ABI: 6 mutations rejected before C/header emission");
    return 0;
}
