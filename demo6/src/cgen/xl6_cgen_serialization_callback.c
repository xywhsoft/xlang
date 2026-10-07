/* Callback state and native status/error separation live in one resident
 * header. The backend selects it from the verified serialization needs. */
void xl6CgenEmitSerializationCallbackRuntime(XL6_Output* out, int needsStatusResult)
{
    if (out == NULL) return;
    if (needsStatusResult)
        xl6OutputPuts("#define XL6_SERIALIZATION_STATUS_RESULT 1\n", out);
    xl6OutputPuts("#include <__xl_runtime/xl6_serialization_callback.h>\n", out);
}
