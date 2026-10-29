#include "xlir_continuation_owner_test.h"
#include "src/compile/xl6_artifact_runtime.h"

/* Network services are explicit managed values. Their code/root retirement is
 * the ordinary execution contract, never a Cgen-created module singleton. */
int main(void)
{
    const struct { const char* name; XL6_ArtifactRuntimeId component; } fixtures[] = {
        {"stdlib_tcp_managed_async", XL6_ARTIFACT_TCP_LANGUAGE},
        {"stdlib_udp_async", XL6_ARTIFACT_UDP_FUTURE},
        {"stdlib_net_resolver_numeric", XL6_ARTIFACT_NET_RESOLVER_LANGUAGE},
        {"stdlib_net_bytes_value", XL6_ARTIFACT_NET_BYTES},
        {"stdlib_net_engine", XL6_ARTIFACT_NET_ENGINE_LANGUAGE}
    };
    for (size_t n=0; n<sizeof(fixtures)/sizeof(*fixtures); ++n) {
        char path[180]; XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        snprintf(path,sizeof(path),"test/cases/%s.xl",fixtures[n].name);
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        int ok=xl6ParseFile(path,&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"network_service") &&
            xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags);
        if (!ok) xl6DiagPrintText(&parse.diags,path);
        assert(ok); verify(&ir);
        assert(ir.abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_NONE);
        ir.abi.retirementAbi=(XL6_IrModuleRetirementAbi)-1;
        reject(&ir,"module retirement ABI");
        ir.abi.retirementAbi=XL6_IR_MODULE_RETIREMENT_NONE;
        assert(xl6IrLowerModule(&ir,&parse.diags)); verify(&ir);
        XL6_IrModuleRetirementAbi retirement=xl6IrModuleRetirementRequirement(&ir);
        assert(ir.abi.retirementAbi==retirement && retirement!=XL6_IR_MODULE_RETIREMENT_NONE);
        for (int invalid=-1; invalid<=5; ++invalid) {
            if (invalid==(int)retirement) continue;
            ir.abi.retirementAbi=(XL6_IrModuleRetirementAbi)invalid;
            reject(&ir,"module retirement ABI");
            XL6_ArtifactRuntimePlan plan={0}; XL6_DiagList diags={0};
            assert(!xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&diags));
            assert(!plan.sourceCount && xl6ArtifactRuntimeSetEmpty(&plan.components));
            xl6DiagFree(&diags);
        }
        ir.abi.retirementAbi=retirement; verify(&ir);
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,fixtures[n].component));
        XL6_IrCgenOptions options={0}; options.emitScriptEntry=1;
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitCEx(&out,&ir,&parse.diags,&options));
        text=xl6OutputTakeMemoryText(&out,NULL);
        const char* entry=retirement==XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4
            ? "xl_script_module_retirement_v4(void)" : "xl_script_module_retirement_v2(void)";
        if (!text || !strstr(text,entry))
            fprintf(stderr,"%s: expected generic retirement %d export %s\n",path,(int)retirement,entry);
        assert(text && strstr(text,entry) &&
            !strstr(text,"xlir_network_services") && !strstr(text,"xl6_net_engine(") &&
            !strstr(text,"xl6_net_resolver(") && !strstr(text,"xl6_net_runtime_unit("));
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out,&ir);
        text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && !strstr(text,"moduleNetworkServiceAbi") && strstr(text,"\"retirementAbi\":"));
        xrtFree(text); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations==35);
    puts("Managed network IR: 5 modules, 35 IR/C/header-gated mutations, explicit resident components and generic retirement");
    return 0;
}
