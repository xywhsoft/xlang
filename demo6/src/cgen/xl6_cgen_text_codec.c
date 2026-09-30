void xl6CgenEmitBase64TextCodecRuntime(XL6_Output* out, int hasXrtDeclarations)
{
    if ( !hasXrtDeclarations )
        xl6OutputPuts(
            "#ifndef XL6_CGEN_BASE64_CONFIG_DECL\n#define XL6_CGEN_BASE64_CONFIG_DECL\n"
            "typedef struct xbase64config { const char* Alphabet; uint32_t Flags; } xbase64config;\n#endif\n"
            "extern char* xrtBase64EncodeNew(const void*, size_t, const xbase64config*);\n"
            "extern unsigned char* xrtBase64DecodeNew(const char*, size_t, size_t*, const xbase64config*);\n",
            out);
    xl6OutputPuts(
        "extern size_t strlen(const char*);\n"
        "xlir_string xl6_xrt_base64_encode_text(xlir_string text, size_t size, void* table)\n"
        "{\n"
        "    xbase64config config; const xbase64config* selected = NULL;\n"
        "    char* output;\n"
        "    if (size > text.Size) size = text.Size;\n"
        "    if (table != NULL) { config.Alphabet = (const char*)table; config.Flags = 0u; selected = &config; }\n"
        "    output = xrtBase64EncodeNew(text.Data, size, selected);\n"
        "    return xlir_string_take_known(output, output != NULL ? strlen(output) : 0u);\n"
        "}\n"
        "xlir_string xl6_xrt_base64_decode_text(xlir_string text, size_t size, void* table)\n"
        "{\n"
        "    xbase64config config; const xbase64config* selected = NULL; size_t outputSize = 0u; unsigned char* output;\n"
        "    if (size > text.Size) size = text.Size;\n"
        "    if (table != NULL) { config.Alphabet = (const char*)table; config.Flags = 0u; selected = &config; }\n"
        "    output = xrtBase64DecodeNew(text.Data, size, &outputSize, selected);\n"
        "    return xlir_string_take_known((char*)output, outputSize);\n"
        "}\n\n",
        out);
}

void xl6CgenEmitHexBytesCodecRuntime(XL6_Output* out, int hasXrtDeclarations)
{
    if ( !hasXrtDeclarations )
        xl6OutputPuts(
            "typedef struct xbuffer xbuffer;\n"
            "extern char* xrtHexEncodeNew(const void*, size_t, uint32_t);\n"
            "extern xbuffer* xrtBufferFromHex(xstrview, uint32_t);\n", out);
    xl6OutputPuts(
        "extern size_t strlen(const char*);\n"
        "void* xl6_xrt_hex_encode_bytes(void* data, size_t size)\n"
        "{ if (data == NULL && size != 0u) return NULL; return xrtHexEncodeNew(data, size, 1u); }\n"
        "void* xl6_xrt_buffer_from_hex(xlir_string text, size_t size)\n"
        "{ if (size > text.Size) size = text.Size; return xrtBufferFromHex((xstrview){ text.Data, size }, 0u); }\n\n", out);
}

void xl6CgenEmitBase64BytesCodecRuntime(XL6_Output* out, int hasXrtDeclarations)
{
    if ( !hasXrtDeclarations )
        xl6OutputPuts(
            "#ifndef XL6_CGEN_BASE64_CONFIG_DECL\n#define XL6_CGEN_BASE64_CONFIG_DECL\n"
            "typedef struct xbase64config { const char* Alphabet; uint32_t Flags; } xbase64config;\n#endif\n"
            "typedef struct xbuffer xbuffer;\n"
            "extern char* xrtBase64EncodeNew(const void*, size_t, const xbase64config*);\n"
            "extern xbuffer* xrtBufferFromBase64(xstrview, const xbase64config*);\n", out);
    xl6OutputPuts(
        "extern size_t strlen(const char*);\n"
        "void* xl6_xrt_base64_encode_bytes(void* data, size_t size, void* table)\n"
        "{\n"
        "    xbase64config config; const xbase64config* selected = NULL;\n"
        "    if (data == NULL && size != 0u) return NULL;\n"
        "    if (table != NULL) { config.Alphabet = (const char*)table; config.Flags = 0u; selected = &config; }\n"
        "    return xrtBase64EncodeNew(data, size, selected);\n"
        "}\n"
        "void* xl6_xrt_buffer_from_base64(xlir_string text, size_t size, void* table)\n"
        "{\n"
        "    xbase64config config; const xbase64config* selected = NULL;\n"
        "    if (size > text.Size) size = text.Size;\n"
        "    if (table != NULL) { config.Alphabet = (const char*)table; config.Flags = 0u; selected = &config; }\n"
        "    return xrtBufferFromBase64((xstrview){ text.Data, size }, selected);\n"
        "}\n\n", out);
}
