#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    const char* fixtures[] = {"test/cases/xlir_module_executor.xl", "test/cases/xlir_module_ownership.xl"};
    assert(argc == 6);
    for (unsigned n = 0; n < 2; ++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        XL6_IrCgenOptions options = {0}; options.emitScriptEntry = 1;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        if (!xl6ParseFile(fixtures[n], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
            !xl6SemCheckModule(parse.root, &parse.diags, "module_executor") ||
            !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags)) {
            xl6DiagPrintText(&parse.diags, fixtures[n]); return 1;
        }
        assert(ir.abi.executorOwnershipAbi == XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE);
        ir.abi.executorOwnershipAbi = XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1;
        reject(&ir, "module executor ownership ABI");
        ir.abi.executorOwnershipAbi = (XL6_IrModuleExecutorOwnershipAbi)99;
        reject(&ir, "module executor ownership ABI");
        ir.abi.executorOwnershipAbi = XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE;
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3;
        reject(&ir, "module retirement ABI"); ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_NONE;
        if (!xl6IrLowerModule(&ir, &parse.diags)) { xl6DiagPrintText(&parse.diags, fixtures[n]); return 1; }
        verify(&ir);
        XL6_IrModuleExecutorOwnershipAbi saved = ir.abi.executorOwnershipAbi;
        XL6_IrModuleRetirementAbi retirement = ir.abi.retirementAbi;
        assert(saved == (n ? XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE : XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1));
        assert(retirement == (n ? XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2 : XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3));
        ir.abi.executorOwnershipAbi = n ? XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1 : XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE;
        reject(&ir, "module executor ownership ABI");
        ir.abi.executorOwnershipAbi = (XL6_IrModuleExecutorOwnershipAbi)99;
        reject(&ir, "module executor ownership ABI"); ir.abi.executorOwnershipAbi = saved;
        const XL6_IrModuleRetirementAbi versions[] = {XL6_IR_MODULE_RETIREMENT_NONE, XL6_IR_MODULE_RETIREMENT_CHECKED_GRAPH_V1,
            XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2, XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3, (XL6_IrModuleRetirementAbi)99};
        for (unsigned i = 0; i < 5; ++i) if (versions[i] != retirement) {
            ir.abi.retirementAbi = versions[i]; reject(&ir, "module retirement ABI");
        }
        ir.abi.retirementAbi = retirement;
        if (!n) {
            XL6_IrModuleContinuationAbi execution = ir.abi.continuationAbi;
            ir.abi.continuationAbi = XL6_IR_MODULE_CONTINUATION_NONE;
            ir.abi.executorOwnershipAbi = XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE;
            ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2;
            reject(&ir, "module executor ownership ABI");
            ir.abi.continuationAbi = execution; ir.abi.executorOwnershipAbi = saved; ir.abi.retirementAbi = retirement;
        }
        assert(ir.exportCount && ir.callTargetCount);
        char* name = ir.exports[0].cName; ir.exports[0].cName = "xl_script_module_retirement_v3";
        reject(&ir, NULL); ir.exports[0].cName = name;
        name = ir.callTargets[0].cName; ir.callTargets[0].cName = "xl_script_module_retirement_v3";
        reject(&ir, NULL); ir.callTargets[0].cName = name; verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options));
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text);
        if (!n) assert(strstr(text, "xl_script_module_retirement_v3(void)") && !strstr(text, "xl_script_module_retirement_v2(void)") &&
            strstr(text, "visit(xl6ContinuationExecutorOwnership(xlir_continuation_executor), context)") &&
            strstr(text, "if (teardown && !xlir_continuation_quiesce())") && !strstr(text, "xlir_continuation_executor != NULL) return false"));
        else assert(strstr(text, "xl_script_module_retirement_v2(void)") && !strstr(text, "xl_script_module_retirement_v3(void)"));
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text && strstr(text, n ? "module-executor-ownership: 0" : "module-executor-ownership: 1")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL); assert(text && strstr(text, n ? "\"moduleExecutorOwnershipAbi\": 0" : "\"moduleExecutorOwnershipAbi\": 1")); xrtFree(text);
        if (!n) {
            assert(ir.asyncMachineCount == 1 && ir.asyncMachines[0].suspensionCount >= 3);
            assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options)); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "MODULE_EXECUTOR_GENERATED_H", &parse.diags)); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out, argv[3]));
            xl6OutputPrintf(&out, "#define MODULE_EXECUTOR_START xlir_async_start_%u\n", ir.asyncMachines[0].targetSymbol); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out, argv[4])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out, argv[5])); xl6IrDumpModuleText(&out, &ir); xl6OutputClose(&out);
        }
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations == 23);
    puts("Module executor IR: 23 C/header-gated mutations, typed/lowered ownership, nonexecutor V2, owned-service V3, reserved linkage, text/JSON and real module trace");
    return 0;
}
