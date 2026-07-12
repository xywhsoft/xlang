static void xl6CgenEmitAsyncFileRuntime(XL6_Output* out)
{
    xl6OutputPuts("char* xrtFileReadAll(const char* sPath, int iCharset, size_t* pSize);\n", out);
    xl6OutputPuts("void* xrtFileGetAll(const char* sPath, size_t* pSize);\n", out);
    xl6OutputPuts("int xrtFileWriteAll(const char* sPath, const char* sText, size_t iSize, int iCharset);\n", out);
    xl6OutputPuts("int xrtFileAppend(const char* sPath, const char* sText, size_t iSize, int iCharset);\n", out);
    xl6OutputPuts("bool xrtFileExists(const char* sPath);\n", out);
    xl6OutputPuts("bool xrtIsUTF8(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("bool xrtFileCopy(const char* sSrc, const char* sDst, bool bReWrite);\n", out);
    xl6OutputPuts("bool xrtFileMove(const char* sSrc, const char* sDst, bool bReWrite);\n", out);
    xl6OutputPuts("bool xrtFileDelete(const char* sPath);\n\n", out);

    xl6OutputPuts("typedef struct xl6_async_file_text_arg {\n", out);
    xl6OutputPuts("    char* path;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    int charset;\n", out);
    xl6OutputPuts("} XL6_AsyncFileTextArg;\n\n", out);

    xl6OutputPuts("typedef struct xl6_async_file_copy_arg {\n", out);
    xl6OutputPuts("    char* source;\n", out);
    xl6OutputPuts("    char* target;\n", out);
    xl6OutputPuts("    int overwrite;\n", out);
    xl6OutputPuts("} XL6_AsyncFileCopyArg;\n\n", out);

    xl6OutputPuts("static void xl6_async_file_text_arg_free(XL6_AsyncFileTextArg* arg)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( arg->path != 0 ) { xrtFree(arg->path); }\n", out);
    xl6OutputPuts("    if ( arg->text != 0 ) { xrtFree(arg->text); }\n", out);
    xl6OutputPuts("    xrtFree(arg);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_async_file_copy_arg_free(XL6_AsyncFileCopyArg* arg)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( arg->source != 0 ) { xrtFree(arg->source); }\n", out);
    xl6OutputPuts("    if ( arg->target != 0 ) { xrtFree(arg->target); }\n", out);
    xl6OutputPuts("    xrtFree(arg);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_file_result_error(xfuture_result* pOut, const char* message)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    pOut->iStatus = -1;\n", out);
    xl6OutputPuts("    pOut->pValue = 0;\n", out);
    xl6OutputPuts("    pOut->sError = xrtCopyStr(message != 0 ? message : \"async file failed\", 0);\n", out);
    xl6OutputPuts("    pOut->iFlags = pOut->sError != 0 ? XFUTURE_RESULT_F_OWN_ERROR : 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeValue = 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeError = pOut->sError != 0 ? xrtFree : 0;\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_file_result_bool(xfuture_result* pOut, int ok)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int* value;\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    value = (int*)xrtMalloc(sizeof(int));\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return xl6_async_file_result_error(pOut, \"out of memory while completing async file task\"); }\n", out);
    xl6OutputPuts("    *value = ok != 0;\n", out);
    xl6OutputPuts("    pOut->iStatus = 0;\n", out);
    xl6OutputPuts("    pOut->pValue = value;\n", out);
    xl6OutputPuts("    pOut->sError = 0;\n", out);
    xl6OutputPuts("    pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;\n", out);
    xl6OutputPuts("    pOut->pfnFreeValue = xrtFree;\n", out);
    xl6OutputPuts("    pOut->pfnFreeError = 0;\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_file_result_string(xfuture_result* pOut, char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char** value;\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    value = (const char**)xrtMalloc(sizeof(const char*));\n", out);
    xl6OutputPuts("    if ( value == 0 ) {\n", out);
    xl6OutputPuts("        if ( text != 0 ) { xrtFree(text); }\n", out);
    xl6OutputPuts("        return xl6_async_file_result_error(pOut, \"out of memory while completing async file task\");\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    *value = text != 0 ? text : xrtCopyStr(\"\", 0);\n", out);
    xl6OutputPuts("    if ( *value == 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(value);\n", out);
    xl6OutputPuts("        return xl6_async_file_result_error(pOut, \"out of memory while completing async file task\");\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    pOut->iStatus = 0;\n", out);
    xl6OutputPuts("    pOut->pValue = value;\n", out);
    xl6OutputPuts("    pOut->sError = 0;\n", out);
    xl6OutputPuts("    pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;\n", out);
    xl6OutputPuts("    pOut->pfnFreeValue = xl6_async_free_string_value;\n", out);
    xl6OutputPuts("    pOut->pfnFreeError = 0;\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_file_text_has_nul(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < size; ++i ) {\n", out);
    xl6OutputPuts("        if ( text[i] == '\\0' ) { return 1; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_file_raw_has_text_bom(const unsigned char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( data == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( size >= 4u ) {\n", out);
    xl6OutputPuts("        if ( data[0] == 0xFFu && data[1] == 0xFEu && data[2] == 0x00u && data[3] == 0x00u ) { return 1; }\n", out);
    xl6OutputPuts("        if ( data[0] == 0x00u && data[1] == 0x00u && data[2] == 0xFEu && data[3] == 0xFFu ) { return 1; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size >= 2u ) {\n", out);
    xl6OutputPuts("        if ( data[0] == 0xFFu && data[1] == 0xFEu ) { return 1; }\n", out);
    xl6OutputPuts("        if ( data[0] == 0xFEu && data[1] == 0xFFu ) { return 1; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_file_raw_looks_binary(const char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return size > 0u && xl6_async_file_text_has_nul(data, size) && !xl6_async_file_raw_has_text_bom((const unsigned char*)data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_AsyncFileTextArg* xl6_async_file_text_arg_new(const char* path, const char* text, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = (XL6_AsyncFileTextArg*)xrtMalloc(sizeof(XL6_AsyncFileTextArg));\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    arg->path = xrtCopyStr(path != 0 ? path : \"\", 0);\n", out);
    xl6OutputPuts("    arg->text = text != 0 ? xrtCopyStr(text, 0) : 0;\n", out);
    xl6OutputPuts("    arg->charset = charset;\n", out);
    xl6OutputPuts("    if ( arg->path == 0 || (text != 0 && arg->text == 0) ) {\n", out);
    xl6OutputPuts("        xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return arg;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_AsyncFileCopyArg* xl6_async_file_copy_arg_new(const char* source, const char* target, bool overwrite)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileCopyArg* arg = (XL6_AsyncFileCopyArg*)xrtMalloc(sizeof(XL6_AsyncFileCopyArg));\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    arg->source = xrtCopyStr(source != 0 ? source : \"\", 0);\n", out);
    xl6OutputPuts("    arg->target = xrtCopyStr(target != 0 ? target : \"\", 0);\n", out);
    xl6OutputPuts("    arg->overwrite = overwrite != 0;\n", out);
    xl6OutputPuts("    if ( arg->source == 0 || arg->target == 0 ) {\n", out);
    xl6OutputPuts("        xl6_async_file_copy_arg_free(arg);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return arg;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_read_text_async_entry(void* pArg, xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = (XL6_AsyncFileTextArg*)pArg;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return xl6_async_file_result_string(pOut, 0); }\n", out);
    xl6OutputPuts("    if ( arg->path == 0 || arg->path[0] == '\\0' || !xrtFileExists(arg->path) ) {\n", out);
    xl6OutputPuts("        xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("        return xl6_async_file_result_error(pOut, \"file does not exist\");\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( arg->charset == -2 ) {\n", out);
    xl6OutputPuts("        size_t rawSize = 0u;\n", out);
    xl6OutputPuts("        char* raw = (char*)xrtFileGetAll(arg->path, &rawSize);\n", out);
    xl6OutputPuts("        if ( rawSize > 0u && raw != 0 ) {\n", out);
    xl6OutputPuts("            if ( xl6_async_file_raw_looks_binary(raw, rawSize) ) {\n", out);
    xl6OutputPuts("                xrtFree(raw);\n", out);
    xl6OutputPuts("                xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("                return xl6_async_file_result_error(pOut, \"file text contains NUL byte; use binary/Buffer API\");\n", out);
    xl6OutputPuts("            }\n", out);
    xl6OutputPuts("            xrtFree(raw);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = xrtFileReadAll(arg->path, arg->charset, &size);\n", out);
    xl6OutputPuts("    xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return xl6_async_file_result_error(pOut, \"file text read failed\"); }\n", out);
    xl6OutputPuts("    if ( size > 0u && xl6_async_file_text_has_nul(text, size) ) {\n", out);
    xl6OutputPuts("        xrtFree(text);\n", out);
    xl6OutputPuts("        return xl6_async_file_result_error(pOut, \"file text contains NUL byte; use binary/Buffer API\");\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && !xrtIsUTF8(text, size) ) {\n", out);
    xl6OutputPuts("        xrtFree(text);\n", out);
    xl6OutputPuts("        return xl6_async_file_result_error(pOut, \"file text is not valid UTF-8 after charset conversion\");\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_async_file_result_string(pOut, text);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_write_text_async_entry(void* pArg, xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = (XL6_AsyncFileTextArg*)pArg;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return xl6_async_file_result_bool(pOut, 0); }\n", out);
    xl6OutputPuts("    ok = xrtFileWriteAll(arg->path, arg->text != 0 ? arg->text : \"\", 0, arg->charset) != 0;\n", out);
    xl6OutputPuts("    xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("    return xl6_async_file_result_bool(pOut, ok);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_append_text_async_entry(void* pArg, xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = (XL6_AsyncFileTextArg*)pArg;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return xl6_async_file_result_bool(pOut, 0); }\n", out);
    xl6OutputPuts("    ok = xrtFileAppend(arg->path, arg->text != 0 ? arg->text : \"\", 0, arg->charset) != 0;\n", out);
    xl6OutputPuts("    xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("    return xl6_async_file_result_bool(pOut, ok);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_copy_async_entry(void* pArg, xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileCopyArg* arg = (XL6_AsyncFileCopyArg*)pArg;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return xl6_async_file_result_bool(pOut, 0); }\n", out);
    xl6OutputPuts("    ok = xrtFileCopy(arg->source, arg->target, arg->overwrite != 0) != 0;\n", out);
    xl6OutputPuts("    xl6_async_file_copy_arg_free(arg);\n", out);
    xl6OutputPuts("    return xl6_async_file_result_bool(pOut, ok);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_move_async_entry(void* pArg, xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileCopyArg* arg = (XL6_AsyncFileCopyArg*)pArg;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return xl6_async_file_result_bool(pOut, 0); }\n", out);
    xl6OutputPuts("    ok = xrtFileMove(arg->source, arg->target, arg->overwrite != 0) != 0;\n", out);
    xl6OutputPuts("    xl6_async_file_copy_arg_free(arg);\n", out);
    xl6OutputPuts("    return xl6_async_file_result_bool(pOut, ok);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_delete_async_entry(void* pArg, xfuture_result* pOut);\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_read_text_async(const char* path, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = xl6_async_file_text_arg_new(path, 0, charset);\n", out);
    xl6OutputPuts("    return arg != 0 ? xl6_task_run_thread_tracked(xl6_xrt_file_read_text_async_entry, arg, 0) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_read_text_async_utf8(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_file_read_text_async(path, -2);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_write_text_async(const char* path, const char* text, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = xl6_async_file_text_arg_new(path, text, charset);\n", out);
    xl6OutputPuts("    return arg != 0 ? xl6_task_run_thread_tracked(xl6_xrt_file_write_text_async_entry, arg, 0) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_write_text_async_utf8(const char* path, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_file_write_text_async(path, text, 65001);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_append_text_async(const char* path, const char* text, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = xl6_async_file_text_arg_new(path, text, charset);\n", out);
    xl6OutputPuts("    return arg != 0 ? xl6_task_run_thread_tracked(xl6_xrt_file_append_text_async_entry, arg, 0) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_append_text_async_utf8(const char* path, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_file_append_text_async(path, text, 65001);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_copy_async(const char* source, const char* target, bool overwrite)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileCopyArg* arg = xl6_async_file_copy_arg_new(source, target, overwrite);\n", out);
    xl6OutputPuts("    return arg != 0 ? xl6_task_run_thread_tracked(xl6_xrt_file_copy_async_entry, arg, 0) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_copy_async_overwrite(const char* source, const char* target)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_file_copy_async(source, target, true);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_move_async(const char* source, const char* target, bool overwrite)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileCopyArg* arg = xl6_async_file_copy_arg_new(source, target, overwrite);\n", out);
    xl6OutputPuts("    return arg != 0 ? xl6_task_run_thread_tracked(xl6_xrt_file_move_async_entry, arg, 0) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_move_async_overwrite(const char* source, const char* target)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_file_move_async(source, target, true);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_delete_async(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = xl6_async_file_text_arg_new(path, 0, 0);\n", out);
    xl6OutputPuts("    return arg != 0 ? xl6_task_run_thread_tracked(xl6_xrt_file_delete_async_entry, arg, 0) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_file_delete_async_entry(void* pArg, xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_AsyncFileTextArg* arg = (XL6_AsyncFileTextArg*)pArg;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return xl6_async_file_result_bool(pOut, 0); }\n", out);
    xl6OutputPuts("    ok = xrtFileDelete(arg->path) != 0;\n", out);
    xl6OutputPuts("    xl6_async_file_text_arg_free(arg);\n", out);
    xl6OutputPuts("    return xl6_async_file_result_bool(pOut, ok);\n", out);
    xl6OutputPuts("}\n\n", out);
}
