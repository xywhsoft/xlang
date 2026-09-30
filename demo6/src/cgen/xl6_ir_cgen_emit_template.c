/* The Template bridge is a generated-program runtime component backed only by
 * declarations in xruntime_decl.h.  Its native handle owns both the compiled
 * template and the last structured XRT error. */
static int xl6IrCgenEmitTemplateRuntime(
    XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if ( out == NULL || needs == NULL ) return 0;
    if ( !needs->needsTemplateBridgeRuntime ) return 1;
    xl6OutputPuts(
        "#ifndef XLIR_TEMPLATE_RUNTIME\n"
        "#define XLIR_TEMPLATE_RUNTIME\n"
        "typedef struct xlir_template_handle { xtemplate* value; xerror* last_error; } xlir_template_handle;\n"
        "static void xlir_template_replace_error(xerror** slot, xerror* error)\n"
        "{\n"
        "    if (slot == NULL) { xrtErrorFree(error); return; }\n"
        "    xrtErrorFree(*slot); *slot = error;\n"
        "}\n"
        "void* xl6_xrt_template_parse(xlir_string text, xlir_string open, xlir_string close)\n"
        "{\n"
        "    xlir_template_handle* handle = (xlir_template_handle*)xrtCalloc(1u, sizeof(*handle));\n"
        "    xtemplateconfig config;\n"
        "    if (handle == NULL) return NULL;\n"
        "    if (text.Data == NULL) text = (xlir_string){ \"\", 0u };\n"
        "    if (open.Data == NULL || open.Size == 0u) open = (xlir_string){ \"{\", 1u };\n"
        "    if (close.Data == NULL || close.Size == 0u) close = (xlir_string){ \"}\", 1u };\n"
        "    xrtTemplateConfigInit(&config);\n"
        "    config.Open = open; config.Close = close;\n"
        "    xrtClearError();\n"
        "    handle->value = xrtTemplateCompileConfig(text, &config);\n"
        "    if (handle->value == NULL) handle->last_error = xrtTakeError();\n"
        "    return handle;\n"
        "}\n"
        "void xl6_xrt_template_destroy(void* value)\n"
        "{\n"
        "    xlir_template_handle* handle = (xlir_template_handle*)value;\n"
        "    if (handle == NULL) return;\n"
        "    xrtTemplateRelease(handle->value); xrtErrorFree(handle->last_error); xrtFree(handle);\n"
        "}\n"
        "void* xl6_xrt_template_render(void* value, xvalue* data)\n"
        "{\n"
        "    xlir_template_handle* handle = (xlir_template_handle*)value; char* output;\n"
        "    if (handle == NULL || handle->value == NULL) return xrtStrDup(\"\");\n"
        "    xrtClearError(); xlir_template_replace_error(&handle->last_error, NULL);\n"
        "    output = xrtTemplateRender(handle->value, data, NULL);\n"
        "    if (output != NULL) return output;\n"
        "    xlir_template_replace_error(&handle->last_error, xrtTakeError());\n"
        "    return xrtStrDup(\"\");\n"
        "}\n"
        "#endif\n\n",
        out);
    return 1;
}
