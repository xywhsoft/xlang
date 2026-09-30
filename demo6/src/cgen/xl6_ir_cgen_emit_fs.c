/* Emission is selected by the already verified closed operation role. */
_Static_assert(XL6_IR_RUNTIME_BRIDGE_FS_END - XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN <= 192,
    "Filesystem operation mask overflow");
static int xl6IrCgenFsSelected(const XL6_IrCgenNeeds* needs, XL6_IrRuntimeBridgeKind kind)
{
    unsigned index = kind - XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN;
    uint64_t mask = index < 64 ? needs->fsBridgeOperations0 : index < 128 ? needs->fsBridgeOperations1 : needs->fsBridgeOperations2;
    return (mask & (UINT64_C(1) << (index % 64))) != 0;
}
static void xl6IrCgenEmitFsRuntime(XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    int file = 0, info = 0, dir = 0;
    for (XL6_IrRuntimeBridgeKind k = XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN; k < XL6_IR_RUNTIME_BRIDGE_FS_END; ++k) {
        if (!xl6IrCgenFsSelected(needs, k)) continue;
        if (k <= XL6_IR_RUNTIME_BRIDGE_FS_FILE_NATIVE || k == XL6_IR_RUNTIME_BRIDGE_FS_INFO_FILE ||
            k == XL6_IR_RUNTIME_BRIDGE_FS_FILE_HANDLE || k == XL6_IR_RUNTIME_BRIDGE_FS_FILE_READ_TEXT) file = 1;
        if (k >= XL6_IR_RUNTIME_BRIDGE_FS_INFO_PATH && k <= XL6_IR_RUNTIME_BRIDGE_FS_INFO_UTF8) info = 1;
        if ((k >= XL6_IR_RUNTIME_BRIDGE_FS_DIR_OPEN && k <= XL6_IR_RUNTIME_BRIDGE_FS_DIR_PATH) || k == XL6_IR_RUNTIME_BRIDGE_FS_INFO_DIRECTORY) dir = 1;
    }
    xl6OutputPuts(
            "static inline bool xlir_fs_size(int64_t request, size_t* size)\n"
            "{ if (request < 0 || (uint64_t)request > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  *size = (size_t)request; return true; }\n"
            "static inline bool xlir_fs_count(int64_t limit, size_t actual, size_t* size)\n"
            "{ if (limit < -1) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  *size = limit >= 0 && (uint64_t)limit < actual ? (size_t)limit : actual; return true; }\n"
            "\n", out);
    if (file) xl6OutputPuts(
            "typedef struct xlir_fs_file { xfile file; xatomic32 refs; } xlir_fs_file;\n"
            "static inline xlir_fs_file* xlir_fs_file_alloc(void)\n"
            "{ xlir_fs_file* owner = (xlir_fs_file*)xrtCalloc(1, sizeof(*owner));\n"
            "  if (owner != NULL) { xrtAtomic32Init(&owner->refs, 1); } return owner; }\n"
            "static inline xfile xlir_fs_file_get(void* handle)\n"
            "{ xlir_fs_file* owner = (xlir_fs_file*)handle;\n"
            "  if (owner == NULL || owner->file == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; } return owner->file; }\n"
            "static inline void xlir_fs_file_drop(xlir_fs_file* owner)\n"
            "{ if (owner != NULL && xrtAtomic32FetchSub(&owner->refs, 1, XMEMORY_ACQ_REL) == 1)\n"
            "    { if (owner->file != NULL) (void)xrtClose(owner->file); xrtFree(owner); } }\n"
            "static inline bool xlir_fs_options(int64_t flags, int64_t mode, int64_t share, xfileoptions* options)\n"
            "{ if (flags <= 0 || flags > 255 || (flags & 3) == 0 || mode < 0 || mode > 4095 || share < 0 || share > 7)\n"
            "    { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "  xrtFileOptionsInit(options); options->Flags = (uint32_t)flags;\n"
            "  options->Mode = (uint32_t)mode; options->Share = (uint32_t)share; return true; }\n"
            "\n", out);
    if (info) xl6OutputPuts(
            "typedef struct xlir_fs_info { xfileinfo info; char* path; char* name; size_t nameSize; uint32_t flags; } xlir_fs_info;\n"
            "static inline void xlir_fs_info_drop(xlir_fs_info* info)\n"
            "{ if (info != NULL) { xrtFree(info->path); xrtFree(info->name); xrtFree(info); } }\n"
            "static inline xlir_fs_info* xlir_fs_info_alloc(const char* path)\n"
            "{ const char* name = path; const char* at; xlir_fs_info* info = (xlir_fs_info*)xrtCalloc(1, sizeof(*info));\n"
            "  if (info == NULL) { return NULL; }\n"
            "  for (at = path; *at != 0; ++at) if (*at == '/'\n"
            "#if defined(_WIN32)\n"
            "      || *at == '\\\\'\n"
            "#endif\n"
            "      ) name = at + 1;\n"
            "  info->path = xrtStrDup(path); info->nameSize = strlen(name); info->name = xrtStrDupN(name, info->nameSize);\n"
            "  if (info->path == NULL || info->name == NULL) { xlir_fs_info_drop(info); return NULL; }\n"
            "  info->flags = 1u; return info; }\n"
            "\n", out);
    if (dir) xl6OutputPuts(
            "typedef struct xlir_fs_dir { xdir dir; xdirentry entry; bool current; } xlir_fs_dir;\n"
            "\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN)) xl6OutputPuts(
            "void* xl6_xrt_fs_file_open(xlir_string path, int64_t flags, int64_t mode, int64_t share)\n"
            "{ const char* name; xlir_fs_file* owner; xfileoptions options;\n"
            "  if (!xlir_fs_options(flags, mode, share, &options)) { return NULL; }\n"
            "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
            "  owner = xlir_fs_file_alloc(); if (owner == NULL) { return NULL; }\n"
            "  owner->file = xrtFileOpen(name, &options);\n"
            "  if (owner->file == NULL) { xrtFree(owner); return NULL; } return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_VFS_OPEN)) xl6OutputPuts(
            "void* xl6_xrt_fs_vfs_open(void* vfs, xlir_string path, int64_t flags, int64_t mode, int64_t share)\n"
            "{ const char* name; xlir_fs_file* owner; xfileoptions options;\n"
            "  if (!xlir_fs_options(flags, mode, share, &options)) { return NULL; }\n"
            "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
            "  owner = xlir_fs_file_alloc(); if (owner == NULL) { return NULL; }\n"
            "  owner->file = xrtVfsOpen((xvfs)vfs, name, &options);\n"
            "  if (owner->file == NULL) { xrtFree(owner); return NULL; } return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_DESTROY)) xl6OutputPuts(
            "void xl6_xrt_fs_file_destroy(void* handle)\n"
            "{ xerror* error = xrtTakeError(); xlir_fs_file_drop((xlir_fs_file*)handle);\n"
            "  xrtClearError(); if (error != NULL) xrtSetErrorTake(error); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_CLOSE)) xl6OutputPuts(
            "bool xl6_xrt_fs_file_close(void* handle)\n"
            "{ xlir_fs_file* owner = (xlir_fs_file*)handle; xfile file;\n"
            "  if (owner == NULL || owner->file == NULL) { return true; }\n"
            "  if (xrtAtomic32Load(&owner->refs, XMEMORY_ACQUIRE) != 1)\n"
            "    { xrtSetErrorKind(XERR_STATE); return false; }\n"
            "  file = owner->file; owner->file = NULL; return xrtClose(file); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_CLOSED)) xl6OutputPuts(
            "bool xl6_xrt_fs_file_closed(void* handle)\n"
            "{ return handle == NULL || ((xlir_fs_file*)handle)->file == NULL; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_READ)) xl6OutputPuts(
            "xvalue* xl6_xrt_fs_file_read(void* handle, int64_t request, bool exact, int64_t offset)\n"
            "{ xfile file; unsigned char* data = NULL; size_t size, done = 0; bool ok; xvalue* result;\n"
            "  if (!xlir_fs_size(request, &size) || offset < -1)\n"
            "    { if (offset < -1) xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "  file = xlir_fs_file_get(handle); if (file == NULL) { return NULL; }\n"
            "  if (size != 0 && (data = (unsigned char*)xrtMalloc(size)) == NULL) { return NULL; }\n"
            "  if (offset >= 0) ok = exact ? xrtReadAtFull(file, (uint64_t)offset, data, size, &done)\n"
            "      : xrtReadAt(file, (uint64_t)offset, data, size, &done);\n"
            "  else ok = exact ? xrtReadFull(file, data, size, &done) : xrtRead(file, data, size, &done);\n"
            "  if (!ok) { xrtFree(data); return NULL; }\n"
            "  result = xrtValueBytesTake(&data, done); xrtFree(data); return result; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_WRITE)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_file_write(void* handle, const xvalue* data, int64_t limit, bool full, int64_t offset)\n"
            "{ xbytesview view; xfile file; size_t done = 0; bool ok;\n"
            "  if (!xrtValueGetBytes(data, &view)) { xrtSetErrorKind(XERR_TYPE); return 0; }\n"
            "  if (!xlir_fs_count(limit, view.Size, &view.Size) || offset < -1)\n"
            "    { if (offset < -1) xrtSetErrorKind(XERR_RANGE); return 0; }\n"
            "  file = xlir_fs_file_get(handle); if (file == NULL) { return 0; }\n"
            "  if (offset >= 0) ok = full ? xrtWriteAtFull(file, (uint64_t)offset, view.Data, view.Size, &done)\n"
            "      : xrtWriteAt(file, (uint64_t)offset, view.Data, view.Size, &done);\n"
            "  else ok = full ? xrtWriteFull(file, view.Data, view.Size, &done)\n"
            "      : xrtWrite(file, view.Data, view.Size, &done);\n"
            "  if (!ok) { return 0; }\n"
            "  if (done > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; } return (int64_t)done; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_WRITE_TEXT)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_file_write_text(void* handle, xlir_string data, int64_t limit)\n"
            "{ xfile file; size_t size, done = 0;\n"
            "  if (!xlir_fs_count(limit, data.Size, &size)) { return 0; }\n"
            "  file = xlir_fs_file_get(handle); if (file == NULL || !xrtWriteFull(file, data.Data, size, &done)) { return 0; }\n"
            "  if (done > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; } return (int64_t)done; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_SEEK)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_file_seek(void* handle, int64_t offset, int64_t origin)\n"
            "{ xfile file; uint64_t result = 0; if (origin < 0 || origin > 2) { xrtSetErrorKind(XERR_RANGE); return 0; }\n"
            "  file = xlir_fs_file_get(handle); if (file != NULL) { (void)xrtSeek(file, offset, (xseek)origin, &result); } return result; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_TELL)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_file_tell(void* handle)\n"
            "{ uint64_t result = 0; xfile file = xlir_fs_file_get(handle);\n"
            "  if (file != NULL) { (void)xrtTell(file, &result); } return result; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_SIZE)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_file_size(void* handle)\n"
            "{ uint64_t result = 0; xfile file = xlir_fs_file_get(handle);\n"
            "  if (file != NULL) { (void)xrtFileSize(file, &result); } return result; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_RESIZE)) xl6OutputPuts(
            "bool xl6_xrt_fs_file_resize(void* handle, uint64_t size)\n"
            "{ xfile file = xlir_fs_file_get(handle); return file != NULL && xrtFileResize(file, size); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_FLUSH)) xl6OutputPuts(
            "bool xl6_xrt_fs_file_flush(void* handle)\n"
            "{ xfile file = xlir_fs_file_get(handle); return file != NULL && xrtFlush(file); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_FLAGS)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_file_flags(void* handle)\n"
            "{ xfile file = xlir_fs_file_get(handle); return file != NULL ? xrtFileFlags(file) : 0; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_NATIVE)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_file_native(void* handle)\n"
            "{ xfile file = xlir_fs_file_get(handle); return file != NULL ? (int64_t)xrtFileNative(file) : -1; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_PATH)) xl6OutputPuts(
            "void* xl6_xrt_fs_info_path(xlir_string path, bool follow)\n"
            "{ const char* name = xlir_string_cstr(path); xlir_fs_info* info;\n"
            "  if (name == NULL) { return NULL; } info = xlir_fs_info_alloc(name);\n"
            "  if (info == NULL) { return NULL; }\n"
            "  if (!xrtPathStat(name, follow, &info->info)) { xlir_fs_info_drop(info); return NULL; } return info; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_FILE)) xl6OutputPuts(
            "void* xl6_xrt_fs_info_file(void* handle)\n"
            "{ xfile file = xlir_fs_file_get(handle); xlir_fs_info* info;\n"
            "  if (file == NULL) { return NULL; } info = xlir_fs_info_alloc(\"\");\n"
            "  if (info == NULL) { return NULL; }\n"
            "  if (!xrtFileStat(file, &info->info)) { xlir_fs_info_drop(info); return NULL; } return info; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_VFS)) xl6OutputPuts(
            "void* xl6_xrt_fs_info_vfs(void* vfs, xlir_string path, bool follow)\n"
            "{ const char* name = xlir_string_cstr(path); xlir_fs_info* info;\n"
            "  if (name == NULL) { return NULL; } info = xlir_fs_info_alloc(name);\n"
            "  if (info == NULL) { return NULL; }\n"
            "  if (!xrtVfsStat((xvfs)vfs, name, follow, &info->info)) { xlir_fs_info_drop(info); return NULL; } return info; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_DIRECTORY)) xl6OutputPuts(
            "void* xl6_xrt_fs_info_directory(void* handle)\n"
            "{ xlir_fs_dir* dir = (xlir_fs_dir*)handle; xlir_fs_info* info;\n"
            "  if (dir == NULL || !dir->current) { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
            "  info = (xlir_fs_info*)xrtCalloc(1, sizeof(*info)); if (info == NULL) { return NULL; }\n"
            "  info->name = xrtStrDupN(dir->entry.Name.Data, dir->entry.Name.Size);\n"
            "  info->nameSize = dir->entry.Name.Size; info->flags = dir->entry.Flags;\n"
            "  if (info->name == NULL) { xlir_fs_info_drop(info); return NULL; }\n"
            "  info->path = xrtDirEntryPath(dir->dir, &dir->entry);\n"
            "  if (info->path == NULL) { xlir_fs_info_drop(info); return NULL; }\n"
            "  info->info = dir->entry.Info; return info; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_DESTROY)) xl6OutputPuts(
            "void xl6_xrt_fs_info_destroy(void* handle) { xlir_fs_info_drop((xlir_fs_info*)handle); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_TYPE)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_type(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Type; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_AVAILABLE)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_available(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Available; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_MODE)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_mode(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Mode; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_ATTRIBUTES)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_attributes(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Attributes; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_SIZE)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_info_size(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (uint64_t)((xlir_fs_info*)handle)->info.Size; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_DEVICE)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_info_device(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (uint64_t)((xlir_fs_info*)handle)->info.Device; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_IDENTITY)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_info_identity(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (uint64_t)((xlir_fs_info*)handle)->info.Identity; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_LINK_COUNT)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_info_link_count(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (uint64_t)((xlir_fs_info*)handle)->info.LinkCount; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_ACCESSED)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_accessed(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Accessed; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_MODIFIED)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_modified(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Modified; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_CREATED)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_created(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Created; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_CHANGED)) xl6OutputPuts(
            "int64_t xl6_xrt_fs_info_changed(void* handle)\n"
            "{ if (handle == NULL) { xrtSetErrorKind(XERR_STATE); return 0; }\n"
            "  return (int64_t)((xlir_fs_info*)handle)->info.Changed; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_NAME)) xl6OutputPuts(
            "xlir_string xl6_xrt_fs_info_name(void* handle)\n"
            "{ xlir_fs_info* info = (xlir_fs_info*)handle;\n"
            "  if (info == NULL) { xrtSetErrorKind(XERR_STATE); return (xlir_string){0}; }\n"
            "  if (!xrtUtf8Valid((xstrview){info->name, info->nameSize}, NULL))\n"
            "    { xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "  return xlir_string_from_utf8(info->name, info->nameSize); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_NAME_BYTES)) xl6OutputPuts(
            "xvalue* xl6_xrt_fs_info_name_bytes(void* handle)\n"
            "{ xlir_fs_info* info = (xlir_fs_info*)handle;\n"
            "  if (info == NULL) { xrtSetErrorKind(XERR_STATE); return NULL; }\n"
            "  return xrtValueBytes((xbytesview){(const unsigned char*)info->name, info->nameSize}); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_PATH_TEXT)) xl6OutputPuts(
            "xlir_string xl6_xrt_fs_info_path_text(void* handle)\n"
            "{ xlir_fs_info* info = (xlir_fs_info*)handle;\n"
            "  if (info == NULL) { xrtSetErrorKind(XERR_STATE); return (xlir_string){0}; }\n"
            "  return xlir_string_from_utf8(info->path, strlen(info->path)); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_INFO_UTF8)) xl6OutputPuts(
            "bool xl6_xrt_fs_info_utf8(void* handle)\n"
            "{ xlir_fs_info* info = (xlir_fs_info*)handle;\n"
            "  if (info == NULL) { xrtSetErrorKind(XERR_STATE); return false; }\n"
            "  return (info->flags & 1u) != 0; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_OPEN)) xl6OutputPuts(
            "void* xl6_xrt_fs_dir_open(xlir_string path, int64_t flags)\n"
            "{ const char* name; xlir_fs_dir* owner;\n"
            "  if (flags < 0 || flags > 7) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
            "  owner = (xlir_fs_dir*)xrtCalloc(1, sizeof(*owner)); if (owner == NULL) { return NULL; }\n"
            "  owner->dir = xrtDirOpen(name, (uint32_t)flags);\n"
            "  if (owner->dir == NULL) { xrtFree(owner); return NULL; } return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_VFS_DIR_OPEN)) xl6OutputPuts(
            "void* xl6_xrt_fs_vfs_dir_open(void* vfs, xlir_string path, int64_t flags)\n"
            "{ const char* name; xlir_fs_dir* owner;\n"
            "  if (flags < 0 || flags > 7) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
            "  owner = (xlir_fs_dir*)xrtCalloc(1, sizeof(*owner)); if (owner == NULL) { return NULL; }\n"
            "  owner->dir = xrtVfsDirOpen((xvfs)vfs, name, (uint32_t)flags);\n"
            "  if (owner->dir == NULL) { xrtFree(owner); return NULL; } return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_DESTROY)) xl6OutputPuts(
            "void xl6_xrt_fs_dir_destroy(void* handle)\n"
            "{ xerror* error = xrtTakeError(); xlir_fs_dir* owner = (xlir_fs_dir*)handle;\n"
            "  if (owner != NULL) { if (owner->dir != NULL) (void)xrtDirClose(owner->dir); xrtFree(owner); }\n"
            "  xrtClearError(); if (error != NULL) xrtSetErrorTake(error); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_CLOSE)) xl6OutputPuts(
            "bool xl6_xrt_fs_dir_close(void* handle)\n"
            "{ xlir_fs_dir* owner = (xlir_fs_dir*)handle; xdir dir;\n"
            "  if (owner == NULL || owner->dir == NULL) { return true; }\n"
            "  dir = owner->dir; owner->dir = NULL; owner->current = false; return xrtDirClose(dir); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_ADVANCE)) xl6OutputPuts(
            "bool xl6_xrt_fs_dir_advance(void* handle)\n"
            "{ xlir_fs_dir* owner = (xlir_fs_dir*)handle; xdirnext next;\n"
            "  if (owner == NULL || owner->dir == NULL) { xrtSetErrorKind(XERR_STATE); return false; }\n"
            "  owner->current = false; next = xrtDirNext(owner->dir, &owner->entry);\n"
            "  owner->current = next == XDIR_NEXT_ITEM; return owner->current; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_PATH)) xl6OutputPuts(
            "xlir_string xl6_xrt_fs_dir_path(void* handle)\n"
            "{ xlir_fs_dir* owner = (xlir_fs_dir*)handle; const char* path;\n"
            "  if (owner == NULL || owner->dir == NULL) { xrtSetErrorKind(XERR_STATE); return (xlir_string){0}; }\n"
            "  path = xrtDirPath(owner->dir); return path != NULL ? xlir_string_from_utf8(path, strlen(path)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PACK_STATS)) xl6OutputPuts(
            "uint64_t xl6_xrt_fs_pack_stats(void* pack, int64_t field)\n"
            "{ xvfspackstats stats; if (field < 0 || field > 5) { xrtSetErrorKind(XERR_RANGE); return 0; }\n"
            "  if (!xrtVfsPackStats((xvfspack)pack, &stats)) { return 0; }\n"
            "  switch (field) { case 0: return stats.Hits; case 1: return stats.Misses; case 2: return stats.Loads;\n"
            "    case 3: return stats.Failures; case 4: return stats.ResidentBytes; default: return stats.Evictions; } }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_HANDLE)) xl6OutputPuts(
            "void* xl6_xrt_fs_file_handle(void* handle) { return xlir_fs_file_get(handle); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_READ_TEXT)) xl6OutputPuts(
            "xlir_string xl6_xrt_fs_file_read_text(void* handle, int64_t count, int64_t encoding, int64_t policy)\n"
            "{ xfile file; size_t request, done = 0, bom = 0, outputSize = 0; uint64_t position, end;\n"
            "  unsigned char* raw; unsigned char* text; xencoding detected; xbytesview data;\n"
            "  if (count < -1 || encoding < 0 || encoding > 5 || policy < 0 || policy > 1)\n"
            "    { xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
            "  file = xlir_fs_file_get(handle); if (file == NULL) { return (xlir_string){0}; }\n"
            "  if (count == -1) {\n"
            "    if (!xrtTell(file, &position) || !xrtFileSize(file, &end)) { return (xlir_string){0}; }\n"
            "    if (end > position && end - position > 67108864u) { xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
            "    request = end > position ? (size_t)(end - position) : 0;\n"
            "  } else if (!xlir_fs_size(count, &request)) { return (xlir_string){0}; }\n"
            "  raw = (unsigned char*)xrtMalloc(request != 0 ? request : 1); if (raw == NULL) { return (xlir_string){0}; }\n"
            "  if (!xrtRead(file, raw, request, &done)) { xrtFree(raw); return (xlir_string){0}; }\n"
            "  data = (xbytesview){raw, done}; detected = xrtEncodingBom(data, &bom);\n"
            "  if (encoding != 0 && detected != XENCODING_UNKNOWN && detected != encoding)\n"
            "    { xrtFree(raw); xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "  if (encoding == 0) encoding = detected != XENCODING_UNKNOWN ? detected :\n"
            "      done == 0 ? XENCODING_UTF8 : xrtEncodingGuess(data).Encoding;\n"
            "  if (encoding == XENCODING_UNKNOWN) { xrtFree(raw); xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "  data.Data += bom; data.Size -= bom;\n"
            "  text = xrtTranscode(data, (xencoding)encoding, XENCODING_UTF8, (xutfpolicy)policy, false, &outputSize);\n"
            "  xrtFree(raw); return xlir_string_take_known((char*)text, outputSize); }\n", out);
}
