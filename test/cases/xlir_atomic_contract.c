/* Ordinary, offline ABI checks against the actual compiler SO. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t rejected;
static void refuse(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (int header = 0; header != 2; ++header) {
        XL6_Output output;
        XL6_DiagList diagnostics;
        size_t size = SIZE_MAX;
        xl6DiagInit(&diagnostics);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_ATOMIC", &diagnostics) :
            xl6IrCgenEmitC(&output, module, &diagnostics)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&diagnostics);
    }
    ++rejected;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    size_t checked = 0, products = 0;
    for (size_t i = 0; i < module->callTargetCount; ++i) {
        XL6_IrCallTarget* target = &module->callTargets[i];
        if (!target->cName || strncmp(target->cName, "xl6Atomic", 9)) continue;
        XL6_IrCallTarget saved = *target;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module, target));
        const XL6_IrType* returned = xl6IrTypeGet(&module->types, target->returnType);
        if (strstr(target->cName, "Load") || strstr(target->cName, "Exchange") ||
            strstr(target->cName, "Fetch")) {
            bool product = strstr(target->cName, "CompareExchange") != NULL;
            if (strstr(target->cName, "AtomicInt32"))
                assert(product ? returned->nativeProductAbi == XL6_IR_NATIVE_PRODUCT_BOOLI32_V1 :
                    returned->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_32);
            if (strstr(target->cName, "AtomicUInt32"))
                assert(product ? returned->nativeProductAbi == XL6_IR_NATIVE_PRODUCT_BOOLU32_V1 :
                    returned->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_32);
        }
        target->effectAbi = XL6_IR_CALL_EFFECT_PURE;
        refuse(module);
        *target = saved;
        target->errorAbi = saved.errorAbi == XL6_IR_ERROR_ABI_NO_RAISE ?
            XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
        refuse(module);
        *target = saved;
        if (strstr(target->cName, "CompareExchange")) {
            XL6_IrType* result = &module->types.items[target->returnType - 1];
            XL6_IrNativeProductAbi product = result->nativeProductAbi;
            result->nativeProductAbi = XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1;
            refuse(module);
            result->nativeProductAbi = product;
            ++products;
        }
        ++checked;
    }
    assert(checked >= 10 && products >= 5);
    assert(xl6IrVerifyModule(module, NULL));
    xl6CompileUnitFree(&unit);
    printf("atomic contracts: %zu entries, %zu products, %zu refusals\n", checked, products, rejected);
    return 0;
}
