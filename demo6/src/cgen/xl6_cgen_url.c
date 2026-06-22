static void xl6CgenEmitUrlRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_URL_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_URL_RUNTIME\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("#define XL6_URL_F_ABSOLUTE 0x00000001u\n", out);
    xl6OutputPuts("#define XL6_URL_F_SECURE 0x00000200u\n", out);
    xl6OutputPuts("#ifndef XL6_XRT_URL_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XRT_URL_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct { const char* sPtr; size_t iLen; } xrtstrview;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    uint16_t iPort;\n", out);
    xl6OutputPuts("    xrtstrview tScheme;\n", out);
    xl6OutputPuts("    xrtstrview tAuthority;\n", out);
    xl6OutputPuts("    xrtstrview tUserInfo;\n", out);
    xl6OutputPuts("    xrtstrview tHost;\n", out);
    xl6OutputPuts("    xrtstrview tPath;\n", out);
    xl6OutputPuts("    xrtstrview tQuery;\n", out);
    xl6OutputPuts("    xrtstrview tFragment;\n", out);
    xl6OutputPuts("    xrtstrview tTarget;\n", out);
    xl6OutputPuts("} xrturlview;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut);\n", out);
    xl6OutputPuts("bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen);\n", out);
    xl6OutputPuts("bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);\n\n", out);

    xl6OutputPuts("static void* xl6_url_empty_string(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_url_copy_text(const char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( data == 0 || size == 0 ) { return xl6_url_empty_string(); }\n", out);
    xl6OutputPuts("    text = (char*)xrtMalloc(size + 1);\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memcpy(text, data, size);\n", out);
    xl6OutputPuts("    text[size] = '\\0';\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_url_parse_view(const char* text, size_t size, xrturlview* out)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0; }\n", out);
    xl6OutputPuts("    return xrtUrlParseViewN(text, size, out);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_url_part(const char* text, size_t size, int part)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrturlview url;\n", out);
    xl6OutputPuts("    xrtstrview view;\n", out);
    xl6OutputPuts("    if ( !xl6_url_parse_view(text, size, &url) ) { return xl6_url_empty_string(); }\n", out);
    xl6OutputPuts("    view.sPtr = 0;\n", out);
    xl6OutputPuts("    view.iLen = 0;\n", out);
    xl6OutputPuts("    switch ( part ) {\n", out);
    xl6OutputPuts("    case 1: view = url.tScheme; break;\n", out);
    xl6OutputPuts("    case 2: view = url.tAuthority; break;\n", out);
    xl6OutputPuts("    case 3: view = url.tUserInfo; break;\n", out);
    xl6OutputPuts("    case 4: view = url.tHost; break;\n", out);
    xl6OutputPuts("    case 5: view = url.tPath; break;\n", out);
    xl6OutputPuts("    case 6: view = url.tQuery; break;\n", out);
    xl6OutputPuts("    case 7: view = url.tFragment; break;\n", out);
    xl6OutputPuts("    case 8: view = url.tTarget; break;\n", out);
    xl6OutputPuts("    default: break;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xl6_url_copy_text(view.sPtr, view.iLen);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_url_flag(const char* text, size_t size, int flag)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrturlview url;\n", out);
    xl6OutputPuts("    if ( !xl6_url_parse_view(text, size, &url) ) { return false; }\n", out);
    xl6OutputPuts("    if ( flag == 0 ) { return true; }\n", out);
    xl6OutputPuts("    if ( flag == 1 ) { return (url.iFlags & XL6_URL_F_ABSOLUTE) != 0u; }\n", out);
    xl6OutputPuts("    if ( flag == 2 ) { return (url.iFlags & XL6_URL_F_SECURE) != 0u; }\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_url_port(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrturlview url;\n", out);
    xl6OutputPuts("    if ( !xl6_url_parse_view(text, size, &url) ) { return 0; }\n", out);
    xl6OutputPuts("    return (int)url.iPort;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_url_resolve(const char* base, size_t baseSize, const char* ref, size_t refSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrturlview baseUrl;\n", out);
    xl6OutputPuts("    char buffer[8192];\n", out);
    xl6OutputPuts("    size_t outLen = 0;\n", out);
    xl6OutputPuts("    if ( !xl6_url_parse_view(base, baseSize, &baseUrl) ) { return xl6_url_empty_string(); }\n", out);
    xl6OutputPuts("    if ( ref == 0 ) { ref = \"\"; refSize = 0; }\n", out);
    xl6OutputPuts("    if ( !xrtUrlResolveTo(&baseUrl, ref, refSize, buffer, sizeof(buffer), &outLen) ) { return xl6_url_empty_string(); }\n", out);
    xl6OutputPuts("    return xl6_url_copy_text(buffer, outLen);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_url_normalize_path(const char* path, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char buffer[8192];\n", out);
    xl6OutputPuts("    size_t outLen = 0;\n", out);
    xl6OutputPuts("    if ( path == 0 ) { path = \"\"; size = 0; }\n", out);
    xl6OutputPuts("    if ( !xrtUrlNormalizePathTo(path, size, buffer, sizeof(buffer), &outLen) ) { return xl6_url_empty_string(); }\n", out);
    xl6OutputPuts("    return xl6_url_copy_text(buffer, outLen);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
