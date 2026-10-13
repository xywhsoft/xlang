#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int collect(void* opaque, const char* name)
{
    size_t* counts = opaque;
    if (!strcmp(name, "xl6ValueAdmitsV1")) ++counts[0];
    if (!strcmp(name, "xl6ValueAdmitsNominalV1")) ++counts[1];
    return 1;
}

static void reject(XL6_IrModule* module)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    assert(!xl6IrVerifyModule(module, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, module, &diags));
    size_t length = 0;
    char* text = xl6OutputTakeMemoryText(&out, &length);
    assert(length == 0); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, module, "INVALID_DOMAIN", &diags));
    text = xl6OutputTakeMemoryText(&out, &length);
    assert(length == 0); xrtFree(text);
    xl6DiagFree(&diags);
}

int main(int argc, char** argv)
{
    assert(argc == 2 || (argc == 3 && !strcmp(argv[2], "--no-domains")));
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    assert(xl6IrVerifyModule(&unit.ir, &unit.parse.diags));
    size_t mutations = 0, domains = 0, nominals = 0, exact = 0;
    for (size_t i = 0; i < unit.ir.dynamicMethodCount; ++i) {
        XL6_IrDynamicMethod* method = &unit.ir.dynamicMethods[i];
        if (method->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED) continue;
        for (size_t p = 0; p < method->fixedParameterCount; ++p) {
            XL6_IrDynamicMethodParameter* parameter =
                &unit.ir.dynamicMethodParameters[method->firstParameter + p];
            XL6_IrDynamicMethodMatchAbi saved = parameter->matchAbi;
            if (saved == XL6_IR_DYNAMIC_METHOD_MATCH_VALUE_DOMAIN) ++domains;
            else if (saved == XL6_IR_DYNAMIC_METHOD_MATCH_NOMINAL) ++nominals;
            else if (saved == XL6_IR_DYNAMIC_METHOD_MATCH_EXACT_IDENTITY) ++exact;
            else continue;
            parameter->matchAbi = XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE;
            reject(&unit.ir); ++mutations; parameter->matchAbi = saved;
            parameter->matchAbi = (XL6_IrDynamicMethodMatchAbi)99;
            reject(&unit.ir); ++mutations; parameter->matchAbi = saved;
        }
    }
    size_t counts[2] = {0};
    assert(xl6IrCgenVisitResidentSymbols(&unit.ir, &unit.parse.diags, NULL, collect, counts));
    if (argc == 2) assert(domains && nominals && exact && mutations >= 6 && counts[0] && counts[1]);
    /* Unreachable library declarations remain in the IR inventory. Only the
     * selected resident closure, not that inventory, must be empty here. */
    else assert(!counts[0] && !counts[1]);
    assert(xl6IrVerifyModule(&unit.ir, &unit.parse.diags));
    printf("dynamic method domains=%zu nominal=%zu exact=%zu mutations=%zu rejected before output\n",
        domains, nominals, exact, mutations);
    printf("bytes admission rule=%u\n", (unsigned int)XL6_VALUE_ADMIT_BYTES);
    xl6CompileUnitFree(&unit);
    return 0;
}
