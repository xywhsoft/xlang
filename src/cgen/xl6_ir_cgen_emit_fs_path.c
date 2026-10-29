/* Path syntax lives in XRT; these adapters only own returned text and snapshots. */
static void xl6IrCgenEmitFsPathRuntime(XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    int parts = 0, iter = 0;
    for (XL6_IrRuntimeBridgeKind k = XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN; k < XL6_IR_RUNTIME_BRIDGE_FS_END; ++k) {
        if (!xl6IrCgenFsSelected(needs,k)) continue;
        parts |= xl6IrRuntimeFsBridgeUsesOwner(k,"xl6_xrt_fs_path_parts_destroy");
        iter |= xl6IrRuntimeFsBridgeUsesOwner(k,"xl6_xrt_fs_path_iter_destroy");
    }
    if (parts) xl6OutputPuts(
        "typedef struct xlir_fs_parts { char* text; xpathparts parts; } xlir_fs_parts;\n", out);
    if (iter) xl6OutputPuts(
        "typedef struct xlir_fs_pathiter { char* text; xpathiter iter; xpathcomponent item; bool current; } xlir_fs_pathiter;\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_PARTS_CREATE)) xl6OutputPuts(
        "void* xl6_xrt_fs_path_parts_create(xlir_string path, int64_t style)\n"
        "{ xlir_fs_parts* owner;\n"
        "  if (style < 0 || style > 2 || xlir_string_cstr(path) == NULL) { if (style < 0 || style > 2) xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  owner = (xlir_fs_parts*)xrtCalloc(1, sizeof(*owner)); if (owner == NULL) { return NULL; }\n"
        "  owner->text = xrtStrDupN(path.Data, path.Size);\n"
        "  if (owner->text == NULL || !xrtPathParse((xstrview){owner->text,path.Size}, (xpathstyle)style, &owner->parts))\n"
        "    { xrtFree(owner->text); xrtFree(owner); return NULL; } return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_PARTS_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_fs_path_parts_destroy(void* handle)\n"
        "{ xlir_fs_parts* owner = (xlir_fs_parts*)handle; if (owner != NULL) { xrtFree(owner->text); xrtFree(owner); } }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_PARTS_TEXT)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_parts_text(void* handle, int64_t field)\n"
        "{ xlir_fs_parts* owner = (xlir_fs_parts*)handle; xstrview text;\n"
        "  if (owner == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return (xlir_string){0}; }\n"
        "  switch (field) { case 0: text = owner->parts.Root; break; case 1: text = owner->parts.Parent; break;\n"
        "    case 2: text = owner->parts.Name; break; case 3: text = owner->parts.Stem; break; case 4: text = owner->parts.Ext; break;\n"
        "    default: xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
        "  return xlir_string_from_utf8(text.Data, text.Size); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_PARTS_NUMBER)) xl6OutputPuts(
        "int64_t xl6_xrt_fs_path_parts_number(void* handle, int64_t field)\n"
        "{ xlir_fs_parts* owner = (xlir_fs_parts*)handle;\n"
        "  if (owner == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  if (field == 0) { return owner->parts.Flags; } if (field == 1) { return owner->parts.RootKind; }\n"
        "  xrtSetErrorKind(XERR_RANGE); return 0; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ITER_CREATE)) xl6OutputPuts(
        "void* xl6_xrt_fs_path_iter_create(xlir_string path, int64_t style)\n"
        "{ xlir_fs_pathiter* owner;\n"
        "  if (style < 0 || style > 2 || xlir_string_cstr(path) == NULL) { if (style < 0 || style > 2) xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  owner = (xlir_fs_pathiter*)xrtCalloc(1, sizeof(*owner)); if (owner == NULL) { return NULL; }\n"
        "  owner->text = xrtStrDupN(path.Data,path.Size);\n"
        "  if (owner->text == NULL || !xrtPathIterInit(&owner->iter,(xstrview){owner->text,path.Size},(xpathstyle)style))\n"
        "    { xrtFree(owner->text); xrtFree(owner); return NULL; } return owner; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ITER_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_fs_path_iter_destroy(void* handle)\n"
        "{ xlir_fs_pathiter* owner = (xlir_fs_pathiter*)handle; if (owner != NULL) { xrtFree(owner->text); xrtFree(owner); } }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ITER_ADVANCE)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_iter_advance(void* handle)\n"
        "{ xlir_fs_pathiter* owner = (xlir_fs_pathiter*)handle;\n"
        "  if (owner == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
        "  owner->current = xrtPathNext(&owner->iter, &owner->item); return owner->current; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ITER_TEXT)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_iter_text(void* handle)\n"
        "{ xlir_fs_pathiter* owner = (xlir_fs_pathiter*)handle;\n"
        "  if (owner == NULL || !owner->current) { xrtSetErrorKind(XERR_STATE); return (xlir_string){0}; }\n"
        "  return xlir_string_from_utf8(owner->item.Text.Data,owner->item.Text.Size); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ITER_KIND)) xl6OutputPuts(
        "int64_t xl6_xrt_fs_path_iter_kind(void* handle)\n"
        "{ xlir_fs_pathiter* owner = (xlir_fs_pathiter*)handle;\n"
        "  if (owner == NULL || !owner->current) { xrtSetErrorKind(XERR_STATE); return 0; } return owner->item.Kind; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_CLEAN)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_clean(xlir_string path, int64_t style)\n"
        "{ if (style < 0 || style > 2 || xlir_string_cstr(path) == NULL)\n"
        "    { if (style < 0 || style > 2) xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
        "  return xlir_string_take_cstr(xrtPathClean((xstrview){path.Data,path.Size},(xpathstyle)style)); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_RELATIVE)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_relative(xlir_string base, xlir_string target, int64_t style)\n"
        "{ if (style < 0 || style > 2 || xlir_string_cstr(base) == NULL || xlir_string_cstr(target) == NULL)\n"
        "    { if (style < 0 || style > 2) xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
        "  return xlir_string_take_cstr(xrtPathRelative((xstrview){base.Data,base.Size},(xstrview){target.Data,target.Size},(xpathstyle)style)); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_BUILD)) xl6OutputPuts(
        "#include <__xl_runtime/xl6_path_build.h>\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_IS_ABS)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_is_abs(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL && xrtPathIsAbs(text); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_IS_ROOT)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_is_root(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL && xrtPathIsRoot(text); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_IS_ROOTED)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_is_rooted(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL && xrtPathIsRooted(text); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_IS_LOCAL)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_is_local(xlir_string path, int64_t style)\n"
        "{ if (style < 0 || style > 2 || xlir_string_cstr(path) == NULL)\n"
        "    { if (style < 0 || style > 2) xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  return xrtPathIsLocal((xstrview){path.Data,path.Size},(xpathstyle)style); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_IS_SAFE)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_is_safe(xlir_string path, bool directory)\n"
        "{ return xlir_string_cstr(path) != NULL && xrtPathIsSafeEntry((xstrview){path.Data,path.Size},directory); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_NAME)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_name(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL ? xlir_string_take_cstr(xrtPathName(text)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_STEM)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_stem(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL ? xlir_string_take_cstr(xrtPathStem(text)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_EXT)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_ext(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL ? xlir_string_take_cstr(xrtPathExt(text)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_PARENT)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_parent(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL ? xlir_string_take_cstr(xrtPathParent(text)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_ABS)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_abs(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL ? xlir_string_take_cstr(xrtPathAbs(text)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_REAL)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_real(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL ? xlir_string_take_cstr(xrtPathReal(text)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_REL)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_rel(xlir_string left, xlir_string right)\n"
        "{ const char* a = xlir_string_cstr(left); const char* b = xlir_string_cstr(right);\n"
        "  return a != NULL && b != NULL ? xlir_string_take_cstr(xrtPathRel(a,b)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_JOIN)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_join(xlir_string left, xlir_string right)\n"
        "{ const char* a = xlir_string_cstr(left); const char* b = xlir_string_cstr(right);\n"
        "  return a != NULL && b != NULL ? xlir_string_take_cstr(xrtPathJoin(a,b)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_WITH_NAME)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_with_name(xlir_string left, xlir_string right)\n"
        "{ const char* a = xlir_string_cstr(left); const char* b = xlir_string_cstr(right);\n"
        "  return a != NULL && b != NULL ? xlir_string_take_cstr(xrtPathWithName(a,b)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_WITH_EXT)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_with_ext(xlir_string left, xlir_string right)\n"
        "{ const char* a = xlir_string_cstr(left); const char* b = xlir_string_cstr(right);\n"
        "  return a != NULL && b != NULL ? xlir_string_take_cstr(xrtPathWithExt(a,b)) : (xlir_string){0}; }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_CWD)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_cwd(void)\n"
        "{ return xlir_string_take_cstr(xrtPathCwd()); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_HOME)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_home(void)\n"
        "{ return xlir_string_take_cstr(xrtPathHome()); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_TEMP)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_temp(void)\n"
        "{ return xlir_string_take_cstr(xrtPathTemp()); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_EXECUTABLE)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_executable(void)\n"
        "{ return xlir_string_take_cstr(xrtPathExecutable()); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_APP_DIR)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_app_dir(void)\n"
        "{ return xlir_string_take_cstr(xrtPathAppDir()); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SET_CWD)) xl6OutputPuts(
        "bool xl6_xrt_fs_path_set_cwd(xlir_string path)\n"
        "{ const char* text = xlir_string_cstr(path); return text != NULL && xrtPathSetCwd(text); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_SEP)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_sep(void)\n"
        "{ char text[1] = {xrtPathSep()}; return xlir_string_from_utf8(text,1); }\n", out);
    if (xl6IrCgenFsSelected(needs, XL6_IR_RUNTIME_BRIDGE_FS_PATH_LIST_SEP)) xl6OutputPuts(
        "xlir_string xl6_xrt_fs_path_list_sep(void)\n"
        "{ char text[1] = {xrtPathListSep()}; return xlir_string_from_utf8(text,1); }\n", out);
}

