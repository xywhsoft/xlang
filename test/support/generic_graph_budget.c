/* White-box budget check, independent of the full production provider gate.
 * Compiles the same saved source; never replaces a shipping provider. */
#include "src/compile/xl6_compile_unit.h"
#include "src/ir/xl6_ir_lower.c"
#include <assert.h>
#include <stdio.h>
int main(int argc, char** argv)
{
    assert(argc == 2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    size_t count = unit.ir.genericSpecializationCount;
    assert(count && count < XL6_IR_GENERIC_SPECIALIZATION_LIMIT);
    XL6_IrGenericSpecialization cached = unit.ir.genericSpecializations[0];
    XL6_IrLowerContext ctx = {0}; ctx.module = &unit.ir; ctx.diags = &unit.parse.diags;
    /* Count is deliberately made full only inside this isolated process;
     * fill remaining rows so the production cache scan remains in bounds. */
    XL6_IrGenericSpecialization* grown = xrtRealloc(unit.ir.genericSpecializations,
        XL6_IR_GENERIC_SPECIALIZATION_LIMIT * sizeof(*grown)); assert(grown);
    unit.ir.genericSpecializations = grown;
    unit.ir.genericSpecializationCapacity = XL6_IR_GENERIC_SPECIALIZATION_LIMIT;
    memset(grown + count, 0, (XL6_IR_GENERIC_SPECIALIZATION_LIMIT - count) * sizeof(*grown));
    unit.ir.genericSpecializationCount = XL6_IR_GENERIC_SPECIALIZATION_LIMIT;
    XL6_SymbolId symbol = 0;
    assert(xl6IrLowerBuildGenericSpecialization(&ctx, 0, cached.sourceDeclaration,
        cached.typeArgs, cached.typeArgCount, &symbol));
    assert(symbol == cached.symbol && !ctx.failed);
    assert(cached.typeArgCount == 1);
    XL6_IrTypeId different = XL6_IR_TYPE_INVALID;
    for (size_t i = 0; i < unit.ir.types.count; ++i) {
        if (unit.ir.types.items[i].cAbi == XL6_IR_C_ABI_BOOL && i + 1 != cached.typeArgs[0]) {
            different = (XL6_IrTypeId)(i + 1); break;
        }
    }
    assert(different);
    assert(!xl6IrLowerBuildGenericSpecialization(&ctx, 0, cached.sourceDeclaration, &different, 1, NULL));
    assert(ctx.failed && unit.parse.diags.count);
    assert(strstr(unit.parse.diags.items[unit.parse.diags.count - 1].message, "finite lowering limit"));
    assert(unit.ir.genericSpecializationCount == XL6_IR_GENERIC_SPECIALIZATION_LIMIT);
    unit.ir.genericSpecializationCount = count;
    xl6CompileUnitFree(&unit);
    puts("shared graph budget: cached reuse allowed; new root rejected at limit"); return 0;
}
