/* The ABI token declaration is shared with the resident host, not maintained
 * as a second struct layout. Artifact binding does not invoke host callbacks. */
static void xl6IrCgenEmitPublicLinkage(XL6_Output* out)
{
    /* Public linkage is selected by verified exports or the fixed host ABI,
     * never by source spelling or the presence of a native implementation. */
    xl6OutputPuts(
        "#ifndef XLIR_C_API\n"
        "#if defined(_WIN32) && defined(XL6_BUILD_SHARED)\n"
        "#define XLIR_C_API __declspec(dllexport)\n"
        "#elif defined(_WIN32) && defined(XL6_USE_SHARED)\n"
        "#define XLIR_C_API __declspec(dllimport)\n"
        "#elif (defined(__GNUC__) || defined(__clang__)) && defined(XL6_BUILD_SHARED)\n"
        "#define XLIR_C_API __attribute__((visibility(\"default\")))\n"
        "#else\n#define XLIR_C_API\n#endif\n#endif\n", out);
}

#define XL6_IR_STRINGIFY_RAW(...) #__VA_ARGS__
#define XL6_IR_STRINGIFY(...) XL6_IR_STRINGIFY_RAW(__VA_ARGS__)
static void xl6IrCgenEmitCodeOwnerDeclaration(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_CODE_OWNER_TYPE_DEFINED\n"
        "#define XL6_CODE_OWNER_TYPE_DEFINED\n"
        XL6_IR_STRINGIFY(XL6_CODE_OWNER_DECLARATION) "\n#endif\n", out);
    xl6OutputPuts("#ifndef XL6_CODE_OWNER_GRAPH_TYPE_DEFINED\n"
        "#define XL6_CODE_OWNER_GRAPH_TYPE_DEFINED\n"
        XL6_IR_STRINGIFY(XL6_CODE_OWNER_GRAPH_DECLARATION) "\n#endif\n", out);
    xl6OutputPuts("#ifndef XL6_MODULE_OWNERSHIP_TYPE_DEFINED\n"
        "#define XL6_MODULE_OWNERSHIP_TYPE_DEFINED\n"
        XL6_IR_STRINGIFY(XL6_MODULE_OWNERSHIP_DECLARATION) "\n#endif\n", out);
}
#undef XL6_IR_STRINGIFY
#undef XL6_IR_STRINGIFY_RAW

static void xl6IrCgenEmitCodeOwnerBinding(XL6_Output* out, const XL6_IrModule* module)
{
    if (module->abi.codeOwnerAbi != XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT) return;
    xl6IrCgenEmitCodeOwnerDeclaration(out);
    xl6OutputPuts(
        "static XL6_CodeOwnerGraph xlir_code_owner_graph = {{0}, 0x584C36434F574E31ULL, NULL};\n"
        "#define xlir_code_owner xlir_code_owner_graph.base\n"
        "static int xlir_code_owner_sealed = 0;\n"
        "/* Serialized host binding before any initialization or exported call.\n"
        " * Unbound artifacts require externally resident code. Never replace a\n"
        " * bound capability, including after failed init or a unit/init cycle. */\n"
        "XLIR_C_API int xl_script_bind_code_owner(const XL6_CodeOwner* owner)\n"
        "{\n"
        "    const struct xrtownershipops* ops = xl6CodeOwnerGraphOps(owner);\n"
        "    if (xlir_code_owner_sealed || xlir_code_owner.size != 0 || owner == NULL ||\n"
        "        owner->size < sizeof(XL6_CodeOwner) || owner->acquire == NULL || owner->release == NULL ||\n"
        "        (ops != NULL && owner->context == NULL)) return 0;\n"
        "    xlir_code_owner.size = ops != NULL ? sizeof(XL6_CodeOwnerGraph) : sizeof(XL6_CodeOwner);\n"
        "    xlir_code_owner_graph.ops = ops;\n"
        "    xlir_code_owner.context = owner->context;\n"
        "    xlir_code_owner.acquire = owner->acquire;\n"
        "    xlir_code_owner.release = owner->release;\n"
        "    return 1;\n"
        "}\n\n", out);
    if (module->abi.readBoundaryAbi == XL6_IR_MODULE_READ_BOUNDARY_RESIDENT_HOST_BIND_V1)
        xl6OutputPuts("#include <__xl_runtime/xl6_ownership_read_abi.h>\n"
            "static XL6_OwnershipReadBoundaryV1 xlir_ownership_read_boundary = {0};\n"
            "/* Independent resident capability; copy, never execute or replace. */\n"
            "XLIR_C_API int xl_script_bind_ownership_read_v1(const XL6_OwnershipReadBoundaryV1* boundary)\n"
            "{\n"
            "    if (xlir_code_owner_sealed || xlir_ownership_read_boundary.size != 0 || boundary == NULL ||\n"
            "        boundary->size != sizeof(*boundary) || boundary->Begin == NULL ||\n"
            "        boundary->Admits == NULL || boundary->End == NULL) return 0;\n"
            "    xlir_ownership_read_boundary = *boundary;\n"
            "    return 1;\n"
            "}\n\n", out);
}
