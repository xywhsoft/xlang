/* Reuse the standard source generator; build once for contract and generation. */
#include "src/cgen/xl6_ir_cgen.h"
static int scriptArtifact;
static int emitArtifact(XL6_Output* out, const XL6_IrModule* ir, XL6_DiagList* diags)
{
    XL6_IrCgenOptions options = {0}; options.emitScriptEntry = scriptArtifact;
    return xl6IrCgenEmitCEx(out, ir, diags, &options);
}
#define xl6IrCgenEmitC emitArtifact
#define main xlir_owner_driver_main
#include "xlir_owner_consumption_contract.c"
#undef main
#undef xl6IrCgenEmitC
static int dynamic_result_contract(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t phase, i, first = 0, discard = 0, before = rejected;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_xcall_expression.xl", &parse) &&
        xl6StdlibExpandModule(parse.root, &parse.diags) &&
        xl6SemCheckModule(parse.root, &parse.diags, "dynamic_results") &&
        xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    for (phase = 0; phase < 2; ++phase) {
        if (phase) assert(xl6IrLowerModule(&ir, &parse.diags));
        assert(xl6IrVerifyModule(&ir, &parse.diags));
        for (i = 0; i < ir.dynamicCallSiteCount; ++i) {
            XL6_IrDynamicCallSite* site = &ir.dynamicCallSites[i];
            XL6_IrDynamicCallResultAbi saved = site->resultAbi;
            if (site->kind != XL6_IR_DYNAMIC_CALL_FUNCTION) continue;
            if (saved == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE) ++first;
            else { assert(saved == XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT); ++discard; }
            site->resultAbi = saved == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE
                ? XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT : XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE;
            reject(&ir, "dynamic call result demand changed", 0);
            if (phase) {
                size_t j;
                for (j = 0; j < ir.instructionCount; ++j) {
                    XL6_IrInstruction* instruction = &ir.instructions[j];
                    if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL || instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL || instruction->dynamicCallSite != i) continue;
                    instruction->dynamicCallResultAbi = site->resultAbi;
                    reject(&ir, "site and instruction conspired to change result demand", 0);
                    instruction->dynamicCallResultAbi = saved;
                }
            }
            site->resultAbi = (XL6_IrDynamicCallResultAbi)99;
            reject(&ir, "unknown dynamic result demand", 0);
            site->resultAbi = saved;
        }
    }
    assert(first && discard && xl6IrVerifyModule(&ir, &parse.diags));
    printf("dynamic result ABI: first=%lu discard=%lu (typed+lowered), %lu rejected mutations\n",
        (unsigned long)first, (unsigned long)discard, (unsigned long)(rejected-before));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
