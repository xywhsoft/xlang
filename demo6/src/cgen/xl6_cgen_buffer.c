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
            "extern bool xrtUtf8Valid(xstrview, size_t*);\n\n",
            out);
    } else {
        /* The declaration-SDK-backed form already imports the public XRT ABI. */
        xl6OutputPuts("#ifndef XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\n#define XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\ntypedef xbytesview XL6_XrtBytesView;\n#endif\n", out);
        xl6OutputPuts("typedef xbuffer* XL6_XBuffer;\n\n", out);
    }
    xl6OutputPuts("typedef struct XL6_BufferIter {\n", out);
    xl6OutputPuts("    XL6_XBuffer buffer;\n", out);
    xl6OutputPuts("    size_t index;\n", out);
    xl6OutputPuts("} XL6_BufferIter;\n\n", out);
    xl6OutputPuts("static void xl6_buffer_raise(const char* kind, const char* message, const char* api)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    (void)api; xrtSetErrorTake(xlir_error_create_at(kind != 0 ? kind : \"BufferError\", message != 0 ? message : \"buffer error\", 0, 0));\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static int xl6_buffer_has_nul(const void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const unsigned char* p = (const unsigned char*)data;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( p == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < size; ++i ) {\n", out);
    xl6OutputPuts("        if ( p[i] == 0u ) { return 1; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
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

    xl6OutputPuts("void* xl6_xrt_buffer_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtBufferCreate();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_create_step(long long step)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( step < 0 || step > 4294967295LL ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    { XL6_XBuffer buf = xrtBufferCreate(); if ( buf != 0 && step > 0 && !xrtBufferReserve(buf, (size_t)step) ) { xrtBufferDestroy(buf); return 0; } return buf; }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_wrap_handle(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_buffer_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtBufferDestroy((XL6_XBuffer)self);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_len(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)((XL6_XBuffer)self)->Size : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_capacity(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)((XL6_XBuffer)self)->Capacity : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_reserve(void* self, long long size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 || size < 0 || size > 4294967295LL ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtBufferReserve((XL6_XBuffer)self, (size_t)size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_clear(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtBufferClear((XL6_XBuffer)self);\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_append_bytes(void* self, const char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 || size > 4294967295u ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size == 0 ) {\n", out);
    xl6OutputPuts("        return 1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtBufferAppend((XL6_XBuffer)self, (XL6_XrtBytesView){ (const unsigned char*)data, size });\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_append_raw(void* self, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_append_bytes(self, (const char*)data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_append_string(void* self, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return text == 0 ? xl6_xrt_buffer_append_bytes(self, \"\", 0u) : xl6_xrt_buffer_append_bytes(self, text, strlen(text));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_insert_raw(void* self, long long index, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XBuffer buf = (XL6_XBuffer)self;\n", out);
    xl6OutputPuts("    if ( buf == 0 || index < 0 || (unsigned long long)index > (unsigned long long)buf->Size || size > 4294967295u ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"RangeError\", \"buffer insert index out of range\", \"Buffer.insert\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { return 1; }\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"ValueError\", \"buffer insert data is null\", \"Buffer.insert\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtBufferInsert(buf, (size_t)index, (XL6_XrtBytesView){ (const unsigned char*)data, size });\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_insert_string(void* self, long long index, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* safe = text != 0 ? text : \"\";\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_insert_raw(self, index, (void*)safe, strlen(safe));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_append_byte(void* self, long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char byte;\n", out);
    xl6OutputPuts("    if ( value < 0 || value > 255 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"ValueError\", \"buffer byte value must be in 0..255\", \"Buffer.appendByte\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    byte = (unsigned char)value;\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_append_bytes(self, (const char*)&byte, 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_byte_at(void* self, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XBuffer buf;\n", out);
    xl6OutputPuts("    if ( self == 0 || index < 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"RangeError\", \"buffer index out of range\", \"Buffer.byteAt\");\n", out);
    xl6OutputPuts("        return -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (XL6_XBuffer)self;\n", out);
    xl6OutputPuts("    if ( (unsigned long long)index >= (unsigned long long)buf->Size || buf->Data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"RangeError\", \"buffer index out of range\", \"Buffer.byteAt\");\n", out);
    xl6OutputPuts("        return -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (long long)buf->Data[index];\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_set_byte(void* self, long long index, long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XBuffer buf;\n", out);
    xl6OutputPuts("    if ( value < 0 || value > 255 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"ValueError\", \"buffer byte value must be in 0..255\", \"Buffer.setByte\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( self == 0 || index < 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"RangeError\", \"buffer index out of range\", \"Buffer.setByte\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (XL6_XBuffer)self;\n", out);
    xl6OutputPuts("    if ( (unsigned long long)index >= (unsigned long long)buf->Size || buf->Data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"RangeError\", \"buffer index out of range\", \"Buffer.setByte\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf->Data[index] = (unsigned char)value;\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_text(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XBuffer buf;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    size_t len;\n", out);
    xl6OutputPuts("    if ( self == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"BufferError\", \"invalid buffer handle\", \"Buffer.text\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (XL6_XBuffer)self;\n", out);
    xl6OutputPuts("    len = buf->Size;\n", out);
    xl6OutputPuts("    if ( len > 0u && (buf->Data == 0 || xl6_buffer_has_nul(buf->Data, len)) ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"BufferError\", \"buffer contains NUL byte; use binary API\", \"Buffer.text\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( len > 0u && !xrtUtf8Valid((xstrview){ (const char*)buf->Data, len }, 0) ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"BufferError\", \"buffer is not valid UTF-8 text\", \"Buffer.text\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = (char*)xrtMalloc(len + 1u);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( len > 0 && buf->Data != 0 ) {\n", out);
    xl6OutputPuts("        memcpy(text, buf->Data, len);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text[len] = 0;\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_data(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? ((XL6_XBuffer)self)->Data : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    /* Buffer 自身拥有 native handle 适配器，任何模块都不应替它补定义。 */
    xl6OutputPuts("void* xl6_xrt_buffer_native_handle(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self;\n", out);
    xl6OutputPuts("}\n\n", out);

    /* 文件桥只在 File 能力实际展开时生成，Buffer 本身不依赖文件系统。 */
    if ( hasFileRuntime ) {
        xl6OutputPuts("void* xl6_xrt_buffer_file_read_bytes(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* safePath = path != 0 ? path : \"\";\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data;\n", out);
    xl6OutputPuts("    if ( safePath[0] == '\\0' || !xrtFileExists(safePath) ) {\n", out);
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

    xl6OutputPuts("bool xl6_xrt_buffer_file_write_bytes(const char* path, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid file path\", \"file.writeBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid binary buffer\", \"file.writeBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtFileWriteAll(path, (XL6_XrtBytesView){ (const unsigned char*)data, size });\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_buffer_file_append_bytes(const char* path, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid file path\", \"file.appendBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_buffer_raise(\"FileError\", \"invalid binary buffer\", \"file.appendBytes\");\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtFileAppend(path, (XL6_XrtBytesView){ (const unsigned char*)data, size });\n", out);
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

    xl6OutputPuts("void* xl6_xrt_buffer_iter_create(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_BufferIter* iter = (XL6_BufferIter*)xrtMalloc(sizeof(XL6_BufferIter));\n", out);
    xl6OutputPuts("    if ( iter == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    iter->buffer = (XL6_XBuffer)self;\n", out);
    xl6OutputPuts("    iter->index = 0u;\n", out);
    xl6OutputPuts("    return iter;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_iter_wrap_handle(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_buffer_iter_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_buffer_iter_has_next(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_BufferIter* iter = (XL6_BufferIter*)self;\n", out);
    xl6OutputPuts("    return iter != 0 && iter->buffer != 0 && iter->index < iter->buffer->Size;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_iter_next(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_BufferIter* iter = (XL6_BufferIter*)self;\n", out);
    xl6OutputPuts("    unsigned char byte;\n", out);
    xl6OutputPuts("    if ( iter == 0 || iter->buffer == 0 || iter->index >= iter->buffer->Size || iter->buffer->Data == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    byte = iter->buffer->Data[iter->index];\n", out);
    xl6OutputPuts("    iter->index++;\n", out);
    xl6OutputPuts("    return (long long)byte;\n", out);
    xl6OutputPuts("}\n\n", out);

}
