/* Length-bearing whole-file and native management contracts. */
static void xl6IrCgenEmitFsWholeRuntime(XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_WHOLE_READ)) xl6OutputPuts(
        "xvalue* xl6_xrt_fs_whole_read(xlir_string path, int64_t limit)\n"
        "{ const char* name = xlir_string_cstr(path); size_t maximum, size = 0; unsigned char* data; xvalue* result;\n"
        "  if (name == NULL || !xlir_fs_size(limit,&maximum)) { return NULL; }\n"
        "  data = xrtFileReadAllLimit(name,maximum,&size); if (data == NULL) { return NULL; }\n"
        "  result = xrtValueBytesTake(&data,size); xrtFree(data); return result; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_WHOLE_WRITE)) xl6OutputPuts(
        "bool xl6_xrt_fs_whole_write(xlir_string path, const xvalue* data, int64_t size, int64_t operation)\n"
        "{ const char* name = xlir_string_cstr(path); xbytesview view;\n"
        "  if (name == NULL) { return false; }\n"
        "  if (!xrtValueGetBytes(data,&view)) { xrtSetErrorKind(XERR_TYPE); return false; }\n"
        "  if (!xlir_fs_count(size,view.Size,&view.Size)) { return false; }\n"
        "  switch (operation) { case 0: return xrtFileWriteAll(name,view); case 1: return xrtFileAppend(name,view);\n"
        "    case 2: return xrtFileWriteAtomic(name,view); default: xrtSetErrorKind(XERR_RANGE); return false; } }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_TEXT_READ)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_text_read(xlir_string path, int64_t encoding, int64_t policy, int64_t limit)\n"
        "{ const char* name; char* text; size_t maximum, size = 0;\n"
        "  if (encoding < 0 || encoding > 5 || policy < 0 || policy > 1)\n"
        "    { xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
        "  name = xlir_string_cstr(path); if (name == NULL || !xlir_fs_size(limit,&maximum)) { return (xlir_string){0}; }\n"
        "  text = xrtFileReadTextLimit(name,(xencoding)encoding,(xutfpolicy)policy,maximum,&size);\n"
        "  return xlir_string_take_known(text,size); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_TEXT_WRITE)) xl6OutputPuts(
        "bool xl6_xrt_fs_text_write(xlir_string path, xlir_string text, int64_t encoding, int64_t policy, bool bom, int64_t size, bool atomic)\n"
        "{ const char* name; xstrview view = {text.Data,text.Size};\n"
        "  if (encoding < 1 || encoding > 5 || policy < 0 || policy > 1)\n"
        "    { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  name = xlir_string_cstr(path); if (name == NULL || !xlir_fs_count(size,view.Size,&view.Size)) { return false; }\n"
        "  return atomic ? xrtFileWriteTextAtomic(name,view,(xencoding)encoding,(xutfpolicy)policy,bom)\n"
        "    : xrtFileWriteText(name,view,(xencoding)encoding,(xutfpolicy)policy,bom); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_TEXT_APPEND)) xl6OutputPuts(
        "bool xl6_xrt_fs_text_append(xlir_string path, xlir_string text, int64_t encoding, int64_t policy, int64_t size)\n"
        "{ const char* name; bytes data; size_t count = 0; xstrview view = {text.Data,text.Size}; bool ok;\n"
        "  if (encoding < 1 || encoding > 5 || policy < 0 || policy > 1)\n"
        "    { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  name = xlir_string_cstr(path); if (name == NULL || !xlir_fs_count(size,view.Size,&view.Size)) { return false; }\n"
        "  data = xrtTranscode((xbytesview){(const uint8*)view.Data,view.Size}, XENCODING_UTF8, (xencoding)encoding,(xutfpolicy)policy,false,&count);\n"
        "  if (data == NULL) { return false; }\n"
        "  ok = xrtFileAppend(name,(xbytesview){data,count}); xrtFree(data); return ok; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_EXISTS)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_exists(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtPathExists(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_EXISTS)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_exists(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtFileExists(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_EXISTS)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_exists(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtDirExists(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_DELETE)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_delete(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtFileDelete(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_TOUCH)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_touch(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtFileTouch(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_LINK_DELETE)) xl6OutputPuts(
        "bool xl6_xrt_fs_link_delete(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtLinkDelete(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_REMOVE)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_remove(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtDirRemove(name); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SIZE)) xl6OutputPuts(
        "uint64_t xl6_xrt_fs_path_size(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); xfileinfo info;\n"
        "  return name != NULL && xrtPathStat(name,true,&info) ? info.Size : 0; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ATTRIBUTES)) xl6OutputPuts(
        "int64_t xl6_xrt_fs_path_attributes(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); xfileinfo info;\n"
        "  return name != NULL && xrtPathStat(name,true,&info) ? info.Attributes : 0; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SET_ATTRIBUTES)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_set_attributes(xlir_string path, int64_t attributes)\n"
        "{ const char* name;\n"
        "  if (attributes < 0 || (uint64_t)attributes > UINT32_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  name = xlir_string_cstr(path); return name != NULL && xrtPathSetAttributes(name,(uint32_t)attributes); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SET_SIZE)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_set_size(xlir_string path, uint64_t size)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL && xrtFileSetSize(name,size); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SET_MODE)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_set_mode(xlir_string path, int64_t mode, bool follow)\n"
        "{ const char* name;\n"
        "  if (mode < 0 || mode > 4095) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  name = xlir_string_cstr(path); return name != NULL && xrtPathSetMode(name,follow,(uint32_t)mode); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_RENAME)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_rename(xlir_string source, xlir_string target, bool option)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtPathRename(a,b,option); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_COPY)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_copy(xlir_string source, xlir_string target, bool option)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtFileCopy(a,b,option); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FILE_MOVE)) xl6OutputPuts(
        "bool xl6_xrt_fs_file_move(xlir_string source, xlir_string target, bool option)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtFileMove(a,b,option); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_COPY)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_copy(xlir_string source, xlir_string target, bool option)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtDirCopy(a,b,option); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_MOVE)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_move(xlir_string source, xlir_string target, bool option)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtDirMove(a,b,option); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_LINK_CREATE)) xl6OutputPuts(
        "bool xl6_xrt_fs_link_create(xlir_string source, xlir_string target, bool option)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtLinkCreate(a,b,option); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_LINK_HARD)) xl6OutputPuts(
        "bool xl6_xrt_fs_link_hard(xlir_string source, xlir_string target)\n"
        "{ const char* a = xlir_string_cstr(source); const char* b = xlir_string_cstr(target);\n"
        "  return a != NULL && b != NULL && xrtLinkHard(a,b); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_LINK_READ)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_link_read(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); return name != NULL ? xlir_string_take_cstr(xrtLinkRead(name)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_FIFO_CREATE)) xl6OutputPuts(
        "bool xl6_xrt_fs_fifo_create(xlir_string path, int64_t mode)\n"
        "{ const char* name;\n"
        "  if (mode < 0 || mode > 4095) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  name = xlir_string_cstr(path); return name != NULL && xrtFifoCreate(name,(uint32_t)mode); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_CREATE)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_create(xlir_string path, int64_t mode, bool recursive)\n"
        "{ const char* name;\n"
        "  if (mode < 0 || mode > 4095) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  name = xlir_string_cstr(path);\n"
        "  return name != NULL && (recursive ? xrtDirCreateAllMode(name,(uint32_t)mode) : xrtDirCreateMode(name,(uint32_t)mode)); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_EMPTY)) xl6OutputPuts(
        "bool xl6_xrt_fs_dir_empty(xlir_string path)\n"
        "{ const char* name = xlir_string_cstr(path); bool empty = false;\n"
        "  return name != NULL && xrtDirEmpty(name,&empty) && empty; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_DIR_TEMP)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_dir_temp(xlir_string directory, xlir_string prefix, xlir_string suffix)\n"
        "{ const char* dir = xlir_string_cstr(directory); const char* head = xlir_string_cstr(prefix); const char* tail = xlir_string_cstr(suffix);\n"
        "  if (dir == NULL || head == NULL || tail == NULL) { return (xlir_string){0}; }\n"
        "  return xlir_string_take_cstr(xrtDirTemp(*dir != 0 ? dir : NULL,head,tail)); }\n", out);
}
