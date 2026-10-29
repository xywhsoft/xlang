/* C export results have ordinary public carriers. The private function keeps
 * its C owner signature, including nullable and multi-result components. */
static XL6_IrTypeId xl6IrTypeInternExportResultDepth(
    XL6_IrModule* module, XL6_IrTypeId bodyType, size_t depth)
{
    if (!module || depth > 64) return XL6_IR_TYPE_INVALID;
    const XL6_IrType* type = xl6IrTypeGet(&module->types, bodyType);
    if (!type) return bodyType;
    if (xl6IrTypeIsCOwned(type)) {
        XL6_AstNode element = {0}, optional = {0}, params = {0};
        XL6_AstNode* elements[] = {&element};
        XL6_AstNode* lists[] = {&params};
        element.kind = XL6_AST_TYPE;
        element.text = type->cAbi == XL6_IR_C_ABI_C_STRING ? "string" : "bytes";
        element.semanticTypeAbiId = xl6TypeAbiIdText(
            type->cAbi == XL6_IR_C_ABI_C_STRING ? "xrt.string" : "xrt.bytes");
        if (type->kind != XL6_IR_TYPE_NULLABLE)
            return xl6IrInternAstType(module, NULL, &element);
        optional.kind = XL6_AST_TYPE; optional.text = "nullable";
        optional.children = lists; optional.childCount = 1;
        params.children = elements; params.childCount = 1;
        return xl6IrInternAstType(module, NULL, &optional);
    }
    if (type->kind == XL6_IR_TYPE_TUPLE && type->paramCount) {
        size_t count = type->paramCount;
        XL6_IrTypeId* fields = xrtMalloc(count * sizeof(*fields));
        int changed = 0;
        if (!fields) return XL6_IR_TYPE_INVALID;
        memcpy(fields, type->params, count * sizeof(*fields));
        for (size_t i = 0; i < count; ++i) {
            XL6_IrTypeId mapped = xl6IrTypeInternExportResultDepth(module, fields[i], depth + 1);
            if (!mapped) { xrtFree(fields); return XL6_IR_TYPE_INVALID; }
            changed |= mapped != fields[i]; fields[i] = mapped;
        }
        XL6_IrTypeId result = changed ? xl6IrTypeInternTuple(module, fields, count) : bodyType;
        xrtFree(fields); return result;
    }
    return bodyType;
}

XL6_IrTypeId xl6IrTypeInternExportResult(XL6_IrModule* module, XL6_IrTypeId bodyType)
{ return xl6IrTypeInternExportResultDepth(module, bodyType, 0); }

static XL6_IrExportResultBridgeAbi xl6IrExportResultBridgeForTypesDepth(
    const XL6_IrModule* module, XL6_IrTypeId bodyType, XL6_IrTypeId publicType, size_t depth)
{
    if (!module || depth > 64) return XL6_IR_EXPORT_RESULT_INVALID;
    const XL6_IrType* body = xl6IrTypeGet(&module->types, bodyType);
    const XL6_IrType* output = xl6IrTypeGet(&module->types, publicType);
    if (!body || !output)
        return bodyType == 0 && publicType == 0 ? XL6_IR_EXPORT_RESULT_DIRECT : XL6_IR_EXPORT_RESULT_INVALID;
    if (xl6IrTypeIsCOwned(body)) {
        if (body->kind == XL6_IR_TYPE_NULLABLE) {
            if (output->kind != XL6_IR_TYPE_NULLABLE || output->paramCount != 1 || !output->params)
                return XL6_IR_EXPORT_RESULT_INVALID;
            output = xl6IrTypeGet(&module->types, output->params[0]);
        }
        if (!output || output->kind != XL6_IR_TYPE_BUILTIN || output->paramCount ||
            output->cAbi != (body->cAbi == XL6_IR_C_ABI_C_STRING ?
                XL6_IR_C_ABI_MANAGED_STRING : XL6_IR_C_ABI_MANAGED_BYTES) ||
            output->abiId != xl6TypeAbiIdText(body->cAbi == XL6_IR_C_ABI_C_STRING ? "xrt.string" : "xrt.bytes"))
            return XL6_IR_EXPORT_RESULT_INVALID;
        return XL6_IR_EXPORT_RESULT_C_OWNED_COPY;
    }
    if (body->kind == XL6_IR_TYPE_TUPLE) {
        int changed = 0;
        if (output->kind != XL6_IR_TYPE_TUPLE || body->paramCount != output->paramCount ||
            !body->params || !output->params) return XL6_IR_EXPORT_RESULT_INVALID;
        for (size_t i = 0; i < body->paramCount; ++i) {
            XL6_IrExportResultBridgeAbi field = xl6IrExportResultBridgeForTypesDepth(
                module, body->params[i], output->params[i], depth + 1);
            if (!field) return XL6_IR_EXPORT_RESULT_INVALID;
            changed |= field == XL6_IR_EXPORT_RESULT_C_OWNED_COPY;
        }
        return changed ? XL6_IR_EXPORT_RESULT_C_OWNED_COPY :
            bodyType == publicType ? XL6_IR_EXPORT_RESULT_DIRECT : XL6_IR_EXPORT_RESULT_INVALID;
    }
    return bodyType == publicType ? XL6_IR_EXPORT_RESULT_DIRECT : XL6_IR_EXPORT_RESULT_INVALID;
}

XL6_IrExportResultBridgeAbi xl6IrExportResultBridgeForTypes(
    const XL6_IrModule* module, XL6_IrTypeId bodyType, XL6_IrTypeId publicType)
{ return xl6IrExportResultBridgeForTypesDepth(module, bodyType, publicType, 0); }
