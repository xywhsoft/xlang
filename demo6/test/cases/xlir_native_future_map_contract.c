#include "xlir_continuation_owner_test.h"
#include "src/compile/xl6_artifact_runtime.h"
int main(void)
{
    const struct { const char* name; int mapping; } fixtures[]={
        {"stdlib_fs_async_minimal",1},{"stdlib_fs_async_cancel",1},
        {"stdlib_tcp_managed_async",0},{"stdlib_udp_async",0},{"stdlib_net_resolver_numeric",0}
    };
    const char* diagnostic="native Future mapper/code ownership ABI is unresolved";
    for (size_t fixture=0;fixture<sizeof(fixtures)/sizeof(*fixtures);++fixture) {
        char path[180]; XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* first; char* second;
        snprintf(path,sizeof(path),"test/cases/%s.xl",fixtures[fixture].name);
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        int ok=xl6ParseFile(path,&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"native_future_map") &&
            xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags);
        if (!ok) xl6DiagPrintText(&parse.diags,path);
        assert(ok); verify(&ir);
        assert(ir.abi.nativeFutureMapAbi==XL6_IR_NATIVE_FUTURE_MAP_NONE);
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1;
        reject(&ir,diagnostic); ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_NONE;
        assert(xl6IrLowerModule(&ir,&parse.diags)); verify(&ir);
        XL6_IrNativeFutureMapAbi expected=fixtures[fixture].mapping
            ? XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1 : XL6_IR_NATIVE_FUTURE_MAP_NONE;
        assert(ir.abi.nativeFutureMapAbi==expected && xl6IrModuleNativeFutureMapRequirement(&ir)==expected);
        for (int invalid=-1;invalid<=2;++invalid) {
            if (invalid==(int)expected) continue;
            ir.abi.nativeFutureMapAbi=(XL6_IrNativeFutureMapAbi)invalid; reject(&ir,diagnostic);
            XL6_ArtifactRuntimePlan plan={0}; XL6_DiagList diags={0};
            assert(!xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&diags));
            assert(xl6ArtifactRuntimeSetEmpty(&plan.components) && !plan.sourceCount && !plan.nativeModuleCount);
            xl6DiagFree(&diags);
        }
        ir.abi.nativeFutureMapAbi=expected; verify(&ir);
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_PAYLOAD));
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));
        first=xl6OutputTakeMemoryText(&out,NULL);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));
        second=xl6OutputTakeMemoryText(&out,NULL);
        assert(first && second && !strcmp(first,second)); xrtFree(second);
        assert(!strstr(first,"xl6_net_future_map") && !strstr(first,"xlir_datagram_future_map") &&
            !strstr(first,"xl6ClassRuntimeV2Get()->Create(xlir_net_owner, NULL)"));
        if (fixtures[fixture].mapping) assert(strstr(first,"xl6FutureCallbackRuntimeV3Get()->StartExclusive"));
        xrtFree(first); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    {
        XL6_IrModule probe={0}; XL6_IrCallTarget target={0};
        probe.callTargets=&target; probe.callTargetCount=1; target.cName="unrelated-c-symbol";
        const XL6_IrRuntimeBridgeKind bridges[]={XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_OPEN,
            XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_ENSURE};
        for (size_t i=0;i<sizeof(bridges)/sizeof(*bridges);++i) {
            target.runtimeBridge=bridges[i]; target.reachability=XL6_IR_REACHABILITY_TRANSITIVE;
            assert(xl6IrModuleNativeFutureMapRequirement(&probe)==XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1);
            target.reachability=XL6_IR_REACHABILITY_UNREACHABLE;
            assert(xl6IrModuleNativeFutureMapRequirement(&probe)==XL6_IR_NATIVE_FUTURE_MAP_NONE);
        }
        XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseSource("assert 6 * 7 == 42\n","no_native_mapper.xl",&parse) &&
            xl6SemCheckModule(parse.root,&parse.diags,"no_native_mapper") &&
            xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags));
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1; reject(&ir,diagnostic);
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_NONE; verify(&ir);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations==21);
    puts("Native Future mapping IR: 2 filesystem and 3 managed network modules, 21 IR/C/header-gated mutations, reachable filesystem requirement and deterministic C");
    return 0;
}
