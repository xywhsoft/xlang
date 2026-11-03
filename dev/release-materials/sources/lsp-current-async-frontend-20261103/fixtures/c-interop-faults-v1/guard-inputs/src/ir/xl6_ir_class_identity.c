#include "src/ir/xl6_ir.h"

typedef struct IdentityWalk {
    const XL6_IrModule* source;
    XL6_IrClassIdentityPlan* plans;
    XL6_IrClassIdentityField* fields;
    size_t fieldCount;
    int verify;
} IdentityWalk;

static int layoutIndex(const XL6_IrModule* module, XL6_IrTypeId type, size_t* result)
{
    size_t found = SIZE_MAX;
    for (size_t i = 0; i < module->classLayoutCount; ++i) {
        if (module->classLayouts[i].type != type) continue;
        if (found != SIZE_MAX) return 0;
        found = i;
    }
    if (found == SIZE_MAX) return 0;
    *result = found; return 1;
}

/* No recursion: hostile/cyclic/deep ancestry must not overflow the compiler
 * stack. The dense vector is streamed in declaration order, base first. */
static int fields(IdentityWalk* walk, size_t derived)
{
    const XL6_IrModule* module = walk->source;
    const XL6_IrClassIdentityPlan* plans = walk->verify ? module->classIdentityPlans : walk->plans;
    size_t depth = 0, cursor = derived;
    for (;;) {
        const XL6_IrClassLayout* layout = &module->classLayouts[cursor];
        if (layout->firstField > module->classFieldCount ||
            layout->fieldCount > module->classFieldCount - layout->firstField ||
            layout->inheritedFieldCount > layout->fieldCount) return 0;
        if (plans[cursor].baseLayout == SIZE_MAX) break;
        if (++depth >= module->classLayoutCount || layout->inheritedFieldCount != 0 ||
            plans[cursor].baseLayout >= module->classLayoutCount) return 0;
        cursor = plans[cursor].baseLayout;
    }
    for (;;) {
        cursor = derived;
        for (size_t step = 0; step < depth; ++step)
            cursor = plans[cursor].baseLayout;
        const XL6_IrClassLayout* layout = &module->classLayouts[cursor];
        for (size_t i = 0; i < layout->fieldCount; ++i) {
            size_t index = layout->firstField + i;
            const XL6_IrClassField* field = &module->classFields[index];
            if (field->compareDeriveIncluded != 0 && field->compareDeriveIncluded != 1) return 0;
            if (!field->compareDeriveIncluded) continue;
            const XL6_IrType* fieldType = xl6IrTypeGet(&module->types, field->type);
            if (!field->name || !*field->name ||
                !fieldType || !fieldType->typeValueId ||
                walk->fieldCount == SIZE_MAX) return 0;
            if (walk->verify) {
                if (walk->fieldCount >= module->classIdentityFieldCount ||
                    module->classIdentityFields[walk->fieldCount].classField != index) return 0;
            } else if (walk->fields) walk->fields[walk->fieldCount].classField = index;
            ++walk->fieldCount;
        }
        if (!depth) return 1;
        --depth;
    }
}

static int samePlan(const XL6_IrClassIdentityPlan* a, const XL6_IrClassIdentityPlan* b)
{
    return a->classLayout == b->classLayout && a->baseLayout == b->baseLayout && a->identity == b->identity &&
        a->runtimeTypeTag == b->runtimeTypeTag && a->hashAbi == b->hashAbi &&
        a->hashTarget == b->hashTarget && a->equalityAbi == b->equalityAbi &&
        a->equalityTarget == b->equalityTarget && a->residentKeyedPair == b->residentKeyedPair;
}

static int prepare(IdentityWalk* walk)
{
    const XL6_IrModule* module = walk->source;
    if (module->classLayoutCount > module->classLayoutCapacity ||
        (module->classLayoutCount && !module->classLayouts) ||
        module->classFieldCount > module->classFieldCapacity ||
        (module->classFieldCount && !module->classFields) ||
        module->types.count > module->types.capacity ||
        (module->types.count && !module->types.items)) return 0;
    for (size_t i = 0; i < module->classLayoutCount; ++i) {
        const XL6_IrClassLayout* layout = &module->classLayouts[i];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, layout->type);
        size_t exact;
        if (!type || !type->typeValueId || !layout->runtimeTypeTag ||
            !layoutIndex(module, layout->type, &exact) || exact != i ||
            layout->hashAbi < XL6_IR_CLASS_HASH_ABI_NONE ||
            layout->hashAbi > XL6_IR_CLASS_HASH_ABI_INTERNAL_CALL ||
            layout->equalityAbi < XL6_IR_CLASS_EQUALITY_ABI_NONE ||
            layout->equalityAbi > XL6_IR_CLASS_EQUALITY_ABI_INTERNAL_CALL ||
            ((layout->hashAbi == XL6_IR_CLASS_HASH_ABI_INTERNAL_CALL) != (layout->hashTarget != 0)) ||
            ((layout->equalityAbi == XL6_IR_CLASS_EQUALITY_ABI_INTERNAL_CALL) != (layout->equalityTarget != 0))) return 0;
        XL6_IrClassIdentityPlan row = {0};
        row.classLayout = i; row.identity = type->typeValueId; row.runtimeTypeTag = layout->runtimeTypeTag;
        row.baseLayout = SIZE_MAX;
        if (layout->baseType != XL6_IR_TYPE_INVALID && !layoutIndex(module, layout->baseType, &row.baseLayout)) return 0;
        row.hashAbi = layout->hashAbi; row.hashTarget = layout->hashTarget;
        row.equalityAbi = layout->equalityAbi; row.equalityTarget = layout->equalityTarget;
        row.residentKeyedPair =
            (layout->hashAbi == XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE &&
             layout->equalityAbi == XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE) ||
            (layout->hashAbi == XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED &&
             layout->equalityAbi == XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED);
        if (walk->verify) {
            if (i >= module->classIdentityPlanCount || !samePlan(&module->classIdentityPlans[i], &row)) return 0;
        } else if (walk->plans) walk->plans[i] = row;
    }
    return 1;
}

