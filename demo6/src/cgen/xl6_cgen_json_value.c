/* Borrowed text/value inputs. Parse/read transfer an owned xvalue; stringify
 * transfers a raw allocation to the explicit TAKE_CSTR operation. */
void xl6CgenEmitJsonValueRuntime(
    XL6_Output* out, unsigned operations, int hasXrtDeclarations)
{
    if ( (operations & (XL6_CGEN_JSON_PARSE | XL6_CGEN_JSON_VALID)) != 0 )
        xl6OutputPuts("extern size_t strlen(const char*);\n", out);
    if ( (operations & XL6_CGEN_JSON_PARSE) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern xvalue* xrtJsonParse(xstrview);\n", out);
        xl6OutputPuts(
            "xvalue* xl6_xrt_json_parse(const char* text)\n"
            "{ return text != NULL ? xrtJsonParse((xstrview){ text, strlen(text) }) : NULL; }\n\n", out);
    }
    if ( (operations & XL6_CGEN_JSON_VALID) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern bool xrtJsonValid(xstrview);\n", out);
        xl6OutputPuts(
            "bool xl6_xrt_json_valid(const char* text)\n"
            "{ return text != NULL && xrtJsonValid((xstrview){ text, strlen(text) }); }\n\n", out);
    }
    if ( (operations & XL6_CGEN_JSON_PARSE_FILE) != 0 ) {
        if ( !hasXrtDeclarations )
            xl6OutputPuts("extern xvalue* xrtJsonParseFile(const char*);\n", out);
        xl6OutputPuts(
            "xvalue* xl6_xrt_json_parse_file(const char* path)\n"
            "{ return path != NULL ? xrtJsonParseFile(path) : NULL; }\n\n", out);
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
            xl6OutputPuts("extern bool xrtJsonStringifyFile(const char*, const xvalue*, bool);\n", out);
        xl6OutputPuts(
            "bool xl6_xrt_json_write_file(const char* path, xvalue* value, int pretty)\n"
            "{ return path != NULL && value != NULL && xrtJsonStringifyFile(path, value, pretty != 0); }\n\n", out);
    }
}
