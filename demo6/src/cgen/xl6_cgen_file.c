/* 生成基于当前 XRT 文件、整文件和文本编码契约的运行时桥接。 */
void xl6CgenEmitFileRuntime(
    XL6_Output* out, int xlirAbi, int hasXrtDeclarations)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_FILE_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_FILE_RUNTIME\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts(xlirAbi
        ? "#ifndef XL6_CGEN_BOOL_TYPEDEF\n#define XL6_CGEN_BOOL_TYPEDEF\ntypedef bool XL6_CgenBool;\n#endif\n"
        : "#ifndef XL6_CGEN_BOOL_TYPEDEF\n#define XL6_CGEN_BOOL_TYPEDEF\ntypedef int XL6_CgenBool;\n#endif\n", out);
    if ( xlirAbi && !hasXrtDeclarations ) {
        /* XLIR output imports no monolithic runtime header.  These declarations
         * are the exact public XRT surface consumed by this component. */
        xl6OutputPuts(
            "#ifndef XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\n#define XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\ntypedef struct XL6_XrtBytesView { const unsigned char* Data; size_t Size; } XL6_XrtBytesView;\n#endif\n"
            "typedef xstrview XL6_XrtStrView;\n"
            "typedef struct XL6_XFileInfo { int Type; uint32_t Available; uint32_t Mode; uint32_t Attributes; uint64_t Size; uint64_t Device; uint64_t Identity; uint64_t LinkCount; int64_t Accessed; int64_t Modified; int64_t Created; int64_t Changed; } XL6_XFileInfo;\n"
            "typedef struct xfile_impl* XL6_XFile;\n"
            "extern void* xrtMalloc(size_t);\n"
            "extern void xrtFree(void*);\n"
            "extern bool xrtUtf8Valid(xstrview, size_t*);\n"
            "extern unsigned char* xrtTranscode(XL6_XrtBytesView, int, int, int, bool, size_t*);\n"
            "extern XL6_XFile xrtOpen(const char*, uint32_t);\n"
            "extern bool xrtClose(XL6_XFile);\n"
            "extern bool xrtRead(XL6_XFile, void*, size_t, size_t*);\n"
            "extern bool xrtWriteFull(XL6_XFile, const void*, size_t, size_t*);\n"
            "extern bool xrtSeek(XL6_XFile, int64_t, int, uint64_t*);\n"
            "extern bool xrtTell(XL6_XFile, uint64_t*);\n"
            "extern bool xrtFileSize(XL6_XFile, uint64_t*);\n"
            "extern bool xrtFileResize(XL6_XFile, uint64_t);\n"
            "extern bool xrtFileExists(const char*);\n"
            "extern bool xrtFileDelete(const char*);\n"
            "extern void xrtClearError(void);\n"
            "extern unsigned char* xrtFileReadAll(const char*, size_t*);\n"
            "extern bool xrtFileWriteAll(const char*, XL6_XrtBytesView);\n"
            "extern bool xrtFileAppend(const char*, XL6_XrtBytesView);\n"
            "extern char* xrtFileReadText(const char*, int, int, size_t*);\n"
            "extern bool xrtFileWriteText(const char*, XL6_XrtStrView, int, int, bool);\n"
            "extern bool xrtFileWriteTextAtomic(const char*, XL6_XrtStrView, int, int, bool);\n"
            "extern bool xrtPathStat(const char*, bool, XL6_XFileInfo*);\n"
            "extern bool xrtPathSetAttributes(const char*, uint32_t);\n"
            "extern bool xrtPathRename(const char*, const char*, bool);\n",
            out);
        xl6OutputPuts("#define XL6_XFILE_READ 1u\n", out);
        xl6OutputPuts("#define XL6_XFILE_WRITE 2u\n", out);
        xl6OutputPuts("#define XL6_XFILE_CREATE 4u\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UNKNOWN 0\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF8 1\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF16_LE 2\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF16_BE 3\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF32_LE 4\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF32_BE 5\n", out);
        xl6OutputPuts("#define XL6_XUTF_STRICT 0\n", out);
    } else {
        /* Legacy output and declaration-SDK-backed XLIR output already import
         * the public XRT ABI. */
        xl6OutputPuts("#define XL6_XFILE_READ XFILE_READ\n", out);
        xl6OutputPuts("#define XL6_XFILE_WRITE XFILE_WRITE\n", out);
        xl6OutputPuts("#define XL6_XFILE_CREATE XFILE_CREATE\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UNKNOWN XENCODING_UNKNOWN\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF8 XENCODING_UTF8\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF16_LE XENCODING_UTF16_LE\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF16_BE XENCODING_UTF16_BE\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF32_LE XENCODING_UTF32_LE\n", out);
        xl6OutputPuts("#define XL6_XENCODING_UTF32_BE XENCODING_UTF32_BE\n", out);
        xl6OutputPuts("#define XL6_XUTF_STRICT XUTF_STRICT\n", out);
        xl6OutputPuts("#ifndef XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\n#define XL6_CGEN_XRT_BYTES_VIEW_TYPEDEF\ntypedef xbytesview XL6_XrtBytesView;\n#endif\n", out);
        xl6OutputPuts("typedef xstrview XL6_XrtStrView;\n", out);
        xl6OutputPuts("typedef xfileinfo XL6_XFileInfo;\n", out);
        xl6OutputPuts("typedef xfile XL6_XFile;\n", out);
    }
    xl6OutputPuts("#define XL6_XRT_FILE_ABI 1\n", out);
    xl6OutputPuts("#ifndef XL6_FILE_HANDLE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_FILE_HANDLE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct XL6_FileHandle { XL6_XFile file; int charset; } XL6_FileHandle;\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("static void xl6_file_raise(const char* kind, const char* message, const char* api)\n", out);
    xl6OutputPuts("{\n", out);
    if ( xlirAbi )
        xl6OutputPuts("    (void)api; xrtSetErrorTake(xlir_error_create_at(kind != 0 ? kind : \"FileError\", message != 0 ? message : \"file error\", 0, 0));\n", out);
    else
        xl6OutputPuts("    xl6_error_raise(kind != 0 ? kind : \"FileError\", message != 0 ? message : \"file error\", api != 0 ? api : \"File\", 0, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_file_utf8_valid(const char* text, size_t size)\n", out);
    xl6OutputPuts("{ return xrtUtf8Valid((xstrview){ text, size }, 0) ? 1 : 0; }\n\n", out);

    /* The language-level file.delete contract is a no-raise status query.
     * XRT deliberately records the native failure details in its thread-local
     * error slot, so this adapter consumes that diagnostic before returning
     * false.  The behavior is now a verified File bridge, not a Cgen guess. */
    xl6OutputPuts("bool xl6_xrt_file_delete_status(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    bool result = xrtFileDelete(path);\n", out);
    xl6OutputPuts("    if ( !result ) { xrtClearError(); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_file_encoding(int charset, bool writing, bool* writeBom)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int normalized = charset;\n", out);
    xl6OutputPuts("    if ( writeBom != 0 ) { *writeBom = (charset >= 0 && (charset & 0x40000000) != 0); }\n", out);
    xl6OutputPuts("    if ( charset >= 0 ) { normalized = charset & 0xBFFFFFFF; }\n", out);
    xl6OutputPuts("    switch ( normalized ) {\n", out);
    xl6OutputPuts("    case -2: return writing ? XL6_XENCODING_UTF8 : XL6_XENCODING_UNKNOWN;\n", out);
    xl6OutputPuts("    case 65001: return XL6_XENCODING_UTF8;\n", out);
    xl6OutputPuts("    case 1200: return XL6_XENCODING_UTF16_LE;\n", out);
    xl6OutputPuts("    case 1201: return XL6_XENCODING_UTF16_BE;\n", out);
    xl6OutputPuts("    case 65005: return XL6_XENCODING_UTF32_LE;\n", out);
    xl6OutputPuts("    case 65006: return XL6_XENCODING_UTF32_BE;\n", out);
    xl6OutputPuts("    default: return -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

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
    xl6OutputPuts("    for ( i = 0u; i < size; ++i ) { if ( text[i] == '\\0' ) { return 1; } }\n", out);
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

    xl6OutputPuts("static char* xl6_file_require_text(char* text, size_t size, const char* api)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        xl6_file_raise(\"FileError\", \"file text read failed\", api != 0 ? api : \"\");\n", out);
    xl6OutputPuts("        return (char*)xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && xl6_file_text_has_nul(text, size) ) {\n", out);
    xl6OutputPuts("        xrtFree(text);\n", out);
    xl6OutputPuts("        xl6_file_raise(\"FileError\", \"file text contains NUL byte; use binary/Buffer API\", api != 0 ? api : \"\");\n", out);
    xl6OutputPuts("        return (char*)xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( size > 0u && !xl6_file_utf8_valid(text, size) ) {\n", out);
    xl6OutputPuts("        xrtFree(text);\n", out);
    xl6OutputPuts("        xl6_file_raise(\"FileError\", \"file text is not valid UTF-8 after charset conversion\", api != 0 ? api : \"\");\n", out);
    xl6OutputPuts("        return (char*)xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_file_write_text(const char* path, const char* text, size_t size, int charset, bool atomic)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    bool writeBom = false;\n", out);
    xl6OutputPuts("    int encoding = xl6_file_encoding(charset, true, &writeBom);\n", out);
    xl6OutputPuts("    XL6_XrtStrView view;\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' || encoding < 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0u; } else if ( size == 0u ) { size = strlen(text); }\n", out);
    xl6OutputPuts("    view.Data = text;\n", out);
    xl6OutputPuts("    view.Size = size;\n", out);
    xl6OutputPuts("    return atomic ? xrtFileWriteTextAtomic(path, view, encoding, XL6_XUTF_STRICT, writeBom) : xrtFileWriteText(path, view, encoding, XL6_XUTF_STRICT, writeBom);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_file_write_text_all(const char* path, const char* text, size_t size, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_file_write_text(path, text, size, charset, false) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_file_write_text_atomic(const char* path, const char* text, size_t size, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_file_write_text(path, text, size, charset, true);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_file_append_text(const char* path, const char* text, size_t size, int charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    bool writeBom = false;\n", out);
    xl6OutputPuts("    int encoding = xl6_file_encoding(charset, true, &writeBom);\n", out);
    xl6OutputPuts("    XL6_XrtBytesView view;\n", out);
    xl6OutputPuts("    unsigned char* converted = 0;\n", out);
    xl6OutputPuts("    size_t convertedSize = 0u;\n", out);
    xl6OutputPuts("    bool result;\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' || encoding < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0u; } else if ( size == 0u ) { size = strlen(text); }\n", out);
    xl6OutputPuts("    view.Data = (const unsigned char*)text;\n", out);
    xl6OutputPuts("    view.Size = size;\n", out);
    xl6OutputPuts("    if ( encoding != XL6_XENCODING_UTF8 || writeBom ) {\n", out);
    xl6OutputPuts("        converted = xrtTranscode(view, XL6_XENCODING_UTF8, encoding, XL6_XUTF_STRICT, writeBom && !xrtFileExists(path), &convertedSize);\n", out);
    xl6OutputPuts("        if ( converted == 0 ) { return 0; }\n", out);
    xl6OutputPuts("        view.Data = converted;\n", out);
    xl6OutputPuts("        view.Size = convertedSize;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    result = xrtFileAppend(path, view);\n", out);
    xl6OutputPuts("    xrtFree(converted);\n", out);
    xl6OutputPuts("    return result ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_read_text_strict(const char* path, int charset, void* retSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* safePath = path != 0 ? path : \"\";\n", out);
    xl6OutputPuts("    int encoding = xl6_file_encoding(charset, false, 0);\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( retSize != 0 ) { *((size_t*)retSize) = 0u; }\n", out);
    xl6OutputPuts("    if ( safePath[0] == '\\0' || !xrtFileExists(safePath) ) {\n", out);
    xl6OutputPuts("        xl6_file_raise(\"FileError\", \"file does not exist\", \"file.readText\");\n", out);
    xl6OutputPuts("        return xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( encoding < 0 ) {\n", out);
    xl6OutputPuts("        xl6_file_raise(\"FileError\", \"unsupported text charset\", \"file.readText\");\n", out);
    xl6OutputPuts("        return xl6_file_empty_result();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( encoding == XL6_XENCODING_UNKNOWN ) {\n", out);
    xl6OutputPuts("        size_t rawSize = 0u;\n", out);
    xl6OutputPuts("        unsigned char* raw = xrtFileReadAll(safePath, &rawSize);\n", out);
    xl6OutputPuts("        if ( raw == 0 ) { return xl6_file_require_text(0, 0u, \"file.readText\"); }\n", out);
    xl6OutputPuts("        if ( rawSize > 0u && xl6_file_text_has_nul((const char*)raw, rawSize) && !xl6_file_raw_has_text_bom(raw, rawSize) ) {\n", out);
    xl6OutputPuts("            xrtFree(raw);\n", out);
    xl6OutputPuts("            xl6_file_raise(\"FileError\", \"file text contains NUL byte; use binary/Buffer API\", \"file.readText\");\n", out);
    xl6OutputPuts("            return xl6_file_empty_result();\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        xrtFree(raw);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = xrtFileReadText(safePath, encoding, XL6_XUTF_STRICT, &size);\n", out);
    xl6OutputPuts("    if ( retSize != 0 ) { *((size_t*)retSize) = size; }\n", out);
    xl6OutputPuts("    return xl6_file_require_text(text, size, \"file.readText\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("size_t xl6_xrt_file_get_size_path(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XFileInfo info;\n", out);
    xl6OutputPuts("    return path != 0 && xrtPathStat(path, true, &info) ? info.Size : 0u;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_file_get_attributes(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XFileInfo info;\n", out);
    xl6OutputPuts("    return path != 0 && xrtPathStat(path, true, &info) ? (int)info.Attributes : -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_file_set_attributes(const char* path, int attributes)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return path != 0 && xrtPathSetAttributes(path, (unsigned int)attributes);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_get_access_time(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XFileInfo info;\n", out);
    xl6OutputPuts("    return path != 0 && xrtPathStat(path, true, &info) ? info.Accessed : 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_get_change_time(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XFileInfo info;\n", out);
    xl6OutputPuts("    return path != 0 && xrtPathStat(path, true, &info) ? info.Modified : 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_file_rename(const char* source, const char* target, bool replace)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtPathRename(source, target, replace);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_file_replace(const char* source, const char* target)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtPathRename(source, target, true);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_open(const char* path, XL6_CgenBool readOnly, long long charset)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int flags = XL6_XFILE_READ;\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle;\n", out);
    xl6OutputPuts("    XL6_XFile file;\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !readOnly ) { flags |= XL6_XFILE_WRITE | XL6_XFILE_CREATE; }\n", out);
    xl6OutputPuts("    file = xrtOpen(path, flags);\n", out);
    xl6OutputPuts("    if ( file == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    handle = (XL6_FileHandle*)xrtMalloc(sizeof(XL6_FileHandle));\n", out);
    xl6OutputPuts("    if ( handle == 0 ) { xrtClose(file); return 0; }\n", out);
    xl6OutputPuts("    handle->file = file;\n", out);
    xl6OutputPuts("    handle->charset = (int)charset;\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts(xlirAbi
        ? "void xl6_xrt_file_close(void* self)\n"
        : "long long xl6_xrt_file_close(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    if ( handle != 0 ) { if ( handle->file != 0 ) { xrtClose(handle->file); } xrtFree(handle); }\n", out);
    if ( !xlirAbi ) xl6OutputPuts("    return 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_native(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    return handle != 0 ? handle->file : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_seek(void* self, long long offset, long long whence)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    unsigned long long position = 0u;\n", out);
    xl6OutputPuts("    return handle != 0 && handle->file != 0 && xrtSeek(handle->file, offset, (int)whence, &position) ? (long long)position : 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_tell(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    unsigned long long position = 0u;\n", out);
    xl6OutputPuts("    return handle != 0 && handle->file != 0 && xrtTell(handle->file, &position) ? (long long)position : 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_size(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    unsigned long long size = 0u;\n", out);
    xl6OutputPuts("    return handle != 0 && handle->file != 0 && xrtFileSize(handle->file, &size) ? (long long)size : 0LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_file_eof(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    unsigned long long position = 0u;\n", out);
    xl6OutputPuts("    unsigned long long size = 0u;\n", out);
    xl6OutputPuts("    return handle == 0 || handle->file == 0 || !xrtTell(handle->file, &position) || !xrtFileSize(handle->file, &size) || position >= size;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("XL6_CgenBool xl6_xrt_file_set_eof(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    unsigned long long position = 0u;\n", out);
    xl6OutputPuts("    return handle != 0 && handle->file != 0 && xrtTell(handle->file, &position) && xrtFileResize(handle->file, position);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_file_read_text(void* self, long long size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    unsigned long long position = 0u;\n", out);
    xl6OutputPuts("    unsigned long long end = 0u;\n", out);
    xl6OutputPuts("    size_t request;\n", out);
    xl6OutputPuts("    size_t readSize = 0u;\n", out);
    xl6OutputPuts("    char* raw;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    int encoding;\n", out);
    xl6OutputPuts("    if ( handle == 0 || handle->file == 0 ) { xl6_file_raise(\"FileError\", \"invalid file handle\", \"File.readText\"); return xl6_file_empty_result(); }\n", out);
    xl6OutputPuts("    if ( size < 0 ) { if ( !xrtTell(handle->file, &position) || !xrtFileSize(handle->file, &end) ) { return xl6_file_empty_result(); } request = end > position ? (size_t)(end - position) : 0u; } else { request = (size_t)size; }\n", out);
    xl6OutputPuts("    if ( request == 0u ) { return xl6_file_empty_result(); }\n", out);
    xl6OutputPuts("    raw = (char*)xrtMalloc(request + 1u);\n", out);
    xl6OutputPuts("    if ( raw == 0 || !xrtRead(handle->file, raw, request, &readSize) ) { xrtFree(raw); return xl6_file_require_text(0, 0u, \"File.readText\"); }\n", out);
    xl6OutputPuts("    raw[readSize] = '\\0';\n", out);
    xl6OutputPuts("    encoding = xl6_file_encoding(handle->charset, false, 0);\n", out);
    xl6OutputPuts("    if ( encoding == XL6_XENCODING_UNKNOWN ) { encoding = XL6_XENCODING_UTF8; }\n", out);
    xl6OutputPuts("    if ( encoding < 0 ) { xrtFree(raw); xl6_file_raise(\"FileError\", \"binary stream cannot be read as text\", \"File.readText\"); return xl6_file_empty_result(); }\n", out);
    xl6OutputPuts("    if ( encoding == XL6_XENCODING_UTF8 ) { return xl6_file_require_text(raw, readSize, \"File.readText\"); }\n", out);
    xl6OutputPuts("    text = (char*)xrtTranscode((XL6_XrtBytesView){ (const unsigned char*)raw, readSize }, encoding, XL6_XENCODING_UTF8, XL6_XUTF_STRICT, false, &readSize);\n", out);
    xl6OutputPuts("    xrtFree(raw);\n", out);
    xl6OutputPuts("    return xl6_file_require_text(text, readSize, \"File.readText\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_file_write_text(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_FileHandle* handle = (XL6_FileHandle*)self;\n", out);
    xl6OutputPuts("    int encoding;\n", out);
    xl6OutputPuts("    size_t written = 0u;\n", out);
    xl6OutputPuts("    unsigned char* converted = 0;\n", out);
    xl6OutputPuts("    size_t convertedSize = 0u;\n", out);
    xl6OutputPuts("    if ( handle == 0 || handle->file == 0 ) { return 0LL; }\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0u; }\n", out);
    xl6OutputPuts("    encoding = xl6_file_encoding(handle->charset, true, 0);\n", out);
    xl6OutputPuts("    if ( encoding < 0 ) { return 0LL; }\n", out);
    xl6OutputPuts("    if ( encoding == XL6_XENCODING_UTF8 ) { return xrtWriteFull(handle->file, text, size, &written) ? (long long)written : 0LL; }\n", out);
    xl6OutputPuts("    converted = xrtTranscode((XL6_XrtBytesView){ (const unsigned char*)text, size }, XL6_XENCODING_UTF8, encoding, XL6_XUTF_STRICT, false, &convertedSize);\n", out);
    xl6OutputPuts("    if ( converted == 0 ) { return 0LL; }\n", out);
    xl6OutputPuts("    if ( !xrtWriteFull(handle->file, converted, convertedSize, &written) ) { xrtFree(converted); return 0LL; }\n", out);
    xl6OutputPuts("    xrtFree(converted);\n", out);
    xl6OutputPuts("    return (long long)size;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
