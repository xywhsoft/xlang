/* 发射 xlang 编码标准库与当前 XRT Unicode 编码契约之间的桥接。 */
static void xl6CgenEmitEncodingRuntime(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_ENCODING_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_ENCODING_RUNTIME\n", out);
    xl6OutputPuts("#include <string.h>\n", out);

    xl6OutputPuts("typedef xbytesview XL6_XrtBytesView;\n", out);
    xl6OutputPuts("typedef xencodingguess XL6_XrtEncodingGuess;\n", out);

    xl6OutputPuts("#ifndef XL6_CP_AUTO\n", out);
    xl6OutputPuts("#define XL6_CP_AUTO -2\n", out);
    xl6OutputPuts("#define XL6_CP_BINARY -1\n", out);
    xl6OutputPuts("#define XL6_CP_OEM 0\n", out);
    xl6OutputPuts("#define XL6_CP_UTF8 65001\n", out);
    xl6OutputPuts("#define XL6_CP_UTF16 1200\n", out);
    xl6OutputPuts("#define XL6_CP_UTF16_BE 1201\n", out);
    xl6OutputPuts("#define XL6_CP_UTF32 65005\n", out);
    xl6OutputPuts("#define XL6_CP_UTF32_BE 65006\n", out);
    xl6OutputPuts("#define XL6_CP_BOM 0x40000000\n", out);
    xl6OutputPuts("#define XL6_MASK_BOM 0xBFFFFFFF\n", out);
    xl6OutputPuts("#ifndef XL6_XENCODING_UNKNOWN\n", out);
    xl6OutputPuts("#define XL6_XENCODING_UNKNOWN XENCODING_UNKNOWN\n", out);
    xl6OutputPuts("#define XL6_XENCODING_UTF8 XENCODING_UTF8\n", out);
    xl6OutputPuts("#define XL6_XENCODING_UTF16_LE XENCODING_UTF16_LE\n", out);
    xl6OutputPuts("#define XL6_XENCODING_UTF16_BE XENCODING_UTF16_BE\n", out);
    xl6OutputPuts("#define XL6_XENCODING_UTF32_LE XENCODING_UTF32_LE\n", out);
    xl6OutputPuts("#define XL6_XENCODING_UTF32_BE XENCODING_UTF32_BE\n", out);
    xl6OutputPuts("#define XL6_XUTF_STRICT XUTF_STRICT\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("static inline char* xl6_xrt_string_dup(const char* text, size_t size);\n", out);
    xl6OutputPuts("\n", out);

    xl6OutputPuts("/* 将语言层的历史数值常量限制到稳定的 Unicode 编码集合。 */\n", out);
    xl6OutputPuts("static int xl6_encoding_charset(long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( (charset < -2LL) || (charset > 2147483647LL) ) { return XL6_CP_BINARY; }\n", out);
    xl6OutputPuts("    return (int)charset;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 把语言层编码常量映射为 XRT xencoding。 */\n", out);
    xl6OutputPuts("static int xl6_encoding_to_xrt(int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    switch ( charset & XL6_MASK_BOM ) {\n", out);
    xl6OutputPuts("    case XL6_CP_UTF8: return XL6_XENCODING_UTF8;\n", out);
    xl6OutputPuts("    case XL6_CP_UTF16: return XL6_XENCODING_UTF16_LE;\n", out);
    xl6OutputPuts("    case XL6_CP_UTF16_BE: return XL6_XENCODING_UTF16_BE;\n", out);
    xl6OutputPuts("    case XL6_CP_UTF32: return XL6_XENCODING_UTF32_LE;\n", out);
    xl6OutputPuts("    case XL6_CP_UTF32_BE: return XL6_XENCODING_UTF32_BE;\n", out);
    xl6OutputPuts("    default: return XL6_XENCODING_UNKNOWN;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 把 XRT 检测结果映射回语言层稳定常量。 */\n", out);
    xl6OutputPuts("static int xl6_encoding_from_xrt(int encoding)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    switch ( encoding ) {\n", out);
    xl6OutputPuts("    case XL6_XENCODING_UTF8: return XL6_CP_UTF8;\n", out);
    xl6OutputPuts("    case XL6_XENCODING_UTF16_LE: return XL6_CP_UTF16;\n", out);
    xl6OutputPuts("    case XL6_XENCODING_UTF16_BE: return XL6_CP_UTF16_BE;\n", out);
    xl6OutputPuts("    case XL6_XENCODING_UTF32_LE: return XL6_CP_UTF32;\n", out);
    xl6OutputPuts("    case XL6_XENCODING_UTF32_BE: return XL6_CP_UTF32_BE;\n", out);
    xl6OutputPuts("    default: return XL6_CP_OEM;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 创建拥有内容副本的 Buffer。 */\n", out);
    xl6OutputPuts("static xbuffer* xl6_encoding_buffer_from_bytes(const void* data, size_t byteSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer* buffer = xrtBufferCreate();\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( (byteSize != 0u) && ((data == 0) || !xrtBufferAppend(buffer, (XL6_XrtBytesView){ (const unsigned char*)data, byteSize })) ) {\n", out);
    xl6OutputPuts("        xrtBufferDestroy(buffer);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return buffer;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 根据显式标志或自动检测结果确定源编码，并去掉输入 BOM。 */\n", out);
    xl6OutputPuts("static int xl6_encoding_source(XL6_XrtBytesView* source, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int encoding;\n", out);
    xl6OutputPuts("    size_t bomSize = 0u;\n", out);
    xl6OutputPuts("    if ( charset == XL6_CP_AUTO ) {\n", out);
    xl6OutputPuts("        XL6_XrtEncodingGuess guess = xrtEncodingGuess(*source);\n", out);
    xl6OutputPuts("        encoding = guess.Encoding;\n", out);
    xl6OutputPuts("        bomSize = guess.BomSize;\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        encoding = xl6_encoding_to_xrt(charset);\n", out);
    xl6OutputPuts("        if ( (charset & XL6_CP_BOM) != 0 ) {\n", out);
    xl6OutputPuts("            int bomEncoding = xrtEncodingBom(*source, &bomSize);\n", out);
    xl6OutputPuts("            if ( bomEncoding != encoding ) { bomSize = 0u; }\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( (bomSize != 0u) && (bomSize <= source->Size) ) { source->Data += bomSize; source->Size -= bomSize; }\n", out);
    xl6OutputPuts("    return encoding;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 返回编码码元的固定字节数。 */\n", out);
    xl6OutputPuts("long long xl6_xrt_encoding_char_size(long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtEncodingUnitSize(xl6_encoding_to_xrt(xl6_encoding_charset(charset)));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 校验零结尾 UTF-8 文本。 */\n", out);
    xl6OutputPuts("bool xl6_xrt_encoding_is_utf8_text(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    return xl6_xrt_utf8_valid(text, strlen(text));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 校验显式长度的 UTF-8 字节。 */\n", out);
    xl6OutputPuts("bool xl6_xrt_encoding_is_utf8_bytes(void* data, size_t byteSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( data == 0 ) { return byteSize == 0u; }\n", out);
    xl6OutputPuts("    return xl6_xrt_utf8_valid(data, byteSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 检测字节编码，并按需保留 BOM 标志。 */\n", out);
    xl6OutputPuts("static long long xl6_encoding_detect(const void* data, size_t byteSize, bool withBom)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XrtEncodingGuess guess;\n", out);
    xl6OutputPuts("    int charset;\n", out);
    xl6OutputPuts("    if ( (data == 0) && (byteSize != 0u) ) { return XL6_CP_BINARY; }\n", out);
    xl6OutputPuts("    guess = xrtEncodingGuess((XL6_XrtBytesView){ (const unsigned char*)data, byteSize });\n", out);
    xl6OutputPuts("    charset = xl6_encoding_from_xrt(guess.Encoding);\n", out);
    xl6OutputPuts("    if ( withBom && (guess.BomSize != 0u) && (charset != XL6_CP_OEM) ) { charset |= XL6_CP_BOM; }\n", out);
    xl6OutputPuts("    return (long long)charset;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 检测零结尾文本的 Unicode 编码。 */\n", out);
    xl6OutputPuts("long long xl6_xrt_encoding_detect_text(const char* text, bool withBom)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    return xl6_encoding_detect(text, strlen(text), withBom);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 检测显式长度字节的 Unicode 编码。 */\n", out);
    xl6OutputPuts("long long xl6_xrt_encoding_detect_bytes(void* data, size_t byteSize, bool withBom)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_encoding_detect(data, byteSize, withBom);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 在受支持的 Unicode 编码之间转换并返回 Buffer。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_encoding_convert_bytes(void* data, size_t byteSize, long long fromCharset, long long toCharset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int fromRaw = xl6_encoding_charset(fromCharset);\n", out);
    xl6OutputPuts("    int toRaw = xl6_encoding_charset(toCharset);\n", out);
    xl6OutputPuts("    int toEncoding = xl6_encoding_to_xrt(toRaw);\n", out);
    xl6OutputPuts("    XL6_XrtBytesView source = { (const unsigned char*)data, byteSize };\n", out);
    xl6OutputPuts("    int fromEncoding;\n", out);
    xl6OutputPuts("    size_t outputSize = 0u;\n", out);
    xl6OutputPuts("    void* output;\n", out);
    xl6OutputPuts("    xbuffer* buffer;\n", out);
    xl6OutputPuts("    if ( (data == 0) && (byteSize != 0u) ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    fromEncoding = xl6_encoding_source(&source, fromRaw);\n", out);
    xl6OutputPuts("    if ( (fromEncoding == XL6_XENCODING_UNKNOWN) || (toEncoding == XL6_XENCODING_UNKNOWN) ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    output = xrtTranscode(source, fromEncoding, toEncoding, XL6_XUTF_STRICT, (toRaw & XL6_CP_BOM) != 0, &outputSize);\n", out);
    xl6OutputPuts("    if ( output == 0 ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    buffer = xl6_encoding_buffer_from_bytes(output, outputSize);\n", out);
    xl6OutputPuts("    xrtFree(output);\n", out);
    xl6OutputPuts("    return buffer;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 把 UTF-8 文本编码为目标 Unicode 字节序列。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_encoding_to_bytes(const char* text, long long toCharset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    return xl6_xrt_encoding_convert_bytes((void*)text, strlen(text), XL6_CP_UTF8, toCharset);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 把目标 Unicode 字节序列转换为 UTF-8 所有权文本。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_encoding_from_bytes(void* data, size_t byteSize, long long fromCharset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int fromRaw = xl6_encoding_charset(fromCharset);\n", out);
    xl6OutputPuts("    XL6_XrtBytesView source = { (const unsigned char*)data, byteSize };\n", out);
    xl6OutputPuts("    int fromEncoding;\n", out);
    xl6OutputPuts("    size_t outputSize = 0u;\n", out);
    xl6OutputPuts("    void* output;\n", out);
    xl6OutputPuts("    if ( (data == 0) && (byteSize != 0u) ) { return xl6_xrt_string_dup(\"\", 0u); }\n", out);
    xl6OutputPuts("    fromEncoding = xl6_encoding_source(&source, fromRaw);\n", out);
    xl6OutputPuts("    if ( fromEncoding == XL6_XENCODING_UNKNOWN ) { return xl6_xrt_string_dup(\"\", 0u); }\n", out);
    xl6OutputPuts("    output = xrtTranscode(source, fromEncoding, XL6_XENCODING_UTF8, XL6_XUTF_STRICT, false, &outputSize);\n", out);
    xl6OutputPuts("    return output != 0 ? output : xl6_xrt_string_dup(\"\", 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 返回语言层编码常量的稳定显示名称。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_encoding_name(long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int cp = xl6_encoding_charset(charset);\n", out);
    xl6OutputPuts("    if ( cp == XL6_CP_AUTO ) { return xl6_xrt_string_dup(\"auto\", 0u); }\n", out);
    xl6OutputPuts("    if ( cp == XL6_CP_BINARY ) { return xl6_xrt_string_dup(\"binary\", 0u); }\n", out);
    xl6OutputPuts("    switch ( cp & XL6_MASK_BOM ) {\n", out);
    xl6OutputPuts("    case XL6_CP_OEM: return xl6_xrt_string_dup(\"oem\", 0u);\n", out);
    xl6OutputPuts("    case XL6_CP_UTF8: return xl6_xrt_string_dup(\"utf-8\", 0u);\n", out);
    xl6OutputPuts("    case XL6_CP_UTF16: return xl6_xrt_string_dup(\"utf-16\", 0u);\n", out);
    xl6OutputPuts("    case XL6_CP_UTF16_BE: return xl6_xrt_string_dup(\"utf-16be\", 0u);\n", out);
    xl6OutputPuts("    case XL6_CP_UTF32: return xl6_xrt_string_dup(\"utf-32\", 0u);\n", out);
    xl6OutputPuts("    case XL6_CP_UTF32_BE: return xl6_xrt_string_dup(\"utf-32be\", 0u);\n", out);
    xl6OutputPuts("    default: return xl6_xrt_string_dup(\"unknown\", 0u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 复制并交换每个 UTF-16 码元的字节序。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_encoding_swap_utf16(void* data, size_t byteSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer* buffer;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( ((data == 0) && (byteSize != 0u)) || ((byteSize & 1u) != 0u) ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    buffer = xl6_encoding_buffer_from_bytes(data, byteSize);\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < byteSize; i += 2u ) { unsigned char value = buffer->Data[i]; buffer->Data[i] = buffer->Data[i + 1u]; buffer->Data[i + 1u] = value; }\n", out);
    xl6OutputPuts("    return buffer;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 复制并反转每个 UTF-32 码元的四个字节。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_encoding_swap_utf32(void* data, size_t byteSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer* buffer;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( ((data == 0) && (byteSize != 0u)) || ((byteSize & 3u) != 0u) ) { return xrtBufferCreate(); }\n", out);
    xl6OutputPuts("    buffer = xl6_encoding_buffer_from_bytes(data, byteSize);\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < byteSize; i += 4u ) {\n", out);
    xl6OutputPuts("        unsigned char first = buffer->Data[i];\n", out);
    xl6OutputPuts("        unsigned char second = buffer->Data[i + 1u];\n", out);
    xl6OutputPuts("        buffer->Data[i] = buffer->Data[i + 3u];\n", out);
    xl6OutputPuts("        buffer->Data[i + 1u] = buffer->Data[i + 2u];\n", out);
    xl6OutputPuts("        buffer->Data[i + 2u] = second;\n", out);
    xl6OutputPuts("        buffer->Data[i + 3u] = first;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return buffer;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
