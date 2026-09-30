/* Immutable options and once-sampled statistics use distinct verified owners. */
static void xl6IrCgenEmitFsPackRuntime(XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_PATH_CREATE) ||
        xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_TAKE_CREATE)) xl6OutputPuts(
        "static inline xvfspack xlir_fs_pack_create(xfile source, const xvfspackoptions* options,\n"
        "    uint64_t offset, int64_t length)\n"
        "{ uint64_t size;\n"
        "  if (length < -1) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  if ((xrtFileCapabilities(source) & (XFILE_CAP_READ_AT | XFILE_CAP_STAT)) !=\n"
        "      (XFILE_CAP_READ_AT | XFILE_CAP_STAT)) { xrtSetErrorKind(XERR_UNSUPPORTED); return NULL; }\n"
        "  if (!xrtFileSize(source,&size)) { return NULL; }\n"
        "  if (offset > size || (length >= 0 && (uint64_t)length > size - offset))\n"
        "    { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  return xrtVfsPackCreate(source,offset,length < 0 ? size - offset : (uint64_t)length,options); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_OPTIONS_CREATE)) xl6OutputPuts(
        "void* xl6_xrt_fs_pack_options_create(int64_t format, uint64_t maxEntries,\n"
        "    uint64_t maxIndexBytes, uint64_t maxEntryBytes, uint64_t maxDecodedBytes, uint64_t cacheBytes)\n"
        "{ xvfspackoptions* options;\n"
        "  if (format < 0 || format > 2 || maxEntries == 0 || maxEntries > UINT32_MAX ||\n"
        "      maxIndexBytes == 0 || maxEntryBytes == 0 || maxDecodedBytes == 0)\n"
        "    { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  options = (xvfspackoptions*)xrtMalloc(sizeof(*options)); if (options == NULL) { return NULL; }\n"
        "  xrtVfsPackOptionsInit(options); options->Format = (xvfspackformat)format;\n"
        "  options->MaxEntries = (uint32_t)maxEntries; options->MaxIndexBytes = maxIndexBytes;\n"
        "  options->MaxEntryBytes = maxEntryBytes; options->MaxDecodedBytes = maxDecodedBytes;\n"
        "  options->CacheBytes = cacheBytes; return options; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_OPTIONS_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_fs_pack_options_destroy(void* handle) { xrtFree(handle); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_OPTIONS_VALUE)) xl6OutputPuts(
        "uint64_t xl6_xrt_fs_pack_options_value(void* handle, int64_t field)\n"
        "{ const xvfspackoptions* options = (const xvfspackoptions*)handle;\n"
        "  if (options == NULL) { xrtSetErrorKind(XERR_CLOSED); return 0; }\n"
        "  switch (field) {\n"
        "    case 0: return options->Format; case 1: return options->MaxEntries;\n"
        "    case 2: return options->MaxIndexBytes; case 3: return options->MaxEntryBytes;\n"
        "    case 4: return options->MaxDecodedBytes; case 5: return options->CacheBytes;\n"
        "    default: xrtSetErrorKind(XERR_RANGE); return 0;\n"
        "  } }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_PATH_CREATE)) xl6OutputPuts(
        "void* xl6_xrt_fs_pack_path_create(xlir_string path, void* options, uint64_t offset, int64_t length)\n"
        "{ const char* name; xfile source; xvfspack pack; xerror* error;\n"
        "  if (options == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; }\n"
        "  if (length < -1) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
        "  source = xrtOpen(name,XFILE_READ); if (source == NULL) { return NULL; }\n"
        "  pack = xlir_fs_pack_create(source,options,offset,length);\n"
        "  if (pack == NULL) {\n"
        "    error = xrtTakeError(); (void)xrtClose(source); xrtClearError(); xrtSetErrorTake(error);\n"
        "  }\n"
        "  return pack; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_TAKE_CREATE)) xl6OutputPuts(
        "void* xl6_xrt_fs_pack_take_create(void* handle, void* options, uint64_t offset, int64_t length)\n"
        "{ xlir_fs_file* owner = (xlir_fs_file*)handle; xvfspack pack;\n"
        "  xfile source = xlir_fs_file_get(handle); if (source == NULL) { return NULL; }\n"
        "  if (options == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; }\n"
        "  if (xrtAtomic32Load(&owner->refs,XMEMORY_ACQUIRE) != 1 || owner->deleteOnClose)\n"
        "    { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
        "  pack = xlir_fs_pack_create(source,options,offset,length);\n"
        "  if (pack != NULL) { owner->file = NULL; }\n"
        "  return pack; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_STATS_CREATE)) xl6OutputPuts(
        "void* xl6_xrt_fs_pack_stats_create(void* handle)\n"
        "{ xvfspackstats* stats;\n"
        "  if (handle == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; }\n"
        "  stats = (xvfspackstats*)xrtMalloc(sizeof(*stats)); if (stats == NULL) { return NULL; }\n"
        "  if (!xrtVfsPackStats((xvfspack)handle,stats)) { xrtFree(stats); return NULL; }\n"
        "  return stats; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_STATS_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_fs_pack_stats_destroy(void* handle) { xrtFree(handle); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_STATS_VALUE)) xl6OutputPuts(
        "uint64_t xl6_xrt_fs_pack_stats_value(void* handle, int64_t field)\n"
        "{ const xvfspackstats* s = (const xvfspackstats*)handle;\n"
        "  if (s == NULL) { xrtSetErrorKind(XERR_CLOSED); return 0; }\n"
        "  switch (field) {\n"
        "    case 0: return s->Hits; case 1: return s->Misses; case 2: return s->Loads;\n"
        "    case 3: return s->Failures; case 4: return s->ResidentBytes; case 5: return s->Evictions;\n"
        "    default: xrtSetErrorKind(XERR_RANGE); return 0;\n"
        "  } }\n", out);
}
