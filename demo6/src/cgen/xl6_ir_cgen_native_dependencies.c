/* The same verified C implementation selection feeds emission and artifact
 * dependencies. This is backend library metadata, not a new semantic IR. */
static int xl6IrCgenVisitValueGraphProviders(unsigned providers,int projection,
    XL6_IrCgenNativeModuleVisitor visitor,void* context)
{
    if (providers&~7u) return 0;
    return (!(providers&XL6_IR_GRAPH_LIST_V1) || visitor(context,projection ?
        "xl6PersistenceListGraphProviderV1" : "xl6ListGraphProviderV1")) &&
        (!(providers&XL6_IR_GRAPH_SET_V1) || visitor(context,"xl6SetGraphProviderV1")) &&
        (!(providers&XL6_IR_GRAPH_DICT_V1) || visitor(context,"xl6DictGraphProviderV1"));
}
static int xl6IrCgenBuildNeeds(const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags, const XL6_IrCgenOptions* options)
{
    memset(needs,0,sizeof(*needs));
    if (!xl6IrCgenVerifyEmittable(module,diags)) return 0;
    if (!xl6IrCgenAnalyzePrimaryNeeds(module,needs,diags) ||
        !xl6IrCgenAnalyzeRuntimeNeeds(module,needs,diags)) return 0;
    needs->needsDebugConsoleRuntime = options != NULL && options->debugMode;
    needs->needsNetLifecycleRuntime=needs->needsNetTcpRuntime || needs->needsNetTlsRuntime || needs->needsNetDatagramRuntime;
    if (!!(needs->needsNetTcpRuntime || needs->needsNetDatagramRuntime ||
            needs->needsNetTlsDialRuntime || needs->needsNetTlsServerRuntime) !=
        (module->abi.networkServiceAbi == XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1))
        return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,
            "XLIR network service ABI disagrees with verified implementation dependencies");
    for(size_t b=0;b<module->callbackBoundaryCount;++b) {
        const XL6_IrCallbackBoundary* boundary=&module->callbackBoundaries[b];
        if(!xl6IrCgenInstructionIsReachable(module,boundary->instruction))continue;
        for(size_t c=0;c<boundary->captureCount;++c)
            if(module->callbackCaptures[boundary->firstCapture+c].destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)
                needs->needsNativeCaptureProfileHeaders=1;
    }
    for (size_t i=0;i<module->storageCount;++i)
        if (module->storages[i].kind==XL6_IR_STORAGE_GLOBAL &&
            xl6IrCgenOwnershipView(module->storages[i].moduleTraceAbi)!=NULL)
            needs->needsModuleOwnershipViews=1;
    if (module->abi.entryKind==XL6_IR_MODULE_ENTRY_XLANG_MAIN ||
        (options && options->emitMainWrapper) || needs->needsNetTcpRuntime ||
        needs->needsNetDatagramRuntime || needs->needsNetTlsDialRuntime || needs->needsNetTlsServerRuntime) {
        needs->needsForeignErrorRuntime=1;
        needs->needsErrorRuntime=1;
    }
    return 1;
}

/* Only canonical reachable raw entries emitted by definitions need these
 * resident symbols. Ordinary closure values and erased callback parameters
 * do not imply a raw entry; no generated-text or source-name inference. */
static int xl6IrCgenHasRawExecutionActivity(const XL6_IrModule* module)
{
    for (size_t i = 0; i < module->closureTargetCount; ++i)
        if (module->closureTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
            module->closureTargets[i].rawExecutionAbi ==
                XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1) return 1;
    return 0;
}

typedef enum XL6_IrCgenNativeGroup {
#define XL6_IR_CGEN_NATIVE_GROUP(Name, ...) XL6_IR_CGEN_NATIVE_##Name,
#include "src/cgen/xl6_ir_cgen_native_groups.inc"
#undef XL6_IR_CGEN_NATIVE_GROUP
    XL6_IR_CGEN_NATIVE_COUNT
} XL6_IrCgenNativeGroup;

