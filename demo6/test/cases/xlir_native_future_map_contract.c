#include "xlir_continuation_owner_test.h"
#include "src/compile/xl6_artifact_runtime.h"
int main(void)
{
    const char* fixtures[]={"stdlib_net_fast_path","stdlib_net_send_file","stdlib_tls_server_loopback",
        "xlir_net_resolver_standalone","xlir_net_datagram_boundaries","xlir_net_buffer_command_lifetime"};
    const char* diagnostic="native Future mapper/code ownership ABI is unresolved";
    for(size_t fixture=0;fixture<sizeof(fixtures)/sizeof(*fixtures);++fixture){
        char path[180];XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char* first;char* second;
        snprintf(path,sizeof(path),"test/cases/%s.xl",fixtures[fixture]);
        xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
        int ok=xl6ParseFile(path,&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"native_future_map") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags);
        if(!ok)xl6DiagPrintText(&parse.diags,path);
        assert(ok);
        assert(ir.abi.nativeFutureMapAbi==XL6_IR_NATIVE_FUTURE_MAP_NONE);verify(&ir);
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1;reject(&ir,diagnostic);
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_NONE;
        assert(xl6IrLowerModule(&ir,&parse.diags));verify(&ir);
        assert(ir.abi.nativeFutureMapAbi==XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1);
        for(int invalid=-1;invalid<=2;++invalid){
            if(invalid==XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1)continue;
            ir.abi.nativeFutureMapAbi=(XL6_IrNativeFutureMapAbi)invalid;reject(&ir,diagnostic);
            XL6_ArtifactRuntimePlan plan={0};XL6_DiagList diags={0};
            assert(!xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&diags));
            assert(plan.components==0 && plan.sourceCount==0 && plan.nativeModuleCount==0);xl6DiagFree(&diags);
        }
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1;verify(&ir);
        XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&parse.diags));
        assert((plan.components & XL6_ARTIFACT_BIT(FUTURE_CALLBACK)) && (plan.components & XL6_ARTIFACT_BIT(FUTURE_PAYLOAD)));
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));first=xl6OutputTakeMemoryText(&out,NULL);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));second=xl6OutputTakeMemoryText(&out,NULL);
        assert(first && second && !strcmp(first,second));xrtFree(second);
        assert(strstr(first,"xl6FutureCallbackRuntimeV3Get()->StartExclusive") &&
            strstr(first,"xl6ClassRuntimeV2Get()->Create(xlir_net_owner, NULL)") &&
            strstr(first,"xl6ClassRuntimeV2Get()->BindIdentity") &&
            strstr(first,"xl6PayloadResolveOwned"));
        assert(!strstr(first,"xlir_tls_legacy") && !strstr(first,"xrtFutureThenOwnedCancelSource(") &&
            !strstr(first,"xl6FutureThenOwned(") && !strstr(first,"xlir_tls_future_link_cancel"));
        xrtFree(first);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleText(&out,&ir);first=xl6OutputTakeMemoryText(&out,NULL);
        assert(first && strstr(first,"native-future-map: 1\n"));xrtFree(first);
        assert(xl6OutputOpenMemory(&out));xl6IrDumpModuleJson(&out,&ir);first=xl6OutputTakeMemoryText(&out,NULL);
        assert(first && strstr(first,"\"nativeFutureMapAbi\": 1"));xrtFree(first);
        xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    }
    {
        const XL6_IrRuntimeBridgeKind bridges[]={XL6_IR_RUNTIME_BRIDGE_NET_TCP_DIAL,XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM,
            XL6_IR_RUNTIME_BRIDGE_NET_TCP_SERVER,XL6_IR_RUNTIME_BRIDGE_NET_TLS_DIAL,XL6_IR_RUNTIME_BRIDGE_NET_TLS_STREAM,
            XL6_IR_RUNTIME_BRIDGE_NET_TLS_SERVER,XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC,XL6_IR_RUNTIME_BRIDGE_NET_UDP_RECV_ASYNC};
        XL6_IrModule probe={0};XL6_IrCallTarget target={0};probe.callTargets=&target;probe.callTargetCount=1;
        target.cName="unrelated-c-symbol";
        for(size_t i=0;i<sizeof(bridges)/sizeof(*bridges);++i){
            target.runtimeBridge=bridges[i];target.reachability=XL6_IR_REACHABILITY_TRANSITIVE;
            assert(xl6IrModuleNativeFutureMapRequirement(&probe)==XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1);
            target.reachability=XL6_IR_REACHABILITY_UNREACHABLE;
            assert(xl6IrModuleNativeFutureMapRequirement(&probe)==XL6_IR_NATIVE_FUTURE_MAP_NONE);
        }
        XL6_ParseResult parse;XL6_IrModule ir;xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
        assert(xl6ParseSource("assert 6 * 7 == 42\n","no_native_mapper.xl",&parse) &&
            xl6SemCheckModule(parse.root,&parse.diags,"no_native_mapper") &&
            xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags));
        assert(ir.abi.nativeFutureMapAbi==XL6_IR_NATIVE_FUTURE_MAP_NONE);
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1;reject(&ir,diagnostic);
        ir.abi.nativeFutureMapAbi=XL6_IR_NATIVE_FUTURE_MAP_NONE;verify(&ir);
        xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    }
    assert(mutations==25);
    puts("Native Future mapping IR: 6 real modules, 8 bridge identities, 25 IR/C/header-gated mutations, invalid artifact plans refused, deterministic C and exact resident dependencies");
    return 0;
}
