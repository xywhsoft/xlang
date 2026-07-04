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
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n\n", out);

    xl6OutputPuts("static void* xl6_file_empty_result(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1u);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
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
    xl6OutputPuts("    if ( self == 0 ) { return xl6_file_empty_result(); }\n", out);
    xl6OutputPuts("    if ( size < 0 ) {\n", out);
    xl6OutputPuts("        size_t pos = xrtTell(self);\n", out);
    xl6OutputPuts("        size_t end = xrtGetEOF(self);\n", out);
    xl6OutputPuts("        readSize = end > pos ? end - pos : 0u;\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        readSize = (size_t)size;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( readSize == 0u ) { return xl6_file_empty_result(); }\n", out);
    xl6OutputPuts("    text = xrtRead(self, readSize, &retSize);\n", out);
    xl6OutputPuts("    return text != 0 ? text : xl6_file_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_write_text(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (long long)xrtWrite(self, text != 0 ? text : \"\", size);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
