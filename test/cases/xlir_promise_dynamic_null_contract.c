#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (int header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList errors;
        size_t size = SIZE_MAX;
        xl6DiagInit(&errors);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_ERASED_NULL", &errors)
                       : xl6IrCgenEmitC(&output, module, &errors)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&errors);
    }
    ++refusals;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    assert(xl6IrRuntimeDirectBridgeValueAddressParameter("\"xl6PromiseResolveV1\"", 1));
    assert(!xl6IrRuntimeDirectBridgeValueAddressParameter("\"xl6PromiseResolveV1\"", 0));
    assert(!xl6IrRuntimeDirectBridgeValueAddressParameter("\"xl6PromiseResolveV1Almost\"", 1));
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    size_t matches = 0;
    for (size_t m = 0; m < module->dynamicMethodCount; ++m) {
        XL6_IrDynamicMethod* method = &module->dynamicMethods[m];
        if (method->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED) continue;
        for (size_t p = 0; p < method->parameterCount; ++p) {
            XL6_IrDynamicMethodParameter* item = &module->dynamicMethodParameters[method->firstParameter + p];
            if (item->matchAbi != XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD) continue;
            assert(xl6IrTypeIsZeroPayload(module, item->type));
            assert(item->abi == XL6_IR_PARAMETER_ABI_VALUE && item->scalarCodec == 0);
            assert(item->defaultAbi == XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE && !item->defaultExpression);
            XL6_IrCallTarget* target = (XL6_IrCallTarget*)xl6IrCallTargetGet(module, method->targetSymbol);
            assert(target && xl6IrRuntimeDirectBridgeTargetMatches(module, target));
            assert(target->parameterPhysicalAbis[p + 1] == XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD);
            item->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE;
            reject(module);
            item->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_SCALAR_CODEC;
            reject(module);
            item->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD;
            item->scalarCodec = 1;
            reject(module);
            item->scalarCodec = 0;
            target->parameterPhysicalAbis[p + 1] = XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_VALUE_ADDRESS_BORROW;
            reject(module);
            target->parameterPhysicalAbis[p + 1] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            reject(module);
            target->parameterPhysicalAbis[p + 1] = XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD;
            XL6_IrNode* declaration = &module->nodes[method->declaration - 1];
            XL6_AstKind kind = declaration->sourceKind;
            declaration->sourceKind = XL6_AST_FUNC;
            assert(!xl6IrDynamicMethodSourceSupported(module, method));
            reject(module);
            declaration->sourceKind = kind;
            char* linkage = declaration->text;
            declaration->text = "\"xl6PromiseResolveV1Almost\"";
            assert(!xl6IrDynamicMethodSourceSupported(module, method));
            reject(module);
            declaration->text = linkage;
            XL6_IrTypeId owner = method->ownerType;
            method->ownerType = method->returnType;
            assert(!xl6IrDynamicMethodSourceSupported(module, method));
            reject(module);
            method->ownerType = owner;
            ++matches;
        }
    }
    assert(matches && xl6IrVerifyModule(module, NULL));
    printf("erased empty payload: %zu exact native null matchers, %zu zero-C/header refusals\n", matches, refusals);
    xl6CompileUnitFree(&unit);
    return 0;
}
