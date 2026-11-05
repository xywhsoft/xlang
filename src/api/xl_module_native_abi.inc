/* The host lookup names and JIT GC roots share one ABI authority. */
#define XL_MODULE_NATIVE_ABI(X) \
    X(OWNER_TRACE, "xl_script_module_ownership_trace") \
    X(OWNER_READ, "xl_script_module_ownership_read_v1") \
    X(RETIREMENT_V1, "xl_script_module_retirement_v1") \
    X(RETIREMENT_V2, "xl_script_module_retirement_v2") \
    X(RETIREMENT_V3, "xl_script_module_retirement_v3") \
    X(RETIREMENT_V4, "xl_script_module_retirement_v4") \
    X(BIND_CODE, "xl_script_bind_code_owner") \
    X(BIND_READ, "xl_script_bind_ownership_read_v1") \
    X(UNIT_CHECKED, "xl_script_module_unit_checked") \
    X(UNIT, "xl_script_module_unit") \
    X(ERROR_UNIT, "xl_script_error_unit") \
    X(ENTRY, "xlang_main") \
    X(ENTRY_CHECKED, "xl_script_main_checked") \
    X(INIT, "xl_script_module_init") \
    X(HAS_ERROR, "xl_script_has_error") \
    X(PRINT_UNCAUGHT, "xl_script_print_uncaught") \
    X(CLEAR_ERROR, "xl_script_clear_error")
#define XL_MODULE_ABI_ENUM(name, text) XL_MODULE_ABI_##name,
typedef enum XL_ModuleNativeAbi {
    XL_MODULE_NATIVE_ABI(XL_MODULE_ABI_ENUM)
    XL_MODULE_ABI_COUNT
} XL_ModuleNativeAbi;
#undef XL_MODULE_ABI_ENUM
#define XL_MODULE_ABI_NAME(name, text) text,
static const char* const xlModuleNativeAbiNames[] = {
    XL_MODULE_NATIVE_ABI(XL_MODULE_ABI_NAME)
};
#undef XL_MODULE_ABI_NAME
#undef XL_MODULE_NATIVE_ABI

typedef struct XL_ModuleNativeRoots {
    const XL6_IrModule* ir;
    XL6_PackageStringList names;
    int failed;
} XL_ModuleNativeRoots;

static void xlModuleNativeRootCollect(void* opaque, const char* name, const void* value)
{
    XL_ModuleNativeRoots* roots = (XL_ModuleNativeRoots*)opaque;
    int selected = 0;
    (void)value; /* An unrelocated defined symbol may legitimately have offset zero. */
    if (roots->failed) return;
    for (size_t i = 0; !selected && i < XL_MODULE_ABI_COUNT; ++i)
        selected = !strcmp(name, xlModuleNativeAbiNames[i]);
    for (size_t i = 0; !selected && i < roots->ir->exportCount; ++i)
        selected = roots->ir->exports[i].cName && !strcmp(name, roots->ir->exports[i].cName);
    if (selected && !xl6PackageStringListAppend(&roots->names, name)) roots->failed = 1;
}

static int xlTccRootLanguageModule(XL_Context* ctx, TCCState* state,
    const XL6_CompileUnit* unit)
{
    if (!(unit->importDeps.nativeLinkFlags & XL6_NATIVE_LINK_GC_SECTIONS)) return 1;
    XL_ModuleNativeRoots roots = {0};
    roots.ir = &unit->ir;
    xl__tcc.tcc_list_symbols(state, &roots, xlModuleNativeRootCollect);
    int ok = !roots.failed && roots.names.count != 0;
    for (size_t i = 0; ok && i < roots.names.count; ++i) {
        size_t length = strlen(roots.names.items[i]);
        static const char prefix[] = "-Wl,--undefined=";
        if (length > SIZE_MAX - sizeof(prefix)) { ok = 0; break; }
        char* option = (char*)xrtMalloc(sizeof(prefix) + length);
        if (!option) { ok = 0; break; }
        memcpy(option, prefix, sizeof(prefix) - 1);
        memcpy(option + sizeof(prefix) - 1, roots.names.items[i], length + 1);
        ok = xl__tcc.tcc_set_options(state, option) >= 0;
        xrtFree(option);
    }
    xl6PackageStringListFree(&roots.names);
    if (!ok) xlSetError(ctx, "cannot retain the language module ABI for native section linking");
    return ok;
}
