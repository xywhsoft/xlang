#include "xlir_continuation_owner_test.h"
int main(void)
{
    static const char* fixtures[] = {
        "test/cases/xlir_async_resume_isolation.xl",
        "test/cases/cgen_native_class.xl",
        "test/cases/cgen_generator.xl",
        "test/cases/xlir_string_repeat_abi.xl",
        "test/cases/xlir_foreign_prepare_ownership.xl"
    };
    size_t fixture, i, targets = 0, foreign = 0, factories = 0, noRaiseForeign = 0;
    unsigned kinds = 0;
    for (fixture = 0; fixture < sizeof(fixtures)/sizeof(fixtures[0]); ++fixture) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* json;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        if (!xl6ParseFile(fixtures[fixture], &parse) ||
            !xl6StdlibExpandModule(parse.root, &parse.diags) ||
            !xl6SemCheckModule(parse.root, &parse.diags, "execution_boundary") ||
            !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
            !xl6IrLowerModule(&ir, &parse.diags)) {
            for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
            return 2;
        }
        verify(&ir);
        for (i = 0; i < ir.callTargetCount; ++i) {
            XL6_IrCallTarget* target = &ir.callTargets[i];
            XL6_IrCallExecutionBoundaryAbi saved = target->executionBoundaryAbi;
            XL6_IrCallPreparationFailureAbi prep = target->preparationFailureAbi;
            XL6_IrErrorAbi error = target->errorAbi;
            kinds |= 1u << target->kind; ++targets;
            if (target->kind == XL6_IR_CALL_TARGET_EXTERN_C || target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) {
                assert(saved == XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK); ++foreign;
                if (target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) ++factories;
                if (error == XL6_IR_ERROR_ABI_NO_RAISE) ++noRaiseForeign;
            } else assert(saved == XL6_IR_CALL_EXECUTION_LANGUAGE_CFG);
            target->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_INVALID;
            reject(&ir, "execution boundary");
            target->executionBoundaryAbi = (XL6_IrCallExecutionBoundaryAbi)99;
            reject(&ir, "execution boundary");
            target->executionBoundaryAbi = saved == XL6_IR_CALL_EXECUTION_LANGUAGE_CFG ?
                XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK : XL6_IR_CALL_EXECUTION_LANGUAGE_CFG;
            reject(&ir, "execution boundary");
            target->executionBoundaryAbi = saved; assert(target->errorAbi == error);
            assert(prep == (target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
                ? XL6_IR_CALL_PREPARATION_RETIRE_OWNED_REVERSE : XL6_IR_CALL_PREPARATION_NONE));
            target->preparationFailureAbi = XL6_IR_CALL_PREPARATION_INVALID;
            reject(&ir, "call preparation failure");
            target->preparationFailureAbi = (XL6_IrCallPreparationFailureAbi)99;
            reject(&ir, "call preparation failure");
            target->preparationFailureAbi = prep == XL6_IR_CALL_PREPARATION_NONE ?
                XL6_IR_CALL_PREPARATION_RETIRE_OWNED_REVERSE : XL6_IR_CALL_PREPARATION_NONE;
            reject(&ir, "call preparation failure"); target->preparationFailureAbi = prep;
        }
        verify(&ir);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
        json = xl6OutputTakeMemoryText(&out, NULL);
        assert(json && strstr(json, "executionBoundaryAbiName") && strstr(json, "preparationFailureAbiName")); xrtFree(json);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(kinds == ((1u << XL6_IR_CALL_TARGET_INTERNAL) | (1u << XL6_IR_CALL_TARGET_EXTERN_C) |
        (1u << XL6_IR_CALL_TARGET_NATIVE_FACTORY) | (1u << XL6_IR_CALL_TARGET_INTERNAL_ASYNC) |
        (1u << XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)));
    assert(foreign && factories && noRaiseForeign);
    printf("call execution boundary: %llu targets, %llu foreign, %llu native factories, %llu no-raise foreign, %llu C/header-gated mutations\n",
        (unsigned long long)targets, (unsigned long long)foreign, (unsigned long long)factories,
        (unsigned long long)noRaiseForeign, (unsigned long long)mutations);
    return 0;
}