int main(int argc, char** argv)
{
    static const char* fixtures[] = { "test/cases/xlir_code_owner_binding.xl",
        "test/cases/xlir_async_resume_isolation.xl", "test/cases/cgen_generator.xl",
        "test/cases/xlir_async_dispatch.xl" };
    size_t n, executionMutations = 0, awaitMutations = 0;
    if (argc == 6 && !strcmp(argv[5], "--script")) { scriptArtifact = 1; return generate(5, argv); }
    if (argc == 5) return generate(argc, argv);
    if (argc == 2 && !strcmp(argv[1], "--owners")) return xlir_owner_driver_main(1, argv);
    if (argc == 2 && !strcmp(argv[1], "--dynamic-results")) return dynamic_result_contract();
    assert(argc == 1);
    for (n = 0; n < sizeof(fixtures)/sizeof(fixtures[0]); ++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n], &parse) && xl6StdlibExpandModule(parse.root, &parse.diags) &&
            xl6SemCheckModule(parse.root, &parse.diags, "code_owner_binding") &&
            xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
        assert(ir.abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_NONE);
        ir.abi.codeOwnerAbi = XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT;
        reject(&ir, "premature typed code owner binding", 0);
        ir.abi.codeOwnerAbi = XL6_IR_MODULE_CODE_OWNER_NONE;
        assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
        for (size_t i = 0; i < ir.instructionCount; ++i) {
            XL6_IrInstruction* instruction = &ir.instructions[i];
            if (instruction->kind != XL6_IR_INSTR_AWAIT) continue;
            assert(instruction->awaitOutcomeAbi ==
                XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR);
            instruction->awaitOutcomeAbi = XL6_IR_AWAIT_OUTCOME_INVALID;
            reject(&ir, "missing await status carrier", 0);
            instruction->awaitOutcomeAbi = (XL6_IrAwaitOutcomeAbi)99;
            reject(&ir, "invalid await status carrier", 0);
            instruction->awaitOutcomeAbi =
                XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR;
            awaitMutations += 2;
        }
        assert(ir.instructions[0].kind != XL6_IR_INSTR_AWAIT);
        ir.instructions[0].awaitOutcomeAbi =
            XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR;
        reject(&ir, "unrelated instruction await status", 0); ++awaitMutations;
        ir.instructions[0].awaitOutcomeAbi = XL6_IR_AWAIT_OUTCOME_INVALID;
        assert(ir.abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT);
        ir.abi.codeOwnerAbi = XL6_IR_MODULE_CODE_OWNER_NONE;
        reject(&ir, "missing lowered code owner binding", 0);
        ir.abi.codeOwnerAbi = (XL6_IrModuleCodeOwnerAbi)99;
        reject(&ir, "invalid code owner binding", 0);
        ir.abi.codeOwnerAbi = XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT;
        assert(xl6IrVerifyModule(&ir, &parse.diags));
        if (ir.asyncMachineCount != 0) {
            XL6_IrModuleContinuationAbi saved = ir.abi.continuationAbi;
            XL6_IrAsyncMachine* machine = &ir.asyncMachines[0];
            assert(saved == XL6_IR_MODULE_CONTINUATION_GUARANTEED_WORKERS);
            assert(machine->executionAbi == XL6_IR_ASYNC_EXECUTION_GUARANTEED_RESUME);
            machine->executionAbi = XL6_IR_ASYNC_EXECUTION_INVALID;
            reject(&ir, "missing resume execution ABI", 0);
            machine->executionAbi = (XL6_IrAsyncExecutionAbi)99;
            reject(&ir, "invalid resume execution ABI", 0);
            machine->executionAbi = XL6_IR_ASYNC_EXECUTION_GUARANTEED_RESUME;
            ir.abi.continuationAbi = XL6_IR_MODULE_CONTINUATION_NONE;
            reject(&ir, "missing module execution service", 0);
            ir.abi.continuationAbi = (XL6_IrModuleContinuationAbi)99;
            reject(&ir, "invalid module execution service", 0);
            ir.abi.continuationAbi = saved; executionMutations += 4;
        }
        if (n == 0) {
            char* saved = ir.exports[0].cName;
            ir.exports[0].cName = "xl_script_bind_code_owner";
            reject(&ir, "reserved binding export", 0); ir.exports[0].cName = saved;
        }
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, "module-code-owner: bind-once-before-init")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, "\"moduleCodeOwnerAbi\": \"bind-once-before-init\"")); xrtFree(text);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(rejected == 13 + executionMutations + awaitMutations);
    for (n = 0; n < 2; ++n) {
        XL6_ParseResult parse; XL6_IrModule ir; size_t i; int found = 0;
        const char* path = n ? "test/cases/xlir_code_owner_reserved_import.xl" :
            "test/cases/xlir_code_owner_reserved_export.xl";
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(path, &parse) && xl6SemCheckModule(parse.root, &parse.diags, "reserved") &&
            xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
        assert(!xl6IrLowerModule(&ir, &parse.diags));
        for (i = 0; i < parse.diags.count; ++i)
            if (strstr(parse.diags.items[i].message, "code owner binding symbol is reserved")) found = 1;
        assert(found); reject(&ir, "reserved source linkage", 0);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(rejected == 15 + executionMutations + awaitMutations && executionMutations == 8 && awaitMutations >= 12);
    printf("code-owner/execution IR: 21 + %zu await mutations + 2 source linkages rejected, C/header preflight and text/JSON\n", awaitMutations);
    return 0;
}
