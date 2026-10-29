/* XID's physical representation is an xvalue byte blob with its exact
 * nominal TypeId.  XRT owns generation, parsing, encoding and ordering;
 * these adapters only translate the language's owned value ABIs. */
#define XL6_CGEN_XID_BIT(Kind) \
    (1u << ((Kind) - XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING))

static int xl6IrCgenEmitXidRuntime(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags)
{
    const unsigned int operations = needs->xidBridgeOperations;
    const unsigned int creates = XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_MAKE) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_PARSE) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_FROM_BYTES);
    const unsigned int reads = XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_TO_STRING) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_TO_BYTES) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_TIMESTAMP) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_COMPARE) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_EQUAL) |
        XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_IS_ZERO);
    uint64 xidTypeId = 0;
    uint64 arrayTypeId = 0;
    size_t i;
    if (out == NULL || module == NULL || needs == NULL) return 0;
    for (i = 0; i < module->callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        const XL6_IrType* type;
        uint64 candidate = 0;
        if (target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
            target->runtimeBridge < XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING ||
            target->runtimeBridge > XL6_IR_RUNTIME_BRIDGE_XID_IS_ZERO)
            continue;
        if (!xl6IrRuntimeXidBridgeTargetMatches(module, target))
            return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR XID emitter received an unverified value bridge");
        type = xl6IrTypeGet(&module->types, target->returnType);
        if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY) {
            arrayTypeId = type->typeValueId;
            type = xl6IrTypeGet(&module->types, type->params[0]);
        } else if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_XID_TO_STRING) {
            type = xl6IrTypeGet(&module->types, target->parameterTypes[0]);
        }
        if (type != NULL && type->kind == XL6_IR_TYPE_CLASS)
            candidate = type->typeValueId;
        if (candidate != 0) {
            if (xidTypeId != 0 && xidTypeId != candidate)
                return xl6IrCgenDiag(diags, module, target->declaration,
                    "XLIR XID bridge has inconsistent nominal identity");
            xidTypeId = candidate;
        }
    }
    if ((operations & (creates | reads)) != 0 && xidTypeId == 0)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR XID bridge has no nominal value identity");
    if ((operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY)) != 0 &&
        arrayTypeId == 0)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR XID batch bridge has no array identity");

    if (operations & creates) {
        xl6OutputPrintf(out,
            "static xvalue* xlir_xid_box(const xid* source)\n"
            "{ xvalue* value; if (source == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }\n"
            "  value = xrtValueBytes((xbytesview){ source->Data, XID_BINARY_SIZE });\n"
            "  if (value != NULL && !xrtValueTypeIdBind(value, 0x%016llXULL))\n"
            "    { xrtValueRelease(value); return NULL; }\n"
            "  return value; }\n\n",
            (unsigned long long)xidTypeId);
    }
    if (operations & reads) {
        xl6OutputPrintf(out,
            "static bool xlir_xid_read(const xvalue* source, xid* output)\n"
            "{ xbytesview view = {0};\n"
            "  if (source == NULL || output == NULL ||\n"
            "      xrtValueTypeId(source) != 0x%016llXULL)\n"
            "    { xrtSetErrorKind(XERR_TYPE); return false; }\n"
            "  if (!xrtValueGetBytes(source, &view) || view.Data == NULL ||\n"
            "      view.Size != XID_BINARY_SIZE)\n"
            "    { xrtSetErrorKind(XERR_TYPE); return false; }\n"
            "  memcpy(output->Data, view.Data, XID_BINARY_SIZE); return true; }\n\n",
            (unsigned long long)xidTypeId);
    }
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING))
        xl6OutputPuts(
            "xlir_string xl6_xrt_xid_make_string(void)\n"
            "{ xid value; char text[XID_TEXT_CAPACITY];\n"
            "  if (!xrtXidMake(&value) || !xrtXidWrite(&value, text, sizeof(text)))\n"
            "    return (xlir_string){0};\n"
            "  return xlir_string_from_utf8(text, XID_TEXT_SIZE); }\n\n", out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_MAKE))
        xl6OutputPuts(
            "xvalue* xl6_xrt_xid_make(void)\n"
            "{ xid value; return xrtXidMake(&value) ? xlir_xid_box(&value) : NULL; }\n\n",
            out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY))
        xl6OutputPrintf(out,
            "xvalue* xl6_xrt_xid_make_many(int64_t requested)\n"
            "{ size_t count; xid* values = NULL; xvalue* result; size_t i;\n"
            "  if (requested < 0 || (uint64_t)requested > SIZE_MAX / sizeof(xid))\n"
            "    { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "  count = (size_t)requested;\n"
            "  if (count != 0) { values = (xid*)xrtMalloc(count * sizeof(xid));\n"
            "    if (values == NULL) return NULL;\n"
            "    if (!xrtXidMakeMany(values, count))\n"
            "      { xrtSecureZero(values, count * sizeof(xid)); xrtFree(values); return NULL; } }\n"
            "  result = xrtValueArray();\n"
            "  if (result == NULL || !xrtValueTypeIdBind(result, 0x%016llXULL) ||\n"
            "      (count != 0 && !xrtValueReserve(result, count))) goto fail;\n"
            "  for (i = 0; i < count; ++i) { xvalue* item = xlir_xid_box(&values[i]);\n"
            "    if (item == NULL) goto fail;\n"
            "    if (!xrtValueArrayAppendTake(result, &item))\n"
            "      { xrtValueRelease(item); goto fail; } }\n"
            "  if (values != NULL) { xrtSecureZero(values, count * sizeof(xid)); xrtFree(values); }\n"
            "  return result;\n"
            "fail:\n"
            "  xrtValueRelease(result);\n"
            "  if (values != NULL) { xrtSecureZero(values, count * sizeof(xid)); xrtFree(values); }\n"
            "  return NULL; }\n\n",
            (unsigned long long)arrayTypeId);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_PARSE))
        xl6OutputPuts(
            "xvalue* xl6_xrt_xid_parse(xlir_string text)\n"
            "{ xid value; return xrtXidParse(text, &value) ? xlir_xid_box(&value) : NULL; }\n\n",
            out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_FROM_BYTES))
        xl6OutputPuts(
            "xvalue* xl6_xrt_xid_from_bytes(const xvalue* data)\n"
            "{ xbytesview view = {0}; xid value;\n"
            "  if (data == NULL || !xrtValueGetBytes(data, &view))\n"
            "    { xrtSetErrorKind(XERR_TYPE); return NULL; }\n"
            "  if (view.Size != XID_BINARY_SIZE || view.Data == NULL)\n"
            "    { xrtSetErrorTake(xrtErrorCreate(XERR_VALUE, \"xlang.xid\", 0,\n"
            "        \"XID requires exactly 24 bytes\")); return NULL; }\n"
            "  memcpy(value.Data, view.Data, XID_BINARY_SIZE);\n"
            "  return xlir_xid_box(&value); }\n\n", out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_TO_STRING))
        xl6OutputPuts(
            "xlir_string xl6_xrt_xid_to_string(const xvalue* source)\n"
            "{ xid value; char text[XID_TEXT_CAPACITY];\n"
            "  if (!xlir_xid_read(source, &value) ||\n"
            "      !xrtXidWrite(&value, text, sizeof(text))) return (xlir_string){0};\n"
            "  return xlir_string_from_utf8(text, XID_TEXT_SIZE); }\n\n", out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_TO_BYTES))
        xl6OutputPuts(
            "xvalue* xl6_xrt_xid_to_bytes(const xvalue* source)\n"
            "{ xid value; return xlir_xid_read(source, &value)\n"
            "    ? xrtValueBytes((xbytesview){ value.Data, XID_BINARY_SIZE }) : NULL; }\n\n",
            out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_TIMESTAMP))
        xl6OutputPuts(
            "int64_t xl6_xrt_xid_timestamp(const xvalue* source)\n"
            "{ xid value; xtime time = 0;\n"
            "  return xlir_xid_read(source, &value) && xrtXidTime(&value, &time)\n"
            "    ? (int64_t)time : 0; }\n\n", out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_COMPARE))
        xl6OutputPuts(
            "int64_t xl6_xrt_xid_compare(const xvalue* left, const xvalue* right)\n"
            "{ xid a, b; return xlir_xid_read(left, &a) && xlir_xid_read(right, &b)\n"
            "    ? (int64_t)xrtXidCompare(&a, &b) : 0; }\n\n", out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_EQUAL))
        xl6OutputPuts(
            "bool xl6_xrt_xid_equal(const xvalue* left, const xvalue* right)\n"
            "{ xid a, b; return xlir_xid_read(left, &a) && xlir_xid_read(right, &b) &&\n"
            "    xrtXidEqual(&a, &b); }\n\n", out);
    if (operations & XL6_CGEN_XID_BIT(XL6_IR_RUNTIME_BRIDGE_XID_IS_ZERO))
        xl6OutputPuts(
            "bool xl6_xrt_xid_is_zero(const xvalue* source)\n"
            "{ xid value; return xlir_xid_read(source, &value) &&\n"
            "    xrtXidIsZero(&value); }\n\n", out);
    return !xl6OutputHasFailed(out);
}

#undef XL6_CGEN_XID_BIT
