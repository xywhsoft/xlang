/* One canonical catalog for lowering and verification. The selected component
 * is compiled independently; Cgen never implements a log function. */
typedef enum XL6_LogShape {
    LOG_VOID, LOG_I64, LOG_U64, LOG_BOOL, LOG_STRING, LOG_POINTER, LOG_LEVEL, LOG_RESULT,
    LOG_LOGGER, LOG_SINK, LOG_STATS, LOG_LOGGER_FACTORY, LOG_SINK_FACTORY, LOG_STATS_FACTORY
} XL6_LogShape;
typedef struct XL6_LogContract {
    const char* name;
    XL6_LogShape result;
    unsigned count;
    XL6_LogShape parameters[8];
} XL6_LogContract;
static const XL6_LogContract xl6LogContracts[] = {
    {"xl6LogCreateV1", LOG_LOGGER_FACTORY, 2, {LOG_STRING, LOG_I64}},
    {"xl6LogTakeV1", LOG_LOGGER_FACTORY, 1, {LOG_POINTER}},
    {"xl6LogDestroyV1", LOG_VOID, 1, {LOG_LOGGER}},
    {"xl6LogHandleV1", LOG_POINTER, 1, {LOG_LOGGER}},
    {"xl6LogNameV1", LOG_STRING, 1, {LOG_LOGGER}},
    {"xl6LogLevelV1", LOG_LEVEL, 1, {LOG_LOGGER}},
    {"xl6LogSetLevelV1", LOG_BOOL, 2, {LOG_LOGGER, LOG_I64}},
    {"xl6LogEnabledV1", LOG_BOOL, 2, {LOG_LOGGER, LOG_I64}},
    {"xl6LogAttachV1", LOG_BOOL, 2, {LOG_LOGGER, LOG_SINK}},
    {"xl6LogDetachV1", LOG_BOOL, 2, {LOG_LOGGER, LOG_SINK}},
    {"xl6LogDetachAllV1", LOG_I64, 1, {LOG_LOGGER}},
    {"xl6LogSinkCountV1", LOG_I64, 1, {LOG_LOGGER}},
    {"xl6LogWriteV1", LOG_RESULT, 4, {LOG_LOGGER, LOG_I64, LOG_STRING, LOG_I64}},
    {"xl6LogFlushV1", LOG_BOOL, 1, {LOG_LOGGER}},
    {"xl6LogDefaultV1", LOG_POINTER, 0, {0}},
    {"xl6LogSetDefaultV1", LOG_BOOL, 1, {LOG_POINTER}},
    {"xl6LogDefaultWriteV1", LOG_RESULT, 3, {LOG_I64, LOG_STRING, LOG_I64}},
    {"xl6LogDefaultFlushV1", LOG_BOOL, 0, {0}},
    {"xl6LogLevelNameV1", LOG_STRING, 1, {LOG_I64}},
    {"xl6LogSinkTakeV1", LOG_SINK_FACTORY, 1, {LOG_POINTER}},
    {"xl6LogSinkDestroyV1", LOG_VOID, 1, {LOG_SINK}},
    {"xl6LogSinkHandleV1", LOG_POINTER, 1, {LOG_SINK}},
    {"xl6LogSinkNameV1", LOG_STRING, 1, {LOG_SINK}},
    {"xl6LogSinkLevelV1", LOG_LEVEL, 1, {LOG_SINK}},
    {"xl6LogSinkSetLevelV1", LOG_BOOL, 2, {LOG_SINK, LOG_I64}},
    {"xl6LogSinkFlushV1", LOG_BOOL, 1, {LOG_SINK}},
    {"xl6LogStatsV1", LOG_POINTER, 2, {LOG_POINTER, LOG_BOOL}},
    {"xl6LogStatsTakeV1", LOG_STATS_FACTORY, 1, {LOG_POINTER}},
    {"xl6LogStatsDestroyV1", LOG_VOID, 1, {LOG_STATS}},
    {"xl6LogStatsCounterV1", LOG_U64, 2, {LOG_STATS, LOG_I64}},
    {"xl6LogConsoleV1", LOG_SINK_FACTORY, 8, {LOG_I64, LOG_I64, LOG_I64, LOG_I64, LOG_BOOL, LOG_I64, LOG_I64, LOG_I64}}
};
static const XL6_LogContract* xl6LogContract(const char* name)
{
    if (name) for (size_t i=0; i<sizeof(xl6LogContracts)/sizeof(*xl6LogContracts); ++i)
        if (!strcmp(name, xl6LogContracts[i].name)) return &xl6LogContracts[i];
    return NULL;
}
static const char* xl6LogOwner(XL6_LogShape shape)
{
    switch (shape) {
    case LOG_LOGGER: case LOG_LOGGER_FACTORY: return "xl6LogDestroyV1";
    case LOG_SINK: case LOG_SINK_FACTORY: return "xl6LogSinkDestroyV1";
    case LOG_STATS: case LOG_STATS_FACTORY: return "xl6LogStatsDestroyV1";
    default: return NULL;
    }
}
int xl6IrRuntimeLogBridgeName(const char* name) { return xl6LogContract(name) != NULL; }
int xl6IrRuntimeLogBridgeFactory(const char* name)
{ const XL6_LogContract* c=xl6LogContract(name); return c && c->result >= LOG_LOGGER_FACTORY; }
XL6_IrErrorAbi xl6IrRuntimeLogBridgeErrorAbi(const char* name)
{ const XL6_LogContract* c=xl6LogContract(name); return c && c->result == LOG_VOID ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE; }
const char* xl6IrRuntimeLogBridgeParameterOwner(const char* name, size_t index)
{ const XL6_LogContract* c=xl6LogContract(name); return c && index<c->count ? xl6LogOwner(c->parameters[index]) : NULL; }
static int xl6LogShapeMatches(const XL6_IrModule* m, XL6_IrTypeId id, XL6_LogShape shape)
{
    const XL6_IrType* t=xl6IrTypeGet(&m->types,id);
    if (shape == LOG_VOID) return id == XL6_IR_TYPE_INVALID;
    if (!t) return 0;
    if (shape == LOG_LEVEL || shape == LOG_RESULT)
        return t->kind == XL6_IR_TYPE_NUMERIC_ENUM && t->cAbi == XL6_IR_C_ABI_I64 && t->name &&
            !strcmp(t->name,shape == LOG_LEVEL ? "LogLevel" : "LogResult");
    if (xl6LogOwner(shape)) return t->kind == XL6_IR_TYPE_CLASS && t->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR && t->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    if (t->kind != XL6_IR_TYPE_BUILTIN || t->collectionAbi != XL6_IR_COLLECTION_NONE || t->paramCount) return 0;
    switch (shape) {
    case LOG_I64: return t->cAbi == XL6_IR_C_ABI_I64;
    case LOG_U64: return t->cAbi == XL6_IR_C_ABI_U64;
    case LOG_BOOL: return t->cAbi == XL6_IR_C_ABI_BOOL;
    case LOG_STRING: return t->cAbi == XL6_IR_C_ABI_MANAGED_STRING && t->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    case LOG_POINTER: return t->cAbi == XL6_IR_C_ABI_RAW_POINTER;
    default: return 0;
    }
}
int xl6IrRuntimeLogBridgeTargetMatches(const XL6_IrModule* m, const XL6_IrCallTarget* t)
{
    const XL6_LogContract* c=t ? xl6LogContract(t->cName) : NULL;
    int factory=c && c->result >= LOG_LOGGER_FACTORY;
    if (!m || !c || t->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_LOG ||
        t->kind != (factory ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C) ||
        t->parameterCount != c->count || t->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        t->effectAbi != XL6_IR_CALL_EFFECT_CONSERVATIVE ||
        t->executionBoundaryAbi != XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK ||
        t->errorAbi != xl6IrRuntimeLogBridgeErrorAbi(c->name) ||
        !xl6LogShapeMatches(m,t->returnType,c->result) ||
        t->resultPhysicalAbi != (factory ? XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE : XL6_IR_CALL_RESULT_PHYSICAL_DIRECT) ||
        (c->count && (!t->parameterTypes || !t->parameterAbis || !t->parameterPhysicalAbis || !t->parameterPhysicalFieldDeclarations || !t->parameterConstraintAbis))) return 0;
    for (size_t i=0;i<c->count;++i) {
        const char* owner=xl6LogOwner(c->parameters[i]);
        if (!xl6LogShapeMatches(m,t->parameterTypes[i],c->parameters[i]) ||
            t->parameterAbis[i] != (owner || c->parameters[i] == LOG_STRING ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE) ||
            t->parameterPhysicalAbis[i] != (owner ? XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW : XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT) ||
            t->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            (owner ? t->parameterPhysicalFieldDeclarations[i] == XL6_IR_NODE_INVALID : t->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID)) return 0;
        if (owner && m->stage == XL6_IR_STAGE_LOWERED) {
            const XL6_IrClassLayout* layout=xl6IrBuildClassLayoutByType(m,t->parameterTypes[i]);
            const XL6_IrCallTarget* destroy;
            if (!layout || layout->fieldCount != 1) return 0;
            const XL6_IrClassField* field=&m->classFields[layout->firstField];
            destroy=xl6IrCallTargetGet(m,field->nativeDestroySymbol);
            if (field->declaration != t->parameterPhysicalFieldDeclarations[i] || !destroy || !destroy->cName || strcmp(destroy->cName,owner)) return 0;
        }
    }
    if (factory) {
        if (t->resultPhysicalFieldDeclaration == XL6_IR_NODE_INVALID || !t->resultPhysicalDestroySymbol) return 0;
        if (m->stage == XL6_IR_STAGE_LOWERED) {
            const XL6_IrCallTarget* destroy=xl6IrCallTargetGet(m,t->resultPhysicalDestroySymbol);
            if (!destroy || !destroy->cName || strcmp(destroy->cName,xl6LogOwner(c->result))) return 0;
        }
    } else if (t->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID || t->resultPhysicalDestroySymbol) return 0;
    if (t->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID && t->errorPhysicalAbi !=
        (c->result == LOG_VOID ? XL6_IR_CALL_ERROR_PHYSICAL_NONE : XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR)) return 0;
    return 1;
}