static int xl6IrCgenVisitNativeGroup(XL6_IrCgenNativeGroup group, uint64_t operations,
    XL6_IrCgenNativeModuleVisitor visitor, void* context)
{
    static const char* const modules[][32] = {
#define XL6_IR_CGEN_NATIVE_GROUP(Name, ...) {__VA_ARGS__},
#include "src/cgen/xl6_ir_cgen_native_groups.inc"
#undef XL6_IR_CGEN_NATIVE_GROUP
    };
    _Static_assert(sizeof(modules)/sizeof(modules[0])==XL6_IR_CGEN_NATIVE_COUNT,
        "Every native implementation group needs a dependency descriptor");
    for (size_t i=0;i<32 && modules[group][i];++i)
        if (!visitor(context,modules[group][i])) return 0;
#define REQUIRE(Name) do { if (!visitor(context,Name)) return 0; } while (0)
    switch (group) {
    case XL6_IR_CGEN_NATIVE_FILESYSTEM0:
    case XL6_IR_CGEN_NATIVE_FILESYSTEM1:
    case XL6_IR_CGEN_NATIVE_FILESYSTEM2: {
        unsigned base = (unsigned)(group - XL6_IR_CGEN_NATIVE_FILESYSTEM0) * 64;
        for (unsigned bit = 0; bit < 64; ++bit) if (operations & (UINT64_C(1) << bit)) {
            XL6_IrRuntimeBridgeKind kind = (XL6_IrRuntimeBridgeKind)(XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN + base + bit);
            const char* name = xl6IrRuntimeFsBridgeModule(kind);
            if (name == NULL || !visitor(context, name)) return 0;
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_OPEN) { REQUIRE("XRT_MODULE_FILE"); REQUIRE("XRT_MODULE_ATOMIC"); }
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TEXT_READ || kind == XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TEXT_WRITE)
                { REQUIRE("XRT_MODULE_CHARSET"); REQUIRE("XRT_MODULE_CHARSET_DETECT"); }
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_COPY || kind == XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_REMOVE ||
                kind == XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_STATS || kind == XL6_IR_RUNTIME_BRIDGE_FS_STATS_FROM_VALUE)
                REQUIRE("XRT_MODULE_VALUE_COLLECTION");
            if (xl6IrRuntimeFsBridgeUsesOwner(kind, "xl6_xrt_fs_file_destroy") ||
                xl6IrRuntimeFsBridgeUsesOwner(kind, "xl6_xrt_fs_map_destroy") ||
                xl6IrRuntimeFsBridgeUsesOwner(kind, "xl6_xrt_fs_lock_destroy")) {
                REQUIRE("XRT_MODULE_FILE"); REQUIRE("XRT_MODULE_ATOMIC");
            }
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_INFO_NAME || kind == XL6_IR_RUNTIME_BRIDGE_FS_DIR_LIST)
                REQUIRE("XRT_MODULE_UNICODE");
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_PATH_BUILD) REQUIRE("XRT_MODULE_VALUE_COLLECTION");
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_DIR_ROOTS || kind == XL6_IR_RUNTIME_BRIDGE_FS_DIR_LIST)
                REQUIRE("XRT_MODULE_VALUE_COLLECTION");
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_WALK_VISIT || kind == XL6_IR_RUNTIME_BRIDGE_FS_DIR_ENTRIES) {
                REQUIRE("XRT_MODULE_VALUE_CONTAINER"); REQUIRE("XRT_MODULE_UNICODE");
            }
            if (kind == XL6_IR_RUNTIME_BRIDGE_FS_DIR_GLOB || kind == XL6_IR_RUNTIME_BRIDGE_FS_DIR_ENTRIES) {
                REQUIRE("XRT_MODULE_VALUE_COLLECTION"); REQUIRE("XRT_MODULE_STRING_GLOB");
                REQUIRE("XRT_MODULE_UNICODE");
            }
        }
        break;
    }
    case XL6_IR_CGEN_NATIVE_CONSOLE_INPUT_SCREEN:
        REQUIRE("XRT_MODULE_CONSOLE");
        if (operations & (XL6_CONSOLE_BIT(READ_LINE) | XL6_CONSOLE_BIT(READ_CHAR)))
            REQUIRE("XRT_MODULE_CONSOLE_INPUT");
        if (operations & (XL6_CONSOLE_BIT(SIZE) | XL6_CONSOLE_BIT(COLOR) | XL6_CONSOLE_BIT(STYLED)))
            REQUIRE("XRT_MODULE_CONSOLE_SCREEN");
        if (operations & ~(XL6_CONSOLE_BIT(READ_LINE) | XL6_CONSOLE_BIT(READ_CHAR) | XL6_CONSOLE_BIT(IS_TERMINAL) |
            XL6_CONSOLE_BIT(SIZE) | XL6_CONSOLE_BIT(COLOR) | XL6_CONSOLE_BIT(STYLED)))
            REQUIRE("XRT_MODULE_CONSOLE_TERMINAL");
        break;
    case XL6_IR_CGEN_NATIVE_VFS:
        if (operations & (XL6_VFS_BIT(MEMORY_CREATE) | XL6_VFS_BIT(MEMORY_DESTROY) |
            XL6_VFS_BIT(MEMORY_PUT_BYTES) | XL6_VFS_BIT(MEMORY_PUT_STRING) |
            XL6_VFS_BIT(MEMORY_SEAL) | XL6_VFS_BIT(MOUNT_MEMORY)))
            REQUIRE("XRT_MODULE_VFS_MEMORY");
        if (operations & (XL6_VFS_BIT(DISK_CREATE) | XL6_VFS_BIT(DISK_DESTROY) | XL6_VFS_BIT(MOUNT_DISK)))
            REQUIRE("XRT_MODULE_VFS_DISK");
        if (operations & (XL6_VFS_BIT(PACK_CREATE) | XL6_VFS_BIT(PACK_DESTROY) |
            XL6_VFS_BIT(PACK_FORMAT) | XL6_VFS_BIT(MOUNT_PACK)))
            REQUIRE("XRT_MODULE_VFS_PACK");
        break;
    case XL6_IR_CGEN_NATIVE_IO_FILES:
        REQUIRE("XRT_MODULE_IO");
        if (operations & (XL6_IO_BIT(READ_FILE) | XL6_IO_BIT(WRITE_FILE) |
            XL6_IO_BIT(WRITE_TEXT_FILE) | XL6_IO_BIT(WRITE_BUFFER_FILE) |
            XL6_IO_BIT(READER_OPEN) | XL6_IO_BIT(WRITER_OPEN))) REQUIRE("XRT_MODULE_IO_FILE");
        if (operations & (XL6_IO_BIT(READ_FILE) | XL6_IO_BIT(READER_BYTES) |
            XL6_IO_BIT(READER_BUFFER) | XL6_IO_BIT(READER_ALL) |
            XL6_IO_BIT(WRITER_BUFFER))) REQUIRE("XRT_MODULE_IO_BUFFER");
        if (operations & (XL6_IO_BIT(READER_INTO) | XL6_IO_BIT(WRITER_WRITE_BUFFER) |
            XL6_IO_BIT(WRITER_ALL_BUFFER) | XL6_IO_BIT(WRITER_TO_BYTES) |
            XL6_IO_BIT(WRITE_BUFFER_FILE))) REQUIRE("XRT_MODULE_BUFFER");
        if (operations & (XL6_IO_BIT(READER_STDIN) | XL6_IO_BIT(WRITER_STANDARD)))
            REQUIRE("XRT_MODULE_IO_STANDARD");
        if (operations & (XL6_IO_BIT(LINES_CREATE) | XL6_IO_BIT(LINES_DESTROY) |
            XL6_IO_BIT(LINES_CLOSE) | XL6_IO_BIT(LINES_ADVANCE) |
            XL6_IO_BIT(LINES_BYTES) | XL6_IO_BIT(LINES_ENDING))) REQUIRE("XRT_MODULE_IO_LINE");
        break;
    case XL6_IR_CGEN_NATIVE_RANDOM:
        if (operations & XL6_CGEN_RNG_EXPLICIT_MASK) {
            REQUIRE("XRT_MODULE_RANDOM");
            REQUIRE("XRT_MODULE_THREAD");
        }
        if (operations & XL6_CGEN_RNG_DEFAULT_MASK) REQUIRE("XRT_MODULE_RANDOM_DEFAULT");
        if (operations & XL6_CGEN_RNG_STRING) REQUIRE("XRT_MODULE_RANDOM_TEXT");
        if (operations & XL6_CGEN_RAND_STRING) REQUIRE("XRT_MODULE_RANDOM_TEXT_DEFAULT");
        if (operations & XL6_CGEN_RNG_SECURE_MASK) REQUIRE("XRT_MODULE_RANDOM_SECURE");
        if (operations & XL6_CGEN_SECURE_STRING) REQUIRE("XRT_MODULE_RANDOM_SECURE_TEXT");
        if (operations & XL6_CGEN_SECURE_HEX) REQUIRE("XRT_MODULE_CODEC_HEX");
        if (operations & (XL6_CGEN_RNG_BYTES_MASK | XL6_CGEN_RNG_ARRAY_MASK)) REQUIRE("XRT_MODULE_VALUE_COLLECTION");
        if (operations & XL6_CGEN_RNG_BUFFER_MASK) REQUIRE("XRT_MODULE_BUFFER");
        break;
    case XL6_IR_CGEN_NATIVE_XID:
        if (operations & (1u << (XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY -
                XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING)))
            REQUIRE("XRT_MODULE_VALUE_COLLECTION");
        break;
    case XL6_IR_CGEN_NATIVE_TLS:
        /* The selected TLS adapter exposes certificate-to-hex conversion. */
        REQUIRE("XRT_MODULE_CODEC_HEX");
        break;
    case XL6_IR_CGEN_NATIVE_JSON_VALUE:
        if (operations & (XL6_CGEN_JSON_PARSE|XL6_CGEN_JSON_VALID)) REQUIRE("XRT_MODULE_JSON_READ");
        if (operations & XL6_CGEN_JSON_STRINGIFY) REQUIRE("XRT_MODULE_JSON_WRITE");
        if (operations & (XL6_CGEN_JSON_PARSE_FILE|XL6_CGEN_JSON_WRITE_FILE)) REQUIRE("XRT_MODULE_JSON_FILE");
        break;
    case XL6_IR_CGEN_NATIVE_STRUCTURED_OPTIONS:
        if (operations & XL6_CGEN_CONFIG_XLON_DECODE) REQUIRE("XRT_MODULE_XLON_READ");
        if (operations & XL6_CGEN_CONFIG_JSON_VALID) REQUIRE("XRT_MODULE_JSON_READ");
        if (operations & XL6_CGEN_CONFIG_XLON_VALID) REQUIRE("XRT_MODULE_XLON_READ");
        if (operations & XL6_CGEN_CONFIG_JSONL_VALID) REQUIRE("XRT_MODULE_JSONL_READ");
        if (operations & XL6_CGEN_CONFIG_XLONL_VALID) REQUIRE("XRT_MODULE_XLONL_READ");
        if (operations & XL6_CGEN_CONFIG_JSON_VISIT) REQUIRE("XRT_MODULE_JSON_READ");
        if (operations & XL6_CGEN_CONFIG_XLON_VISIT) REQUIRE("XRT_MODULE_XLON_READ");
        if (operations & XL6_CGEN_CONFIG_JSON_QUOTE) { REQUIRE("XRT_MODULE_JSON_ESCAPE"); REQUIRE("XRT_MODULE_BUFFER"); }
        if (operations & XL6_CGEN_CONFIG_JSON_QUOTE_TO) REQUIRE("XRT_MODULE_JSON_ESCAPE");
        if (operations & XL6_CGEN_CONFIG_JSON_WRITE_TO) REQUIRE("XRT_MODULE_JSON_WRITE");
        if (operations & XL6_CGEN_CONFIG_XLON_WRITE_TO) REQUIRE("XRT_MODULE_XLON_WRITE");
        if (operations & XL6_CGEN_CONFIG_JSONL_WRITE_TO) REQUIRE("XRT_MODULE_JSONL_WRITE");
        if (operations & XL6_CGEN_CONFIG_XLONL_WRITE_TO) REQUIRE("XRT_MODULE_XLONL_WRITE");
        if (operations & XL6_CGEN_CONFIG_JSON_PARSE) REQUIRE("XRT_MODULE_JSON_READ");
        if (operations & XL6_CGEN_CONFIG_JSON_READ_FILE) REQUIRE("XRT_MODULE_JSON_FILE");
        if (operations & XL6_CGEN_CONFIG_JSON_STRINGIFY) { REQUIRE("XRT_MODULE_JSON_WRITE"); REQUIRE("XRT_MODULE_BUFFER"); }
        if (operations & XL6_CGEN_CONFIG_JSON_WRITE_FILE) REQUIRE("XRT_MODULE_JSON_FILE");
        if (operations & XL6_CGEN_CONFIG_XLON_PARSE) REQUIRE("XRT_MODULE_XLON_READ");
        if (operations & XL6_CGEN_CONFIG_XLON_READ_FILE) REQUIRE("XRT_MODULE_XLON_FILE");
        if (operations & XL6_CGEN_CONFIG_XLON_STRINGIFY) { REQUIRE("XRT_MODULE_XLON_WRITE"); REQUIRE("XRT_MODULE_BUFFER"); }
        if (operations & XL6_CGEN_CONFIG_XLON_WRITE_FILE) REQUIRE("XRT_MODULE_XLON_FILE");
        if (operations & XL6_CGEN_CONFIG_JSONL_PARSE) REQUIRE("XRT_MODULE_JSONL_READ");
        if (operations & XL6_CGEN_CONFIG_JSONL_READ_FILE) REQUIRE("XRT_MODULE_JSONL_FILE");
        if (operations & XL6_CGEN_CONFIG_JSONL_STRINGIFY) { REQUIRE("XRT_MODULE_JSONL_WRITE"); REQUIRE("XRT_MODULE_BUFFER"); }
        if (operations & XL6_CGEN_CONFIG_JSONL_WRITE_FILE) REQUIRE("XRT_MODULE_JSONL_FILE");
        if (operations & XL6_CGEN_CONFIG_XLONL_PARSE) REQUIRE("XRT_MODULE_XLONL_READ");
        if (operations & XL6_CGEN_CONFIG_XLONL_READ_FILE) REQUIRE("XRT_MODULE_XLONL_FILE");
        if (operations & XL6_CGEN_CONFIG_XLONL_STRINGIFY) { REQUIRE("XRT_MODULE_XLONL_WRITE"); REQUIRE("XRT_MODULE_BUFFER"); }
        if (operations & XL6_CGEN_CONFIG_XLONL_WRITE_FILE) REQUIRE("XRT_MODULE_XLONL_FILE");
        break;
    case XL6_IR_CGEN_NATIVE_XLON:
        if (operations & (XL6_CGEN_XLON_PARSE|XL6_CGEN_XLON_VALID|XL6_CGEN_XLON_VISIT)) REQUIRE("XRT_MODULE_XLON_READ");
        if (operations & (XL6_CGEN_XLON_STRINGIFY|XL6_CGEN_XLON_WRITER)) REQUIRE("XRT_MODULE_XLON_WRITE");
        if (operations & (XL6_CGEN_XLON_READ|XL6_CGEN_XLON_WRITE)) REQUIRE("XRT_MODULE_XLON_FILE");
        break;
    case XL6_IR_CGEN_NATIVE_PERSISTENCE:
        if (operations & XL6_IR_PERSIST_FORMAT_JSON) REQUIRE("XRT_MODULE_JSON_WRITE");
        if (operations & XL6_IR_PERSIST_FORMAT_XLON) REQUIRE("XRT_MODULE_XLON_WRITE");
        break;
    default: break;
    }
