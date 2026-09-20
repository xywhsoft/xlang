/* The same verified C implementation selection feeds emission and artifact
 * dependencies. This is backend library metadata, not a new semantic IR. */
static int xl6IrCgenBuildNeeds(const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags, const XL6_IrCgenOptions* options)
{
    memset(needs,0,sizeof(*needs));
    if (!xl6IrCgenVerifyEmittable(module,diags)) return 0;
    if (!xl6IrCgenAnalyzePrimaryNeeds(module,needs,diags) ||
        !xl6IrCgenAnalyzeRuntimeNeeds(module,needs,diags)) return 0;
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

static int xl6IrCgenVisitNativeGroup(XL6_IrCgenNativeGroup group, uintptr_t operations,
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
    case XL6_IR_CGEN_NATIVE_TLS:
        /* The selected TLS adapter exposes certificate-to-hex conversion. */
        REQUIRE("XRT_MODULE_CODEC_HEX");
        break;
    case XL6_IR_CGEN_NATIVE_JSON_VALUE:
        if (operations & (XL6_CGEN_JSON_PARSE|XL6_CGEN_JSON_VALID)) REQUIRE("XRT_MODULE_JSON_READ");
        if (operations & XL6_CGEN_JSON_STRINGIFY) REQUIRE("XRT_MODULE_JSON_WRITE");
        if (operations & (XL6_CGEN_JSON_PARSE_FILE|XL6_CGEN_JSON_WRITE_FILE)) REQUIRE("XRT_MODULE_JSON_FILE");
        break;
    case XL6_IR_CGEN_NATIVE_XSON:
        if (operations & (XL6_CGEN_XSON_PARSE|XL6_CGEN_XSON_VALID|XL6_CGEN_XSON_VISIT)) REQUIRE("XRT_MODULE_XSON_READ");
        if (operations & (XL6_CGEN_XSON_STRINGIFY|XL6_CGEN_XSON_WRITER)) REQUIRE("XRT_MODULE_XSON_WRITE");
        if (operations & (XL6_CGEN_XSON_READ|XL6_CGEN_XSON_WRITE)) REQUIRE("XRT_MODULE_XSON_FILE");
        break;
    case XL6_IR_CGEN_NATIVE_PERSISTENCE:
        if (operations & XL6_IR_PERSIST_FORMAT_JSON) REQUIRE("XRT_MODULE_JSON_FILE");
        if (operations & XL6_IR_PERSIST_FORMAT_XSON) REQUIRE("XRT_MODULE_XSON_FILE");
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
            (uintptr_t)needs.Field,visitor,context)) return 0;
#include "src/cgen/xl6_ir_cgen_needs.inc"
#undef XL6_IR_CGEN_NEED
    return 1;
}
