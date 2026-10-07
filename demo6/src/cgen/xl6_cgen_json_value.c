/* Borrowed text/value inputs. Parse/read transfer an owned xvalue; stringify
 * transfers a raw allocation to the explicit TAKE_CSTR operation. */
void xl6CgenEmitJsonValueRuntime(
    XL6_Output* out, unsigned operations, int hasXrtDeclarations)
{
    if (operations & (XL6_CGEN_JSON_PARSE | XL6_CGEN_JSON_PARSE_FILE))
        xl6OutputPuts("#include <__xl_runtime/xl6_serialization_read.h>\n", out);
    if ( (operations & XL6_CGEN_JSON_PARSE) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern xvalue* xrtJsonParse(xstrview);\nextern void xrtClearError(void);\n", out);
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_json_parse(xlir_string text)\n"
            "{ xerror* prior = xrtTakeError();\n"
            "  xvalue* value = text.Data != NULL ? xrtJsonParse(text) : NULL;\n"
            "  bool ok = xl6SerializeReadFinishV1(value, prior);\n"
            "  return (xlir_result_xvalue){ ok, ok ? value : NULL }; }\n\n", out);
    }
    if ( (operations & XL6_CGEN_JSON_VALID) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern bool xrtJsonValid(xstrview);\nextern void xrtClearError(void);\n", out);
        xl6OutputPuts(
            "bool xl6_xrt_json_valid(xlir_string text)\n"
            "{ bool valid = text.Data != NULL && xrtJsonValid(text);\n"
            "  if (!valid) xrtClearError();\n  return valid; }\n\n", out);
    }
    if ( (operations & XL6_CGEN_JSON_PARSE_FILE) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern xvalue* xrtJsonParseFile(const char*);\nextern void xrtClearError(void);\n", out);
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_json_parse_file(xlir_string path)\n"
            "{ xerror* prior = xrtTakeError(); const char* cpath = xlir_string_cstr(path);\n"
            "  xvalue* value = cpath != NULL ? xrtJsonParseFile(cpath) : NULL;\n"
            "  bool ok = xl6SerializeReadFinishV1(value, prior);\n"
            "  return (xlir_result_xvalue){ ok, ok ? value : NULL }; }\n\n", out);
    }
    if ( (operations & XL6_CGEN_JSON_STRINGIFY) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern char* xrtJsonStringify(const xvalue*, bool, size_t*);\n", out);
        xl6OutputPuts(
            "void* xl6_xrt_json_stringify(xvalue* value, int pretty)\n"
            "{ return value != NULL ? xrtJsonStringify(value, pretty != 0, NULL) : NULL; }\n\n", out);
    }
    if ( (operations & XL6_CGEN_JSON_WRITE_FILE) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern bool xrtJsonStringifyFile(const char*, const xvalue*, bool);\nextern void xrtClearError(void);\n", out);
        xl6OutputPuts(
            "bool xl6_xrt_json_write_file(xlir_string path, xvalue* value, int pretty)\n"
            "{ const char* cpath = xlir_string_cstr(path);\n"
            "  bool ok = cpath != NULL && value != NULL && xrtJsonStringifyFile(cpath, value, pretty != 0);\n"
            "  if (!ok) xrtClearError();\n  return ok; }\n\n", out);
    }
}