#undef REQUIRE
    return 1;
}

int xl6IrCgenVisitNativeModules(const XL6_IrModule* module, XL6_DiagList* diags,
    const XL6_IrCgenOptions* options, XL6_IrCgenNativeModuleVisitor visitor, void* context)
{
    XL6_IrCgenNeeds needs;
    if (!visitor) return 0;
    if (!xl6IrCgenBuildNeeds(module,&needs,diags,options)) return 0;
    if ((module->exportCount != 0 || needs.needsBoundMethodRuntime || xl6IrCgenHasRawExecutionActivity(module) ||
         (module->abi.hostExecutionAbi == XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1 &&
          (module->abi.entryKind != XL6_IR_MODULE_ENTRY_NONE || (options && options->emitScriptEntry)))) &&
        (!visitor(context,"XRT_MODULE_MEMORY") || !visitor(context,"XRT_MODULE_ATOMIC") ||
         !visitor(context,"XRT_MODULE_THREAD"))) return 0;
    if ((module->abi.entryKind!=XL6_IR_MODULE_ENTRY_NONE || (options && options->emitScriptEntry)) &&
        !visitor(context,"XRT_MODULE_CONSOLE")) return 0;
#define XL6_IR_CGEN_NEED(Type, Field, Group) \
    if (needs.Field && !xl6IrCgenVisitNativeGroup(XL6_IR_CGEN_NATIVE_##Group, \
            (uint64_t)needs.Field,visitor,context)) return 0;
#include "src/cgen/xl6_ir_cgen_needs.inc"
#undef XL6_IR_CGEN_NEED
    if ((needs.needsPersistenceRestoreFormats & XL6_IR_PERSIST_FORMAT_JSON) &&
        !visitor(context,"XRT_MODULE_JSON_READ")) return 0;
    if ((needs.needsPersistenceRestoreFormats & XL6_IR_PERSIST_FORMAT_XLON) &&
        !visitor(context,"XRT_MODULE_XLON_READ")) return 0;
    if ((needs.needsPersistenceFileFormats & XL6_IR_PERSIST_FORMAT_JSON) &&
        !visitor(context,"XRT_MODULE_JSON_FILE")) return 0;
    if ((needs.needsPersistenceFileFormats & XL6_IR_PERSIST_FORMAT_XLON) &&
        !visitor(context,"XRT_MODULE_XLON_FILE")) return 0;
    for (size_t i = 0; i < module->instructionCount; ++i) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        const XL6_IrPersistenceInvocationContract* configured =
            instruction->kind == XL6_IR_INSTR_RUNTIME_CALL
                ? xl6IrPersistenceInvocationRuntime(instruction->runtimeOp) : NULL;
        if (configured != NULL && xl6IrCgenInstructionIsReachable(module, i) &&
            (!visitor(context, configured->nativeModule) ||
                (configured->action == XL6_IR_PERSISTENCE_INVOKE_ENCODE &&
                    !visitor(context, "XRT_MODULE_BUFFER")))) return 0;
    }
    return 1;
}

static int xl6IrCgenVisitSlotProviders(XL6_IrNativeSlotAbi slot,int projection,
    XL6_IrCgenNativeModuleVisitor visitor,void* context)
{
    switch (slot) {
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,Release,Type,Converter,Sequence,Profile,ValueId,...) \
    case XL6_IR_NATIVE_SLOT_##Tag: return visitor(context,#Type) && (!projection || visitor(context,#Converter));
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
#define XL6_IR_CGEN_SLOT_PROVIDER(Slot,Type,Converter) \
    case XL6_IR_NATIVE_SLOT_##Slot: return visitor(context,#Type) && (!projection || visitor(context,#Converter));
#include "src/cgen/xl6_ir_cgen_slot_providers.inc"
#undef XL6_IR_CGEN_SLOT_PROVIDER
    default: return 1; /* Builtin XRT descriptors need no private provider. */
    }
}

