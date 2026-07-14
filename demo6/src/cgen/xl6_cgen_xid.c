static void xl6CgenEmitXidRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_XID_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_XID_RUNTIME\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("long long xl6_xrt_xid_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
