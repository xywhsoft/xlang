#include "xlir_continuation_owner_test.h"
#include "src/compile/xl6_artifact_runtime.h"
int main(void)
{
    const char* fixtures[]={"stdlib_net_fast_path","stdlib_net_send_file",
        "xlir_net_resolver_standalone","xlir_net_datagram_boundaries","xlir_net_buffer_command_lifetime"};
    for(size_t n=0;n<sizeof(fixtures)/sizeof(*fixtures);++n){
        char path[180];XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char* text;
        snprintf(path,sizeof(path),"test/cases/%s.xl",fixtures[n]);xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
        assert(xl6ParseFile(path,&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"network_service") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        assert(ir.abi.networkServiceAbi==XL6_IR_MODULE_NETWORK_SERVICE_NONE);
        ir.abi.networkServiceAbi=XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1;
        reject(&ir,"module network service ABI");ir.abi.networkServiceAbi=XL6_IR_MODULE_NETWORK_SERVICE_NONE;
        ir.abi.retirementAbi=XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4;
        reject(&ir,"module retirement ABI");ir.abi.retirementAbi=XL6_IR_MODULE_RETIREMENT_NONE;
        assert(xl6IrLowerModule(&ir,&parse.diags));verify(&ir);
        assert(ir.abi.networkServiceAbi==XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1 &&
            ir.abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4);
        for(int invalid=-1;invalid<=2;++invalid){
            if(invalid==XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1)continue;
            ir.abi.networkServiceAbi=(XL6_IrModuleNetworkServiceAbi)invalid;reject(&ir,"module network service ABI");
            XL6_ArtifactRuntimePlan plan={0};XL6_DiagList diags={0};
            assert(!xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&diags) && !plan.sourceCount);
            xl6DiagFree(&diags);
        }
        ir.abi.networkServiceAbi=XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1;
        for(int invalid=0;invalid<XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4;++invalid){
            ir.abi.retirementAbi=(XL6_IrModuleRetirementAbi)invalid;reject(&ir,"module retirement ABI");
        }
        ir.abi.retirementAbi=XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4;verify(&ir);
        XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NETWORK_SERVICE));
        XL6_IrCgenOptions options={0};options.emitScriptEntry=1;
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitCEx(&out,&ir,&parse.diags,&options));
        text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"xl_script_module_retirement_v4(void)") && strstr(text,"xl_script_module_retire_step(void)") &&
            strstr(text,"xl6NetworkServiceRuntimeV2Get()->Trace(") &&
            strstr(text,"&xlir_network_services, visit, context") &&
            !strstr(text,"xl_script_module_retirement_v3(void)") && !strstr(text,"xl6_net_runtime_unit("));xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleText(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"module-network-service: 1\n"));xrtFree(text);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleJson(&out,&ir);text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"moduleNetworkServiceAbi\": 1"));xrtFree(text);
        xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    }
    XL6_IrModule probe={0};XL6_IrCallTarget target={0};probe.callTargets=&target;probe.callTargetCount=1;
    target.cName="arbitrary-source-name";target.runtimeBridge=XL6_IR_RUNTIME_BRIDGE_NONE;
    target.reachability=XL6_IR_REACHABILITY_TRANSITIVE;
    assert(xl6IrModuleNetworkServiceRequirement(&probe)==XL6_IR_MODULE_NETWORK_SERVICE_NONE);
    target.runtimeBridge=XL6_IR_RUNTIME_BRIDGE_NET_TCP_DIAL;
    assert(xl6IrModuleNetworkServiceRequirement(&probe)==XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1);
    target.reachability=XL6_IR_REACHABILITY_UNREACHABLE;
    assert(xl6IrModuleNetworkServiceRequirement(&probe)==XL6_IR_MODULE_NETWORK_SERVICE_NONE);
    assert(mutations==45);
    puts("Network service IR: 5 legacy modules, 45 IR/C/header-gated mutations, exact resident dependency and typed retirement contract");
    return 0;
}
