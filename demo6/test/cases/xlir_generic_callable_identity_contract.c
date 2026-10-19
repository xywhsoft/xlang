#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals, conventionPairs, fieldConventions;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (int header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList errors;
        size_t size = SIZE_MAX;
        xl6DiagInit(&errors);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_CALLABLE_IDENTITY", &errors)
                       : xl6IrCgenEmitC(&output, module, &errors)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&errors);
    }
    ++refusals;
}

static int samePhysicalParametersAndResults(
    const XL6_IrModule* module, const XL6_IrType* a, const XL6_IrType* b)
{
    if (a->kind != XL6_IR_TYPE_FUNCTION || b->kind != XL6_IR_TYPE_FUNCTION ||
        a->paramCount != b->paramCount || a->returnCount != b->returnCount || a->cAbi != b->cAbi ||
        (a->returnCount && memcmp(a->returns, b->returns, a->returnCount * sizeof(*a->returns)))) return 0;
    for (size_t i = 0; i < a->paramCount; ++i) {
        const XL6_IrType* left = xl6IrTypeGet(&module->types, a->params[i]);
        const XL6_IrType* right = xl6IrTypeGet(&module->types, b->params[i]);
        if (!left || !right || left->cAbi != right->cAbi || left->ownershipAbi != right->ownershipAbi ||
            left->nativeSlotAbi != right->nativeSlotAbi || a->parameterModes[i] != b->parameterModes[i] ||
            a->parameterAbis[i] != b->parameterAbis[i]) return 0;
    }
    return 1;
}

static void duplicate(XL6_IrModule* module, XL6_IrTypeId id, XL6_TypeAbiId label)
{
    XL6_IrTypePool* pool = &module->types;
    if (pool->count == pool->capacity) {
        assert(pool->count < SIZE_MAX / sizeof(*pool->items) - 1);
        XL6_IrType* items = (XL6_IrType*)xrtRealloc(pool->items, (pool->count + 1) * sizeof(*pool->items));
        assert(items);
        pool->items = items;
        pool->capacity = pool->count + 1;
    }
    pool->items[pool->count] = *xl6IrTypeGet(pool, id);
    pool->items[pool->count].abiId = label;
    pool->items[pool->count].sourceTypeId = XL6_TYPE_ID_INVALID;
    ++pool->count;
    reject(module);
    --pool->count;
    /* The injected row only borrowed the real row's allocations. */
    memset(&pool->items[pool->count], 0, sizeof(*pool->items));
}

static void exercise(const char* source)
{
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, source, 1));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    XL6_TypeAbiId label = XL6_TYPE_ABI_ID_INVALID;
    XL6_IrTypeId function = XL6_IR_TYPE_INVALID;
    size_t erased = 0;
    for (size_t i = 0; i < module->types.count; ++i) {
        XL6_IrType* type = &module->types.items[i];
        if (type->kind != XL6_IR_TYPE_FUNCTION) continue;
        if (type->abiId) label = type->abiId;
        function = (XL6_IrTypeId)(i + 1);
        type->abiId = XL6_TYPE_ABI_ID_INVALID;
        type->sourceTypeId = XL6_TYPE_ID_INVALID;
        ++erased;
    }
    assert(erased && label && function && xl6IrVerifyModule(module, NULL));
    duplicate(module, function, label);
    assert(xl6IrVerifyModule(module, NULL));
    /* Lowering must work from complete XLIR shapes, without recovering a
     * lost frontend signature label or manufacturing a second TypeId. */
    assert(xl6IrLowerModule(module, &unit.parse.diags));
    assert(xl6IrVerifyModule(module, NULL));
    duplicate(module, function, label);
    for (size_t f = 0; f < module->classFieldCount; ++f) {
        XL6_IrClassField* field = &module->classFields[f];
        const XL6_IrNode* declaration = xl6IrNodeGet(module, field->declaration);
        const XL6_IrType* original = declaration ? xl6IrTypeGet(&module->types, declaration->storageType) : NULL;
        const XL6_IrType* type = xl6IrTypeGet(&module->types, field->type);
        if (!original || original->kind != XL6_IR_TYPE_FUNCTION) continue;
        assert(type && type->kind == XL6_IR_TYPE_FUNCTION &&
            type->callableErasureAbi == original->callableErasureAbi);
        ++fieldConventions;
        for (size_t i = 0; i < module->types.count; ++i) {
            const XL6_IrType* other = &module->types.items[i];
            if (!samePhysicalParametersAndResults(module, type, other) ||
                type->callableErasureAbi == other->callableErasureAbi) continue;
            XL6_IrTypeId saved = field->type;
            field->type = (XL6_IrTypeId)(i + 1);
            reject(module);
            field->type = saved;
            ++conventionPairs;
        }
    }
    assert(xl6IrVerifyModule(module, NULL));
    for (int header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList errors;
        size_t size = 0;
        xl6DiagInit(&errors);
        assert(xl6OutputOpenMemory(&output));
        assert(header ? xl6IrCgenEmitHeader(&output, module, "CLOSED_CALLABLE_IDENTITY", &errors)
                      : xl6IrCgenEmitC(&output, module, &errors));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size && text);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&errors);
    }
    printf("callable identity: %zu frontend labels erased before successful lowering/emission\n", erased);
    xl6CompileUnitFree(&unit);
}

int main(int argc, char** argv)
{
    assert(argc >= 3);
    for (int kind = XL6_IR_TYPE_BUILTIN; kind <= XL6_IR_TYPE_PERSISTENCE_WIRE; ++kind)
        assert(xl6IrTypeIgnoresFrontendAbiId((XL6_IrTypeKind)kind) ==
            (kind == XL6_IR_TYPE_FUNCTION || kind == XL6_IR_TYPE_TUPLE || kind == XL6_IR_TYPE_NULLABLE));
    for (int i = 1; i < argc; ++i) exercise(argv[i]);
    assert(fieldConventions && conventionPairs && refusals >= 5);
    printf("callable identity: %zu field conventions, %zu convention collisions, %zu zero-C/header refusals\n",
        fieldConventions, conventionPairs, refusals);
    return 0;
}
