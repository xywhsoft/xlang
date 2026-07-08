static void xl6CgenEmitFileRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_FILE_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_FILE_RUNTIME\n", out);
    xl6OutputPuts("void* xrtOpen(const char* sPath, int bReadOnly, int iCharset);\n", out);
    xl6OutputPuts("void xrtClose(void* objFile);\n", out);
    xl6OutputPuts("size_t xrtSeek(void* objFile, long long iOffset, int iMoveMethod);\n", out);
    xl6OutputPuts("size_t xrtTell(void* objFile);\n", out);
    xl6OutputPuts("size_t xrtGetEOF(void* objFile);\n", out);
    xl6OutputPuts("bool xrtEOF(void* objFile);\n", out);
    xl6OutputPuts("bool xrtSetEOF(void* objFile);\n", out);
    xl6OutputPuts("char* xrtRead(void* objFile, size_t iSize, size_t* iRetSize);\n", out);
    xl6OutputPuts("size_t xrtWrite(void* objFile, const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("char* xrtFileReadAll(const char* sPath, int iCharset, size_t* pSize);\n", out);
    xl6OutputPuts("void* xrtFileGetAll(const char* sPath, size_t* pSize);\n", out);
    xl6OutputPuts("bool xrtFileExists(const char* sPath);\n", out);
    xl6OutputPuts("bool xrtIsUTF8(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n\n", out);

    xl6OutputPuts("static void* xl6_file_empty_result(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1u);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_file_text_has_nul(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < size; ++i ) {\n", out);
    xl6OutputPuts("        if ( text[i] == '\\0' ) { return 1; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_file_raw_has_text_bom(const unsigned char* data, size_t size)\n", out);
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

    xl6OutputPuts("static int xl6_file_raw_looks_binary(const char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return size > 0u && xl6_file_text_has_nul(data, size) && !xl6_file_raw_has_text_bom((const unsigned char*)data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static char* xl6_file_require_text(char* text, size_t size, const char* api)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"FileError\", \"file text read failed\", api != 0 ? api : \"\", 0, 0);\n", out);
    xl6OutputPuts("        return (char*)xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && xl6_file_text_has_nul(text, size) ) {\n", out);
    xl6OutputPuts("        xrtFree(text);\n", out);
    xl6OutputPuts("        xl6_error_raise(\"FileError\", \"file text contains NUL byte; use binary/Buffer API\", api != 0 ? api : \"\", 0, 0);\n", out);
    xl6OutputPuts("        return (char*)xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && !xrtIsUTF8(text, size) ) {\n", out);
    xl6OutputPuts("        xrtFree(text);\n", out);
    xl6OutputPuts("        xl6_error_raise(\"FileError\", \"file text is not valid UTF-8 after charset conversion\", api != 0 ? api : \"\", 0, 0);\n", out);
    xl6OutputPuts("        return (char*)xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_read_text_strict(const char* path, int charset, void* retSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* safePath = path != 0 ? path : \"\";\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( retSize != 0 ) { *((size_t*)retSize) = 0u; }\n", out);
    xl6OutputPuts("    if ( safePath[0] == '\\0' || !xrtFileExists(safePath) ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"FileError\", \"file does not exist\", \"file.readText\", 0, 0);\n", out);
    xl6OutputPuts("        return xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( charset == -2 ) {\n", out);
    xl6OutputPuts("        size_t rawSize = 0u;\n", out);
    xl6OutputPuts("        char* raw = (char*)xrtFileGetAll(safePath, &rawSize);\n", out);
    xl6OutputPuts("        if ( rawSize > 0u && raw != 0 ) {\n", out);
    xl6OutputPuts("            if ( xl6_file_raw_looks_binary(raw, rawSize) ) {\n", out);
    xl6OutputPuts("                xrtFree(raw);\n", out);
    xl6OutputPuts("                xl6_error_raise(\"FileError\", \"file text contains NUL byte; use binary/Buffer API\", \"file.readText\", 0, 0);\n", out);
    xl6OutputPuts("                return xl6_file_empty_result();\n", out);
    xl6OutputPuts("            }\n", out);
    xl6OutputPuts("            xrtFree(raw);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = xrtFileReadAll(safePath, charset, &size);\n", out);
    xl6OutputPuts("    if ( retSize != 0 ) { *((size_t*)retSize) = size; }\n", out);
    xl6OutputPuts("    return xl6_file_require_text(text, size, \"file.readText\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_open(const char* path, int readOnly, long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtOpen(path != 0 ? path : \"\", readOnly != 0, (int)charset);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_close(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtClose(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_seek(void* self, long long offset, long long whence)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (long long)xrtSeek(self, offset, (int)whence);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_tell(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)xrtTell(self) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_size(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)xrtGetEOF(self) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_file_eof(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self == 0 || xrtEOF(self) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_file_set_eof(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 && xrtSetEOF(self) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_read_text(void* self, long long size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t readSize;\n", out);
    xl6OutputPuts("    size_t retSize = 0u;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( self == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"FileError\", \"invalid file handle\", \"File.readText\", 0, 0);\n", out);
    xl6OutputPuts("        return xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size < 0 ) {\n", out);
    xl6OutputPuts("        size_t pos = xrtTell(self);\n", out);
    xl6OutputPuts("        size_t end = xrtGetEOF(self);\n", out);
    xl6OutputPuts("        readSize = end > pos ? end - pos : 0u;\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        readSize = (size_t)size;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( readSize == 0u ) { return xl6_file_empty_result(); }\n", out);
    xl6OutputPuts("    text = xrtRead(self, readSize, &retSize);\n", out);
    xl6OutputPuts("    return xl6_file_require_text(text, retSize, \"File.readText\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_write_text(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (long long)xrtWrite(self, text != 0 ? text : \"\", size);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
