static void xl6CgenEmitAsyncFileRuntime(XL6_Output* out)
{
    /* XRT Future 结果与 xlang future<T> 之间只做所有权和类型适配。 */
    xl6OutputPuts("typedef struct xasyncfile_struct xasyncfile;\n", out);
    xl6OutputPuts("typedef struct xasyncfileconfig { unsigned int iFlags; unsigned int iShareFlags; const char* sPath; } xasyncfileconfig;\n", out);
    xl6OutputPuts("typedef struct xasyncfilebuf { void* pData; size_t iSize; uint64_t iOffset; bool bEOF; } xasyncfilebuf;\n", out);
    xl6OutputPuts("typedef struct xasyncfileio { uint64_t iValue; uint64_t iOffset; } xasyncfileio;\n", out);
    xl6OutputPuts("typedef int (*xl6_future_cont_fn)(const xfuture_result* pIn, void* pArg, xfuture_result* pOut);\n", out);
    xl6OutputPuts("xfuture* xFutureThenSource(xfuture* pFuture, xl6_future_cont_fn pfnCont, void* pArg);\n", out);
    xl6OutputPuts("void xrtAsyncFileConfigInit(xasyncfileconfig* pConfig);\n", out);
    xl6OutputPuts("xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig);\nvoid xrtAsyncFileClose(xasyncfile* pFile);\n", out);
    xl6OutputPuts("void xrtAsyncFileBufDestroy(xasyncfilebuf* pBuf);\nvoid* xrtAsyncFileBufDetach(xasyncfilebuf* pBuf, size_t* pSize, uint64_t* pOffset, bool* pEOF);\n", out);
    xl6OutputPuts("void xrtAsyncFileIoDestroy(xasyncfileio* pInfo);\n", out);
    xl6OutputPuts("xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64_t iOffset, size_t iSize);\n", out);
    xl6OutputPuts("xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64_t iOffset, const void* pData, size_t iSize);\n", out);
    xl6OutputPuts("xfuture* xrtAsyncFileFlush(xasyncfile* pFile);\nxfuture* xrtAsyncFileGetSize(xasyncfile* pFile);\nxfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64_t iSize);\n", out);
    xl6OutputPuts("xfuture* xrtFileAppendAsync(const char* sPath, const char* sText, size_t iSize, int iCharset);\n", out);
    xl6OutputPuts("xfuture* xrtFileReadAllAsync(const char* sPath, int iCharset);\nxfuture* xrtFileWriteAllAsync(const char* sPath, const char* sText, size_t iSize, int iCharset);\n", out);
    xl6OutputPuts("xfuture* xrtFileGetAllAsync(const char* sPath);\nxfuture* xrtFilePutAllAsync(const char* sPath, const void* pData, size_t iSize);\n", out);
    xl6OutputPuts("xfuture* xrtFileCopyAsync(const char* sSrc, const char* sDst, bool bReWrite);\nxfuture* xrtFileMoveAsync(const char* sSrc, const char* sDst, bool bReWrite);\nxfuture* xrtFileDeleteAsync(const char* sPath);\n", out);
    xl6OutputPuts("xfuture* xrtDirCreateAsync(const char* sPath);\nxfuture* xrtDirCreateAllAsync(const char* sPath);\n", out);
    xl6OutputPuts("xfuture* xrtDirCopyAsync(const char* sSrc, const char* sDst, bool bReWrite);\nxfuture* xrtDirMoveAsync(const char* sSrc, const char* sDst, bool bReWrite);\nxfuture* xrtDirDeleteAsync(const char* sPath);\n", out);
    xl6OutputPuts("bool xrtIsUTF8(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtCalloc(size_t iCount, size_t iSize);\n", out);
    xl6OutputPuts("static void* xl6_xrt_buffer_from_owned_bytes(void* data, size_t size);\n\n", out);
    xl6OutputPuts("long long xl6_xrt_buffer_destroy(void* self);\n\n", out);

    xl6OutputPuts("static int xl6_async_file_result_error(xfuture_result* pOut, const char* message)\n{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return -1; } memset(pOut, 0, sizeof(*pOut)); pOut->iStatus = -1;\n", out);
    xl6OutputPuts("    pOut->sError = xrtCopyStr(message != 0 ? message : \"async file failed\", 0); pOut->iFlags = pOut->sError != 0 ? XFUTURE_RESULT_F_OWN_ERROR : 0; pOut->pfnFreeError = pOut->sError != 0 ? xrtFree : 0; return -1;\n}\n\n", out);
    xl6OutputPuts("static int xl6_async_file_result_bool(xfuture_result* pOut, int value)\n{\n", out);
    xl6OutputPuts("    int* box = (int*)xrtMalloc(sizeof(int)); if ( box == 0 ) { return xl6_async_file_result_error(pOut, \"async file result allocation failed\"); } *box = value != 0; memset(pOut, 0, sizeof(*pOut)); pOut->pValue = box; pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE; pOut->pfnFreeValue = xrtFree; return 0;\n}\n\n", out);
    xl6OutputPuts("static int xl6_async_file_result_i64(xfuture_result* pOut, long long value)\n{\n", out);
    xl6OutputPuts("    long long* box = (long long*)xrtMalloc(sizeof(long long)); if ( box == 0 ) { return xl6_async_file_result_error(pOut, \"async file result allocation failed\"); } *box = value; memset(pOut, 0, sizeof(*pOut)); pOut->pValue = box; pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE; pOut->pfnFreeValue = xrtFree; return 0;\n}\n\n", out);
    xl6OutputPuts("static int xl6_async_file_result_point(xfuture_result* pOut, void* value)\n{\n", out);
    xl6OutputPuts("    void** box; if ( value == 0 ) { return xl6_async_file_result_error(pOut, \"async file buffer allocation failed\"); } box = (void**)xrtMalloc(sizeof(void*)); if ( box == 0 ) { (void)xl6_xrt_buffer_destroy(value); return xl6_async_file_result_error(pOut, \"async file result allocation failed\"); } *box = value; memset(pOut, 0, sizeof(*pOut)); pOut->pValue = box; pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE; pOut->pfnFreeValue = xrtFree; return 0;\n}\n\n", out);
    xl6OutputPuts("static int xl6_async_file_result_string(xfuture_result* pOut, char* text)\n{\n", out);
    xl6OutputPuts("    const char** box = (const char**)xrtMalloc(sizeof(const char*)); if ( box == 0 ) { xrtFree(text); return xl6_async_file_result_error(pOut, \"async file result allocation failed\"); } *box = text != 0 ? text : xrtCopyStr(\"\", 0); if ( *box == 0 ) { xrtFree(box); return xl6_async_file_result_error(pOut, \"async file text allocation failed\"); } memset(pOut, 0, sizeof(*pOut)); pOut->pValue = box; pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE; pOut->pfnFreeValue = xl6_async_free_string_value; return 0;\n}\n\n", out);
    xl6OutputPuts("static int xl6_async_file_has_nul(const char* text, size_t size)\n{ size_t i; for ( i = 0u; text != 0 && i < size; ++i ) { if ( text[i] == '\\0' ) { return 1; } } return 0; }\n\n", out);

    xl6OutputPuts("static int xl6_async_file_map_bool(const xfuture_result* input, void* arg, xfuture_result* output)\n{\n", out);
    xl6OutputPuts("    xasyncfileio* info = input != 0 ? (xasyncfileio*)input->pValue : 0; (void)arg; if ( info == 0 ) { return xl6_async_file_result_error(output, \"async file returned no result\"); } xrtAsyncFileIoDestroy(info); return xl6_async_file_result_bool(output, 1);\n}\n", out);
    xl6OutputPuts("static int xl6_async_file_map_success(const xfuture_result* input, void* arg, xfuture_result* output)\n{\n", out);
    xl6OutputPuts("    (void)input; (void)arg; return xl6_async_file_result_bool(output, 1);\n}\n", out);
    xl6OutputPuts("static int xl6_async_file_map_i64(const xfuture_result* input, void* arg, xfuture_result* output)\n{\n", out);
    xl6OutputPuts("    xasyncfileio* info = input != 0 ? (xasyncfileio*)input->pValue : 0; long long value; (void)arg; if ( info == 0 ) { return xl6_async_file_result_error(output, \"async file returned no result\"); } value = info->iValue > 0x7fffffffffffffffULL ? 0x7fffffffffffffffLL : (long long)info->iValue; xrtAsyncFileIoDestroy(info); return xl6_async_file_result_i64(output, value);\n}\n", out);
    xl6OutputPuts("static int xl6_async_file_map_text(const xfuture_result* input, void* arg, xfuture_result* output)\n{\n", out);
    xl6OutputPuts("    xasyncfilebuf* info = input != 0 ? (xasyncfilebuf*)input->pValue : 0; void* data; size_t size = 0u; char* text; (void)arg; if ( info == 0 ) { return xl6_async_file_result_error(output, \"async file returned no text\"); } data = xrtAsyncFileBufDetach(info, &size, 0, 0); if ( size > 0u && data == 0 ) { return xl6_async_file_result_error(output, \"async file returned invalid text\"); } if ( xl6_async_file_has_nul((const char*)data, size) ) { xrtFree(data); return xl6_async_file_result_error(output, \"file text contains NUL byte; use readBytesAsync\"); } if ( size > 0u && !xrtIsUTF8((const char*)data, size) ) { xrtFree(data); return xl6_async_file_result_error(output, \"file text is not valid UTF-8 after charset conversion\"); } text = xrtCopyStr(data != 0 ? (const char*)data : \"\", size); xrtFree(data); return text != 0 ? xl6_async_file_result_string(output, text) : xl6_async_file_result_error(output, \"async file text allocation failed\");\n}\n", out);
    xl6OutputPuts("static int xl6_async_file_map_buffer(const xfuture_result* input, void* arg, xfuture_result* output)\n{\n", out);
    xl6OutputPuts("    xasyncfilebuf* info = input != 0 ? (xasyncfilebuf*)input->pValue : 0; void* data; void* buffer; size_t size = 0u; (void)arg; if ( info == 0 ) { return xl6_async_file_result_error(output, \"async file returned no buffer\"); } data = xrtAsyncFileBufDetach(info, &size, 0, 0); if ( size > 0u && data == 0 ) { return xl6_async_file_result_error(output, \"async file returned invalid buffer\"); } buffer = xl6_xrt_buffer_from_owned_bytes(data, size); return xl6_async_file_result_point(output, buffer);\n}\n", out);
    xl6OutputPuts("static xfuture* xl6_async_file_chain(xfuture* source, xl6_future_cont_fn mapper)\n{\n", out);
    xl6OutputPuts("    xfuture* result; if ( source == 0 ) { return 0; } result = xFutureThenSource(source, mapper, 0); if ( result != 0 ) { (void)xFutureForwardCancelTo(result, source); } xFutureRelease(source); return result;\n}\n\n", out);

    xl6OutputPuts("xfuture* xl6_xrt_file_read_text_async(const char* path, int charset) { return xl6_async_file_chain(xrtFileReadAllAsync(path, charset), xl6_async_file_map_text); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_read_text_async_utf8(const char* path) { return xl6_xrt_file_read_text_async(path, -2); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_read_bytes_async(const char* path) { return xl6_async_file_chain(xrtFileGetAllAsync(path), xl6_async_file_map_buffer); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_write_text_async(const char* path, const char* text, int charset) { return xl6_async_file_chain(xrtFileWriteAllAsync(path, text != 0 ? text : \"\", 0u, charset), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_write_text_async_utf8(const char* path, const char* text) { return xl6_xrt_file_write_text_async(path, text, 65001); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_append_text_async(const char* path, const char* text, int charset) { return xl6_async_file_chain(xrtFileAppendAsync(path, text != 0 ? text : \"\", 0u, charset), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_append_text_async_utf8(const char* path, const char* text) { return xl6_xrt_file_append_text_async(path, text, 65001); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_write_bytes_async(const char* path, void* data, size_t size) { return xl6_async_file_chain(xrtFilePutAllAsync(path, data, size), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_copy_async(const char* source, const char* target, bool overwrite) { return xl6_async_file_chain(xrtFileCopyAsync(source, target, overwrite), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_copy_async_overwrite(const char* source, const char* target) { return xl6_xrt_file_copy_async(source, target, true); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_move_async(const char* source, const char* target, bool overwrite) { return xl6_async_file_chain(xrtFileMoveAsync(source, target, overwrite), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_move_async_overwrite(const char* source, const char* target) { return xl6_xrt_file_move_async(source, target, true); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_file_delete_async(const char* path) { return xl6_async_file_chain(xrtFileDeleteAsync(path), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_dir_create_async(const char* path) { return xl6_async_file_chain(xrtDirCreateAsync(path), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_dir_create_all_async(const char* path) { return xl6_async_file_chain(xrtDirCreateAllAsync(path), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_dir_copy_async(const char* source, const char* target) { return xl6_async_file_chain(xrtDirCopyAsync(source, target, true), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_dir_move_async(const char* source, const char* target) { return xl6_async_file_chain(xrtDirMoveAsync(source, target, true), xl6_async_file_map_bool); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_dir_delete_async(const char* path) { return xl6_async_file_chain(xrtDirDeleteAsync(path), xl6_async_file_map_bool); }\n\n", out);

    xl6OutputPuts("typedef struct XL6_AsyncFileHandle { xasyncfile* file; } XL6_AsyncFileHandle;\n", out);
    xl6OutputPuts("void* xl6_xrt_async_file_open(const char* path, long long flags, long long shareFlags)\n{\n", out);
    xl6OutputPuts("    xasyncfileconfig config; XL6_AsyncFileHandle* handle; xrtAsyncFileConfigInit(&config); config.sPath = path; config.iFlags = flags < 0 ? 0u : (unsigned int)flags; config.iShareFlags = shareFlags < 0 ? 0u : (unsigned int)shareFlags; handle = (XL6_AsyncFileHandle*)xrtCalloc(1u, sizeof(XL6_AsyncFileHandle)); if ( handle == 0 ) { return 0; } handle->file = xrtAsyncFileOpen(&config); if ( handle->file == 0 ) { xrtFree(handle); return 0; } return handle;\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_async_file_destroy(void* value) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; if ( handle != 0 ) { if ( handle->file != 0 ) { xrtAsyncFileClose(handle->file); handle->file = 0; } xrtFree(handle); } return 0; }\n", out);
    xl6OutputPuts("int xl6_xrt_async_file_close(void* value) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; if ( handle == 0 ) { return 0; } if ( handle->file != 0 ) { xrtAsyncFileClose(handle->file); handle->file = 0; } return 1; }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_async_file_read(void* value, long long offset, long long size) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; if ( handle == 0 || handle->file == 0 || offset < 0 || size < 0 ) { return 0; } return xl6_async_file_chain(xrtAsyncFileReadAt(handle->file, (uint64_t)offset, (size_t)size), xl6_async_file_map_buffer); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_async_file_write(void* value, long long offset, void* data, size_t size) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; if ( handle == 0 || handle->file == 0 || offset < 0 ) { return 0; } return xl6_async_file_chain(xrtAsyncFileWriteAt(handle->file, (uint64_t)offset, data, size), xl6_async_file_map_i64); }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_async_file_flush(void* value) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; return handle != 0 && handle->file != 0 ? xl6_async_file_chain(xrtAsyncFileFlush(handle->file), xl6_async_file_map_success) : 0; }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_async_file_size(void* value) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; return handle != 0 && handle->file != 0 ? xl6_async_file_chain(xrtAsyncFileGetSize(handle->file), xl6_async_file_map_i64) : 0; }\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_async_file_resize(void* value, long long size) { XL6_AsyncFileHandle* handle = (XL6_AsyncFileHandle*)value; return handle != 0 && handle->file != 0 && size >= 0 ? xl6_async_file_chain(xrtAsyncFileSetSize(handle->file, (uint64_t)size), xl6_async_file_map_bool) : 0; }\n\n", out);
}
