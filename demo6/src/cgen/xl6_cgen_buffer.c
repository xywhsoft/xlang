static void xl6CgenEmitBufferRuntime(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xbuffer_struct {\n", out);
    xl6OutputPuts("    void* Buffer;\n", out);
    xl6OutputPuts("    unsigned int Length;\n", out);
    xl6OutputPuts("    unsigned int AllocLength;\n", out);
    xl6OutputPuts("    unsigned int AllocStep;\n", out);
    xl6OutputPuts("} xbuffer_struct, *xbuffer;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", out);
    xl6OutputPuts("void* xrtBufferCreate(unsigned int iStep);\n", out);
    xl6OutputPuts("void xrtBufferDestroy(xbuffer pBuf);\n", out);
    xl6OutputPuts("void xrtBufferUnit(xbuffer pBuf);\n", out);
    xl6OutputPuts("bool xrtBufferMalloc(xbuffer pBuf, unsigned int iCount);\n", out);
    xl6OutputPuts("bool xrtBufferAppend(xbuffer pBuf, void* pData, unsigned int iSize, unsigned int bStrMode);\n\n", out);
    xl6OutputPuts("void* xrtNetStreamReadBytes(void* pStream, size_t iMaxBytes, size_t* pOutLen);\n", out);
    xl6OutputPuts("void* xrtNetStreamRecvBytes(void* pStream, size_t iMaxBytes, long long iTimeoutMs, size_t* pOutLen);\n", out);
    xl6OutputPuts("void* xrtNetDgramPacketBytesCopy(void* pPacket, size_t* pOutLen);\n\n", out);

    xl6OutputPuts("static void* xl6_xrt_buffer_from_owned_bytes(void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    if ( size > 4294967295u ) {\n", out);
    xl6OutputPuts("        xrtFree(data);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (xbuffer)xrtBufferCreate(0u);\n", out);
    xl6OutputPuts("    if ( buf == 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(data);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u ) {\n", out);
    xl6OutputPuts("        if ( data == 0 || !xrtBufferAppend(buf, data, (unsigned int)size, 0u) ) {\n", out);
    xl6OutputPuts("            xrtBufferDestroy(buf);\n", out);
    xl6OutputPuts("            xrtFree(data);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(data);\n", out);
    xl6OutputPuts("    return buf;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_xrt_buffer_from_view_bytes(const void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    if ( size > 4294967295u ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (xbuffer)xrtBufferCreate(0u);\n", out);
    xl6OutputPuts("    if ( buf == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u ) {\n", out);
    xl6OutputPuts("        if ( data == 0 || !xrtBufferAppend(buf, (void*)data, (unsigned int)size, 0u) ) {\n", out);
    xl6OutputPuts("            xrtBufferDestroy(buf);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return buf;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtBufferCreate(0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_create_step(long long step)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( step < 0 || step > 4294967295LL ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtBufferCreate((unsigned int)step);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_wrap_handle(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtBufferDestroy((xbuffer)self);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_len(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)((xbuffer)self)->Length : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_capacity(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)((xbuffer)self)->AllocLength : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_buffer_reserve(void* self, long long size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 || size < 0 || size > 4294967295LL ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtBufferMalloc((xbuffer)self, (unsigned int)size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_buffer_clear(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtBufferUnit((xbuffer)self);\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_buffer_append_bytes(void* self, const char* data, size_t size)\n", out);
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
    xl6OutputPuts("    return xrtBufferAppend((xbuffer)self, (void*)data, (unsigned int)size, 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_buffer_append_string(void* self, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return text == 0 ? xl6_xrt_buffer_append_bytes(self, \"\", 0u) : xl6_xrt_buffer_append_bytes(self, text, strlen(text));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_buffer_append_byte(void* self, long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char byte;\n", out);
    xl6OutputPuts("    if ( value < 0 || value > 255 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    byte = (unsigned char)value;\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_append_bytes(self, (const char*)&byte, 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_buffer_byte_at(void* self, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    if ( self == 0 || index < 0 ) {\n", out);
    xl6OutputPuts("        return -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (xbuffer)self;\n", out);
    xl6OutputPuts("    if ( (unsigned long long)index >= (unsigned long long)buf->Length || buf->Buffer == 0 ) {\n", out);
    xl6OutputPuts("        return -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (long long)((unsigned char*)buf->Buffer)[index];\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_buffer_set_byte(void* self, long long index, long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    if ( self == 0 || index < 0 || value < 0 || value > 255 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (xbuffer)self;\n", out);
    xl6OutputPuts("    if ( (unsigned long long)index >= (unsigned long long)buf->Length || buf->Buffer == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    ((unsigned char*)buf->Buffer)[index] = (unsigned char)value;\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_text(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    size_t len;\n", out);
    xl6OutputPuts("    if ( self == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = (xbuffer)self;\n", out);
    xl6OutputPuts("    len = (size_t)buf->Length;\n", out);
    xl6OutputPuts("    text = (char*)xrtMalloc(len + 1u);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( len > 0 && buf->Buffer != 0 ) {\n", out);
    xl6OutputPuts("        memcpy(text, buf->Buffer, len);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text[len] = 0;\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_buffer_data(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? ((xbuffer)self)->Buffer : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_tcp_stream_read_buffer(void* stream, size_t maxBytes)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data = xrtNetStreamReadBytes(stream, maxBytes, &size);\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_owned_bytes(data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_tcp_stream_recv_buffer(void* stream, size_t maxBytes, long long timeoutMs)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data = xrtNetStreamRecvBytes(stream, maxBytes, timeoutMs, &size);\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_owned_bytes(data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_udp_packet_buffer(void* packet)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data = xrtNetDgramPacketBytesCopy(packet, &size);\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_owned_bytes(data, size);\n", out);
    xl6OutputPuts("}\n\n", out);
}
