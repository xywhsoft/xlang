void xl6CgenEmitBufferRuntime(
    XL6_Output* out, int hasFileRuntime, int hasXrtDeclarations)
{
    xl6OutputPuts(
        "#ifndef XL6_CGEN_BOOL_TYPEDEF\n#define XL6_CGEN_BOOL_TYPEDEF\ntypedef bool XL6_CgenBool;\n#endif\n",
        out);
    if ( !hasXrtDeclarations ) {
        /* XLIR output deliberately does not import xrt.h/xruntime.h.  Keep the
         * public buffer layout and the exact functions used by this component
         * as a narrow, local ABI declaration surface. */
        xl6OutputPuts(
            "#ifndef XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\n#define XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\ntypedef struct XL6_XrtBytesView { const unsigned char* Data; size_t Size; } XL6_XrtBytesView;\n#endif\n"
            "typedef struct XL6_XBufferLayout { unsigned char* Data; size_t Size; size_t Capacity; } XL6_XBufferLayout;\n"
            "typedef XL6_XBufferLayout* XL6_XBuffer;\n"
            "extern void* xrtMalloc(size_t);\n"
            "extern void xrtFree(void*);\n"
            "extern XL6_XBuffer xrtBufferCreate(void);\n"
            "extern XL6_XBuffer xrtBufferCreateTake(unsigned char**, size_t, size_t);\n"
            "extern void xrtBufferDestroy(XL6_XBuffer);\n"
            "extern void xrtBufferClear(XL6_XBuffer);\n"
            "extern bool xrtBufferReserve(XL6_XBuffer, size_t);\n"
            "extern bool xrtBufferAppend(XL6_XBuffer, XL6_XrtBytesView);\n"
            "extern bool xrtBufferInsert(XL6_XBuffer, size_t, XL6_XrtBytesView);\n"
            "extern bool xrtBufferWrite(XL6_XBuffer, size_t, XL6_XrtBytesView);\n"
            "extern bool xrtUtf8Valid(xstrview, size_t*);\n\n",
            out);
    } else {
        /* The declaration-SDK-backed form already imports the public XRT ABI. */
        xl6OutputPuts("#ifndef XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\n#define XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\ntypedef xbytesview XL6_XrtBytesView;\n#endif\n", out);
        xl6OutputPuts("typedef xbuffer* XL6_XBuffer;\n\n", out);
    }
    xl6OutputPuts("static void xl6_buffer_raise(const char* kind, const char* message, const char* api)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    (void)api; xrtSetErrorTake(xlir_error_create_at(kind != 0 ? kind : \"BufferError\", message != 0 ? message : \"buffer error\", 0, 0));\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void* xl6_xrt_buffer_from_owned_bytes(void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( size > 4294967295u ) {\n", out);
    xl6OutputPuts("        xrtFree(data);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { xrtFree(data); return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    {\n", out);
    xl6OutputPuts("        unsigned char* owned = (unsigned char*)data;\n", out);
    xl6OutputPuts("        XL6_XBuffer buf = xrtBufferCreateTake(&owned, size, size);\n", out);
    xl6OutputPuts("        if ( buf == 0 ) { xrtFree(owned); }\n", out);
    xl6OutputPuts("        return buf;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_xrt_buffer_from_view_bytes(const void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XBuffer buf;\n", out);
    xl6OutputPuts("    if ( size > 4294967295u ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && data == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    buf = xrtBufferCreate();\n", out);
    xl6OutputPuts("    if ( buf == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( size > 0u && !xrtBufferAppend(buf, (XL6_XrtBytesView){ (const unsigned char*)data, size }) ) { xrtBufferDestroy(buf); return 0; }\n", out);
    xl6OutputPuts("    return buf;\n", out);
    xl6OutputPuts("}\n\n", out);


    /* 文件桥只在 File 能力实际展开时生成，Buffer 本身不依赖文件系统。 */
    if ( hasFileRuntime ) {
        xl6OutputPuts("void* xl6_xrt_buffer_file_read_bytes(xlir_string path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* safePath = xlir_string_cstr(path);\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data;\n", out);
    xl6OutputPuts("    if ( safePath == NULL || safePath[0] == '\\0' || !xrtFileExists(safePath) ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"file does not exist\", \"file.readBytes\");\n", out);
    xl6OutputPuts("        return xrtBufferCreate();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    data = xrtFileReadAll(safePath, &size);\n", out);
    xl6OutputPuts("    if ( data == 0 && size > 0u ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"file binary read failed\", \"file.readBytes\");\n", out);
    xl6OutputPuts("        return xrtBufferCreate();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_owned_bytes(data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_buffer_file_write_bytes(xlir_string path, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* cpath = xlir_string_cstr(path);\n", out);
    xl6OutputPuts("    if ( cpath == 0 || cpath[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid file path\", \"file.writeBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid binary buffer\", \"file.writeBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtFileWriteAll(cpath, (XL6_XrtBytesView){ (const unsigned char*)data, size });\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_buffer_file_append_bytes(xlir_string path, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* cpath = xlir_string_cstr(path);\n", out);
    xl6OutputPuts("    if ( cpath == 0 || cpath[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid file path\", \"file.appendBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid binary buffer\", \"file.appendBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtFileAppend(cpath, (XL6_XrtBytesView){ (const unsigned char*)data, size });\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_read_bytes(void* self, long long size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    size_t readSize;\n", out);
    xl6OutputPuts("    size_t retSize = 0u;\n", out);
    xl6OutputPuts("    char* data;\n", out);
    xl6OutputPuts("    if ( handle == 0 || handle->file == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid file handle\", \"File.readBytes\");\n", out);
    xl6OutputPuts("        return xrtBufferCreate();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size < 0 ) {\n", out);
    xl6OutputPuts("        unsigned long long pos = 0u;\n", out);
    xl6OutputPuts("        unsigned long long end = 0u;\n", out);
    xl6OutputPuts("        if ( !xrtTell(handle->file, &pos) || !xrtFileSize(handle->file, &end) ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("        readSize = end > pos ? (size_t)(end - pos) : 0u;\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        readSize = (size_t)size;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( readSize == 0u ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    data = (char*)xrtMalloc(readSize + 1u);\n", out);
    xl6OutputPuts("    if ( data == 0 || !xrtRead(handle->file, data, readSize, &retSize) ) { xrtFree(data); return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    data[retSize] = '\\0';\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_owned_bytes(data, retSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_write_bytes(void* self, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    size_t written = 0u;\n", out);
    xl6OutputPuts("    if ( handle == 0 || handle->file == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid file handle\", \"File.writeBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid binary buffer\", \"File.writeBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtWriteFull(handle->file, data, size, &written) ? (long long)written : 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);
    }


}
