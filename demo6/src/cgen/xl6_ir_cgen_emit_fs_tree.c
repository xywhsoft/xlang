/* Selected closed filesystem operations; XRT owns traversal and tree policy. */
static void xl6IrCgenEmitFsTreeRuntime(XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    int stats = 0;
    for (XL6_IrRuntimeBridgeKind k = XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN; k < XL6_IR_RUNTIME_BRIDGE_FS_END; ++k)
        if (xl6IrCgenFsSelected(needs,k) && xl6IrRuntimeFsBridgeUsesOwner(k,"xl6_xrt_fs_stats_destroy")) stats = 1;
    if (stats) xl6OutputPuts(
        "typedef struct xlir_fs_stats { xwalkstats value; } xlir_fs_stats;\n"
        "static inline xlir_fs_stats* xlir_fs_stats_alloc(void)\n"
        "{ return (xlir_fs_stats*)xrtCalloc(1,sizeof(xlir_fs_stats)); }\n"
        "static inline bool xlir_fs_walk_options(int64_t flags, int64_t depth, xwalkoptions* options)\n"
        "{ if (flags < 0 || flags > 3 || depth < -1 || (depth >= 0 && (uint64_t)depth > SIZE_MAX))\n"
        "    { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  xrtWalkOptionsInit(options); options->Flags = (uint32_t)flags;\n"
        "  options->MaxDepth = depth < 0 ? XRT_NPOS : (size_t)depth; return true; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_TEMP)) xl6OutputPuts(
        "void* xl6_xrt_fs_file_temp(xlir_string directory, xlir_string prefix, xlir_string suffix)\n"
        "{ const char* dir = xlir_string_cstr(directory); const char* head = xlir_string_cstr(prefix);\n"
        "  const char* tail = xlir_string_cstr(suffix); xlir_fs_file* owner;\n"
        "  if (dir == NULL || head == NULL || tail == NULL) { return NULL; }\n"
        "  owner = xlir_fs_file_alloc(); if (owner == NULL) { return NULL; }\n"
        "  owner->file = xrtFileTemp(*dir != 0 ? dir : NULL,head,tail,&owner->tempPath);\n"
        "  if (owner->file == NULL) { xrtFree(owner); return NULL; }\n"
        "  owner->deleteOnClose = true; return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_TEMP_PATH)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_file_temp_path(void* handle)\n"
        "{ xlir_fs_file* owner = (xlir_fs_file*)handle;\n"
        "  return xlir_string_from_utf8(owner != NULL && owner->tempPath != NULL ? owner->tempPath : \"\",\n"
        "    owner != NULL && owner->tempPath != NULL ? strlen(owner->tempPath) : 0); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_TEMP_KEEP)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_temp_keep(void* handle)\n"
        "{ xlir_fs_file* owner = (xlir_fs_file*)handle;\n"
        "  if (owner == NULL || owner->tempPath == NULL || owner->file == NULL || !owner->deleteOnClose)\n"
        "    { xrtSetErrorKind(XERR_STATE); return false; }\n"
        "  owner->deleteOnClose = false; return true; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_ROOTS)) xl6OutputPuts(
        "xvalue* xl6_xrt_fs_dir_roots(void)\n"
        "{ xdirroots roots = {0}; size_t i; xvalue* result;\n"
        "  if (!xrtDirRoots(&roots)) { return NULL; }\n"
        "  result = xrtValueArray();\n"
        "  for (i = 0; result != NULL && i < roots.Count; ++i) {\n"
        "    if (!xrtValueArrayAppendNew(result,xrtValueString((xstrview){roots.Items[i],strlen(roots.Items[i])}))) {\n"
        "      xrtValueRelease(result); result = NULL; break;\n"
        "    }\n"
        "  }\n"
        "  xrtDirRootsFree(&roots); return result; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SET_TIMES)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_set_times(xlir_string path, int64_t accessed, int64_t modified,\n"
        "    bool setAccessed, bool setModified, bool follow)\n"
        "{ const char* name = xlir_string_cstr(path); xtime a = (xtime)accessed, m = (xtime)modified;\n"
        "  if (!setAccessed && !setModified) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
        "  return name != NULL && xrtPathSetTimes(name,follow,setAccessed ? &a : NULL,setModified ? &m : NULL); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_WALK_STATS)) xl6OutputPuts(
        "void* xl6_xrt_fs_walk_stats(xlir_string path, int64_t flags, int64_t depth)\n"
        "{ const char* name; xwalkoptions options; xlir_fs_stats* stats;\n"
        "  if (!xlir_fs_walk_options(flags,depth,&options)) { return NULL; }\n"
        "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
        "  stats = xlir_fs_stats_alloc(); if (stats == NULL) { return NULL; }\n"
        "  if (!xrtFileWalk(name,&options,NULL,NULL,&stats->value)) { xrtFree(stats); return NULL; }\n"
        "  return stats; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_STATS)) xl6OutputPuts(
        "void* xl6_xrt_fs_dir_stats(xlir_string path, bool recursive)\n"
        "{ const char* name = xlir_string_cstr(path); xlir_fs_stats* stats;\n"
        "  if (name == NULL) { return NULL; }\n"
        "  stats = xlir_fs_stats_alloc(); if (stats == NULL) { return NULL; }\n"
        "  if (!xrtDirStats(name,recursive,&stats->value)) { xrtFree(stats); return NULL; }\n"
        "  return stats; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_TREE_COPY)) xl6OutputPuts(
        "void* xl6_xrt_fs_tree_copy(xlir_string source, xlir_string target, int64_t flags)\n"
        "{ const char* a; const char* b; xtreecopyoptions options; xlir_fs_stats* stats;\n"
        "  if (flags < 0 || flags > 127 || (flags & 12) == 12) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  a = xlir_string_cstr(source); b = xlir_string_cstr(target);\n"
        "  if (a == NULL || b == NULL) { return NULL; }\n"
        "  stats = xlir_fs_stats_alloc(); if (stats == NULL) { return NULL; }\n"
        "  xrtTreeCopyOptionsInit(&options); options.Flags = (uint32_t)flags;\n"
        "  if (!xrtFileTreeCopy(a,b,&options,&stats->value)) { xrtFree(stats); return NULL; }\n"
        "  return stats; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_TREE_REMOVE)) xl6OutputPuts(
        "void* xl6_xrt_fs_tree_remove(xlir_string path, bool keepRoot)\n"
        "{ const char* name = xlir_string_cstr(path); xlir_fs_stats* stats;\n"
        "  if (name == NULL) { return NULL; }\n"
        "  stats = xlir_fs_stats_alloc(); if (stats == NULL) { return NULL; }\n"
        "  if (!xrtFileTreeRemove(name,keepRoot,&stats->value)) { xrtFree(stats); return NULL; }\n"
        "  return stats; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_STATS_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_fs_stats_destroy(void* handle) { xrtFree(handle); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_STATS_VALUE)) xl6OutputPuts(
        "uint64_t xl6_xrt_fs_stats_value(void* handle, int64_t field)\n"
        "{ const xwalkstats* s;\n"
        "  if (handle == NULL) { xrtSetErrorKind(XERR_CLOSED); return 0; }\n"
        "  s = &((xlir_fs_stats*)handle)->value;\n"
        "  switch (field) {\n"
        "    case 0: return s->Items; case 1: return s->Files; case 2: return s->Directories;\n"
        "    case 3: return s->Links; case 4: return s->Others; case 5: return s->Bytes;\n"
        "    default: xrtSetErrorKind(XERR_RANGE); return 0;\n"
        "  } }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_STATS_STOPPED)) xl6OutputPuts(
        "bool xl6_xrt_fs_stats_stopped(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_CLOSED); return false; }\n"
        "  return ((xlir_fs_stats*)handle)->value.Stopped; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_ENSURE_EMPTY)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_ensure_empty(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtDirEnsureEmpty(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_REMOVE_ALL)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_remove_all(xlir_string path, bool keepRoot)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtFileTreeRemove(name,keepRoot,NULL); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_SIZE)) xl6OutputPuts(
        "uint64_t xl6_xrt_fs_dir_size(xlir_string path, bool recursive)\n"
        "{ const char* name = xlir_string_cstr(path); uint64_t size = 0;\n"
        "  return name != NULL && xrtDirSize(name,recursive,&size) ? size : 0; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_LIST)) xl6OutputPuts(
        "typedef struct xlir_fs_list { xvalue* result; size_t limit; int64_t kind; } xlir_fs_list;\n"
        "static xwalkcontrol xlir_fs_list_visit(const xwalkentry* entry, void* context)\n"
        "{ xlir_fs_list* list = (xlir_fs_list*)context;\n"
        "  if (entry->Depth == 0 || entry->Event == XWALK_LEAVE) { return XWALK_CONTINUE; }\n"
        "  if ((list->kind == 1 && entry->Info.Type != XFILE_TYPE_FILE) ||\n"
        "      (list->kind == 2 && entry->Info.Type != XFILE_TYPE_DIRECTORY)) { return XWALK_CONTINUE; }\n"
        "  if (xrtValueCount(list->result) >= list->limit) { xrtSetErrorKind(XERR_RANGE); return XWALK_ERROR; }\n"
        "  if (!xrtValueArrayAppendNew(list->result,xrtValueString((xstrview){entry->Path,strlen(entry->Path)})))\n"
        "    { return XWALK_ERROR; }\n"
        "  return XWALK_CONTINUE; }\n"
        "xvalue* xl6_xrt_fs_dir_list(xlir_string path, bool recursive, int64_t kind, int64_t maxEntries)\n"
        "{ const char* name; xlir_fs_list list = {0}; xwalkoptions options; xfileinfo info;\n"
        "  if (kind < 0 || kind > 2 || !xlir_fs_size(maxEntries,&list.limit)) {\n"
        "    if (kind < 0 || kind > 2) { xrtSetErrorKind(XERR_RANGE); } return NULL;\n"
        "  }\n"
        "  name = xlir_string_cstr(path); if (name == NULL) { return NULL; }\n"
        "  if (!xrtPathStat(name,true,&info)) { return NULL; }\n"
        "  if (info.Type != XFILE_TYPE_DIRECTORY) { xrtSetErrorKind(XERR_TYPE); return NULL; }\n"
        "  xrtWalkOptionsInit(&options); options.MaxDepth = recursive ? XRT_NPOS : 1;\n"
        "  list.kind = kind; list.result = xrtValueArray(); if (list.result == NULL) { return NULL; }\n"
        "  if (!xrtFileWalk(name,&options,xlir_fs_list_visit,&list,NULL)) {\n"
        "    xerror* error = xrtTakeError(); xrtValueRelease(list.result); xrtClearError(); xrtSetErrorTake(error); return NULL;\n"
        "  }\n"
        "  return list.result; }\n", out);
}
