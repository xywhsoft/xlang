#include "src/sem/xl6_type_registry.h"

#include <stdio.h>

typedef struct XL6_BuiltinTypeSeed {
    const char* name;
    const char* abiName;
    int allowGeneric;
    int genericArity;
} XL6_BuiltinTypeSeed;

static const XL6_BuiltinTypeSeed g_xl6BuiltinTypeSeeds[] = {
    {"null", "xrt.null", 0, 0},
    {"bool", "xrt.bool", 0, 0},
    {"int", "xrt.int64", 0, 0}, {"int8", "xrt.int8", 0, 0},
    {"uint8", "xrt.uint8", 0, 0}, {"int16", "xrt.int16", 0, 0},
    {"uint16", "xrt.uint16", 0, 0}, {"int32", "xrt.int32", 0, 0},
    {"uint32", "xrt.uint32", 0, 0}, {"char", "xrt.char", 0, 0},
    {"int64", "xrt.int64", 0, 0},
    {"uint64", "xrt.uint64", 0, 0},
    {"float", "xrt.float64", 0, 0}, {"float32", "xrt.float32", 0, 0},
    {"float64", "xrt.float64", 0, 0},
    {"c_char", NULL, 0, 0}, {"c_schar", NULL, 0, 0},
    {"c_uchar", NULL, 0, 0}, {"c_short", NULL, 0, 0},
    {"c_ushort", NULL, 0, 0}, {"c_int", NULL, 0, 0},
    {"c_uint", NULL, 0, 0}, {"c_long", NULL, 0, 0},
    {"c_ulong", NULL, 0, 0}, {"c_longlong", NULL, 0, 0},
    {"c_ulonglong", NULL, 0, 0}, {"c_size", NULL, 0, 0},
    {"c_ssize", NULL, 0, 0}, {"c_ptrdiff", NULL, 0, 0},
    {"c_intptr", NULL, 0, 0}, {"c_uintptr", NULL, 0, 0},
    {"c_bool", NULL, 0, 0}, {"c_float", NULL, 0, 0},
    {"c_double", NULL, 0, 0}, {"c_wchar", NULL, 0, 0},
    {"c_vararg", NULL, 0, 0},
    {"string", "xrt.string", 0, 0}, {"bytes", "xrt.bytes", 0, 0},
    {"time", "xrt.time", 0, 0},
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,...) {Name,Abi,0,0},
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
    {"point", "xrt.pointer", 1, 1}, {"error", NULL, 0, 0},
    {"function", NULL, 0, 0}, {"var", NULL, 0, 0},
    {"type", "xrt.type", 0, 0},
    {"array", NULL, 1, 1}, {"list", NULL, 1, 1},
    {"set", NULL, 1, 1}, {"dict", NULL, 1, 1},
    {"future", NULL, 1, 1}, {"task", NULL, 1, 1},
    {"channel", NULL, 1, 1}, {"generator", NULL, 1, 1},
    {"weak", "xrt.value_weak", 1, 1},
    {"Stack", NULL, 1, 1}, {"OrderedMap", NULL, 1, 2}, {"SlotMap", NULL, 1, 1},
    {"SpscQueue", NULL, 1, 1}, {"MpscQueue", NULL, 1, 1},
    {"MpmcQueue", NULL, 1, 1}, {"Queue", "xlang.MpmcQueue", 1, 1}, {"WaitQueue", NULL, 1, 1},
    {"thread", NULL, 0, 0}, {"mutex", NULL, 0, 0},
    {"cond", NULL, 0, 0}, {"rwlock", NULL, 0, 0},
    {"sem", NULL, 0, 0},
    /* Append new builtins so existing semantic registry identities stay stable. */
    {"cstring", "xlang.cstring.v1", 0, 0}, {"cbytes", "xlang.cbytes.v1", 0, 0}
};

int xl6TypeRegistryIsBuiltinName(const char* name)
{
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(g_xl6BuiltinTypeSeeds); ++i ) {
        if ( strcmp(g_xl6BuiltinTypeSeeds[i].name, name) == 0 ) {
            return 1;
        }
    }
    return 0;
}

int xl6TypeRegistryIsNativeValueAbi(XL6_TypeAbiId abi)
{
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,...) if (abi==xl6TypeAbiIdText(Abi)) return 1;
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
    return 0;
}

