static void xl6CgenEmitEncodingRuntime(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_ENCODING_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_ENCODING_RUNTIME\n", out);

    xl6OutputPuts("#ifndef XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xbuffer_struct {\n", out);
    xl6OutputPuts("    void* Buffer;\n", out);
    xl6OutputPuts("    unsigned int Length;\n", out);
    xl6OutputPuts("    unsigned int AllocLength;\n", out);
    xl6OutputPuts("    unsigned int AllocStep;\n", out);
    xl6OutputPuts("} xbuffer_struct, *xbuffer;\n", out);
    xl6OutputPuts("#endif\n", out);

    xl6OutputPuts("#ifndef XRT_CP_AUTO\n", out);
    xl6OutputPuts("#define XRT_CP_AUTO -2\n", out);
    xl6OutputPuts("#define XRT_CP_BINARY -1\n", out);
    xl6OutputPuts("#define XRT_CP_OEM 0\n", out);
    xl6OutputPuts("#define XRT_CP_UTF8 65001\n", out);
    xl6OutputPuts("#define XRT_CP_UTF16 1200\n", out);
    xl6OutputPuts("#define XRT_CP_UTF16_BE 1201\n", out);
    xl6OutputPuts("#define XRT_CP_UTF32 65005\n", out);
    xl6OutputPuts("#define XRT_CP_UTF32_BE 65006\n", out);
    xl6OutputPuts("#define XRT_CP_BOM 0x40000000\n", out);
    xl6OutputPuts("#define XRT_MASK_BOM 0xBFFFFFFF\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", out);
    xl6OutputPuts("void* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtBufferCreate(unsigned int iStep);\n", out);
    xl6OutputPuts("void xrtBufferDestroy(xbuffer pBuf);\n", out);
    xl6OutputPuts("bool xrtBufferAppend(xbuffer pBuf, void* pData, unsigned int iSize, unsigned int bStrMode);\n", out);
    xl6OutputPuts("void* xrtConvCharset(void* sText, size_t iSize, int iInCP, int iOutCP, size_t* iRetSize);\n", out);
    xl6OutputPuts("bool xrtIsUTF8(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("int xrtDetectCharset(void* sText, size_t iSize, bool bBOM);\n", out);
    xl6OutputPuts("int xrtGetCharSize(int iCP);\n\n", out);

    xl6OutputPuts("static int xl6_encoding_charset(long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( charset < -2LL || charset > 2147483647LL ) {\n", out);
    xl6OutputPuts("        return XRT_CP_BINARY;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (int)charset;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_encoding_is_convertible(int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return charset >= 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static size_t xl6_encoding_units_from_bytes(size_t byteSize, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int charSize = xrtGetCharSize(charset);\n", out);
    xl6OutputPuts("    if ( charSize <= 0 ) {\n", out);
    xl6OutputPuts("        return byteSize;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( (byteSize % (size_t)charSize) != 0u ) {\n", out);
    xl6OutputPuts("        return (size_t)-1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return byteSize / (size_t)charSize;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static size_t xl6_encoding_units_len(const void* data, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int normalized = charset & XRT_MASK_BOM;\n", out);
    xl6OutputPuts("    size_t i = 0u;\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        return 0u;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( normalized == XRT_CP_UTF16 || normalized == XRT_CP_UTF16_BE ) {\n", out);
    xl6OutputPuts("        const unsigned short* p = (const unsigned short*)data;\n", out);
    xl6OutputPuts("        while ( p[i] != 0u ) { ++i; }\n", out);
    xl6OutputPuts("        return i;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( normalized == XRT_CP_UTF32 || normalized == XRT_CP_UTF32_BE ) {\n", out);
    xl6OutputPuts("        const unsigned int* p = (const unsigned int*)data;\n", out);
    xl6OutputPuts("        while ( p[i] != 0u ) { ++i; }\n", out);
    xl6OutputPuts("        return i;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return strlen((const char*)data);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static size_t xl6_encoding_byte_len(const void* data, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int charSize;\n", out);
    xl6OutputPuts("    size_t units;\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        return 0u;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    charSize = xrtGetCharSize(charset);\n", out);
    xl6OutputPuts("    if ( charSize <= 0 ) {\n", out);
    xl6OutputPuts("        charSize = 1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    units = xl6_encoding_units_len(data, charset);\n", out);
    xl6OutputPuts("    return units * (size_t)charSize;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xbuffer xl6_encoding_empty_buffer(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (xbuffer)xrtBufferCreate(0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xbuffer xl6_encoding_buffer_from_bytes(const void* data, size_t byteSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    if ( byteSize > 4294967295u ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    buf = xl6_encoding_empty_buffer();\n", out);
    xl6OutputPuts("    if ( buf == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( byteSize > 0u ) {\n", out);
    xl6OutputPuts("        if ( data == 0 || !xrtBufferAppend(buf, (void*)data, (unsigned int)byteSize, 0u) ) {\n", out);
    xl6OutputPuts("            xrtBufferDestroy(buf);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return buf;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_encoding_char_size(long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtGetCharSize(xl6_encoding_charset(charset));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_encoding_is_utf8_text(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        text = \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtIsUTF8(text, strlen(text));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_encoding_is_utf8_bytes(void* data, size_t byteSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        return byteSize == 0u;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtIsUTF8((const char*)data, byteSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_encoding_detect_text(const char* text, bool withBom)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        text = \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (long long)xrtDetectCharset((void*)text, strlen(text), withBom);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_encoding_detect_bytes(void* data, size_t byteSize, bool withBom)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        return XRT_CP_BINARY;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (long long)xrtDetectCharset((void*)data, byteSize, withBom);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_encoding_convert_bytes(void* data, size_t byteSize, long long fromCharset, long long toCharset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int fromCP = xl6_encoding_charset(fromCharset);\n", out);
    xl6OutputPuts("    int toCP = xl6_encoding_charset(toCharset);\n", out);
    xl6OutputPuts("    size_t inputUnits;\n", out);
    xl6OutputPuts("    void* converted;\n", out);
    xl6OutputPuts("    size_t outputBytes;\n", out);
    xl6OutputPuts("    xbuffer buf;\n", out);
    xl6OutputPuts("    if ( byteSize == 0u ) {\n", out);
    xl6OutputPuts("        return xl6_encoding_empty_buffer();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( data == 0 || !xl6_encoding_is_convertible(fromCP) || !xl6_encoding_is_convertible(toCP) ) {\n", out);
    xl6OutputPuts("        return xl6_encoding_empty_buffer();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    inputUnits = xl6_encoding_units_from_bytes(byteSize, fromCP);\n", out);
    xl6OutputPuts("    if ( inputUnits == (size_t)-1 ) {\n", out);
    xl6OutputPuts("        return xl6_encoding_empty_buffer();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    converted = xrtConvCharset((void*)data, inputUnits, fromCP, toCP, 0);\n", out);
    xl6OutputPuts("    if ( converted == 0 ) {\n", out);
    xl6OutputPuts("        return xl6_encoding_empty_buffer();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    outputBytes = xl6_encoding_byte_len(converted, toCP);\n", out);
    xl6OutputPuts("    buf = xl6_encoding_buffer_from_bytes(converted, outputBytes);\n", out);
    xl6OutputPuts("    xrtFree(converted);\n", out);
    xl6OutputPuts("    return buf;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_encoding_to_bytes(const char* text, long long toCharset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        text = \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_xrt_encoding_convert_bytes((void*)text, strlen(text), XRT_CP_UTF8, toCharset);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_encoding_from_bytes(void* data, size_t byteSize, long long fromCharset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int fromCP = xl6_encoding_charset(fromCharset);\n", out);
    xl6OutputPuts("    size_t inputUnits;\n", out);
    xl6OutputPuts("    void* converted;\n", out);
    xl6OutputPuts("    if ( byteSize == 0u ) {\n", out);
    xl6OutputPuts("        return xrtCopyStr(\"\", 0u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( data == 0 || !xl6_encoding_is_convertible(fromCP) ) {\n", out);
    xl6OutputPuts("        return xrtCopyStr(\"\", 0u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    inputUnits = xl6_encoding_units_from_bytes(byteSize, fromCP);\n", out);
    xl6OutputPuts("    if ( inputUnits == (size_t)-1 ) {\n", out);
    xl6OutputPuts("        return xrtCopyStr(\"\", 0u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    converted = xrtConvCharset((void*)data, inputUnits, fromCP, XRT_CP_UTF8, 0);\n", out);
    xl6OutputPuts("    return converted != 0 ? converted : xrtCopyStr(\"\", 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_encoding_name(long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int cp = xl6_encoding_charset(charset);\n", out);
    xl6OutputPuts("    if ( cp == XRT_CP_AUTO ) { return xrtCopyStr(\"auto\", 0u); }\n", out);
    xl6OutputPuts("    if ( cp == XRT_CP_BINARY ) { return xrtCopyStr(\"binary\", 0u); }\n", out);
    xl6OutputPuts("    switch ( cp & XRT_MASK_BOM ) {\n", out);
    xl6OutputPuts("    case XRT_CP_OEM: return xrtCopyStr(\"oem\", 0u);\n", out);
    xl6OutputPuts("    case XRT_CP_UTF8: return xrtCopyStr(\"utf-8\", 0u);\n", out);
    xl6OutputPuts("    case XRT_CP_UTF16: return xrtCopyStr(\"utf-16\", 0u);\n", out);
    xl6OutputPuts("    case XRT_CP_UTF16_BE: return xrtCopyStr(\"utf-16be\", 0u);\n", out);
    xl6OutputPuts("    case XRT_CP_UTF32: return xrtCopyStr(\"utf-32\", 0u);\n", out);
    xl6OutputPuts("    case XRT_CP_UTF32_BE: return xrtCopyStr(\"utf-32be\", 0u);\n", out);
    xl6OutputPuts("    default: return xrtCopyStr(\"unknown\", 0u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
