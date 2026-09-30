/* Closed resource identities and physical ABI are verified before C emission. */
typedef enum XL6_IrVfsShape {
    XL6_VFS_STRING, XL6_VFS_BYTES, XL6_VFS_INT, XL6_VFS_U64,
    XL6_VFS_BOOL, XL6_VFS_HANDLE, XL6_VFS_VOID, XL6_VFS_FACTORY
} XL6_IrVfsShape;
typedef struct XL6_IrVfsContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrVfsShape result;
    size_t count;
    XL6_IrVfsShape parameters[7];
    const char* parameterOwners[7];
    const char* resultOwner;
} XL6_IrVfsContract;
static const XL6_IrVfsContract xl6IrVfsContracts[] = {
    {"xl6_xrt_vfs_create", XL6_IR_RUNTIME_BRIDGE_VFS_CREATE, XL6_VFS_FACTORY,
        0, {0},
        {NULL}, "xl6_xrt_vfs_destroy"},
    {"xl6_xrt_vfs_destroy", XL6_IR_RUNTIME_BRIDGE_VFS_DESTROY, XL6_VFS_VOID,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_destroy"}, NULL},
    {"xl6_xrt_vfs_read", XL6_IR_RUNTIME_BRIDGE_VFS_READ, XL6_VFS_BYTES,
        3, {XL6_VFS_HANDLE,XL6_VFS_STRING,XL6_VFS_INT},
        {"xl6_xrt_vfs_destroy",NULL,NULL}, NULL},
    {"xl6_xrt_vfs_memory_create", XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_CREATE, XL6_VFS_FACTORY,
        0, {0},
        {NULL}, "xl6_xrt_vfs_memory_destroy"},
    {"xl6_xrt_vfs_memory_destroy", XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_DESTROY, XL6_VFS_VOID,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_memory_destroy"}, NULL},
    {"xl6_xrt_vfs_memory_put_bytes", XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_PUT_BYTES, XL6_VFS_BOOL,
        4, {XL6_VFS_HANDLE,XL6_VFS_STRING,XL6_VFS_BYTES,XL6_VFS_INT},
        {"xl6_xrt_vfs_memory_destroy",NULL,NULL,NULL}, NULL},
    {"xl6_xrt_vfs_memory_put_string", XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_PUT_STRING, XL6_VFS_BOOL,
        4, {XL6_VFS_HANDLE,XL6_VFS_STRING,XL6_VFS_STRING,XL6_VFS_INT},
        {"xl6_xrt_vfs_memory_destroy",NULL,NULL,NULL}, NULL},
    {"xl6_xrt_vfs_memory_seal", XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_SEAL, XL6_VFS_BOOL,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_memory_destroy"}, NULL},
    {"xl6_xrt_vfs_disk_create", XL6_IR_RUNTIME_BRIDGE_VFS_DISK_CREATE, XL6_VFS_FACTORY,
        2, {XL6_VFS_STRING,XL6_VFS_INT},
        {NULL,NULL}, "xl6_xrt_vfs_disk_destroy"},
    {"xl6_xrt_vfs_disk_destroy", XL6_IR_RUNTIME_BRIDGE_VFS_DISK_DESTROY, XL6_VFS_VOID,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_disk_destroy"}, NULL},
    {"xl6_xrt_vfs_pack_create", XL6_IR_RUNTIME_BRIDGE_VFS_PACK_CREATE, XL6_VFS_FACTORY,
        7, {XL6_VFS_STRING,XL6_VFS_INT,XL6_VFS_INT,XL6_VFS_INT,XL6_VFS_INT,XL6_VFS_INT,XL6_VFS_INT},
        {NULL,NULL,NULL,NULL,NULL,NULL,NULL}, "xl6_xrt_vfs_pack_destroy"},
    {"xl6_xrt_vfs_pack_destroy", XL6_IR_RUNTIME_BRIDGE_VFS_PACK_DESTROY, XL6_VFS_VOID,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_pack_destroy"}, NULL},
    {"xl6_xrt_vfs_pack_format", XL6_IR_RUNTIME_BRIDGE_VFS_PACK_FORMAT, XL6_VFS_INT,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_pack_destroy"}, NULL},
    {"xl6_xrt_vfs_mount_memory", XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_MEMORY, XL6_VFS_FACTORY,
        5, {XL6_VFS_HANDLE,XL6_VFS_HANDLE,XL6_VFS_STRING,XL6_VFS_INT,XL6_VFS_INT},
        {"xl6_xrt_vfs_destroy","xl6_xrt_vfs_memory_destroy",NULL,NULL,NULL}, "xl6_xrt_vfs_mount_destroy"},
    {"xl6_xrt_vfs_mount_disk", XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_DISK, XL6_VFS_FACTORY,
        5, {XL6_VFS_HANDLE,XL6_VFS_HANDLE,XL6_VFS_STRING,XL6_VFS_INT,XL6_VFS_INT},
        {"xl6_xrt_vfs_destroy","xl6_xrt_vfs_disk_destroy",NULL,NULL,NULL}, "xl6_xrt_vfs_mount_destroy"},
    {"xl6_xrt_vfs_mount_pack", XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_PACK, XL6_VFS_FACTORY,
        5, {XL6_VFS_HANDLE,XL6_VFS_HANDLE,XL6_VFS_STRING,XL6_VFS_INT,XL6_VFS_INT},
        {"xl6_xrt_vfs_destroy","xl6_xrt_vfs_pack_destroy",NULL,NULL,NULL}, "xl6_xrt_vfs_mount_destroy"},
    {"xl6_xrt_vfs_mount_destroy", XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_DESTROY, XL6_VFS_VOID,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_mount_destroy"}, NULL},
    {"xl6_xrt_vfs_unmount", XL6_IR_RUNTIME_BRIDGE_VFS_UNMOUNT, XL6_VFS_BOOL,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_mount_destroy"}, NULL},
    {"xl6_xrt_vfs_mount_id", XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_ID, XL6_VFS_U64,
        1, {XL6_VFS_HANDLE},
        {"xl6_xrt_vfs_mount_destroy"}, NULL},
};
static const XL6_IrVfsContract* xl6IrVfsContract(const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrVfsContracts)/sizeof(xl6IrVfsContracts[0]); ++i)
        if (strcmp(name, xl6IrVfsContracts[i].name) == 0) return &xl6IrVfsContracts[i];
    return NULL;
}
XL6_IrRuntimeBridgeKind xl6IrRuntimeVfsBridgeKind(const char* name)
{
    const XL6_IrVfsContract* c = xl6IrVfsContract(name);
    return c != NULL ? c->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}
const char* xl6IrRuntimeVfsBridgeParameterOwner(const char* name, size_t parameter)
{
    const XL6_IrVfsContract* c = xl6IrVfsContract(name);
    return c != NULL && parameter < c->count ? c->parameterOwners[parameter] : NULL;
}
static int xl6IrVfsTypeMatches(const XL6_IrModule* module, XL6_IrTypeId id, XL6_IrVfsShape shape)
{
    const XL6_IrType* t = xl6IrTypeGet(&module->types, id);
    if (shape == XL6_VFS_VOID) return id == XL6_IR_TYPE_INVALID;
    if (t == NULL) return 0;
    if (shape == XL6_VFS_HANDLE || shape == XL6_VFS_FACTORY)
        return t->kind == XL6_IR_TYPE_CLASS && t->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
               t->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    if (t->kind != XL6_IR_TYPE_BUILTIN || t->collectionAbi != XL6_IR_COLLECTION_NONE || t->paramCount != 0) return 0;
    switch (shape) {
    case XL6_VFS_STRING: return t->cAbi == XL6_IR_C_ABI_MANAGED_STRING && t->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    case XL6_VFS_BYTES: return t->cAbi == XL6_IR_C_ABI_MANAGED_BYTES && t->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    case XL6_VFS_INT: return t->cAbi == XL6_IR_C_ABI_I64;
    case XL6_VFS_U64: return t->cAbi == XL6_IR_C_ABI_U64;
    case XL6_VFS_BOOL: return t->cAbi == XL6_IR_C_ABI_BOOL;
    default: return 0;
    }
}
static int xl6IrVfsOwnerMatches(const XL6_IrModule* module, XL6_SymbolId symbol, const char* name)
{
    const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, symbol);
    return destroy != NULL && destroy->cName != NULL && name != NULL && strcmp(destroy->cName,name) == 0;
}
int xl6IrRuntimeVfsBridgeTargetMatches(const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrVfsContract* c;
    size_t i;
    int factory;
    if (module == NULL || target == NULL || (c=xl6IrVfsContract(target->cName)) == NULL) return 0;
    factory = c->result == XL6_VFS_FACTORY;
    if (target->runtimeBridge != c->kind ||
        target->kind != (factory ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C) ||
        target->errorAbi != (c->result == XL6_VFS_VOID ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE) ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != (c->result == XL6_VFS_VOID ? XL6_IR_CALL_ERROR_PHYSICAL_NONE : XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR)) ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE || target->parameterCount != c->count ||
        !xl6IrVfsTypeMatches(module, target->returnType, c->result) ||
        target->resultPhysicalAbi != (factory ? XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE : XL6_IR_CALL_RESULT_PHYSICAL_DIRECT) ||
        (factory ? (target->resultPhysicalFieldDeclaration == XL6_IR_NODE_INVALID || target->resultPhysicalDestroySymbol == 0) :
                   (target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID || target->resultPhysicalDestroySymbol != 0)) ||
        (c->count != 0 && (target->parameterTypes == NULL || target->parameterAbis == NULL ||
         target->parameterPhysicalAbis == NULL || target->parameterPhysicalFieldDeclarations == NULL || target->parameterConstraintAbis == NULL)))
        return 0;
    for (i = 0; i < c->count; ++i) {
        XL6_IrVfsShape s = c->parameters[i];
        int handle = s == XL6_VFS_HANDLE;
        if (!xl6IrVfsTypeMatches(module, target->parameterTypes[i], s) ||
            target->parameterAbis[i] != ((handle || s == XL6_VFS_STRING || s == XL6_VFS_BYTES) ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE) ||
            target->parameterPhysicalAbis[i] != (handle ? XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW : XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT) ||
            target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            (handle ? target->parameterPhysicalFieldDeclarations[i] == XL6_IR_NODE_INVALID : target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID))
            return 0;
        if (handle && module->stage == XL6_IR_STAGE_LOWERED) {
            const XL6_IrClassLayout* layout = xl6IrBuildClassLayoutByType(module, target->parameterTypes[i]);
            const XL6_IrClassField* field;
            if (layout == NULL || layout->fieldCount != 1) return 0;
            field = &module->classFields[layout->firstField];
            if (field->declaration != target->parameterPhysicalFieldDeclarations[i] ||
                !xl6IrVfsOwnerMatches(module,field->nativeDestroySymbol,c->parameterOwners[i])) return 0;
        }
    }
    if (factory && module->stage == XL6_IR_STAGE_LOWERED &&
        !xl6IrVfsOwnerMatches(module,target->resultPhysicalDestroySymbol,c->resultOwner)) return 0;
    return 1;
}
