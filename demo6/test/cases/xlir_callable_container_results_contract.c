/* Complete function-value results must agree before lowering and remain a
 * strict backend input contract. No destination type may repair the call. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/cgen/xl6_ir_cgen_native_header.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static size_t mutations;
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned view = 0; view != 3; ++view) {
        XL6_Output out; size_t size = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        int emitted = view == 0 ? xl6IrCgenEmitC(&out, ir, NULL) :
            view == 1 ? xl6IrCgenEmitHeader(&out, ir, "BAD_CALLABLE_RESULT", NULL) :
            xl6IrCgenEmitNativeContractHeader(&out, ir, "BAD_CALLABLE_RESULT", NULL);
        assert(!emitted);
        char* text = xl6OutputTakeMemoryText(&out, &size);
        assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir;
    size_t calls = 0, generic = 0, nullable = 0, products = 0;
    assert(argc == 2); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "callable_container_results") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (size_t i = 0; i < ir.callSiteCount; ++i) {
        const XL6_IrCallSite* site = &ir.callSites[i];
        if (site->callKind != XL6_SEM_CALL_FUNCTION_VALUE) continue;
        const XL6_IrNode* call = xl6IrNodeGet(&ir, site->origin);
        const XL6_IrNode* callee = xl6IrNodeGet(&ir, site->receiver);
        const XL6_IrType* signature = callee ? xl6IrTypeGet(&ir.types, callee->storageType) : NULL;
        const XL6_IrType* result = signature ? xl6IrTypeGet(&ir.types, signature->callableResultType) : NULL;
        assert(call && signature && signature->kind == XL6_IR_TYPE_FUNCTION && result);
        assert(site->resultType == signature->callableResultType &&
            call->type == site->resultType && call->storageType == site->resultType);
        ++calls;
        if (result->kind == XL6_IR_TYPE_GENERIC_INSTANCE) {
            assert(result->paramCount == 1); ++generic;
        } else if (result->kind == XL6_IR_TYPE_NULLABLE) {
            assert(result->paramCount == 1);
            const XL6_IrType* payload = xl6IrTypeGet(&ir.types, result->params[0]);
            assert(payload && payload->kind == XL6_IR_TYPE_GENERIC_INSTANCE && payload->paramCount == 1);
            ++nullable;
        } else {
            assert(result->kind == XL6_IR_TYPE_TUPLE && result->paramCount == 2);
            ++products;
        }
    }
    assert(calls == 21 && generic == 18 && nullable == 1 && products == 2);
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    for (size_t i = 0; i < ir.callSiteCount; ++i) {
        XL6_IrCallSite* site = &ir.callSites[i];
        if (site->callKind != XL6_SEM_CALL_FUNCTION_VALUE) continue;
        XL6_IrNode* call = &ir.nodes[site->origin - 1];
        XL6_IrNode* callee = &ir.nodes[site->receiver - 1];
        XL6_IrType* signature = &ir.types.items[callee->storageType - 1];
        XL6_IrTypeId saved = site->resultType;
        site->resultType = XL6_IR_TYPE_INVALID; reject(&ir); site->resultType = saved;
        saved = call->storageType;
        call->storageType = XL6_IR_TYPE_INVALID; reject(&ir); call->storageType = saved;
        saved = call->type;
        call->type = XL6_IR_TYPE_INVALID; reject(&ir); call->type = saved;
        saved = signature->callableResultType;
        signature->callableResultType = XL6_IR_TYPE_INVALID; reject(&ir); signature->callableResultType = saved;
        saved = signature->returns[0];
        signature->returns[0] = XL6_IR_TYPE_INVALID; reject(&ir); signature->returns[0] = saved;
    }
    assert(xl6IrVerifyModule(&ir, NULL) && mutations == 105);
    printf("callable container results: %zu exact calls, %zu generic, %zu nullable, %zu products; %zu mutations refused before C/public/native output\n",
        calls, generic, nullable, products, mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
