#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "release/include/xrt_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations;
static void materializedPrefix(const XL6_IrModule* ir, const XL6_IrClassIdentityPlan* plan)
{
    XL6_IrModule synthetic = {0};
    XL6_IrClassLayout layout = ir->classLayouts[plan->classLayout];
    XL6_IrClassField fields[4];
    assert(plan->baseLayout != SIZE_MAX);
    const XL6_IrClassLayout* base = &ir->classLayouts[plan->baseLayout];
    assert(base->fieldCount == 3 && layout.fieldCount == 1);
    memcpy(fields, &ir->classFields[base->firstField], 3 * sizeof(*fields));
    fields[3] = ir->classFields[layout.firstField];
    for (size_t i = 0; i < 4; ++i) fields[i].ownerType = layout.type;
    layout.baseType = XL6_IR_TYPE_INVALID; layout.firstField = 0;
    layout.fieldCount = 4; layout.inheritedFieldCount = 3;
    synthetic.types = ir->types; synthetic.classLayouts = &layout;
    synthetic.classLayoutCount = synthetic.classLayoutCapacity = 1;
    synthetic.classFields = fields; synthetic.classFieldCount = synthetic.classFieldCapacity = 4;
    assert(xl6IrBuildClassIdentityPlans(&synthetic) && xl6IrVerifyClassIdentityPlans(&synthetic));
    assert(synthetic.classIdentityPlanCount == 1 && synthetic.classIdentityFieldCount == 3 &&
        synthetic.classIdentityPlans[0].baseLayout == SIZE_MAX &&
        synthetic.classIdentityFields[0].classField == 0 &&
        synthetic.classIdentityFields[1].classField == 1 &&
        synthetic.classIdentityFields[2].classField == 3);
    XL6_IrClassIdentityPlan* old = synthetic.classIdentityPlans;
    layout.inheritedFieldCount = SIZE_MAX;
    assert(!xl6IrBuildClassIdentityPlans(&synthetic) && synthetic.classIdentityPlans == old);
    xrtFree(synthetic.classIdentityPlans); xrtFree(synthetic.classIdentityFields);
}
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out;
    assert(!xl6IrVerifyClassIdentityPlans(ir));
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitC(&out, ir, &diags));
    char* text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_IDENTITY", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void check(XL6_IrModule* ir)
{
    assert(xl6IrVerifyClassIdentityPlans(ir));
    for (size_t i = 0; i < ir->classIdentityPlanCount; ++i) {
        XL6_IrClassIdentityPlan* p = &ir->classIdentityPlans[i], saved = *p;
        const XL6_IrClassLayout* layout = &ir->classLayouts[i];
        assert(xl6IrClassIdentityPlanGet(ir, layout->type) == p);
#define BAD(Member, Value) p->Member = (Value); reject(ir); *p = saved
        BAD(classLayout, SIZE_MAX); BAD(identity, 0); BAD(runtimeTypeTag, 0);
        BAD(baseLayout, saved.baseLayout == SIZE_MAX ? i : SIZE_MAX);
        BAD(hashAbi, saved.hashAbi == XL6_IR_CLASS_HASH_ABI_INTERNAL_CALL ?
            XL6_IR_CLASS_HASH_ABI_NONE : XL6_IR_CLASS_HASH_ABI_INTERNAL_CALL); BAD(hashTarget, UINT32_MAX);
        BAD(equalityAbi, saved.equalityAbi == XL6_IR_CLASS_EQUALITY_ABI_INTERNAL_CALL ?
            XL6_IR_CLASS_EQUALITY_ABI_NONE : XL6_IR_CLASS_EQUALITY_ABI_INTERNAL_CALL); BAD(equalityTarget, UINT32_MAX);
        BAD(residentKeyedPair, !saved.residentKeyedPair);
        BAD(firstField, SIZE_MAX); BAD(fieldCount, SIZE_MAX);
#undef BAD
    }
    for (size_t i = 0; i < ir->classIdentityFieldCount; ++i) {
        size_t index = ir->classIdentityFields[i].classField;
        ir->classIdentityFields[i].classField = SIZE_MAX; reject(ir);
        ir->classIdentityFields[i].classField = index;
        int included = ir->classFields[index].compareDeriveIncluded;
        ir->classFields[index].compareDeriveIncluded = 0; reject(ir);
        ir->classFields[index].compareDeriveIncluded = included;
    }
    size_t count = ir->classIdentityPlanCount;
    --ir->classIdentityPlanCount; reject(ir); ir->classIdentityPlanCount = count;
    size_t capacity = ir->classIdentityPlanCapacity;
    ir->classIdentityPlanCapacity = 0; reject(ir); ir->classIdentityPlanCapacity = capacity;
    XL6_IrClassIdentityPlan* plans = ir->classIdentityPlans;
    ir->classIdentityPlans = NULL; reject(ir); ir->classIdentityPlans = plans;
    assert(xl6IrVerifyClassIdentityPlans(ir));
    // Malformed ancestry is rejected without recursion or changing old rows.
    for (size_t i = 0; i < ir->classLayoutCount; ++i) {
        XL6_IrClassLayout* layout = &ir->classLayouts[i];
        if (layout->equalityAbi != XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED) continue;
        XL6_IrClassLayout saved = *layout;
        XL6_IrClassIdentityPlan* old = ir->classIdentityPlans;
        layout->baseType = layout->type;
        assert(!xl6IrBuildClassIdentityPlans(ir) && ir->classIdentityPlans == old); reject(ir);
        *layout = saved;
        layout->firstField = SIZE_MAX;
        assert(!xl6IrBuildClassIdentityPlans(ir) && ir->classIdentityPlans == old); reject(ir);
        *layout = saved;
    }
}
static void rebuild(XL6_IrModule* ir)
{
    bool terminal = false;
    for (size_t point = 0; point < 10; ++point) {
        xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        XL6_IrClassIdentityPlan* plans = ir->classIdentityPlans;
        XL6_IrClassIdentityField* fields = ir->classIdentityFields;
        size_t plansCount = ir->classIdentityPlanCount, fieldCount = ir->classIdentityFieldCount;
        assert(xrtMemDebugFailAfter(point));
        int ok = xl6IrBuildClassIdentityPlans(ir);
        bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(hit ? !ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY : ok);
        if (hit) assert(ir->classIdentityPlans == plans && ir->classIdentityFields == fields);
        assert(ir->classIdentityPlanCount == plansCount && ir->classIdentityFieldCount == fieldCount);
        xrtClearError(); assert(xl6IrVerifyClassIdentityPlans(ir));
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        if (!hit) { assert(point == 2); terminal = true; break; }
    }
    assert(terminal);
    // The verifier is allocation-free even when the next allocation must fail.
    assert(xrtMemDebugFailAfter(0) && xl6IrVerifyClassIdentityPlans(ir) && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
}
int main(int argc, char** argv)
{
    assert(argc == 5 && xrtMemDebugEnable(true));
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "identity_plan") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags)) {
        xl6DiagPrintText(&parse.diags, argv[1]); return 2;
    }
    assert(ir.classIdentityPlanCount && ir.classIdentityFieldCount);
    if (strstr(argv[1], "stdlib_class_identity_inherited")) {
        size_t found = 0;
        const char* names[2][3] = {{"baseNumber", "baseText", "childNumber"}, {"inherited", "own", NULL}};
        for (size_t i = 0; i < ir.classIdentityPlanCount; ++i) {
            const XL6_IrClassIdentityPlan* plan = &ir.classIdentityPlans[i];
            const XL6_IrType* type = xl6IrTypeGet(&ir.types, ir.classLayouts[i].type);
            int choice = !strcmp(type->displayName, "IdentityChild") ? 0 :
                !strcmp(type->displayName, "UnconstructedIdentityChild") ? 1 : -1;
            if (choice < 0) continue;
            assert(plan->fieldCount == (choice == 0 ? 3 : 2));
            for (size_t j = 0; j < plan->fieldCount; ++j)
                assert(!strcmp(ir.classFields[ir.classIdentityFields[plan->firstField + j].classField].name, names[choice][j]));
            if (!choice) materializedPrefix(&ir, plan);
            ++found;
        }
        assert(found == 2);
    }
    check(&ir); rebuild(&ir);
    if (!xl6IrLowerModule(&ir, &parse.diags)) {
        xl6DiagPrintText(&parse.diags, argv[1]); return 3;
    }
    check(&ir); rebuild(&ir);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3]) && xl6IrCgenEmitHeader(&out, &ir, "IDENTITY_PLAN", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[4])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    printf("identity plan: %zu rejected C/header mutations; typed/lowered rebuild OOM 0,1,2 balanced\n", mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