static int xl6IrCgenVisitSequenceFactorySymbols(const XL6_IrModule* module,XL6_IrTypeId id,
    XL6_IrCgenNativeModuleVisitor visitor,void* context,size_t depth,bool projectArrays)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if (!type || depth>64 ||
        !xl6IrSequenceConstructionRecipe(module,id,type->sequenceLayout.role,NULL)) return 0;
    /* Match the verified seed renderer: projected Array witnesses need their
     * physical converter, while retained references and ordinary construction
     * do not. Nested sequences inherit this policy; tree/queue schemas do not. */
    bool converter=projectArrays && type->sequenceLayout.role==XL6_IR_SEQUENCE_ROLE_ARRAY &&
        type->sequenceLayout.transferAbi!=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN;
    if (!xl6IrCgenVisitSlotProviders(type->sequenceLayout.slotAbi,converter,visitor,context)) return 0;
    const char* factory=NULL;
    switch (type->sequenceLayout.abi) {
    case XL6_IR_SEQUENCE_LAYOUT_UNION_VALUE_V1: factory="xl6SequenceUnionCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_NATIVE_VALUE_V1:
        {
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(
                xl6IrTypeGet(&module->types,type->sequenceLayout.elementType));
            if (!native) return 0;
            factory=native->sequenceSymbol;
        }
        break;
    case XL6_IR_SEQUENCE_LAYOUT_DYNAMIC_VALUE:
    case XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2: factory="xl6SequenceReferenceCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_PRIMITIVE_V2: factory="xl6SequencePrimitiveCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_NOMINAL_V2: factory="xl6SequenceNominalCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_C_SCALAR_V1: factory="xl6SequenceCScalarCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_NULLABLE_C_SCALAR_V1: factory="xl6SequenceNullableCScalarCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_NUMERIC_ENUM_V1: factory="xl6SequenceNumericEnumCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1: factory="xl6SequenceCallableCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_FUTURE_REFERENCE_V1: factory="xl6SequenceFutureCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_SEMAPHORE_REFERENCE_V1: factory="xl6SequenceSemCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1: factory="xl6SequenceThreadCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_MUTEX_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_COND_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_RWLOCK_REFERENCE_V1: factory="xl6SequenceSyncCreateV1"; break;
    case XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1:
        factory="xl6SequenceNestedCreateTakeV2";
        if (!xl6IrCgenVisitSequenceFactorySymbols(module,type->sequenceLayout.baseElementType,visitor,context,depth+1,projectArrays)) return 0;
        break;
    case XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1: {
        XL6_IrValueCollectionRecipe child={0};
        XL6_IrCollectionAbi family=xl6IrCgenValueCollectionFamily(type->sequenceLayout.abi);
        const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(family);
        if (!target || !xl6IrValueCollectionConstructionRecipe(module,type->sequenceLayout.baseElementType,&child) ||
            child.family!=family ||
            !visitor(context,target->create) ||
            !visitor(context,target->project) ||
            !visitor(context,"xl6ArrayProjectionTakeV1") ||
            !xl6IrCgenVisitSequenceFactorySymbols(module,child.valueSchemaType,visitor,context,depth+1,true)) return 0;
        factory=target->schema;
        break;
    }
    case XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1: {
        XL6_IrTypeId key=0,value=0;
        factory="xl6SequenceTreeCreateTakeV1";
        if (!xl6IrAvlConstructionRecipe(module,type->sequenceLayout.baseElementType,&key,&value,NULL,NULL) ||
            !xl6IrCgenVisitSequenceFactorySymbols(module,key,visitor,context,depth+1,false) ||
            !xl6IrCgenVisitSequenceFactorySymbols(module,value,visitor,context,depth+1,false)) return 0;
        break;
    }
    case XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1: {
        XL6_IrTypeId schema=0; XL6_IrRuntimeResourceAbi family=XL6_IR_RUNTIME_RESOURCE_NONE;
        if (!xl6IrConcurrentQueueConstructionRecipe(module,type->sequenceLayout.baseElementType,&family,&schema,NULL) ||
            !xl6IrCgenVisitSequenceFactorySymbols(module,schema,visitor,context,depth+1,false)) return 0;
        factory=xl6IrCgenConcurrentQueueAdmissionFactory(family);
        if (!factory) return 0;
        break;
    }
    default: return 0;
    }
    return visitor(context,factory);
}

static int xl6IrCgenVisitNativeValueDrops(const XL6_IrModule* module,XL6_IrTypeId id,
    XL6_IrCgenNativeModuleVisitor visitor,void* context)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if (!type) return id==XL6_IR_TYPE_INVALID;
    const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(type);
    if (plan) return visitor(context,plan->dropSymbol);
    if (type->cAbi!=XL6_IR_C_ABI_PRODUCT) return 1;
    /* Products form a DAG, not a tree. Visit each type once: repeated child
     * products must not cause exponential work or recurse on the C stack. */
    unsigned char* seen=xrtCalloc(module->types.count,1);
    XL6_IrTypeId* pending=xrtCalloc(module->types.count,sizeof(*pending));
    bool ok=seen && pending; size_t count=0;
    if (ok) { pending[count++]=id; seen[id-1]=1; }
    for (size_t at=0;ok && at<count;++at) {
        type=xl6IrTypeGet(&module->types,pending[at]);
        plan=xl6IrNativeValuePlanForType(type);
        if (plan) { ok=visitor(context,plan->dropSymbol)!=0; continue; }
        if (type->cAbi!=XL6_IR_C_ABI_PRODUCT) continue;
        if (type->paramCount && !type->params) { ok=false; break; }
        for (size_t i=0;i<type->paramCount;++i) {
            XL6_IrTypeId child=type->params[i];
            if (!child || child>module->types.count) { ok=false; break; }
            if (!seen[child-1]) { seen[child-1]=1; pending[count++]=child; }
        }
    }
    xrtFree(pending); xrtFree(seen); return ok;
}

static int xl6IrCgenVisitOptionsSymbols(XL6_IrCgenNativeModuleVisitor visitor,void* context)
{
    return visitor(context,"xl6DictErasedProbeV1") && visitor(context,"xl6DictErasedReadV1") &&
        visitor(context,"xl6DictErasedVisitKeysV1") && visitor(context,"xl6DictValueFinishReadV1");
}

