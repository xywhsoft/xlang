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
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int mode;\n", out);
    xl6OutputPuts("} XL6_Dir_List_Ctx;\n\n", out);

    xl6OutputPuts("static int xl6_dir_list_each(const char* dir, size_t dirSize, const char* name, size_t nameSize, const char* path, size_t pathSize, int isDir, void* data, void* param)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Dir_List_Ctx* ctx = (XL6_Dir_List_Ctx*)param;\n", out);
    xl6OutputPuts("    xvalue item;\n", out);
    xl6OutputPuts("    (void)dir;\n", out);
    xl6OutputPuts("    (void)dirSize;\n", out);
    xl6OutputPuts("    (void)name;\n", out);
    xl6OutputPuts("    (void)nameSize;\n", out);
    xl6OutputPuts("    (void)data;\n", out);
    xl6OutputPuts("    if ( ctx == 0 || ctx->result == 0 || path == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( ctx->mode == 1 && isDir ) { return 0; }\n", out);
    xl6OutputPuts("    if ( ctx->mode == 2 && !isDir ) { return 0; }\n", out);
    xl6OutputPuts("    if ( pathSize > 0xffffffffu ) { return 0; }\n", out);
    xl6OutputPuts("    item = xvoCreateText((void*)path, (unsigned int)pathSize, false);\n", out);
    xl6OutputPuts("    if ( item != 0 && !xvoArrayAppendValue(ctx->result, item, true) ) { xvoUnref(item); }\n", out);
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
    xl6OutputPuts("    (void)xrtDirScanEx(path, recursive ? 1 : 0, xl6_dir_list_each, &ctx);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
