static int xl6CgenNodeNeedsTccRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->text != NULL ) {
        if ( (node->kind == XL6_AST_TYPE || node->kind == XL6_AST_NAME) &&
             (xl6CTextEquals(node->text, "TccContext") ||
              xl6CTextEquals(node->text, "TccHandler")) ) {
            return 1;
        }
        if ( node->kind == XL6_AST_NAME &&
             (strncmp(node->text, "__xl_std_tcc_", 13) == 0 ||
              strncmp(node->text, "xl6_tcc_", 8) == 0) ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTccRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitTccRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_TCC_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_TCC_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xbuffer_struct { char* Buffer; unsigned int Length; unsigned int AllocLength; unsigned int AllocStep; } xbuffer_struct, *xbuffer;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef struct xrt_net_future xfuture;\n", out);
    xl6OutputPuts("typedef struct xrt_web_app xwebapp;\n", out);
    xl6OutputPuts("typedef struct xrt_web_server xwebserver;\n", out);
    xl6OutputPuts("typedef struct xrt_web_request xwebrequest;\n", out);
    xl6OutputPuts("typedef struct xrt_web_response xwebresponse;\n", out);
    xl6OutputPuts("void* tcc_new(void);\n", out);
    xl6OutputPuts("void tcc_delete(void* s);\n", out);
    xl6OutputPuts("void tcc_set_lib_path(void* s, const char* path);\n", out);
    xl6OutputPuts("int tcc_set_output_type(void* s, int output_type);\n", out);
    xl6OutputPuts("int tcc_add_include_path(void* s, const char* pathname);\n", out);
    xl6OutputPuts("int tcc_add_sysinclude_path(void* s, const char* pathname);\n", out);
    xl6OutputPuts("int tcc_add_library_path(void* s, const char* pathname);\n", out);
    xl6OutputPuts("int tcc_compile_string(void* s, const char* source);\n", out);
    xl6OutputPuts("int tcc_add_file(void* s, const char* filename);\n", out);
    xl6OutputPuts("int tcc_add_symbol(void* s, const char* name, void* value);\n", out);
    xl6OutputPuts("int tcc_relocate(void* s);\n", out);
    xl6OutputPuts("void* tcc_get_symbol(void* s, const char* name);\n", out);
    xl6OutputPuts("int tcc_output_file(void* s, const char* filename);\n", out);
    xl6OutputPuts("void* xrtInit(void);\n", out);
    xl6OutputPuts("void xrtUnit(void);\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void* xrtCalloc(size_t iCount, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtRealloc(void* p, size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("void* xrtTempMemory(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFreeTempMemory(void* p);\n", out);
    xl6OutputPuts("xbuffer xrtBufferCreate(unsigned int iStep);\n", out);
    xl6OutputPuts("void xrtBufferDestroy(xbuffer pBuffer);\n", out);
    xl6OutputPuts("void xrtBufferInit(xbuffer pBuffer, unsigned int iStep);\n", out);
    xl6OutputPuts("void xrtBufferUnit(xbuffer pBuffer);\n", out);
    xl6OutputPuts("bool xrtBufferMalloc(xbuffer pBuffer, unsigned int iCount);\n", out);
    xl6OutputPuts("bool xrtBufferAppend(xbuffer pBuffer, void* pData, unsigned int iSize, unsigned int bStrMode);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("unsigned int xrtRand32(void);\n", out);
    xl6OutputPuts("unsigned long long xrtRand64(void);\n", out);
    xl6OutputPuts("int xrtRandRange(int iMin, int iMax);\n", out);
    xl6OutputPuts("long long xrtStrToI64(const void* pStr);\n", out);
    xl6OutputPuts("double xrtStrToNum(const void* pStr);\n", out);
    xl6OutputPuts("int xrtI64ToStr(long long num, char* buffer);\n", out);
    xl6OutputPuts("int xrtNumToStr(double num, char* buffer);\n", out);
    xl6OutputPuts("int xrtStrComp(const char* sStr1, const char* sStr2, size_t iSize, bool bCase);\n", out);
    xl6OutputPuts("size_t xrtStrByteLen(const char* sStr, size_t iSize);\n", out);
    xl6OutputPuts("size_t xrtStrCharLen(const char* sStr, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtCheckStr(void* sText, size_t iSize, void* sSubText, size_t iSubSize);\n", out);
    xl6OutputPuts("void* xrtHexEncode(const char* sStr, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtHexDecode(const char* sStr, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtBase64Encode(void* pData, size_t iSize, const char* sTable);\n", out);
    xl6OutputPuts("void* xrtBase64Decode(char* sText, size_t iSize, const char* sTable);\n", out);
    xl6OutputPuts("void* xrtParseJSON(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtStringifyJSON(xvalue varVal, int bFormat, void* pRetSize);\n", out);
    xl6OutputPuts("void* xrtParseXSON(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtStringifyXSON(xvalue varVal, int bFormat, unsigned int iFlags, void* pRetSize);\n", out);
    xl6OutputPuts("bool xrtWebResponseReserveBody(xwebresponse* pResp, size_t iCap);\n", out);
    xl6OutputPuts("bool xrtWebResponseAppendBody(xwebresponse* pResp, const void* pData, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtWebResponseAppendText(xwebresponse* pResp, const char* sText);\n", out);
    xl6OutputPuts("bool xrtWebAppStreamRouteAsyncEx(xwebapp* pApp, unsigned int iMethodMask, const char* sPattern, bool (*pBegin)(xwebrequest* pReq, void* pUserData), bool (*pBody)(xwebrequest* pReq, const void* pData, size_t iLen, void* pUserData), xfuture* (*pEndAsync)(xwebrequest* pReq, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebAppStreamRouteAsync(xwebapp* pApp, unsigned int iMethodMask, const char* sPattern, bool (*pBegin)(xwebrequest* pReq, void* pUserData), bool (*pBody)(xwebrequest* pReq, const void* pData, size_t iLen, void* pUserData), xfuture* (*pEndAsync)(xwebrequest* pReq, void* pUserData), void* pUserData);\n", out);
    xl6OutputPuts("bool xrtWebServerStreamRouteAsyncEx(xwebserver* pServer, unsigned int iMethodMask, const char* sPattern, bool (*pBegin)(xwebrequest* pReq, void* pUserData), bool (*pBody)(xwebrequest* pReq, const void* pData, size_t iLen, void* pUserData), xfuture* (*pEndAsync)(xwebrequest* pReq, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebServerStreamRouteAsync(xwebserver* pServer, unsigned int iMethodMask, const char* sPattern, bool (*pBegin)(xwebrequest* pReq, void* pUserData), bool (*pBody)(xwebrequest* pReq, const void* pData, size_t iLen, void* pUserData), xfuture* (*pEndAsync)(xwebrequest* pReq, void* pUserData), void* pUserData);\n", out);
    xl6OutputPuts("unsigned int xvoArrayItemCount(xvalue pArr);\n", out);
    xl6OutputPuts("xvalue xvoArrayGetValue(xvalue pArr, unsigned int index);\n", out);
    xl6OutputPuts("const char* xvoGetText(xvalue pVal);\n", out);
    xl6OutputPuts("xvalue xvoCreateNull(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateBool(bool bVal);\n", out);
    xl6OutputPuts("xvalue xvoCreateInt(long long iVal);\n", out);
    xl6OutputPuts("xvalue xvoCreateFloat(double fVal);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sText, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("void xvoAddRef(xvalue pVal);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n", out);
    xl6OutputPuts("int xvoType(xvalue pVal);\n", out);
    xl6OutputPuts("bool xvoIsNull(xvalue pVal);\n", out);
    xl6OutputPuts("bool xvoIsArray(xvalue pVal);\n", out);
    xl6OutputPuts("bool xvoGetBool(xvalue pVal);\n", out);
    xl6OutputPuts("long long xvoGetInt(xvalue pVal);\n", out);
    xl6OutputPuts("double xvoGetFloat(xvalue pVal);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("int tcc_vfs_mount_memory(const char* path, const void* data, size_t size);\n", out);
    xl6OutputPuts("void tcc_vfs_clear_dynamic(void);\n\n", out);

    xl6OutputPuts("static int xl6_tcc_add_symbol_checked(void* state, const char* name, void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return state != 0 && name != 0 && value != 0 && tcc_add_symbol(state, name, value) == 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_tcc_register_xrt_environment(void* state)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("#define XL6_TCC_ADD_SYMBOL(name) do { if ( !xl6_tcc_add_symbol_checked(state, #name, (void*)(name)) ) { return 0; } } while (0)\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtInit);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtUnit);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtMalloc);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtCalloc);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtRealloc);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtFree);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtTempMemory);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtFreeTempMemory);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBufferCreate);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBufferDestroy);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBufferInit);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBufferUnit);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBufferMalloc);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBufferAppend);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtCopyStr);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtRand32);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtRand64);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtRandRange);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStrToI64);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStrToNum);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtI64ToStr);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtNumToStr);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStrComp);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStrByteLen);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStrCharLen);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtCheckStr);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtHexEncode);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtHexDecode);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBase64Encode);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtBase64Decode);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtParseJSON);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStringifyJSON);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtParseXSON);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtStringifyXSON);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebResponseReserveBody);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebResponseAppendBody);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebResponseAppendText);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebAppStreamRouteAsyncEx);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebAppStreamRouteAsync);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebServerStreamRouteAsyncEx);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xrtWebServerStreamRouteAsync);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoCreateNull);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoCreateBool);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoCreateInt);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoCreateFloat);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoCreateText);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoCreateArray);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoAddRef);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoUnref);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoType);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoIsNull);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoIsArray);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoGetBool);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoGetInt);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoGetFloat);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoGetText);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoArrayAppendValue);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoArrayItemCount);\n", out);
    xl6OutputPuts("    XL6_TCC_ADD_SYMBOL(xvoArrayGetValue);\n", out);
    xl6OutputPuts("#undef XL6_TCC_ADD_SYMBOL\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_tcc_context_create_with_output(int outputType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* state = tcc_new();\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    tcc_set_lib_path(state, \".\");\n", out);
    xl6OutputPuts("    (void)tcc_add_include_path(state, \".\");\n", out);
    xl6OutputPuts("    (void)tcc_add_include_path(state, \"include\");\n", out);
    xl6OutputPuts("    (void)tcc_add_include_path(state, \"include/winapi\");\n", out);
    xl6OutputPuts("    (void)tcc_add_sysinclude_path(state, \"include\");\n", out);
    xl6OutputPuts("    (void)tcc_add_sysinclude_path(state, \"include/winapi\");\n", out);
    xl6OutputPuts("    (void)tcc_add_library_path(state, \"lib\");\n", out);
    xl6OutputPuts("    if ( tcc_set_output_type(state, outputType) != 0 ) { tcc_delete(state); return 0; }\n", out);
    xl6OutputPuts("    if ( !xl6_tcc_register_xrt_environment(state) ) { tcc_delete(state); return 0; }\n", out);
    xl6OutputPuts("    return state;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_context_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_tcc_context_create_with_output(1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_context_create_output(int outputType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_tcc_context_create_with_output(outputType);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_tcc_context_destroy(void* state)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( state != 0 ) { tcc_delete(state); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("typedef int (*XL6_TccMainProc)(int, char**);\n", out);
    xl6OutputPuts("typedef struct xl6_tcc_handler { void* state; XL6_TccMainProc mainProc; } XL6_TccHandler;\n\n", out);

    xl6OutputPuts("static int xl6_tcc_run_state(void* state, xvalue args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int count = args != 0 ? xvoArrayItemCount(args) : 0u;\n", out);
    xl6OutputPuts("    char** argv = 0;\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    typedef int (*xl6_tcc_main_proc)(int, char**);\n", out);
    xl6OutputPuts("    xl6_tcc_main_proc mainProc;\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    if ( count > 0u ) {\n", out);
    xl6OutputPuts("        argv = (char**)xrtCalloc(count, sizeof(char*));\n", out);
    xl6OutputPuts("        if ( argv == 0 ) { return -1; }\n", out);
    xl6OutputPuts("        for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("            const char* item = xvoGetText(xvoArrayGetValue(args, i));\n", out);
    xl6OutputPuts("            argv[i] = (char*)(item != 0 ? item : \"\");\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    rc = tcc_relocate(state);\n", out);
    xl6OutputPuts("    if ( rc == 0 ) {\n", out);
    xl6OutputPuts("        mainProc = (xl6_tcc_main_proc)tcc_get_symbol(state, \"main\");\n", out);
    xl6OutputPuts("        rc = mainProc != 0 ? mainProc((int)count, argv) : -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( argv != 0 ) { xrtFree(argv); }\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_context_run_args(void* state, xvalue args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_tcc_run_state(state, args);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_handler_create(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TccHandler* handler;\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' ) { return 0; }\n", out);
    xl6OutputPuts("    handler = (XL6_TccHandler*)xrtCalloc(1u, sizeof(XL6_TccHandler));\n", out);
    xl6OutputPuts("    if ( handler == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    handler->state = xl6_tcc_context_create();\n", out);
    xl6OutputPuts("    if ( handler->state == 0 ) { xrtFree(handler); return 0; }\n", out);
    xl6OutputPuts("    if ( tcc_add_file(handler->state, path) != 0 || tcc_relocate(handler->state) != 0 ) {\n", out);
    xl6OutputPuts("        xl6_tcc_context_destroy(handler->state);\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    handler->mainProc = (XL6_TccMainProc)tcc_get_symbol(handler->state, \"main\");\n", out);
    xl6OutputPuts("    if ( handler->mainProc == 0 ) {\n", out);
    xl6OutputPuts("        xl6_tcc_context_destroy(handler->state);\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return handler;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_tcc_handler_destroy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TccHandler* handler = (XL6_TccHandler*)value;\n", out);
    xl6OutputPuts("    if ( handler != 0 ) {\n", out);
    xl6OutputPuts("        if ( handler->state != 0 ) { xl6_tcc_context_destroy(handler->state); }\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_handler_run_args(void* value, xvalue args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TccHandler* handler = (XL6_TccHandler*)value;\n", out);
    xl6OutputPuts("    unsigned int count = args != 0 ? xvoArrayItemCount(args) : 0u;\n", out);
    xl6OutputPuts("    char** argv = 0;\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( handler == 0 || handler->mainProc == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    if ( count > 0u ) {\n", out);
    xl6OutputPuts("        argv = (char**)xrtCalloc(count, sizeof(char*));\n", out);
    xl6OutputPuts("        if ( argv == 0 ) { return -1; }\n", out);
    xl6OutputPuts("        for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("            const char* item = xvoGetText(xvoArrayGetValue(args, i));\n", out);
    xl6OutputPuts("            argv[i] = (char*)(item != 0 ? item : \"\");\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    rc = handler->mainProc((int)count, argv);\n", out);
    xl6OutputPuts("    if ( argv != 0 ) { xrtFree(argv); }\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_context_output_file(void* state, const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("#ifdef XL_RUNTIME_ONLY_DLL\n", out);
    xl6OutputPuts("    (void)state; (void)path; return -1;\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("    if ( state == 0 || path == 0 || path[0] == '\\0' ) { return -1; }\n", out);
    xl6OutputPuts("    return tcc_output_file(state, path);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_run_source(const char* source)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* state = xl6_tcc_context_create();\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    rc = tcc_set_output_type(state, 1);\n", out);
    xl6OutputPuts("    if ( rc == 0 ) { rc = tcc_compile_string(state, source != 0 ? source : \"\"); }\n", out);
    xl6OutputPuts("    if ( rc == 0 ) { rc = xl6_tcc_run_state(state, 0); }\n", out);
    xl6OutputPuts("    xl6_tcc_context_destroy(state);\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_run_file(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* state = xl6_tcc_context_create();\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( state == 0 || path == 0 || path[0] == '\\0' ) { xl6_tcc_context_destroy(state); return -1; }\n", out);
    xl6OutputPuts("    rc = tcc_set_output_type(state, 1);\n", out);
    xl6OutputPuts("    if ( rc == 0 ) { rc = tcc_add_file(state, path); }\n", out);
    xl6OutputPuts("    if ( rc == 0 ) { rc = xl6_tcc_run_state(state, 0); }\n", out);
    xl6OutputPuts("    xl6_tcc_context_destroy(state);\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_vfs_mount_text(const char* path, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return tcc_vfs_mount_memory(path != 0 ? path : \"\", text != 0 ? text : \"\", text != 0 ? strlen(text) : 0u);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
