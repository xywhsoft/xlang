#include "xlir_continuation_owner_test.h"
int main(void)
{
    static const char* fixtures[] = {
        "test/cases/xlir_module_ownership.xl", "test/cases/xlir_object_escape.xl",
        "test/cases/xlir_frame_ownership.xl", "test/cases/cgen_generator.xl"
    };
    size_t n, globals = 0, nonGlobals = 0; unsigned kinds = 0;
    for (n = 0; n < sizeof(fixtures) / sizeof(fixtures[0]); ++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n], &parse) && xl6StdlibExpandModule(parse.root, &parse.diags) &&
            xl6SemCheckModule(parse.root, &parse.diags, "module_ownership") &&
            xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
        assert(ir.abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_NONE);
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_CHECKED_GRAPH_V1;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = (XL6_IrModuleRetirementAbi)99;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_NONE;
        assert(xl6IrLowerModule(&ir, &parse.diags)); verify(&ir);
        /* The existing frame fixture owns an await executor. Preserve every
         * original global/slot mutation while testing its new exact contract. */
        const bool executor = n == 2;
        const XL6_IrModuleRetirementAbi retirement = executor ? XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3 : XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2;
        assert(ir.abi.retirementAbi == retirement);
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_NONE;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = (XL6_IrModuleRetirementAbi)99;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = XL6_IR_MODULE_RETIREMENT_CHECKED_GRAPH_V1;
        reject(&ir, "module retirement ABI");
        ir.abi.retirementAbi = executor ? XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2 : XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3;
        reject(&ir, "module retirement ABI"); ir.abi.retirementAbi = retirement;
        for (size_t i = 0; i < ir.storageCount; ++i) {
            XL6_IrStorage* slot = &ir.storages[i];
            XL6_IrContinuationFrameTraceAbi saved = slot->moduleTraceAbi;
            if (slot->kind == XL6_IR_STORAGE_GLOBAL) {
                ++globals; kinds |= 1u << saved;
                assert(saved >= XL6_IR_FRAME_TRACE_NONE && saved <= XL6_IR_FRAME_TRACE_GENERATOR);
            } else { ++nonGlobals; assert(saved == XL6_IR_FRAME_TRACE_INVALID); }
            slot->moduleTraceAbi = saved == XL6_IR_FRAME_TRACE_NONE ? XL6_IR_FRAME_TRACE_XVALUE : XL6_IR_FRAME_TRACE_NONE;
            reject(&ir, "module storage ownership trace ABI");
            slot->moduleTraceAbi = (XL6_IrContinuationFrameTraceAbi)99;
            reject(&ir, "module storage ownership trace ABI");
            slot->moduleTraceAbi = saved;
        }
        if (ir.exportCount) {
            char* saved = ir.exports[0].cName;
            ir.exports[0].cName = "xl_script_module_ownership_trace";
            reject(&ir, NULL); ir.exports[0].cName = saved;
            ir.exports[0].cName = "xl_script_module_retirement_v1";
            reject(&ir, NULL); ir.exports[0].cName = saved;
            ir.exports[0].cName = "xl_script_module_retirement_v2";
            reject(&ir, NULL); ir.exports[0].cName = saved;
            ir.exports[0].cName = "xl_script_module_retirement_v3";
            reject(&ir, NULL); ir.exports[0].cName = saved;
        }
        verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out, &ir, &parse.diags));
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, "xl_script_module_ownership_trace") &&
            strstr(text, "xlir_code_owner_graph.ops = ops")); xrtFree(text);
        {
            XL6_IrCgenOptions options = {0}; options.emitScriptEntry = 1;
            assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options));
            text = xl6OutputTakeMemoryText(&out, NULL);
            assert(text && strstr(text, "xl_script_module_retirement_v1(void)") &&
                strstr(text, "0x584C364D525431ULL") && strstr(text, executor ? "xl_script_module_retirement_v3(void)" : "xl_script_module_retirement_v2(void)") &&
                strstr(text, executor ? "0x584C364D525433ULL" : "0x584C364D525432ULL") && strstr(text, "xlir_module_prepare_resources")); xrtFree(text);
        }
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, "module-trace=") && strstr(text, executor ? "module-retirement: 3" : "module-retirement: 2")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
        text = xl6OutputTakeMemoryText(&out, NULL);
        assert(text && strstr(text, executor ? "\"retirementAbi\": 3" : "\"retirementAbi\": 2")); xrtFree(text);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(globals && nonGlobals && (kinds & (1u << XL6_IR_FRAME_TRACE_XVALUE)) &&
        (kinds & (1u << XL6_IR_FRAME_TRACE_CLOSURE)));
    printf("Module ownership IR: %zu globals, %zu other slots, %zu C/header-gated mutations, kinds=%u\n",
        globals, nonGlobals, mutations, kinds);
    return 0;
}
