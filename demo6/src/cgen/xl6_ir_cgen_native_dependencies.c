/* The same verified C implementation selection feeds emission and artifact
 * dependencies. This is backend library metadata, not a new semantic IR. */
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
    for (size_t i=0;i<module->storageCount;++i)
        if (module->storages[i].kind==XL6_IR_STORAGE_GLOBAL &&
            xl6IrCgenOwnershipView(module->storages[i].moduleTraceAbi)!=NULL)
            needs->needsModuleOwnershipViews=1;
    if ((options && options->emitMainWrapper) || needs->needsNetTcpRuntime ||
        needs->needsNetDatagramRuntime || needs->needsNetTlsDialRuntime || needs->needsNetTlsServerRuntime) {
        needs->needsForeignErrorRuntime=1;
        needs->needsErrorRuntime=1;
    }
    return 1;
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
        if (operations & XL6_IR_PERSIST_FORMAT_JSON) REQUIRE("XRT_MODULE_JSON_FILE");
        if (operations & XL6_IR_PERSIST_FORMAT_XLON) REQUIRE("XRT_MODULE_XLON_FILE");
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
    if ((module->abi.entryKind!=XL6_IR_MODULE_ENTRY_NONE || (options && options->emitScriptEntry)) &&
        !visitor(context,"XRT_MODULE_CONSOLE")) return 0;
#define XL6_IR_CGEN_NEED(Type, Field, Group) \
    if (needs.Field && !xl6IrCgenVisitNativeGroup(XL6_IR_CGEN_NATIVE_##Group, \
            (uint64_t)needs.Field,visitor,context)) return 0;
#include "src/cgen/xl6_ir_cgen_needs.inc"
#undef XL6_IR_CGEN_NEED
    return 1;
}

int xl6IrCgenVisitResidentSymbols(const XL6_IrModule* module, XL6_DiagList* diags,
    const XL6_IrCgenOptions* options, XL6_IrCgenNativeModuleVisitor visitor, void* context)
{
    XL6_IrCgenNeeds needs;
    if (!visitor || !xl6IrCgenBuildNeeds(module,&needs,diags,options)) return 0;
    /* All Error text operations share one resident immutable-error provider.
     * This is a verified emission need, not a scan of generated C tokens. */
    if (needs.needsErrorRuntime && !visitor(context,"xl6ErrorCreateV1")) return 0;
    for (size_t i=0;i<module->instructionCount;++i) {
        const XL6_IrInstruction* instruction=&module->instructions[i];
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL) {
            const char* method=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE ?
                "xl6ArrayMethodRemoveValueV1" :
                instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                instruction->collectionAbi==XL6_IR_COLLECTION_ARRAY ?
                    xl6IrCgenArrayMethodSymbol(instruction->typedCollectionOp) : NULL;
            if (method && !visitor(context,method)) return 0;
            if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                (instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP || instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE)) {
                const char* extract=instruction->collectionAbi==XL6_IR_COLLECTION_ARRAY ?
                    xl6IrCgenArrayExtractSymbol(xl6IrTypeGet(&module->types,instruction->type)) : "xl6CollectionExtractValueV1";
                if (!extract || !visitor(context,extract)) return 0;
            }
            if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_RESIZE &&
                !visitor(context,"xl6ArrayResizeDefaultV1")) return 0;
        }
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && xl6IrOwnedIndexReadRuntimeOpIs(instruction->runtimeOp)) {
            const char* symbol=instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_READ_VALUE_COPY ?
                "xl6ArrayValueReadCopyV1" : instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_READ_VALUE_OR_NULL_COPY ?
                "xl6ArrayValueReadOrNullCopyV1" : "xl6ValueIndexReadCopyV1";
            if (!xl6IrOwnedIndexReadRuntimeInstructionCompatible(module,instruction) || !visitor(context,symbol)) return 0;
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
            const char* symbol=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW ?
                "xl6ArrayValueAppendTakeV1" : instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW ?
                "xl6ArrayValueSetTakeV1" : instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW ?
                "xl6ValueIndexSetTakeV1" : NULL;
            if (symbol && !visitor(context,symbol)) return 0;
        }
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            !xl6IrCollectionCountRuntimeOpIs(instruction->runtimeOp)) continue;
        if (!xl6IrCollectionCountRuntimeInstructionCompatible(module,instruction)) return 0;
        const char* symbol=instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COUNT ? "xl6CollectionCountV1" :
            instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE ?
                "xl6CollectionCountIfPresentV1" : "xl6CollectionCountOrNullV1";
        if (!visitor(context,symbol)) return 0;
    }
    if ((needs.randomBridgeOperations & XL6_CGEN_RNG_SHUFFLE_ARRAY) &&
        !visitor(context,"xl6ArrayRngShuffleV1")) return 0;
    if ((needs.randomBridgeOperations & XL6_CGEN_RAND_SHUFFLE_ARRAY) &&
        !visitor(context,"xl6ArrayRandShuffleV1")) return 0;
    if (needs.needsCallableNativeScalarCodec &&
        (!visitor(context,"xl6ValueReadNativeScalarV1") || !visitor(context,"xl6ValueBoxNativeScalarV1"))) return 0;
    if (needs.needsXValueErrorBoxRuntime && !visitor(context,"xl6ErrorValueRefV1")) return 0;
    if ((needs.needsXValueErrorProjectionRuntime || needs.needsXValueCheckedCastRuntime) &&
        !visitor(context,"xl6ErrorValueReadRefV1")) return 0;
    if (needs.needsXValueToStringRuntime && !visitor(context,"xl6ValueTextV1")) return 0;
    if (needs.needsClassDerivedToStringRuntime && !visitor(context,"xl6ClassValueTextV1")) return 0;
    if (needs.needsCallableIntegerCodec &&
        (!visitor(context,"xl6ValueIntegerSlotAdmitsV1") ||
         !visitor(context,"xl6ValueReadIntegerV1") || !visitor(context,"xl6ValueBoxIntegerV1"))) return 0;
    return !needs.needsCallableF32Reader || visitor(context,"xl6ValueReadF32V1");
}
