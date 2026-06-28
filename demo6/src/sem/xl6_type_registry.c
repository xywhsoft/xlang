#include "src/sem/xl6_type_registry.h"

typedef struct XL6_BuiltinTypeSeed {
    const char* name;
    int allowGeneric;
    int genericArity;
} XL6_BuiltinTypeSeed;

static const XL6_BuiltinTypeSeed g_xl6BuiltinTypeSeeds[] = {
    {"null", 0, 0},
    {"bool", 0, 0},
    {"int", 0, 0}, {"int8", 0, 0}, {"uint8", 0, 0},
    {"int16", 0, 0}, {"uint16", 0, 0}, {"int32", 0, 0}, {"uint32", 0, 0},
    {"int64", 0, 0}, {"uint64", 0, 0},
    {"float", 0, 0}, {"float32", 0, 0}, {"float64", 0, 0},
    {"c_char", 0, 0}, {"c_schar", 0, 0}, {"c_uchar", 0, 0},
    {"c_short", 0, 0}, {"c_ushort", 0, 0}, {"c_int", 0, 0}, {"c_uint", 0, 0},
    {"c_long", 0, 0}, {"c_ulong", 0, 0}, {"c_longlong", 0, 0}, {"c_ulonglong", 0, 0},
    {"c_size", 0, 0}, {"c_ssize", 0, 0}, {"c_ptrdiff", 0, 0},
    {"c_intptr", 0, 0}, {"c_uintptr", 0, 0}, {"c_bool", 0, 0},
    {"c_float", 0, 0}, {"c_double", 0, 0}, {"c_wchar", 0, 0},
    {"string", 0, 0}, {"time", 0, 0}, {"point", 0, 0}, {"error", 0, 0},
    {"function", 0, 0}, {"var", 0, 0}, {"type", 0, 0},
    {"array", 1, 1}, {"list", 1, 1}, {"set", 1, 1}, {"dict", 1, 1},
    {"future", 1, 1}, {"task", 1, 1}, {"channel", 1, 1}, {"generator", 1, 1},
    {"thread", 0, 0}, {"mutex", 0, 0}, {"cond", 0, 0}, {"rwlock", 0, 0}, {"sem", 0, 0}
};

void xl6TypeRegistryInit(XL6_TypeRegistry* registry)
{
    if ( registry != NULL ) {
        memset(registry, 0, sizeof(*registry));
    }
}

void xl6TypeRegistryFree(XL6_TypeRegistry* registry)
{
    size_t i;

    if ( registry == NULL ) {
        return;
    }
    for ( i = 0; i < registry->count; ++i ) {
        xrtFree(registry->items[i].name);
    }
    xrtFree(registry->items);
    memset(registry, 0, sizeof(*registry));
}

const XL6_TypeInfo* xl6TypeRegistryFind(const XL6_TypeRegistry* registry, const char* name)
{
    size_t i;

    if ( registry == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < registry->count; ++i ) {
        if ( registry->items[i].name != NULL && strcmp(registry->items[i].name, name) == 0 ) {
            return &registry->items[i];
        }
    }
    return NULL;
}

int xl6TypeRegistryAdd(XL6_TypeRegistry* registry, const char* name, XL6_TypeKind kind, int allowGeneric, int genericArity, XL6_AstNode* decl)
{
    XL6_TypeInfo* newItems;
    XL6_TypeInfo* item;
    size_t newCapacity;

    if ( registry == NULL || name == NULL ) {
        return 0;
    }
    if ( xl6TypeRegistryFind(registry, name) != NULL ) {
        return 0;
    }

    if ( registry->count == registry->capacity ) {
        newCapacity = registry->capacity == 0 ? 16 : registry->capacity * 2;
        newItems = (XL6_TypeInfo*)xrtRealloc(registry->items, newCapacity * sizeof(XL6_TypeInfo));
        if ( newItems == NULL ) {
            return 0;
        }
        registry->items = newItems;
        registry->capacity = newCapacity;
    }

    item = &registry->items[registry->count];
    memset(item, 0, sizeof(*item));
    item->name = xrtCopyStr((char*)name, strlen(name));
    if ( item->name == NULL ) {
        return 0;
    }
    item->kind = kind;
    item->allowGeneric = allowGeneric;
    item->genericArity = genericArity;
    item->decl = decl;
    registry->count += 1;
    return 1;
}

int xl6TypeRegistryAddBuiltins(XL6_TypeRegistry* registry)
{
    size_t i;

    for ( i = 0; i < XL6_ARRAY_COUNT(g_xl6BuiltinTypeSeeds); ++i ) {
        const XL6_BuiltinTypeSeed* seed = &g_xl6BuiltinTypeSeeds[i];
        if ( !xl6TypeRegistryAdd(registry, seed->name, XL6_TYPE_BUILTIN, seed->allowGeneric, seed->genericArity, NULL) ) {
            return 0;
        }
    }
    return 1;
}
