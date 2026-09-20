/* Legacy backend selects the complete shared serialization runtime.
 * XLIR selects individual verified components in its dependency analysis. */
static void xl6CgenEmitJsonRuntime(XL6_Output* out)
{
    if (out == NULL) return;
    xl6OutputPuts("#ifndef XL6_JSON_RUNTIME\n#define XL6_JSON_RUNTIME\n", out);
    xl6CgenEmitJsonValueRuntime(out, XL6_CGEN_JSON_VALUE_ALL, 1);
    xl6CgenEmitSerializationCallbackRuntime(out, 0);
    xl6CgenEmitJsonStreamRuntime(out, 1, 1, 0);
    xl6CgenEmitXsonRuntime(out, XL6_CGEN_XSON_ALL, 0);
    xl6OutputPuts("#endif\n", out);
}
