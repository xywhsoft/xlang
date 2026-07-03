static void xl6CgenEmitDirRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_DIR_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_DIR_RUNTIME\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef int (*xrtDirScanExProc)(const char* sDir, size_t iDirSize, const char* sName, size_t iNameSize, const char* sPath, size_t iPathSize, int bDir, void* pData, void* Param);\n", out);
    xl6OutputPuts("int xrtDirScanEx(const char* sPath, int bRecu, xrtDirScanExProc pProc, void* Param);\n", out);
    xl6OutputPuts("bool xrtStrLike(const char* sText, size_t iTextSize, const char* sPattern, size_t iPatSize, bool bCase);\n", out);
    xl6OutputPuts("size_t xrtFileGetSize(const char* sPath);\n", out);
    xl6OutputPuts("int xrtFileGetAttr(const char* sPath);\n", out);
    xl6OutputPuts("long long xrtFileGetAccessTime(const char* sPath);\n", out);
    xl6OutputPuts("long long xrtFileGetChangeTime(const char* sPath);\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateTable(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("xvalue xvoCreateBool(bool bVal);\n", out);
    xl6OutputPuts("xvalue xvoCreateInt(long long iVal);\n", out);
    xl6OutputPuts("xvalue xvoCreateTime(long long tVal);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoTableSetValue(xvalue pTbl, const void* key, unsigned int kl, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int mode;\n", out);
    xl6OutputPuts("    const char* pattern;\n", out);
    xl6OutputPuts("    size_t patternSize;\n", out);
    xl6OutputPuts("} XL6_Dir_List_Ctx;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int mode;\n", out);
    xl6OutputPuts("    int includeLeave;\n", out);
    xl6OutputPuts("    const char* pattern;\n", out);
    xl6OutputPuts("    size_t patternSize;\n", out);
    xl6OutputPuts("} XL6_Dir_Entry_Ctx;\n\n", out);

    xl6OutputPuts("static int xl6_dir_match_name(const char* name, size_t nameSize, const char* pattern, size_t patternSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pattern == 0 || patternSize == 0 ) { return 1; }\n", out);
    xl6OutputPuts("    if ( name == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtStrLike(name, nameSize, pattern, patternSize, false) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_table_set_value(xvalue table, const char* key, xvalue value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( table == 0 || key == 0 || value == 0 ) { if ( value != 0 ) { xvoUnref(value); } return 0; }\n", out);
    xl6OutputPuts("    if ( !xvoTableSetValue(table, key, 0, value, true) ) { xvoUnref(value); return 0; }\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_table_set_text(xvalue table, const char* key, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0; }\n", out);
    xl6OutputPuts("    if ( size > 0xffffffffu ) { return 0; }\n", out);
    xl6OutputPuts("    return xl6_dir_table_set_value(table, key, xvoCreateText((void*)text, (unsigned int)size, false));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_table_set_bool(xvalue table, const char* key, bool value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_dir_table_set_value(table, key, xvoCreateBool(value));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_table_set_int(xvalue table, const char* key, long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_dir_table_set_value(table, key, xvoCreateInt(value));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_table_set_time(xvalue table, const char* key, long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_dir_table_set_value(table, key, xvoCreateTime(value));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_list_each(const char* dir, size_t dirSize, const char* name, size_t nameSize, const char* path, size_t pathSize, int isDir, void* data, void* param)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Dir_List_Ctx* ctx = (XL6_Dir_List_Ctx*)param;\n", out);
    xl6OutputPuts("    xvalue item;\n", out);
    xl6OutputPuts("    (void)dir;\n", out);
    xl6OutputPuts("    (void)dirSize;\n", out);
    xl6OutputPuts("    (void)data;\n", out);
    xl6OutputPuts("    if ( ctx == 0 || ctx->result == 0 || path == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( isDir == 2 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( ctx->mode == 1 && isDir != 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( ctx->mode == 2 && isDir != 1 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xl6_dir_match_name(name, nameSize, ctx->pattern, ctx->patternSize) ) { return 0; }\n", out);
    xl6OutputPuts("    if ( pathSize > 0xffffffffu ) { return 0; }\n", out);
    xl6OutputPuts("    item = xvoCreateText((void*)path, (unsigned int)pathSize, false);\n", out);
    xl6OutputPuts("    if ( item != 0 && !xvoArrayAppendValue(ctx->result, item, true) ) { xvoUnref(item); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_dir_entry_each(const char* dir, size_t dirSize, const char* name, size_t nameSize, const char* path, size_t pathSize, int kind, void* data, void* param)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Dir_Entry_Ctx* ctx = (XL6_Dir_Entry_Ctx*)param;\n", out);
    xl6OutputPuts("    xvalue item;\n", out);
    xl6OutputPuts("    (void)data;\n", out);
    xl6OutputPuts("    if ( ctx == 0 || ctx->result == 0 || path == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( kind == 2 && !ctx->includeLeave ) { return 0; }\n", out);
    xl6OutputPuts("    if ( ctx->mode == 1 && kind != 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( ctx->mode == 2 && kind == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xl6_dir_match_name(name, nameSize, ctx->pattern, ctx->patternSize) ) { return 0; }\n", out);
    xl6OutputPuts("    if ( pathSize > 0xffffffffu ) { return 0; }\n", out);
    xl6OutputPuts("    item = xvoCreateTable();\n", out);
    xl6OutputPuts("    if ( item == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_dir_table_set_text(item, \"dir\", dir, dirSize);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_text(item, \"name\", name, nameSize);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_text(item, \"path\", path, pathSize);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_bool(item, \"isFile\", kind == 0 ? true : false);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_bool(item, \"isDir\", kind != 0 ? true : false);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_bool(item, \"enter\", kind == 1 ? true : false);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_bool(item, \"leave\", kind == 2 ? true : false);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_int(item, \"size\", kind == 0 ? (long long)xrtFileGetSize(path) : 0);\n", out);
    xl6OutputPuts("    xl6_dir_table_set_int(item, \"attr\", (long long)xrtFileGetAttr(path));\n", out);
    xl6OutputPuts("    xl6_dir_table_set_time(item, \"accessTime\", xrtFileGetAccessTime(path));\n", out);
    xl6OutputPuts("    xl6_dir_table_set_time(item, \"changeTime\", xrtFileGetChangeTime(path));\n", out);
    xl6OutputPuts("    if ( !xvoArrayAppendValue(ctx->result, item, true) ) { xvoUnref(item); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_dir_list(const char* path, bool recursive, int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Dir_List_Ctx ctx;\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( path == 0 ) { path = \"\"; }\n", out);
    xl6OutputPuts("    ctx.result = result;\n", out);
    xl6OutputPuts("    ctx.mode = mode;\n", out);
    xl6OutputPuts("    ctx.pattern = 0;\n", out);
    xl6OutputPuts("    ctx.patternSize = 0;\n", out);
    xl6OutputPuts("    (void)xrtDirScanEx(path, recursive ? 1 : 0, xl6_dir_list_each, &ctx);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_dir_glob(const char* path, const char* pattern, bool recursive, int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Dir_List_Ctx ctx;\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( path == 0 ) { path = \"\"; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    ctx.result = result;\n", out);
    xl6OutputPuts("    ctx.mode = mode;\n", out);
    xl6OutputPuts("    ctx.pattern = pattern;\n", out);
    xl6OutputPuts("    ctx.patternSize = strlen(pattern);\n", out);
    xl6OutputPuts("    (void)xrtDirScanEx(path, recursive ? 1 : 0, xl6_dir_list_each, &ctx);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_dir_entries(const char* path, bool recursive, bool includeLeave, const char* pattern, int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Dir_Entry_Ctx ctx;\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( path == 0 ) { path = \"\"; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    ctx.result = result;\n", out);
    xl6OutputPuts("    ctx.mode = mode;\n", out);
    xl6OutputPuts("    ctx.includeLeave = includeLeave ? 1 : 0;\n", out);
    xl6OutputPuts("    ctx.pattern = pattern;\n", out);
    xl6OutputPuts("    ctx.patternSize = strlen(pattern);\n", out);
    xl6OutputPuts("    (void)xrtDirScanEx(path, recursive ? 1 : 0, xl6_dir_entry_each, &ctx);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