int xl6IrCgenVisitResidentSymbols(const XL6_IrModule* module, XL6_DiagList* diags,
    const XL6_IrCgenOptions* options, XL6_IrCgenNativeModuleVisitor visitor, void* context)
{
    XL6_IrCgenNeeds needs;
    if (!visitor || !xl6IrCgenBuildNeeds(module,&needs,diags,options)) return 0;
    if(needs.needsNativeFuturePayloadRuntime && !visitor(context,"xl6NativeFuturePayloadRuntimeV1Get"))return 0;
    for(size_t t=0;t<module->closureTargetCount;++t){
        const XL6_IrClosureTarget* target=&module->closureTargets[t];
        if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        if(xl6IrClosureEnvironmentAbi(module,target)!=XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3)continue;
        if(!visitor(context,"xl6ClosureRuntimeV3Get"))return 0;
        for(size_t c=0;c<target->captureCount;++c){
            const XL6_IrClosureCapture* capture=&module->closureCaptures[target->firstCapture+c];
            if(capture->abi!=XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE)continue;
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,capture->type));
            if(!native || !visitor(context,native->captureProfileSymbol) || !visitor(context,native->copySymbol))return 0;
        }
    }
    for(size_t b=0;b<module->callbackBoundaryCount;++b) {
        const XL6_IrCallbackBoundary* boundary=&module->callbackBoundaries[b];
        if(!xl6IrCgenInstructionIsReachable(module,boundary->instruction))continue;
        for(size_t c=0;c<boundary->captureCount;++c) {
            const XL6_IrCallbackCapture* capture=&module->callbackCaptures[boundary->firstCapture+c];
            if(capture->destroyAbi!=XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)continue;
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,capture->type));
            if(!native || !visitor(context,native->captureProfileSymbol) || !visitor(context,native->copySymbol))return 0;
        }
    }
    for(size_t t=0;t<module->callTargetCount;++t) {
        const XL6_IrCallTarget* target=&module->callTargets[t];
        if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE ||
            target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC || !target->asyncCaptureDestroyAbis)continue;
        for(size_t p=0;p<target->parameterCount;++p) {
            if(target->asyncCaptureDestroyAbis[p]!=XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)continue;
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,target->parameterTypes[p]));
            if(!native || !visitor(context,native->captureProfileSymbol) || !visitor(context,native->copySymbol))return 0;
        }
    }
    for(size_t t=0;t<module->callTargetCount;++t) {
        const XL6_IrCallTarget* target=&module->callTargets[t];
        if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE ||
            target->asyncResultAbi!=XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)continue;
        const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,target->workerReturnType));
        if(!native || !visitor(context,native->captureProfileSymbol) || !visitor(context,native->dropSymbol))return 0;
        if(target->asyncDispatch==XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL && target->asyncMachine==SIZE_MAX &&
            !visitor(context,"xl6TaskSubmitOwnedPolicyV1"))return 0;
    }
    for(size_t i=0;i<module->instructionCount;++i) {
        const XL6_IrInstruction* instruction=&module->instructions[i];
        if(!xl6IrCgenInstructionIsReachable(module,i))continue;
        XL6_IrTypeId payloads[2]={
            instruction->asyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE ?
                (instruction->kind==XL6_IR_INSTR_AWAIT ? instruction->type : instruction->payloadType) : 0,
            instruction->sourceAsyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE ? instruction->sourcePayloadType : 0};
        for(size_t p=0;p<2;++p)if(payloads[p]) {
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,payloads[p]));
            if(!native || !visitor(context,native->captureProfileSymbol))return 0;
        }
        if(instruction->runtimeOp==XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE &&
            instruction->asyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE &&
            !visitor(context,"xl6TaskSubmitOwnedPolicyV1"))return 0;
    }
    for(size_t m=0;m<module->asyncMachineCount;++m){
        const XL6_IrAsyncMachine* machine=&module->asyncMachines[m];
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,machine->targetSymbol);
        if(!target || target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        for(size_t f=0;f<machine->frameSlotCount;++f){
            const XL6_IrContinuationFrameSlot* frame=&module->asyncMachineFrameSlots[machine->firstFrameSlot+f];
            if(frame->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE){
                const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,frame->type));
                if(!native || !native->captureProfileSymbol || !visitor(context,native->captureProfileSymbol))return 0;
                if(frame->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE && !visitor(context,native->copySymbol))return 0;
            }
        }
    }
    for(size_t m=0;m<module->generatorMachineCount;++m){
        const XL6_IrGeneratorMachine* machine=&module->generatorMachines[m];
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,machine->targetSymbol);
        if(!target || target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        if(machine->yieldAbi==XL6_IR_GENERATOR_YIELD_MOVE_NATIVE_VALUE){
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,machine->itemType));
            if(!native || !native->captureProfileSymbol || !visitor(context,native->captureProfileSymbol) ||
                !visitor(context,native->dropSymbol))return 0;
        }
        for(size_t f=0;f<machine->frameSlotCount;++f){
            const XL6_IrContinuationFrameSlot* frame=&module->generatorFrameSlots[machine->firstFrameSlot+f];
            if(frame->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE){
                const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,frame->type));
                if(!native || !native->captureProfileSymbol || !visitor(context,native->captureProfileSymbol))return 0;
                if(frame->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE && !visitor(context,native->copySymbol))return 0;
            }
        }
    }
    if((needs.namedValueOperations&1u) && !visitor(context,"xl6ValueNameReadCopyV1")) return 0;
    if((needs.namedValueOperations&2u) && !visitor(context,"xl6ValueNameSetTakeV1")) return 0;
    if (needs.structuredOptionsBridgeOperations && !xl6IrCgenVisitOptionsSymbols(visitor,context)) return 0;
    if ((xl6IrCgenHasRawExecutionActivity(module) ||
         (module->abi.hostExecutionAbi == XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1 &&
          (module->abi.entryKind != XL6_IR_MODULE_ENTRY_NONE || (options && options->emitScriptEntry)))) &&
        (!visitor(context,"xl6CodeActivityEnterV1") || !visitor(context,"xl6CodeActivityLeaveV1"))) return 0;
    if (module->exportCount != 0 &&
        (!visitor(context,"xl6CodeLeaseAcquireScoped") || !visitor(context,"xl6CodeLeaseRelease") ||
         !visitor(context,"xl6CodeScopeEnter") || !visitor(context,"xl6CodeScopeLeave"))) return 0;
    if (needs.needsBoundMethodRuntime &&
        (!visitor(context,"xl6ClosureRuntimeV1Get") || !visitor(context,"xl6ClosureRuntimeV2Get") ||
         !visitor(context,"xl6ClosureBoxErasedV1"))) return 0;
    if(module->abi.entryKind==XL6_IR_MODULE_ENTRY_XLANG_MAIN &&
        (!visitor(context,"xl6ArrayEntryAcquireV1") ||
         (options && options->emitMainWrapper && !visitor(context,"xl6ArrayOsArgvCreateV1")))) return 0;
    /* All Error text operations share one resident immutable-error provider.
     * This is a verified emission need, not a scan of generated C tokens. */
    if (needs.needsErrorRuntime && !visitor(context,"xl6ErrorCreateV1")) return 0;
    if (needs.needsXValueDynamicCollectionRuntime && !visitor(context,"xl6DynamicCollectionCallTakeV1")) return 0;
    for (size_t i=0;i<module->persistenceProjectSchemaCount;++i)
        if (module->persistenceProjectSchemas[i].dynamicDestroySymbol &&
            !visitor(context,"xl6PersistenceProjectOpenV1")) return 0;
    /* Restore plans produce admitted Sequence owners without an explicit
     * ARRAY_IMPORT instruction. Their declared resident dependency belongs
     * here, not in a fallback scan of emitted C or a force-link-all workaround. */
    for (size_t i=0;i<module->types.count;++i)
        if (module->types.items[i].persistenceRestoreCarrierAbi == XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 &&
            (xl6IrCgenPersistenceTypeUsed(module, (XL6_IrTypeId)(i + 1), XL6_IR_PERSIST_FORMAT_JSON, 1) ||
             xl6IrCgenPersistenceTypeUsed(module, (XL6_IrTypeId)(i + 1), XL6_IR_PERSIST_FORMAT_XLON, 1)) &&
            (!visitor(context,"xl6ArrayProjectionTakeV1") || !visitor(context,"xl6ArrayImportTakeV1"))) return 0;
    for (size_t r=0;r<module->persistenceRestoreRootCount;++r) {
        const XL6_IrPersistenceRestoreRoot* root=&module->persistenceRestoreRoots[r];
        if (root->pending) {
            if (!visitor(context,"xl6SerializeRestoreReadFinishTakeV1")) return 0;
            continue;
        }
        if (!visitor(context,"xl6PersistenceRestoreParsedTakeV1") ||
            !visitor(context,xl6IrCgenRestoreObjectProviderSymbol(module,root))) return 0;
        if ((root->ownershipGraphProviders&1u) && !visitor(context,"xl6ListOwnershipAdapterV1")) return 0;
        if ((root->ownershipGraphProviders&2u) && !visitor(context,"xl6SetOwnershipAdapterV1")) return 0;
        if ((root->ownershipGraphProviders&4u) && !visitor(context,"xl6DictOwnershipAdapterV1")) return 0;
        for (size_t n=0;n<root->nodeCount;++n) {
            const XL6_IrPersistenceRestoreNode* node=&module->persistenceRestoreNodes[root->firstNode+n];
            if (node->openClassLayout!=SIZE_MAX) {
                if (!visitor(context,"xl6RestoreOpenProviderV1")) return 0;
                const char* isolation=xl6IrCgenRestoreIsolationSymbol(module,
                    module->classLayouts[node->openClassLayout].type,node->openIsolationAbi);
                if (isolation && !visitor(context,isolation)) return 0;
                if (!xl6IrCgenVisitValueGraphProviders(node->openIsolationGraphProviders,0,visitor,context)) return 0;
            }
            if (node->abi==XL6_IR_RESTORE_NODE_ARRAY &&
                (!visitor(context,"xl6ArrayProjectionTakeV1") ||
                 !xl6IrCgenVisitSequenceFactorySymbols(module,node->payloadType,visitor,context,0,true))) return 0;
            if (node->abi==XL6_IR_RESTORE_NODE_LIST || node->abi==XL6_IR_RESTORE_NODE_SET ||
                node->abi==XL6_IR_RESTORE_NODE_DICT) {
                XL6_IrValueCollectionRecipe recipe={0};
                if (!xl6IrValueCollectionConstructionRecipe(module,node->payloadType,&recipe)) return 0;
                const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(recipe.family);
                if (!target) return 0;
                if (!visitor(context,target->create) || !visitor(context,target->project) ||
                    !visitor(context,"xl6ArrayProjectionTakeV1") ||
                    !visitor(context,target->importProvider) ||
                    !xl6IrCgenVisitSequenceFactorySymbols(module,recipe.valueSchemaType,visitor,context,0,true)) return 0;
            }
            const char* isolation=xl6IrCgenRestoreIsolationSymbol(module,node->payloadType,node->isolationAbi);
            if (isolation && !visitor(context,isolation)) return 0;
            if (!xl6IrCgenVisitValueGraphProviders(node->isolationGraphProviders,0,visitor,context)) return 0;
        }
    }
    for (size_t i=0;i<module->types.count;++i) {
        XL6_IrCollectionCarrierAbi carrier=module->types.items[i].persistenceRestoreCarrierAbi;
        if ((carrier!=XL6_IR_COLLECTION_CARRIER_LIST_V1 && carrier!=XL6_IR_COLLECTION_CARRIER_SET_V1 &&
             carrier!=XL6_IR_COLLECTION_CARRIER_DICT_V1) ||
            (!xl6IrCgenPersistenceTypeUsed(module,(XL6_IrTypeId)(i+1),XL6_IR_PERSIST_FORMAT_JSON,1) &&
             !xl6IrCgenPersistenceTypeUsed(module,(XL6_IrTypeId)(i+1),XL6_IR_PERSIST_FORMAT_XLON,1))) continue;
        XL6_IrValueCollectionRecipe recipe={0};
        if (!xl6IrValueCollectionConstructionRecipe(module,(XL6_IrTypeId)(i+1),&recipe)) return 0;
        const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(recipe.family);
        if (!target || !visitor(context,target->importProvider) ||
            !visitor(context,target->create) || !visitor(context,target->project) ||
            !visitor(context,"xl6ArrayProjectionTakeV1") ||
            !visitor(context,"xl6CollectionImportChainSourceTakeV1") ||
            !xl6IrCgenVisitSequenceFactorySymbols(module,recipe.valueSchemaType,visitor,context,0,true)) return 0;
    }
    for (size_t i=0;i<module->instructionCount;++i) {
        const XL6_IrInstruction* instruction=&module->instructions[i];
        const XL6_IrPersistenceInvocationContract* configured=instruction->kind==XL6_IR_INSTR_RUNTIME_CALL
            ? xl6IrPersistenceInvocationRuntime(instruction->runtimeOp) : NULL;
        if (configured && !configured->nativePretty && !xl6IrPersistenceInvocationIsWriter(configured) &&
            xl6IrCgenInstructionIsReachable(module,i) && !xl6IrCgenVisitOptionsSymbols(visitor,context)) return 0;
        /* Includes nested products; unused builtin registrations are not
         * roots. Never recover these dependencies by scanning generated C. */
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            !xl6IrCgenVisitNativeValueDrops(module,instruction->type,visitor,context)) return 0;
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            instruction->kind==XL6_IR_INSTR_RUNTIME_CALL &&
            xl6IrRuntimeNativeValueOpIs(instruction->runtimeOp)) {
            const XL6_IrNativeValuePlan* plan=xl6IrNativeValueInstructionPlan(module,instruction);
            const char* symbol=xl6IrRuntimeNativeValueSymbol(plan,instruction->runtimeOp);
            if (!symbol || !visitor(context,symbol)) return 0;
        }
        if(instruction->kind==XL6_IR_INSTR_CELL_NEW && xl6IrCgenInstructionIsReachable(module,i)){
            const XL6_IrStorage* storage=xl6IrStorageGet(module,instruction->targetSymbol);
            const XL6_IrNativeValuePlan* native=storage ? xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,storage->type)) : NULL;
            if(native && (!native->captureProfileSymbol || !visitor(context,"xl6CellCreateNativeV2") ||
                !visitor(context,native->captureProfileSymbol)))return 0;
        }
        XL6_IrTypeId queueId=xl6IrConcurrentQueueInstructionType(module,instruction);
        if(instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && xl6IrSlotMapIteratorOpIs(instruction->runtimeOp) &&
            xl6IrCgenInstructionIsReachable(module,i)) {
            if(!xl6IrSlotMapIteratorInstructionCompatible(module,instruction) ||
                !visitor(context,xl6IrSlotMapIteratorNativeSymbol(instruction->runtimeOp))) return 0;
            if(instruction->runtimeOp!=XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_KEY_COPY) {
                const XL6_IrInstruction* source=xl6IrCgenValueDefinition(module,module->operands[instruction->firstOperand],NULL);
                XL6_IrValueCollectionRecipe p={0};
                if(!source || !xl6IrSlotMapConstructionRecipe(module,source->type,&p) ||
                    !xl6IrCgenVisitSlotProviders(p.slotAbi,0,visitor,context)) return 0;
                if((instruction->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_PAIRS ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_ITERATOR_VALUES) &&
                    (!xl6IrCgenVisitSlotProviders(p.slotAbi,1,visitor,context) ||
                     (p.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN &&
                      !visitor(context,"xl6ArrayReferenceConverterV1")))) return 0;
            }
        }
        const char* listIterator=xl6IrCgenListIteratorCreateSymbol(module,instruction);
        if (listIterator && xl6IrCgenInstructionIsReachable(module,i) &&
            !visitor(context,listIterator)) return 0;
        const char* arrayIterator=xl6IrCgenFamilyIteratorCreateSymbol(module,instruction);
        if (arrayIterator && xl6IrCgenInstructionIsReachable(module,i) &&
            !visitor(context,arrayIterator)) return 0;
        if (queueId) {
            if (!xl6IrCgenInstructionIsReachable(module,i)) continue;
            if (!xl6IrConcurrentQueueInstructionCompatible(module,instruction)) return 0;
            const XL6_IrType* type=xl6IrTypeGet(&module->types,queueId);
            const XL6_IrType* schema=xl6IrTypeGet(&module->types,type->messageSchemaType);
            if (!schema || !xl6IrCgenVisitSlotProviders(schema->sequenceLayout.slotAbi,
                    instruction->runtimeOp==XL6_IR_RUNTIME_CONCURRENT_QUEUE_POP_BATCH,visitor,context)) return 0;
            const char* prefix=xl6IrCgenConcurrentQueuePrefix(type->runtimeResourceAbi);
            const char* method=xl6IrCgenConcurrentQueueMethod(instruction->runtimeOp);
            char symbol[64];
            if (!prefix || !method || snprintf(symbol,sizeof(symbol),"%s%s",prefix,method)<0 ||
                !visitor(context,symbol)) return 0;
            if (instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE &&
                !xl6IrCgenVisitSequenceFactorySymbols(module,type->messageSchemaType,visitor,context,0,false)) return 0;
        }
        if (xl6IrChannelRuntimeOpIs(instruction->runtimeOp) && xl6IrCgenInstructionIsReachable(module,i)) {
            XL6_IrTypeId channelId=xl6IrChannelInstructionType(module,instruction),schemaId=0;
            const XL6_IrChannelReceiveSpec* receive=xl6IrChannelReceiveSpecRuntime(instruction->runtimeOp);
            const char* method=receive ? "xl6ChannelReceiveV1" :
                instruction->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE ? "xl6ChannelCreateTakeV1" :
                instruction->runtimeOp==XL6_IR_RUNTIME_CHANNEL_SEND || instruction->runtimeOp==XL6_IR_RUNTIME_CHANNEL_TRY_SEND ? "xl6ChannelSendV1" :
                instruction->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CLOSE ? "xl6ChannelCloseV1" : "xl6ChannelQueryV1";
            if (!xl6IrChannelInstructionCompatible(module,instruction) ||
                !xl6IrChannelConstructionRecipe(module,channelId,&schemaId,NULL) || !visitor(context,method)) return 0;
            const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
            if (!xl6IrCgenVisitSlotProviders(schema->sequenceLayout.slotAbi,0,visitor,context) ||
                (instruction->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE &&
                 !xl6IrCgenVisitSequenceFactorySymbols(module,schemaId,visitor,context,0,false))) return 0;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL) {
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            instruction->runtimeOp>=XL6_IR_RUNTIME_STACK_PUSH_BATCH &&
            instruction->runtimeOp<=XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT) {
            bool push=instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PUSH_BATCH;
            if (!xl6IrStackBatchInstructionCompatible(module,instruction) ||
                !visitor(context,push ? "xl6StackPushBatchV1" : "xl6StackExtractArrayV1")) return 0;
            if (!push) {
                const XL6_IrType* array=xl6IrTypeGet(&module->types,instruction->type);
                if (!xl6IrCgenVisitSlotProviders(array->sequenceLayout.slotAbi,1,visitor,context) ||
                    (array->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN &&
                     !visitor(context,"xl6ArrayReferenceConverterV1"))) return 0;
            }
        }
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            (instruction->runtimeOp==XL6_IR_RUNTIME_LIST_CREATE ||
             instruction->runtimeOp==XL6_IR_RUNTIME_DICT_CREATE ||
             instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
             instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_NEW ||
             instruction->runtimeOp==XL6_IR_RUNTIME_SET_IMPORT_CHAIN ||
             instruction->runtimeOp==XL6_IR_RUNTIME_DICT_IMPORT_CHAIN)) {
            XL6_IrValueCollectionRecipe recipe={0}; XL6_IrTypeId chain[65]; size_t count=0;
            bool list=instruction->runtimeOp==XL6_IR_RUNTIME_LIST_CREATE || instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN;
            bool dict=instruction->runtimeOp==XL6_IR_RUNTIME_DICT_CREATE || instruction->runtimeOp==XL6_IR_RUNTIME_DICT_IMPORT_CHAIN;
            bool importing=instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
                instruction->runtimeOp==XL6_IR_RUNTIME_SET_IMPORT_CHAIN || instruction->runtimeOp==XL6_IR_RUNTIME_DICT_IMPORT_CHAIN;
            if (!(dict ? importing ? xl6IrDictImportRuntimeInstructionCompatible(module,instruction) :
                                    xl6IrDictCreateRuntimeInstructionCompatible(module,instruction) :
                         list ? importing ? xl6IrListImportRuntimeInstructionCompatible(module,instruction) :
                                    xl6IrListCreateRuntimeInstructionCompatible(module,instruction) :
                         importing ? xl6IrSetImportRuntimeInstructionCompatible(module,instruction) :
                                    xl6IrSetCreateRuntimeInstructionCompatible(module,instruction)) ||
                !xl6IrValueCollectionConstructionRecipe(module,instruction->type,&recipe) ||
                (importing && !xl6IrCollectionImportChainRecipe(module,instruction->type,chain,65,&count)) ||
                !xl6IrCgenVisitSequenceFactorySymbols(module,recipe.valueSchemaType,visitor,context,0,true) ||
                !visitor(context,"xl6ArrayProjectionTakeV1") ||
                !visitor(context,dict ? "xl6DictCreateTakeV1" : list ? "xl6ListCreateTakeV1" : "xl6SetCreateTakeV1") ||
                !visitor(context,dict ? "xl6DictProjectionTakeV1" : list ? "xl6ListProjectionTakeV1" : "xl6SetProjectionTakeV1")) return 0;
            if (importing) {
                if (!visitor(context,"xl6CollectionImportChainSourceTakeV1") ||
                    (count>1 && !visitor(context,"xl6ArrayEraseV1"))) return 0;
                for (size_t j=0;j<count;++j) {
                    XL6_IrCollectionAbi family=xl6IrTypeGet(&module->types,chain[j])->collectionAbi;
                    if (family!=XL6_IR_COLLECTION_ARRAY) {
                        const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(family);
                        if (!target || !visitor(context,target->importProvider)) return 0;
                    }
                }
            }
        }
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            xl6IrCgenThreadRuntimeOpIs(instruction->runtimeOp) &&
            (!xl6IrCgenRuntimeThreadAbi(module,instruction) ||
             !visitor(context,xl6IrCgenThreadHelper(instruction->runtimeOp)))) return 0;
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            xl6IrCgenSyncRuntimeOpIs(instruction->runtimeOp) &&
            (!xl6IrCgenRuntimeSyncAbi(module,instruction) ||
             !visitor(context,xl6IrCgenSyncHelper(instruction->runtimeOp)))) return 0;
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            (instruction->runtimeOp==XL6_IR_RUNTIME_STACK_CREATE || instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE ||
             instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN) &&
            !xl6IrCgenVisitSequenceFactorySymbols(module,instruction->type,visitor,context,0,false)) return 0;
        if(xl6IrCgenInstructionIsReachable(module,i) &&
            (instruction->runtimeOp==XL6_IR_RUNTIME_STACK_POP || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK ||
             instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK_AT || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_POP ||
             instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK_AT)) {
            const XL6_IrType* payload=xl6IrTypeGet(&module->types,instruction->payloadType);
            if(payload && payload->cAbi==XL6_IR_C_ABI_CLOSURE_PTR && !visitor(context,"xl6StackCallableTakeV1")) return 0;
        }
        if(xl6IrCgenInstructionIsReachable(module,i) &&
            xl6IrSlotMapResourceOpIs(xl6IrSlotMapResourceOp(instruction->runtimeOp))) {
            if(!xl6IrSlotMapInstructionCompatible(module,instruction) ||
                !visitor(context,xl6IrSlotMapNativeSymbol(instruction->runtimeOp))) return 0;
            const XL6_IrInstruction* owner=instruction->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_CREATE ? instruction :
                xl6IrCgenValueDefinition(module,module->operands[instruction->firstOperand],NULL);
            XL6_IrValueCollectionRecipe domain={0};
            bool projection=instruction->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_VALUES;
            if(!owner || !xl6IrSlotMapConstructionRecipe(module,owner->type,&domain) ||
                !xl6IrCgenVisitSlotProviders(domain.slotAbi,projection,visitor,context)) return 0;
            if(projection && domain.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN &&
                !visitor(context,"xl6ArrayReferenceConverterV1")) return 0;
            if(instruction->runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_CREATE) {
                XL6_IrValueCollectionRecipe plan={0};
                if(!xl6IrSlotMapConstructionRecipe(module,instruction->type,&plan) ||
                    !xl6IrCgenVisitSequenceFactorySymbols(module,plan.valueSchemaType,visitor,context,0,false)) return 0;
            }
        }
        if (xl6IrCgenInstructionIsReachable(module,i) && instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN) {
            XL6_IrTypeId chain[65]; size_t count=0; bool mixed=false;
            if (!xl6IrArrayImportChainRecipe(module,instruction->type,chain,65,&count)) return 0;
            for (size_t d=0;d<count;++d) {
                XL6_IrCollectionAbi family=xl6IrTypeGet(&module->types,chain[d])->collectionAbi;
                if (family==XL6_IR_COLLECTION_ARRAY) continue;
                mixed=true;
                const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(family);
                if (!target || !visitor(context,target->importProvider)) return 0;
            }
            if (mixed && !visitor(context,"xl6CollectionImportChainSourceTakeV1")) return 0;
        }
        if (xl6IrCgenInstructionIsReachable(module,i) && xl6IrCgenAvlRuntimeOpIs(instruction->runtimeOp)) {
            const XL6_IrInstruction* receiver=instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_CREATE ? NULL :
                xl6IrCgenValueDefinition(module,module->operands[instruction->firstOperand],NULL);
            XL6_IrTypeId owner=receiver ? receiver->type : instruction->type,keyId=0,valueId=0;
            const char* method=xl6IrCgenTreeNativeMethod(instruction->runtimeOp);
            if (!method || !xl6IrAvlConstructionRecipe(module,owner,&keyId,&valueId,NULL,NULL) ||
                !visitor(context,method)) return 0;
            const XL6_IrType* key=xl6IrTypeGet(&module->types,keyId);
            const XL6_IrType* value=xl6IrTypeGet(&module->types,valueId);
            if (!xl6IrCgenVisitSlotProviders(key->sequenceLayout.slotAbi,0,visitor,context) ||
                !xl6IrCgenVisitSlotProviders(value->sequenceLayout.slotAbi,0,visitor,context)) return 0;
            if (instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_CREATE &&
                (!xl6IrCgenVisitSequenceFactorySymbols(module,keyId,visitor,context,0,false) ||
                 !xl6IrCgenVisitSequenceFactorySymbols(module,valueId,visitor,context,0,false))) return 0;
        }
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            xl6IrCgenAvlRuntimeOpIs(instruction->runtimeOp) &&
            instruction->runtimeResourcePayloadAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN &&
            !visitor(context,"xl6SequenceStorageTypeV1")) return 0;
        if (xl6IrCgenInstructionIsReachable(module,i) &&
            (instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_KEYS ||
             instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_VALUES)) {
            const XL6_IrType* array=xl6IrTypeGet(&module->types,instruction->type);
            if (!array || !visitor(context,"xl6TreeProjectTakeV1") ||
                !xl6IrCgenVisitSequenceFactorySymbols(module,instruction->type,visitor,context,0,false) ||
                !xl6IrCgenVisitSlotProviders(array->sequenceLayout.slotAbi,1,visitor,context) ||
                (array->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN &&
                 !visitor(context,"xl6ArrayReferenceConverterV1"))) return 0;
        }
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE &&
            instruction->collectionAbi==XL6_IR_COLLECTION_ARRAY && !visitor(context,"xl6ArrayMethodRemoveV1")) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE &&
            instruction->collectionAbi==XL6_IR_COLLECTION_LIST && !visitor(context,"xl6ListValueRemoveV1")) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_CONTAINS &&
            !visitor(context,xl6IrCgenCollectionContainsSymbol(module,instruction))) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
            instruction->collectionAbi==XL6_IR_COLLECTION_LIST) {
            const char* listMethod=instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_CLEAR ? "xl6ListValueClearV1" :
                instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TRIM ||
                instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TRIM_TO ? "xl6ListValueTrimV1" : NULL;
            if (listMethod && !visitor(context,listMethod)) return 0;
        }
        if ((instruction->runtimeOp==XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT ||
             instruction->runtimeOp==XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT) &&
            (!xl6IrPersistenceProjectInstructionAbi(module,instruction) ||
             !visitor(context,xl6IrCgenPersistenceProjectSymbol(module,instruction)))) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT ||
            instruction->runtimeOp==XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT) {
            if (!xl6IrCgenVisitValueGraphProviders(
                xl6IrValueGraphProvidersForType(module,instruction->payloadType),1,visitor,context)) return 0;
        }
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_CLONE &&
            !visitor(context,xl6IrCgenValueCloneSymbol(module,instruction))) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT &&
            !visitor(context,"xl6ListValueMergeRightV1")) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_LIST_WITHOUT_I64_KEY &&
            !visitor(context,"xl6ListValueWithoutKeyV1")) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_DEEP_CLONE &&
            (!visitor(context,xl6IrCgenValueDeepCloneSymbol(module,instruction)) ||
             !xl6IrCgenVisitValueGraphProviders(instruction->copyGraphProviders,0,visitor,context))) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE &&
            (!xl6IrArrayCreateRuntimeInstructionCompatible(module,instruction) ||
             !visitor(context,"xl6ArrayProjectionTakeV1"))) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN &&
            (!xl6IrArrayImportRuntimeInstructionCompatible(module,instruction) ||
             !visitor(context,"xl6ArrayImportChainSourceTakeV1") ||
             !visitor(context,"xl6ArrayEraseV1"))) return 0;
        if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_CONCAT &&
            !visitor(context,"xl6ArrayConcatV1")) return 0;
            const char* method=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE ?
                "xl6ArrayMethodRemoveValueV1" :
                instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                instruction->collectionAbi==XL6_IR_COLLECTION_ARRAY ?
                    xl6IrCgenArrayMethodSymbol(instruction->typedCollectionOp) : NULL;
            if (method && !visitor(context,method)) return 0;
            if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                (instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP || instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE)) {
                const char* extract=instruction->collectionAbi==XL6_IR_COLLECTION_ARRAY ?
                    xl6IrCgenArrayExtractSymbol(xl6IrTypeGet(&module->types,instruction->type)) :
                    instruction->collectionAbi==XL6_IR_COLLECTION_LIST ? "xl6ListValueTakeV1" :
                    instruction->collectionAbi==XL6_IR_COLLECTION_DICT ? "xl6DictValueTakeV1" : NULL;
                if (!extract || !visitor(context,extract)) return 0;
                const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,instruction->type));
                if (native && !visitor(context,native->profileSymbol)) return 0;
            }
            if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_RESIZE &&
                !visitor(context,"xl6ArrayResizeDefaultV1")) return 0;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && xl6IrOwnedIndexReadRuntimeOpIs(instruction->runtimeOp)) {
            const char* symbol=xl6IrCgenOwnedIndexReadSymbol(instruction->runtimeOp);
            if (!xl6IrOwnedIndexReadRuntimeInstructionCompatible(module,instruction) || !visitor(context,symbol)) return 0;
            continue;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && xl6IrDictDirectSlotRuntimeOpIs(instruction->runtimeOp)) {
            const XL6_IrInstruction* source=xl6IrCgenValueDefinition(module,module->operands[instruction->firstOperand],NULL);
            XL6_IrValueCollectionRecipe recipe={0};
            if (!xl6IrDictDirectSlotRuntimeInstructionCompatible(module,instruction) || !source ||
                !xl6IrDictDirectSlotRecipe(module,source->type,&recipe)) return 0;
            const XL6_IrType* schema=xl6IrTypeGet(&module->types,recipe.valueSchemaType);
            if (!xl6IrCgenVisitSlotProviders(schema->sequenceLayout.slotAbi,0,visitor,context) ||
                !visitor(context,instruction->runtimeOp==XL6_IR_RUNTIME_DICT_READ_SLOT ?
                    "xl6DictReadSlotV1" : "xl6DictSetV1")) return 0;
            continue;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL &&
            (xl6IrListTypedReadRuntimeOpIs(instruction->runtimeOp) ||
             instruction->runtimeOp==XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE)) {
            if (!xl6IrListIndexRuntimeInstructionCompatible(module,instruction) ||
                !visitor(context,instruction->runtimeOp==XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE ?
                    "xl6ListValueSetTakeV1" : xl6IrCgenListTypedReadSymbol(instruction->runtimeOp))) return 0;
            continue;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && instruction->runtimeOp==XL6_IR_RUNTIME_DICT_SET_VALUE_TAKE) {
            if (!xl6IrDictIndexWriteRuntimeInstructionCompatible(module,instruction) ||
                !visitor(context,"xl6DictValueSetTakeV1")) return 0;
            continue;
        }
        if (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            (xl6IrArrayScalarReadRuntimeOpIs(instruction->runtimeOp) ||
             instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY)) {
            if (!(instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY ?
                    xl6IrArrayStringReadRuntimeInstructionCompatible(module,instruction) :
                    xl6IrArrayScalarReadRuntimeInstructionCompatible(module, instruction)) ||
                !visitor(context, xl6IrCgenArrayScalarReadSymbol(instruction->runtimeOp))) return 0;
            continue;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL) {
            const char* setMethod=xl6IrCgenSetMethodSymbol(module,instruction);
            if (setMethod && !visitor(context,setMethod)) return 0;
            const char* dictMethod=xl6IrCgenDictMethodSymbol(module,instruction);
            if (dictMethod && !visitor(context,dictMethod)) return 0;
            const char* symbol=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW ?
                "xl6ArrayValueAppendTakeV1" : instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW ?
                "xl6ArrayValueSetTakeV1" : instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW ?
                "xl6ValueIndexSetTakeV1" : NULL;
            if (symbol && !visitor(context,symbol)) return 0;
        }
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            !xl6IrCollectionCountRuntimeOpIs(instruction->runtimeOp)) continue;
        if (!xl6IrCollectionCountRuntimeInstructionCompatible(module,instruction)) return 0;
        const char* symbol=xl6IrCgenCollectionCountSymbol(module,instruction);
        if (!visitor(context,symbol)) return 0;
    }
    if ((needs.randomBridgeOperations & XL6_CGEN_RNG_SHUFFLE_ARRAY) &&
        !visitor(context,"xl6ArrayRngShuffleV1")) return 0;
    if ((needs.randomBridgeOperations & XL6_CGEN_RAND_SHUFFLE_ARRAY) &&
        !visitor(context,"xl6ArrayRandShuffleV1")) return 0;
    if (needs.needsCallableArrayFrameProducer && !visitor(context,"xl6ArrayCallFramePackV1")) return 0;
    if (needs.needsCallableNativeScalarCodec &&
        (!visitor(context,"xl6ValueReadNativeScalarV1") || !visitor(context,"xl6ValueBoxNativeScalarV1"))) return 0;
    if (needs.needsXValueDynamicFieldRuntime &&
        (!visitor(context,"xl6ValueObjectFieldFindV1") ||
         !visitor(context,"xl6ValueObjectFieldSetTakeV1"))) return 0;
    if (needs.needsXValueErrorBoxRuntime && !visitor(context,"xl6ErrorValueRefV1")) return 0;
    if (needs.needsXValueAddRuntime &&
        (!visitor(context,"xl6ArrayIsValueV1") || !visitor(context,"xl6ArrayConcatDynamicV1") ||
         !visitor(context,"xl6ValueStringConcatV1"))) return 0;
    if (needs.needsXValueFutureRuntime && !visitor(context,"xl6PayloadBoxFutureV1")) return 0;
    if ((needs.needsXValueErrorProjectionRuntime || needs.needsXValueCheckedCastRuntime) &&
        !visitor(context,"xl6ErrorValueReadRefV1")) return 0;
    if (needs.needsXValueToStringRuntime && !visitor(context,"xl6ValueTextV1")) return 0;
    if (needs.needsClassDerivedToStringRuntime && !visitor(context,"xl6ClassValueTextV1")) return 0;
    if (needs.needsCallableIntegerCodec &&
        (!visitor(context,"xl6ValueIntegerSlotAdmitsV1") ||
         !visitor(context,"xl6ValueReadIntegerV1") || !visitor(context,"xl6ValueBoxIntegerV1"))) return 0;
    return !needs.needsCallableF32Reader || visitor(context,"xl6ValueReadF32V1");
}
