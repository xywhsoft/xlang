static void xl6CgenEmitXidRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_XID_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_XID_RUNTIME\n", out);
    xl6OutputPuts("#ifndef XL_RUNTIME_H\n", out);
    xl6OutputPuts("typedef struct { unsigned char Data[24]; } xid;\n", out);
    xl6OutputPuts("bool xrtXidMake(xid* value);\n", out);
    xl6OutputPuts("bool xrtXidParse(xstrview text, xid* value);\n", out);
    xl6OutputPuts("char* xrtXidFormat(const xid* value);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void* xl6_xrt_xid_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xid* value = (xid*)xrtMalloc(sizeof(*value));\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtXidMake(value) ) { xrtFree(value); return 0; }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("void* xl6_xrt_xid_parse(const char* encoded)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xid* value;\n", out);
    xl6OutputPuts("    xstrview text;\n", out);
    xl6OutputPuts("    if ( encoded == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    value = (xid*)xrtMalloc(sizeof(*value));\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    text.Data = encoded;\n", out);
    xl6OutputPuts("    text.Size = strlen(encoded);\n", out);
    xl6OutputPuts("    if ( !xrtXidParse(text, value) ) { xrtFree(value); return 0; }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("long long xl6_xrt_xid_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("void* xl6_xrt_xid_format(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? xrtXidFormat((const xid*)self) : 0;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