static int walkArena(IdentityWalk* walk)
{
    const XL6_IrModule* module = walk->source;
    const XL6_IrClassIdentityPlan* plans = walk->verify ? module->classIdentityPlans : walk->plans;
    /* prepare has certified every ancestry index before this pass follows it.
     * No repeated linear TypeId lookup at each inheritance depth. */
    for (size_t i = 0; i < module->classLayoutCount; ++i) {
        size_t cursor = i, depth = 0;
        while (plans[cursor].baseLayout != SIZE_MAX) {
            if (++depth >= module->classLayoutCount) return 0;
            cursor = plans[cursor].baseLayout;
        }
        size_t first = walk->fieldCount;
        if ((plans[i].hashAbi == XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED ||
             plans[i].equalityAbi == XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED) && !fields(walk, i)) return 0;
        size_t count = walk->fieldCount - first;
        if (walk->verify) {
            if (plans[i].firstField != first || plans[i].fieldCount != count) return 0;
        } else {
            walk->plans[i].firstField = first; walk->plans[i].fieldCount = count;
        }
    }
    return 1;
}

int xl6IrBuildClassIdentityPlans(XL6_IrModule* module)
{
    if (!module) return 0;
    IdentityWalk count = {0}; count.source = module;
    if (!prepare(&count) || module->classLayoutCount > SIZE_MAX / sizeof(XL6_IrClassIdentityPlan)) return 0;
    IdentityWalk build = {0}; build.source = module;
    if (module->classLayoutCount) {
        build.plans = xrtMalloc(module->classLayoutCount * sizeof(*build.plans));
        if (!build.plans) return 0;
    }
    if (!prepare(&build) || !walkArena(&build) ||
        build.fieldCount > SIZE_MAX / sizeof(XL6_IrClassIdentityField)) {
        xrtFree(build.plans); return 0;
    }
    count.fieldCount = build.fieldCount; build.fieldCount = 0;
    if (count.fieldCount) {
        build.fields = xrtMalloc(count.fieldCount * sizeof(*build.fields));
        if (!build.fields) { xrtFree(build.plans); return 0; }
    }
    if (!walkArena(&build) || build.fieldCount != count.fieldCount) {
        xrtFree(build.fields); xrtFree(build.plans); return 0;
    }
    xrtFree(module->classIdentityPlans); xrtFree(module->classIdentityFields);
    module->classIdentityPlans = build.plans;
    module->classIdentityPlanCount = module->classIdentityPlanCapacity = module->classLayoutCount;
    module->classIdentityFields = build.fields;
    module->classIdentityFieldCount = module->classIdentityFieldCapacity = build.fieldCount;
    return 1;
}

int xl6IrVerifyClassIdentityPlans(const XL6_IrModule* module)
{
    if (!module || module->classIdentityPlanCount != module->classLayoutCount ||
        module->classLayoutCount > module->classLayoutCapacity ||
        module->classIdentityPlanCount > module->classIdentityPlanCapacity ||
        (module->classIdentityPlanCount && !module->classIdentityPlans) ||
        module->classIdentityFieldCount > module->classIdentityFieldCapacity ||
        (module->classIdentityFieldCount && !module->classIdentityFields)) return 0;
    IdentityWalk walk = {0}; walk.source = module; walk.verify = 1;
    return prepare(&walk) && walkArena(&walk) && walk.fieldCount == module->classIdentityFieldCount;
}

const XL6_IrClassIdentityPlan* xl6IrClassIdentityPlanGet(const XL6_IrModule* module, XL6_IrTypeId type)
{
    size_t index;
    if (!module || module->classIdentityPlanCount != module->classLayoutCount ||
        module->classLayoutCount > module->classLayoutCapacity ||
        module->classIdentityPlanCount > module->classIdentityPlanCapacity ||
        !module->classIdentityPlans || !module->classLayouts || !layoutIndex(module, type, &index)) return NULL;
    return module->classIdentityPlans[index].classLayout == index ? &module->classIdentityPlans[index] : NULL;
}
