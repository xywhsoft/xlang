/* Materialization belongs to the foreign entry transaction, never to the
 * private language function. Partial products remain owned on either side. */
static int xl6IrCgenEmitExportResultMaterialize(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId bodyType,
    XL6_IrTypeId publicType, const char* source, const char* destination)
{
    const XL6_IrType* body = xl6IrTypeGet(&module->types, bodyType);
    const XL6_IrType* output = xl6IrTypeGet(&module->types, publicType);
    if (!body || !output) return 0;
    if (bodyType == publicType) {
        xl6OutputPrintf(out, "    %s = %s; memset(&%s, 0, sizeof(%s));\n",
            destination, source, source, source);
    } else if (xl6IrTypeIsCOwned(body)) {
        int text = body->cAbi == XL6_IR_C_ABI_C_STRING;
        xl6OutputPrintf(out, "    %s = %s(%s);\n", destination,
            text ? "xl6CStringCopyV1" : "xl6CBytesCopyV1", source);
        if (text)
            xl6OutputPrintf(out, "    if (!xl6COwnedIsNoneV1(%s) && %s.Data == NULL) {\n", source, destination);
        else xl6OutputPrintf(out, "    if (%s == NULL) {\n", destination);
        xl6OutputPuts("        xl6ErrorScopeCaptureFailure(&entry); goto xlir_export_finish;\n    }\n", out);
    } else if (body->kind == XL6_IR_TYPE_TUPLE) {
        for (size_t i = 0; i < body->paramCount; ++i) {
            char from[128], to[128];
            int a = snprintf(from, sizeof(from), "%s.e%lu", source, (unsigned long)i);
            int b = snprintf(to, sizeof(to), "%s.e%lu", destination, (unsigned long)i);
            if (a < 0 || (size_t)a >= sizeof(from) || b < 0 || (size_t)b >= sizeof(to) ||
                !xl6IrCgenEmitExportResultMaterialize(out, module, body->params[i],
                    output->params[i], from, to)) return 0;
        }
    } else return 0;
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitExportBodyResultDrop(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId bodyType)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, bodyType);
    if (type && type->cAbi == XL6_IR_C_ABI_PRODUCT && xl6IrCgenProductDropNeeded(module, bodyType)) {
        xl6OutputPrintf(out, "    xlir_product_drop_t%u(&body_result);\n", bodyType);
        return 1;
    }
    return xl6IrCgenEmitDetachedRelease(out, xl6IrCgenType(module, bodyType),
        xl6IrCgenStorageReleaseFunction(type), "body_result", NULL);
}