static unsigned long long xl6TypeRegistryHash(const char* name)
{
    unsigned long long hash = 14695981039346656037ULL;

    while ( name != NULL && *name != '\0' ) {
        hash ^= (unsigned char)*name++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int xl6TypeRegistryEnsureIndex(XL6_TypeRegistry* registry, size_t requiredCount)
{
    size_t capacity;
    size_t* slots;
    size_t i;

    if ( registry->slotCapacity != 0 && requiredCount * 2 <= registry->slotCapacity ) {
        return 1;
    }
    capacity = registry->slotCapacity == 0 ? 128 : registry->slotCapacity * 2;
    while ( requiredCount * 2 > capacity ) {
        capacity *= 2;
    }
    slots = (size_t*)xrtCalloc(capacity, sizeof(size_t));
    if ( slots == NULL ) {
        return 0;
    }
    for ( i = 0; i < registry->count; ++i ) {
        size_t slot = (size_t)(xl6TypeRegistryHash(registry->items[i].name) & (capacity - 1));
        while ( slots[slot] != 0 ) {
            slot = (slot + 1) & (capacity - 1);
        }
        slots[slot] = i + 1;
    }
    xrtFree(registry->slots);
    registry->slots = slots;
    registry->slotCapacity = capacity;
    return 1;
}

void xl6TypeRegistryInit(XL6_TypeRegistry* registry)
{
    if ( registry != NULL ) {
        memset(registry, 0, sizeof(*registry));
        registry->nextId = 1;
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
        xrtFree(registry->items[i].abiName);
    }
    xrtFree(registry->items);
    xrtFree(registry->slots);
    memset(registry, 0, sizeof(*registry));
}

const XL6_TypeInfo* xl6TypeRegistryFind(const XL6_TypeRegistry* registry, const char* name)
{
    size_t slot;
    size_t start;

    if ( registry == NULL || name == NULL || registry->slotCapacity == 0 ) {
        return NULL;
    }
    slot = (size_t)(xl6TypeRegistryHash(name) & (registry->slotCapacity - 1));
    start = slot;
    while ( registry->slots[slot] != 0 ) {
        size_t index = registry->slots[slot] - 1;
        if ( index < registry->count && strcmp(registry->items[index].name, name) == 0 ) {
            return &registry->items[index];
        }
        slot = (slot + 1) & (registry->slotCapacity - 1);
        if ( slot == start ) {
            break;
        }
    }
    return NULL;
}

const XL6_TypeInfo* xl6TypeRegistryFindId(
    const XL6_TypeRegistry* registry,
    XL6_TypeId id
)
{
    size_t i;

    if ( registry == NULL || id == XL6_TYPE_ID_INVALID ) {
        return NULL;
    }
    for ( i = 0; i < registry->count; ++i ) {
        if ( registry->items[i].id == id ) {
            return &registry->items[i];
        }
    }
    return NULL;
}

XL6_TypeId xl6TypeRegistryReserveId(XL6_TypeRegistry* registry)
{
    XL6_TypeId id;

    if ( registry == NULL || registry->nextId == XL6_TYPE_ID_INVALID ) {
        return XL6_TYPE_ID_INVALID;
    }
    id = registry->nextId;
    registry->nextId += 1;
    return id;
}

int xl6TypeRegistryAdd(
    XL6_TypeRegistry* registry,
    const char* name,
    const char* abiName,
    XL6_TypeKind kind,
    int allowGeneric,
    int genericArity,
    XL6_AstNode* decl
)
{
    XL6_TypeInfo* newItems;
    XL6_TypeInfo* item;
    size_t newCapacity;

    if ( registry == NULL || name == NULL || name[0] == '\0' ||
         abiName == NULL || abiName[0] == '\0' ||
         registry->nextId == XL6_TYPE_ID_INVALID ) {
        return 0;
    }
    if ( xl6TypeRegistryFind(registry, name) != NULL ) {
        return 0;
    }
    if ( !xl6TypeRegistryEnsureIndex(registry, registry->count + 1) ) {
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
    item->name = xrtStrDupN(name, strlen(name));
    if ( item->name == NULL ) {
        return 0;
    }
    item->abiName = xrtStrDupN(abiName, strlen(abiName));
    if ( item->abiName == NULL ) {
        xrtFree(item->name);
        memset(item, 0, sizeof(*item));
        return 0;
    }
    item->id = xl6TypeRegistryReserveId(registry);
    item->abiId = xl6TypeAbiIdText(abiName);
    item->kind = kind;
    item->allowGeneric = allowGeneric;
    item->genericArity = genericArity;
    item->decl = decl;
    if ( decl != NULL ) {
        decl->semanticTypeId = item->id;
        decl->semanticTypeAbiId = item->abiId;
    }
    registry->count += 1;
    {
        size_t slot = (size_t)(xl6TypeRegistryHash(item->name) & (registry->slotCapacity - 1));
        while ( registry->slots[slot] != 0 ) {
            slot = (slot + 1) & (registry->slotCapacity - 1);
        }
        registry->slots[slot] = registry->count;
    }
    return 1;
}

int xl6TypeRegistryAddBuiltins(XL6_TypeRegistry* registry)
{
    size_t i;

    for ( i = 0; i < XL6_ARRAY_COUNT(g_xl6BuiltinTypeSeeds); ++i ) {
        const XL6_BuiltinTypeSeed* seed = &g_xl6BuiltinTypeSeeds[i];
        char abiName[160];
        const char* resolvedAbiName = seed->abiName;

        if ( resolvedAbiName == NULL ) {
            snprintf(abiName, sizeof(abiName), "xlang.%s", seed->name);
            resolvedAbiName = abiName;
        }
        if ( !xl6TypeRegistryAdd(
            registry,
            seed->name,
            resolvedAbiName,
            XL6_TYPE_BUILTIN,
            seed->allowGeneric,
            seed->genericArity,
            NULL
        ) ) {
            return 0;
        }
    }
    return 1;
}
