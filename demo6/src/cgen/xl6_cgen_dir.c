/* 生成基于当前 XRT 文件遍历和 Value 契约的目录运行时。 */
void xl6CgenEmitDirRuntime(XL6_Output* out, unsigned operations)
{
    if ( out == NULL || operations == 0 ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_DIR_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_DIR_RUNTIME\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    if (operations & (XL6_CGEN_DIR_LIST | XL6_CGEN_DIR_GLOB | XL6_CGEN_DIR_ENTRIES)) {
        /* 目录遍历结构必须与 xl_runtime.h 中的 XRT ABI 完全一致。 */
        xl6OutputPuts("#define XL6_XFILE_TYPE_FILE XFILE_TYPE_FILE\n", out);
        xl6OutputPuts("#define XL6_XFILE_TYPE_DIRECTORY XFILE_TYPE_DIRECTORY\n", out);
        xl6OutputPuts("#define XL6_XWALK_ENTER XWALK_ENTER\n", out);
        xl6OutputPuts("#define XL6_XWALK_ITEM XWALK_ITEM\n", out);
        xl6OutputPuts("#define XL6_XWALK_LEAVE XWALK_LEAVE\n", out);
        xl6OutputPuts("#define XL6_XWALK_CONTINUE XWALK_CONTINUE\n", out);
        xl6OutputPuts("#define XL6_XWALK_ERROR XWALK_ERROR\n", out);
        xl6OutputPuts("typedef xfileinfo XL6_XFileInfo;\n", out);
        xl6OutputPuts("typedef xwalkoptions XL6_XWalkOptions;\n", out);
        xl6OutputPuts("typedef xwalkentry XL6_XWalkEntry;\n", out);
        xl6OutputPuts("typedef xwalkstats XL6_XWalkStats;\n", out);
        xl6OutputPuts("typedef xwalkcontrol XL6_XWalkControl;\n", out);
        xl6OutputPuts("typedef xwalkproc XL6_XWalkProc;\n", out);
        xl6OutputPuts("\n", out);

        xl6OutputPuts("typedef struct XL6_Dir_List_Ctx {\n", out);
        xl6OutputPuts("    xvalue* result;\n", out);
        xl6OutputPuts("    int mode;\n", out);
        xl6OutputPuts("    const char* pattern;\n", out);
        xl6OutputPuts("    size_t patternSize;\n", out);
        xl6OutputPuts("} XL6_Dir_List_Ctx;\n\n", out);

        xl6OutputPuts("typedef struct XL6_Dir_Entry_Ctx {\n", out);
        xl6OutputPuts("    xvalue* result;\n", out);
        xl6OutputPuts("    int mode;\n", out);
        xl6OutputPuts("    bool includeLeave;\n", out);
        xl6OutputPuts("    const char* pattern;\n", out);
        xl6OutputPuts("    size_t patternSize;\n", out);
        xl6OutputPuts("} XL6_Dir_Entry_Ctx;\n\n", out);

        xl6OutputPuts("static bool xl6_dir_match_name(xstrview name, const char* pattern, size_t patternSize)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    if ( pattern == 0 || patternSize == 0u ) { return true; }\n", out);
        xl6OutputPuts("    return xrtStrGlob(name, (xstrview){ pattern, patternSize }, 0u);\n", out);
        xl6OutputPuts("}\n\n", out);

        if (operations & XL6_CGEN_DIR_ENTRIES) {
            xl6OutputPuts("static bool xl6_dir_object_set(xvalue* object, const char* key, xvalue* value)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    if ( object == 0 || key == 0 || value == 0 ) { if ( value != 0 ) { xrtValueRelease(value); } return false; }\n", out);
            xl6OutputPuts("    return xrtValueObjectSetNew(object, (xstrview){ key, strlen(key) }, value);\n", out);
            xl6OutputPuts("}\n\n", out);

            xl6OutputPuts("static bool xl6_dir_object_set_text(xvalue* object, const char* key, const char* text, size_t size)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0u; }\n", out);
            xl6OutputPuts("    return xl6_dir_object_set(object, key, xrtValueString((xstrview){ text, size }));\n", out);
            xl6OutputPuts("}\n\n", out);

        }
        if (operations & (XL6_CGEN_DIR_LIST | XL6_CGEN_DIR_GLOB)) {
            xl6OutputPuts("static bool xl6_dir_append_path(xvalue* array, const char* path)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    if ( path == 0 ) { path = \"\"; }\n", out);
            xl6OutputPuts("    return xrtValueArrayAppendNew(array, xrtValueString((xstrview){ path, strlen(path) }));\n", out);
            xl6OutputPuts("}\n\n", out);

            xl6OutputPuts("static XL6_XWalkControl xl6_dir_list_each(const XL6_XWalkEntry* entry, void* data)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    XL6_Dir_List_Ctx* ctx = (XL6_Dir_List_Ctx*)data;\n", out);
            xl6OutputPuts("    bool directory;\n", out);
            xl6OutputPuts("    if ( entry == 0 || ctx == 0 || ctx->result == 0 || entry->Depth == 0u ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    if ( entry->Event == XL6_XWALK_LEAVE ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    directory = entry->Event == XL6_XWALK_ENTER;\n", out);
            xl6OutputPuts("    if ( (ctx->mode == 1 && directory) || (ctx->mode == 2 && !directory) ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    if ( !xl6_dir_match_name(entry->Name, ctx->pattern, ctx->patternSize) ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    return xl6_dir_append_path(ctx->result, entry->Path) ? XL6_XWALK_CONTINUE : XL6_XWALK_ERROR;\n", out);
            xl6OutputPuts("}\n\n", out);

        }
        if (operations & XL6_CGEN_DIR_ENTRIES) {
            xl6OutputPuts("static bool xl6_dir_entry_append(XL6_Dir_Entry_Ctx* ctx, const XL6_XWalkEntry* entry)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    xvalue* item = xrtValueObject();\n", out);
            xl6OutputPuts("    bool file = entry->Event == XL6_XWALK_ITEM;\n", out);
            xl6OutputPuts("    bool directory = !file;\n", out);
            xl6OutputPuts("    if ( item == 0 ) { return false; }\n", out);
            xl6OutputPuts("    if ( !xl6_dir_object_set_text(item, \"dir\", entry->Parent.Data, entry->Parent.Size) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set_text(item, \"name\", entry->Name.Data, entry->Name.Size) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set_text(item, \"path\", entry->Path, entry->Path != 0 ? strlen(entry->Path) : 0u) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"isFile\", xrtValueBool(file)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"isDir\", xrtValueBool(directory)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"enter\", xrtValueBool(entry->Event == XL6_XWALK_ENTER)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"leave\", xrtValueBool(entry->Event == XL6_XWALK_LEAVE)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"size\", xrtValueInt(file ? (long long)entry->Info.Size : 0LL)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"attr\", xrtValueInt((long long)entry->Info.Attributes)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"accessTime\", xrtValueTime(entry->Info.Accessed)) ||\n", out);
            xl6OutputPuts("         !xl6_dir_object_set(item, \"changeTime\", xrtValueTime(entry->Info.Modified)) ) {\n", out);
            xl6OutputPuts("        xrtValueRelease(item);\n", out);
            xl6OutputPuts("        return false;\n", out);
            xl6OutputPuts("    }\n", out);
            xl6OutputPuts("    return xrtValueArrayAppendNew(ctx->result, item);\n", out);
            xl6OutputPuts("}\n\n", out);

            xl6OutputPuts("static XL6_XWalkControl xl6_dir_entry_each(const XL6_XWalkEntry* entry, void* data)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    XL6_Dir_Entry_Ctx* ctx = (XL6_Dir_Entry_Ctx*)data;\n", out);
            xl6OutputPuts("    bool file;\n", out);
            xl6OutputPuts("    if ( entry == 0 || ctx == 0 || ctx->result == 0 || entry->Depth == 0u ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    if ( entry->Event == XL6_XWALK_LEAVE && !ctx->includeLeave ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    file = entry->Event == XL6_XWALK_ITEM;\n", out);
            xl6OutputPuts("    if ( (ctx->mode == 1 && !file) || (ctx->mode == 2 && file) ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    if ( !xl6_dir_match_name(entry->Name, ctx->pattern, ctx->patternSize) ) { return XL6_XWALK_CONTINUE; }\n", out);
            xl6OutputPuts("    return xl6_dir_entry_append(ctx, entry) ? XL6_XWALK_CONTINUE : XL6_XWALK_ERROR;\n", out);
            xl6OutputPuts("}\n\n", out);

        }
        xl6OutputPuts("static bool xl6_dir_walk(const char* path, bool recursive, XL6_XWalkProc proc, void* data)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    XL6_XWalkOptions options = { 0u, recursive ? (size_t)-1 : 1u, 0 };\n", out);
        xl6OutputPuts("    return path != 0 && path[0] != '\\0' && xrtFileWalk(path, &options, proc, data, 0);\n", out);
        xl6OutputPuts("}\n\n", out);

        /* The language directory APIs are no-raise and return an empty array on
         * traversal failure, not a null carrier requiring a failing checked cast.
         * Discard partial results; retain XRT's native diagnostic for lastError. */
        xl6OutputPuts(
            "static xvalue* xl6_dir_walk_result(const char* path, bool recursive, XL6_XWalkProc proc, void* data, xvalue* result)\n"
            "{\n"
            "    if (result == NULL) return NULL;\n"
            "    if (xl6_dir_walk(path, recursive, proc, data)) return result;\n"
            "    xrtValueRelease(result);\n"
            "    return xrtValueArray();\n"
            "}\n", out);

        if (operations & XL6_CGEN_DIR_LIST) {
            xl6OutputPuts("xvalue* xl6_xrt_dir_list(xlir_string path, bool recursive, int mode)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    XL6_Dir_List_Ctx ctx = { xrtValueArray(), mode, 0, 0u };\n", out);
            xl6OutputPuts("    return xl6_dir_walk_result(xlir_string_cstr(path), recursive, xl6_dir_list_each, &ctx, ctx.result);\n", out);
            xl6OutputPuts("}\n\n", out);

        }
        if (operations & XL6_CGEN_DIR_GLOB) {
            xl6OutputPuts("xvalue* xl6_xrt_dir_glob(xlir_string path, xlir_string pattern, bool recursive, int mode)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    XL6_Dir_List_Ctx ctx = { xrtValueArray(), mode, pattern.Data, pattern.Size };\n", out);
            xl6OutputPuts("    return xl6_dir_walk_result(xlir_string_cstr(path), recursive, xl6_dir_list_each, &ctx, ctx.result);\n", out);
            xl6OutputPuts("}\n\n", out);

        }
        if (operations & XL6_CGEN_DIR_ENTRIES) {
            xl6OutputPuts("xvalue* xl6_xrt_dir_entries(xlir_string path, bool recursive, bool includeLeave, xlir_string pattern, int mode)\n", out);
            xl6OutputPuts("{\n", out);
            xl6OutputPuts("    XL6_Dir_Entry_Ctx ctx = { xrtValueArray(), mode, includeLeave, pattern.Data, pattern.Size };\n", out);
            xl6OutputPuts("    return xl6_dir_walk_result(xlir_string_cstr(path), recursive, xl6_dir_entry_each, &ctx, ctx.result);\n", out);
            xl6OutputPuts("}\n\n", out);

        }
    }
    if (operations & XL6_CGEN_DIR_IS_EMPTY) {
        xl6OutputPuts("bool xl6_xrt_dir_is_empty(xlir_string path)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    bool empty = false;\n", out);
        xl6OutputPuts("    const char* cpath = xlir_string_cstr(path);\n", out);
        xl6OutputPuts("    return cpath != 0 && xrtDirEmpty(cpath, &empty) && empty;\n", out);
        xl6OutputPuts("}\n\n", out);

    }
    if (operations & XL6_CGEN_DIR_COUNT) {
        xl6OutputPuts("long long xl6_xrt_dir_count(xlir_string path, bool recursive, int mode)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xwalkstats stats;\n", out);
        xl6OutputPuts("    const char* cpath = xlir_string_cstr(path);\n", out);
        xl6OutputPuts("    if ( cpath == 0 || !xrtDirStats(cpath, recursive, &stats) ) { return 0LL; }\n", out);
        xl6OutputPuts("    if ( mode == 1 ) { return (long long)stats.Files; }\n", out);
        xl6OutputPuts("    if ( mode == 2 ) { return stats.Directories > 0u ? (long long)(stats.Directories - 1u) : 0LL; }\n", out);
        xl6OutputPuts("    return stats.Items > 0u ? (long long)(stats.Items - 1u) : 0LL;\n", out);
        xl6OutputPuts("}\n\n", out);

    }
    if (operations & XL6_CGEN_DIR_SIZE) {
        xl6OutputPuts("long long xl6_xrt_dir_size(xlir_string path, bool recursive)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    unsigned long long size = 0u;\n", out);
        xl6OutputPuts("    const char* cpath = xlir_string_cstr(path);\n", out);
        xl6OutputPuts("    return cpath != 0 && xrtDirSize(cpath, recursive, &size) ? (long long)size : 0LL;\n", out);
        xl6OutputPuts("}\n", out);
    }
    xl6OutputPuts("#endif\n\n", out);
}
