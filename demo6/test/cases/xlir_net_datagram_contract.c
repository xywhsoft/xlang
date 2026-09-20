#include "xlir_continuation_owner_test.h"
static void component_output(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(xl6OutputOpenMemory(&out));
    assert(xl6IrCgenEmitC(&out, ir, &diags)); text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && strstr(text, "bool xl6_xrt_udp_send_vec") && strstr(text, "xlir_datagram_map_addresses")); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(xl6IrCgenEmitHeader(&out, ir, "NET_DATAGRAM", &diags)); text = xl6OutputTakeMemoryText(&out, NULL);
    assert(text && *text); xrtFree(text); assert(!diags.count); xl6DiagFree(&diags);
}
int main(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, p, count = 0, factories = 0, futures = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/stdlib_net_fast_path.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "net_datagram_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) || !xl6IrLowerModule(&ir, &parse.diags)) {
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (i = 0; i < ir.callTargetCount; ++i) {
        XL6_IrCallTarget* target = &ir.callTargets[i], saved = *target;
        if (!xl6IrRuntimeNetDatagramBridgeKindIs(target->runtimeBridge)) continue;
        ++count; assert(xl6IrRuntimeNetDatagramBridgeTargetMatches(&ir, target));
        assert(target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE &&
            target->preparationFailureAbi == XL6_IR_CALL_PREPARATION_RETIRE_OWNED_REVERSE);
        target->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;
        assert(!xl6IrRuntimeNetDatagramBridgeTargetMatches(&ir, target)); *target = saved;
        if (target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) ++factories;
        if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC ||
            target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_UDP_RECV_ASYNC) {
            const XL6_IrType* future = xl6IrTypeGet(&ir.types, target->returnType);
            XL6_IrClassField* field = (XL6_IrClassField*)xl6IrRuntimeNetNativeField(&ir, future->params[0],
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC ?
                    XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST : XL6_IR_RUNTIME_BRIDGE_NET_UDP_PACKET);
            XL6_SymbolId destroy;
            assert(field); destroy = field->nativeDestroySymbol; ++futures;
            field->nativeDestroySymbol = xl6IrRuntimeNetBufferField(&ir)->nativeDestroySymbol;
            assert(!xl6IrRuntimeNetDatagramBridgeTargetMatches(&ir, target));
            reject(&ir, NULL); field->nativeDestroySymbol = destroy;
        }
        target->runtimeBridge = XL6_IR_RUNTIME_BRIDGE_NONE; reject(&ir, NULL); *target = saved;
        target->runtimeBridge = saved.runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_UDP_STATS ?
            XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST : XL6_IR_RUNTIME_BRIDGE_NET_UDP_STATS;
        reject(&ir, NULL); *target = saved;
        target->errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_INVALID; reject(&ir, NULL); *target = saved;
        target->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE; reject(&ir, NULL); *target = saved;
        target->parameterCount++; reject(&ir, NULL); *target = saved;
        target->returnType = saved.returnType ? XL6_IR_TYPE_INVALID : 1; reject(&ir, NULL); *target = saved;
        target->resultPhysicalAbi = XL6_IR_CALL_RESULT_PHYSICAL_INVALID; reject(&ir, NULL); *target = saved;
        for (p = 0; p < target->parameterCount; ++p) {
            XL6_IrParameterAbi old = target->parameterAbis[p];
            XL6_IrCallParameterPhysicalAbi physical = target->parameterPhysicalAbis[p];
            target->parameterAbis[p] = old == XL6_IR_PARAMETER_ABI_BORROW ? XL6_IR_PARAMETER_ABI_VALUE : XL6_IR_PARAMETER_ABI_BORROW;
            reject(&ir, NULL); target->parameterAbis[p] = old;
            target->parameterPhysicalAbis[p] = physical == XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ?
                XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW : XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            reject(&ir, NULL); target->parameterPhysicalAbis[p] = physical;
            if (physical == XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW) {
                size_t f;
                for (f = 0; f < ir.classFieldCount; ++f) {
                    XL6_IrClassField* field = &ir.classFields[f];
                    XL6_SymbolId destroy;
                    if (field->declaration != target->parameterPhysicalFieldDeclarations[p]) continue;
                    destroy = field->nativeDestroySymbol;
                    field->nativeDestroySymbol = xl6IrRuntimeNetBufferField(&ir)->nativeDestroySymbol;
                    assert(!xl6IrRuntimeNetDatagramBridgeTargetMatches(&ir, target));
                    reject(&ir, NULL); field->nativeDestroySymbol = destroy;
                    break;
                }
                assert(f < ir.classFieldCount);
            }
        }
    }
    assert(count == 29 && factories == 1 && futures == 2);
    assert(xl6IrRuntimeNetDatagramBridgeKind("xl6_xrt_udp_not_registered") == XL6_IR_RUNTIME_BRIDGE_NONE);
    verify(&ir); component_output(&ir);
    assert(xl6OutputOpenFile(&out, "build/test/xlir_net_datagram_contract.json"));
    xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("Resolver/datagram: %zu closed targets, %zu owning constructor, %zu class Future results, %zu rejected ABI mutations; verified component emits C/header\n",
        count, factories, futures, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
