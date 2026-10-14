static int xl6IrCgenEmitXValueOperationRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if(needs->needsFloat32As)xl6OutputPuts("extern bool xl6ValueReadF32V1(const xvalue*, uint64_t, float*);\n"
        "extern bool xl6ValueTryReadF32V1(const xvalue*, uint64_t, bool*, float*);\n",out);
    if (needs->needsNativeContainerProjection) {
        xl6OutputPuts("extern bool xl6SequenceProjectionWitnessTakeV1(xvalue*, unsigned int, const xvalue*, bool, xvalue**);\n",out);
        xl6OutputPuts("extern bool xl6ListProjectionWitnessTakeV1(xvalue*, const xvalue*, bool, xvalue**);\n"
            "extern bool xl6SetProjectionWitnessTakeV1(xvalue*, const xvalue*, bool, xvalue**);\n"
            "extern bool xl6DictProjectionWitnessTakeV1(xvalue*, const xvalue*, bool, xvalue**);\n"
            "extern bool xl6TreeProjectionWitnessTakeV1(xvalue*, const xvalue*, bool, xvalue**);\n",out);
        xl6OutputPuts("extern bool xl6ValueDomainProjectionWitnessTakeV1(xvalue*, const xvalue*, bool, xvalue**);\n",out);
    }
    if (needs->needsExplicitF32Text)
        xl6OutputPuts("extern bool xl6Float32TextConvertV1(xstrview, float*);\n",out);
    if (needs->needsExplicitF32Value)
        xl6OutputPuts("extern bool xl6Float32ValueConvertV1(const xvalue*, float*);\n",out);
    if (needs->needsExplicitF32Scalar)
        xl6OutputPuts("extern bool xl6Float32ConvertV1(double, float*);\n",out);
    /* Dynamic-method adapters and boxed-callable adapters independently use
     * the resident frame producer. Its declaration follows that exact need,
     * not whether an unrelated closure/call bridge happens to be present. */
    if (needs->needsCallableArrayFrameProducer)
        xl6OutputPuts("extern xvalue* xl6ArrayCallFramePackV1(uint64_t, uint64_t, const xrtcallframe*, size_t);\n",out);
    if ( needs->needsXValueRuntime || module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN ) {
        /* Direct scalar boxing and callable bridges share this resident ABI.
         * Declaration selection must not depend on an unrelated call bridge. */
        if (needs->needsCallableNativeScalarCodec)
            xl6OutputPuts("extern bool xl6ValueReadNativeScalarV1(const xvalue*, unsigned int, uint64_t, void*);\n"
                "extern xvalue* xl6ValueBoxNativeScalarV1(const void*, unsigned int, uint64_t);\n", out);
        if ( needs->needsXValueTaggedNullableRuntime &&
             !xl6IrCgenEmitTaggedNullableXValueRuntime(out, module, diags) ) return 0;
        if ( needs->needsXValueBinaryRuntime || needs->needsXValueCompareRuntime ) {
            xl6OutputPuts(
                "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "static int xlir_xvalue_is_numeric(int type)\n"
                "{ return type == XVALUE_BOOL || type == XVALUE_INT || type == XVALUE_FLOAT || type == XVALUE_UINT; }\n"
                "static int64_t xlir_xvalue_i64(const xvalue* value)\n"
                "{ int type = xrtValueType(value); int64_t result = 0; bool boolean = false;\n"
                "  if (type == XVALUE_BOOL) { (void)xrtValueGetBool(value, &boolean); return boolean ? 1 : 0; }\n"
                "  (void)xrtValueGetInt(value, &result); return result; }\n"
                "static double xlir_xvalue_f64(const xvalue* value)\n"
                "{ int type = xrtValueType(value); double result = 0.0; uint64_t unsignedValue = 0;\n"
                "  if (type == XVALUE_UINT && xrtValueGetUInt(value, &unsignedValue)) return (double)unsignedValue;\n"
                "  if (type == XVALUE_BOOL || type == XVALUE_INT) return (double)xlir_xvalue_i64(value);\n"
                "  (void)xrtValueGetFloat(value, &result); return result; }\n"
                "static uint64_t xlir_xvalue_u64(const xvalue* value)\n"
                "{ uint64_t result = 0;\n"
                "  if (xrtValueType(value) == XVALUE_UINT) { (void)xrtValueGetUInt(value, &result); return result; }\n"
                "  return (uint64_t)xlir_xvalue_i64(value); }\n", out);
        }
        if ( needs->needsXValueBinaryRuntime ) {
            if ( needs->needsErrorRuntime )
                xl6OutputPuts(
                    "static xvalue* xlir_xvalue_binary_failure(const char* kind, const char* message)\n"
                    "{ xrtSetErrorTake(xlir_error_create(kind, message)); return NULL; }\n",
                    out);
            else
                xl6OutputPuts(
                    "static xvalue* xlir_xvalue_binary_failure(const char* kind, const char* message)\n"
                    "{ (void)kind; (void)message; return xrtValueNull(); }\n",
                    out);
            if (needs->needsXValueAddRuntime)
                xl6OutputPuts(
                    "extern bool xl6ArrayIsValueV1(const xvalue*);\n"
                    "extern xvalue* xl6ArrayConcatDynamicV1(const xvalue*, const xvalue*);\n"
                    "extern xvalue* xl6ValueStringConcatV1(const xvalue*, const xvalue*);\n", out);
            xl6OutputPuts(
                "static xvalue* xlir_xvalue_binary(const xvalue* left, const xvalue* right, int op)\n"
                "{\n"
                "    int leftType = xrtValueType(left); int rightType = xrtValueType(right);\n", out);
            if (needs->needsXValueAddRuntime)
                xl6OutputPuts(
                "    if (op == 0 && leftType == XVALUE_STRING && rightType == XVALUE_STRING) {\n"
                "        return xl6ValueStringConcatV1(left, right);\n"
                "    }\n"
                "    if (op == 0 && xl6ArrayIsValueV1(left) && xl6ArrayIsValueV1(right))\n"
                "        return xl6ArrayConcatDynamicV1(left, right);\n", out);
            xl6OutputPuts(
                "    if (!xlir_xvalue_is_numeric(leftType) || !xlir_xvalue_is_numeric(rightType)) return xrtValueNull();\n"
                "    if (op >= 5) {\n"
                "        if (leftType == XVALUE_FLOAT || rightType == XVALUE_FLOAT) return xrtValueNull();\n"
                "        if (leftType == XVALUE_UINT || rightType == XVALUE_UINT) {\n"
                "            uint64_t a = xlir_xvalue_u64(left); uint64_t b = xlir_xvalue_u64(right);\n"
                "            if (op == 5) return xrtValueUInt(a & b);\n"
                "            if (op == 6) return xrtValueUInt(a | b);\n"
                "            if (op == 7) return xrtValueUInt(a ^ b);\n"
                "            if (b >= 64u) return xlir_xvalue_binary_failure(\"RangeError\", \"shift count is outside the left operand width\");\n"
                "            return op == 8 ? xrtValueUInt(a << (uint32_t)b) : xrtValueUInt(a >> (uint32_t)b);\n"
                "        }\n"
                "        { int64_t a = xlir_xvalue_i64(left); int64_t b = xlir_xvalue_i64(right);\n"
                "          if (op == 5) return xrtValueInt(a & b);\n"
                "          if (op == 6) return xrtValueInt(a | b);\n"
                "          if (op == 7) return xrtValueInt(a ^ b);\n"
                "          if (b < 0 || b >= 64) return xlir_xvalue_binary_failure(\"RangeError\", \"shift count is outside the left operand width\");\n"
                "          return op == 8 ? xrtValueInt((int64_t)((uint64_t)a << (uint32_t)b)) :\n"
                "                           xrtValueInt((int64_t)((uint64_t)a >> (uint32_t)b)); }\n"
                "    }\n"
                "    if (leftType == XVALUE_FLOAT || rightType == XVALUE_FLOAT) {\n"
                "        double a = xlir_xvalue_f64(left); double b = xlir_xvalue_f64(right);\n"
                "        return op == 0 ? xrtValueFloat(a + b) : op == 1 ? xrtValueFloat(a - b) :\n"
                "               op == 2 ? xrtValueFloat(a * b) : op == 3 && b != 0.0 ? xrtValueFloat(a / b) : xrtValueNull();\n"
                "    }\n"
                "    if (leftType == XVALUE_UINT || rightType == XVALUE_UINT) {\n"
                "        uint64_t a = xlir_xvalue_u64(left); uint64_t b = xlir_xvalue_u64(right);\n"
                "        if (op == 0) return xrtValueUInt(a + b);\n"
                "        if (op == 1) return xrtValueUInt(a - b);\n"
                "        if (op == 2) return xrtValueUInt(a * b);\n"
                "        if (op == 3) return b != 0 ? xrtValueUInt(a / b) : xlir_xvalue_binary_failure(\"ZeroDivisionError\", \"integer division by zero\");\n"
                "        if (op == 4) return b != 0 ? xrtValueUInt(a % b) : xlir_xvalue_binary_failure(\"ZeroDivisionError\", \"integer modulo by zero\");\n"
                "        return xrtValueNull();\n"
                "    }\n"
                "    { int64_t a = xlir_xvalue_i64(left); int64_t b = xlir_xvalue_i64(right);\n"
                "      if (op == 0) return xrtValueInt((int64_t)((uint64_t)a + (uint64_t)b));\n"
                "      if (op == 1) return xrtValueInt((int64_t)((uint64_t)a - (uint64_t)b));\n"
                "      if (op == 2) return xrtValueInt((int64_t)((uint64_t)a * (uint64_t)b));\n"
                "      if (op == 3) {\n"
                "          if (b == 0) return xlir_xvalue_binary_failure(\"ZeroDivisionError\", \"integer division by zero\");\n"
                "          if (a == INT64_MIN && b == -1) return xlir_xvalue_binary_failure(\"OverflowError\", \"integer division overflow\");\n"
                "          return xrtValueInt(a / b);\n"
                "      }\n"
                "      if (op == 4) {\n"
                "          if (b == 0) return xlir_xvalue_binary_failure(\"ZeroDivisionError\", \"integer modulo by zero\");\n"
                "          if (a == INT64_MIN && b == -1) return xlir_xvalue_binary_failure(\"OverflowError\", \"integer modulo overflow\");\n"
                "          return xrtValueInt(a % b);\n"
                "      }\n"
                "      return xrtValueNull(); }\n"
                "}\n", out);
            xl6OutputPuts(
                "static xvalue* xlir_xvalue_unary(const xvalue* value, int op)\n"
                "{\n"
                "    int type = value != NULL ? xrtValueType(value) : XVALUE_NULL;\n"
                "    if (!xlir_xvalue_is_numeric(type)) return xrtValueNull();\n"
                "    if (type == XVALUE_FLOAT) {\n"
                "        double v = xlir_xvalue_f64(value);\n"
                "        return op == 0 ? xrtValueFloat(v) : op == 1 ? xrtValueFloat(-v) : xrtValueNull();\n"
                "    }\n"
                "    { int64_t v = type == XVALUE_UINT ? (int64_t)xlir_xvalue_u64(value) : xlir_xvalue_i64(value);\n"
                "      return op == 0 ? xrtValueInt(v) : op == 1 ? xrtValueInt(v == INT64_MIN ? INT64_MIN : -v) : xrtValueInt(~v); }\n"
                "}\n", out);
        }
        if ( needs->needsXValueCompareRuntime )
            xl6OutputPuts(
                "extern bool xrtValueEqual(const xvalue*, const xvalue*);\n"
                "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n"
                "extern bool xrtTimeNear(int64_t, int64_t, uint64_t);\n"
                "static inline bool xlir_xvalue_equal(const xvalue* left, const xvalue* right)\n"
                "{\n"
                "    if (left == NULL || right == NULL)\n"
                "        return (left == NULL || xrtValueType(left) == XVALUE_NULL) &&\n"
                "               (right == NULL || xrtValueType(right) == XVALUE_NULL);\n"
                "    if (xrtValueType(left) == XVALUE_FLOAT && xlir_xvalue_f64(left) != xlir_xvalue_f64(left)) return false;\n"
                "    if (xrtValueType(right) == XVALUE_FLOAT && xlir_xvalue_f64(right) != xlir_xvalue_f64(right)) return false;\n"
                "    return xrtValueEqual(left, right) || left == right;\n"
                "}\n"
                "static inline bool xlir_xvalue_order(const xvalue* left, const xvalue* right, int op)\n"
                "{\n"
                "    int leftType = left != NULL ? xrtValueType(left) : XVALUE_NULL;\n"
                "    int rightType = right != NULL ? xrtValueType(right) : XVALUE_NULL; int compare = 0;\n"
                "    if (leftType == XVALUE_NULL && rightType == XVALUE_NULL) compare = 0;\n"
                "    else if (xlir_xvalue_is_numeric(leftType) && xlir_xvalue_is_numeric(rightType)) {\n"
                "        if (leftType == XVALUE_FLOAT || rightType == XVALUE_FLOAT) {\n"
                "            double a = xlir_xvalue_f64(left); double b = xlir_xvalue_f64(right);\n"
                "            if (a != a || b != b) return false;\n"
                "            compare = a < b ? -1 : a > b ? 1 : 0;\n"
                "        } else if (leftType == XVALUE_UINT && rightType != XVALUE_UINT && xlir_xvalue_i64(right) < 0)\n"
                "            compare = 1;\n"
                "        else if (rightType == XVALUE_UINT && leftType != XVALUE_UINT && xlir_xvalue_i64(left) < 0)\n"
                "            compare = -1;\n"
                "        else if (leftType == XVALUE_UINT || rightType == XVALUE_UINT) {\n"
                "            uint64_t a = xlir_xvalue_u64(left); uint64_t b = xlir_xvalue_u64(right);\n"
                "            compare = a < b ? -1 : a > b ? 1 : 0;\n"
                "        } else {\n"
                "            int64_t a = xlir_xvalue_i64(left); int64_t b = xlir_xvalue_i64(right);\n"
                "            compare = a < b ? -1 : a > b ? 1 : 0;\n"
                "        }\n"
                "    } else if (leftType == XVALUE_STRING && rightType == XVALUE_STRING) {\n"
                "        xlir_string_view a = {0}; xlir_string_view b = {0}; int raw;\n"
                "        if (!xrtValueGetString(left, &a) || !xrtValueGetString(right, &b)) return false;\n"
                "        raw = xrtStrCompare(a, b); compare = raw < 0 ? -1 : raw > 0 ? 1 : 0;\n"
                "    } else if (leftType == XVALUE_TIME && rightType == XVALUE_TIME) {\n"
                "        int64_t a = 0; int64_t b = 0;\n"
                "        if (!xrtValueGetTime(left, &a) || !xrtValueGetTime(right, &b)) return false;\n"
                "        compare = a < b ? -1 : a > b ? 1 : 0;\n"
                "    } else return false;\n"
                "    return op == 0 ? compare < 0 : op == 1 ? compare <= 0 :\n"
                "           op == 2 ? compare > 0 : compare >= 0;\n"
                "}\n"
                "static inline bool xlir_xvalue_approx(const xvalue* left, const xvalue* right)\n"
                "{\n"
                "    int leftType = left != NULL ? xrtValueType(left) : XVALUE_NULL;\n"
                "    int rightType = right != NULL ? xrtValueType(right) : XVALUE_NULL;\n"
                "    if (leftType == XVALUE_STRING || rightType == XVALUE_STRING) {\n"
                "        xlir_string_view a = {0}; xlir_string_view b = {0};\n"
                "        return leftType == XVALUE_STRING && rightType == XVALUE_STRING &&\n"
                "               xrtValueGetString(left, &a) && xrtValueGetString(right, &b) &&\n"
                "               xrtUtf8Similarity(a, b) >= 0.95;\n"
                "    }\n"
                "    if (leftType == XVALUE_TIME || rightType == XVALUE_TIME) {\n"
                "        int64_t a = 0; int64_t b = 0;\n"
                "        return leftType == XVALUE_TIME && rightType == XVALUE_TIME &&\n"
                "               xrtValueGetTime(left, &a) && xrtValueGetTime(right, &b) &&\n"
                "               xrtTimeNear(a, b, UINT64_C(10000000));\n"
                "    }\n"
                "    if (!xlir_xvalue_is_numeric(leftType) || !xlir_xvalue_is_numeric(rightType)) return false;\n"
                "    if (leftType == XVALUE_FLOAT || rightType == XVALUE_FLOAT)\n"
                "        return xlir_f64_approx(xlir_xvalue_f64(left), xlir_xvalue_f64(right));\n"
                "    if (leftType == XVALUE_UINT || rightType == XVALUE_UINT) {\n"
                "        int64_t signedValue;\n"
                "        if (leftType != XVALUE_UINT && (signedValue = xlir_xvalue_i64(left)) < 0) return false;\n"
                "        if (rightType != XVALUE_UINT && (signedValue = xlir_xvalue_i64(right)) < 0) return false;\n"
                "        return xlir_u64_approx(xlir_xvalue_u64(left), xlir_xvalue_u64(right));\n"
                "    }\n"
                "    return xlir_i64_approx(xlir_xvalue_i64(left), xlir_xvalue_i64(right));\n"
                "}\n", out);
        if ( needs->needsXValueIntegerConvertRuntime )
            xl6OutputPuts(
                "static inline int64_t xlir_xvalue_to_i64_checked(const xvalue* value, int target)\n"
                "{\n"
                "    int type = value != NULL ? xrtValueType(value) : 0;\n"
                "    int64_t integer = 0; uint64_t unsignedInteger = 0; double number = 0.0; bool boolean = false;\n"
                "    if (type == 0) return 0;\n"
                "    if (type == 1 && xrtValueGetBool(value, &boolean)) return xlir_i64_range_checked(boolean ? 1 : 0, target);\n"
                "    if (type == 2 && xrtValueGetInt(value, &integer)) return xlir_i64_range_checked(integer, target);\n"
                "    if (type == 13 && xrtValueGetUInt(value, &unsignedInteger)) {\n"
                "        if (unsignedInteger <= (uint64_t)INT64_MAX) return xlir_i64_range_checked((int64_t)unsignedInteger, target);\n"
                "        xrtSetErrorTake(xlir_error_create(\"RangeError\", \"unsigned integer value is outside signed integer range\")); return 0;\n"
                "    }\n"
                "    if (type == 3 && xrtValueGetFloat(value, &number)) return xlir_f64_to_i64_checked(number, target);\n"
                "    if (type == 4) {\n"
                "        xlir_string_view text; char* owned; char* start; char* end = NULL;\n"
                "        if (!xrtValueGetString(value, &text) || (owned = xrtStrDupN(text.Data, text.Size)) == NULL) {\n"
                "            xrtSetErrorTake(xlir_error_create(\"ValueError\", \"invalid integer string\")); return 0;\n"
                "        }\n"
                "        start = owned; while (*start == ' ' || *start == '\\t' || *start == '\\r' || *start == '\\n' || *start == '\\v' || *start == '\\f') ++start;\n"
                "        errno = 0; integer = (int64_t)strtoll(start, &end, 10);\n"
                "        while (end != NULL && (*end == ' ' || *end == '\\t' || *end == '\\r' || *end == '\\n' || *end == '\\v' || *end == '\\f')) ++end;\n"
                "        if (end == start || errno == ERANGE || end == NULL || *end != '\\0') {\n"
                "            xrtFree(owned); xrtSetErrorTake(xlir_error_create(\"ValueError\", \"invalid integer string\")); return 0;\n"
                "        }\n"
                "        xrtFree(owned); return xlir_i64_range_checked(integer, target);\n"
                "    }\n"
                "    xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value cannot convert to integer\"));\n"
                "    return 0;\n"
                "}\n", out);
        if ( needs->needsXValueIntegerConvertRuntime && needs->needsCheckedU64Runtime )
            xl6OutputPuts(
                "static inline uint64_t xlir_xvalue_to_u64_checked(const xvalue* value)\n"
                "{\n"
                "    int type = value != NULL ? xrtValueType(value) : 0;\n"
                "    int64_t integer = 0; uint64_t unsignedInteger = 0; double number = 0.0; bool boolean = false;\n"
                "    if (type == 0) return 0;\n"
                "    if (type == 1 && xrtValueGetBool(value, &boolean)) return boolean ? 1u : 0u;\n"
                "    if (type == 2 && xrtValueGetInt(value, &integer)) return xlir_i64_to_u64_checked(integer);\n"
                "    if (type == 13 && xrtValueGetUInt(value, &unsignedInteger)) return unsignedInteger;\n"
                "    if (type == 3 && xrtValueGetFloat(value, &number)) return xlir_f64_to_u64_checked(number);\n"
                "    if (type == 4) {\n"
                "        xlir_string_view text; char* owned; char* start; char* end = NULL;\n"
                "        if (!xrtValueGetString(value, &text) || (owned = xrtStrDupN(text.Data, text.Size)) == NULL) {\n"
                "            xrtSetErrorTake(xlir_error_create(\"ValueError\", \"invalid uint64 string\")); return 0;\n"
                "        }\n"
                "        start = owned; while (*start == ' ' || *start == '\\t' || *start == '\\r' || *start == '\\n' || *start == '\\v' || *start == '\\f') ++start;\n"
                "        if (*start == '-') { xrtFree(owned); xrtSetErrorTake(xlir_error_create(\"RangeError\", \"negative value is outside uint64 range\")); return 0; }\n"
                "        errno = 0; unsignedInteger = (uint64_t)strtoull(start, &end, 10);\n"
                "        while (end != NULL && (*end == ' ' || *end == '\\t' || *end == '\\r' || *end == '\\n' || *end == '\\v' || *end == '\\f')) ++end;\n"
                "        if (end == start || errno == ERANGE || end == NULL || *end != '\\0') {\n"
                "            xrtFree(owned); xrtSetErrorTake(xlir_error_create(\"ValueError\", \"invalid uint64 string\")); return 0;\n"
                "        }\n"
                "        xrtFree(owned); return unsignedInteger;\n"
                "    }\n"
                "    xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value cannot convert to uint64\"));\n"
                "    return 0;\n"
                "}\n", out);
        if ( needs->needsXCallRuntime ) {
            size_t signatureIndex;
            xl6OutputPuts(
                "typedef xstrview xlir_xcall_name_view;\n"
                "extern xvalue* xrtValueBool(bool);\n"
                "extern xvalue* xrtValueInt(int64_t);\n"
                "extern xvalue* xrtValueUInt(uint64_t);\n"
                "extern xvalue* xrtValueFloat(double);\n"
                "extern xvalue* xrtValueArray(void);\n"
                "extern bool xrtValueArrayAppend(xvalue*, const xvalue*);\n"
                "extern xvalue* xrtValueRetain(const xvalue*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern bool xrtValueGetPointer(const xvalue*, void**);\n"
                "extern xvalue* xrtValueString(xlir_string_view);\n"
                "extern xvalue* xrtValuePointer(void*);\n"
                "extern char* xrtStrDup(const char*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n", out);
            if (needs->needsCallableF32Reader)
                xl6OutputPuts("extern bool xl6ValueReadF32V1(const xvalue*, uint64_t, float*);\n", out);
            if (needs->needsCallableIntegerCodec)
                xl6OutputPuts("extern bool xl6ValueIntegerSlotAdmitsV1(const void*, unsigned int);\n"
                    "extern bool xl6ValueReadIntegerV1(const xvalue*, unsigned int, uint64_t, bool, void*);\n"
                    "extern xvalue* xl6ValueBoxIntegerV1(const void*, unsigned int, uint64_t, bool);\n", out);
            /* Pure erased invocation and bound methods use XRT callables,
             * not native XLIR closures. Emit this bridge only when required. */
            if (needs->needsClosureRuntime)
                xl6OutputPuts(
                "#define xlir_closure_to_xvalue_scalar(closure, entry, typeId) (xl6ClosureRuntimeV1Get()->Box)(xlir_closure_owner, (closure), (entry), (typeId))\n", out);
            if ( needs->needsXValueClosureBridgeRuntime )
                xl6OutputPuts(
                    "extern bool xrtValueIsCallable(const xvalue*);\n", out);
            if ( needs->needsXValueClosureProjectionRuntime ||
                 needs->needsXValueIteratorNullableClosureRuntime ||
                 needs->needsXValueClosureFieldRuntime )
                xl6OutputPuts(
                    "static bool xlir_xvalue_project_closure_try_copy_into(const xvalue* source, void (*entry)(void), bool nullable, xlir_closure** output)\n"
                    "{\n"
                    "    xlir_closure* closure; *output = NULL;\n"
                    "    if (source == NULL || entry == NULL) goto incompatible;\n"
                    "    if (nullable && xrtValueType(source) == 0) return true;\n"
                    "    if (xrtValueType(source) == 0 || !xrtValueIsCallable(source)) goto incompatible;\n"
                    "    closure = xl6ClosureRuntimeV2Get()->CreateValue(xlir_closure_owner, entry, source);\n"
                    "    if (closure == NULL) return false;\n"
                    "    *output = closure; return true;\n"
                    "incompatible:\n"
                    "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime value is not callable\"));\n"
                    "    return false;\n"
                    "}\n"
                    "static xlir_closure* xlir_xvalue_project_closure_try_copy(const xvalue* source, void (*entry)(void))\n"
                    "{\n"
                    "    xlir_closure* closure = NULL;\n"
                    "    (void)xlir_xvalue_project_closure_try_copy_into(source, entry, false, &closure);\n"
                    "    return closure;\n"
                    "}\n", out);
            if ( needs->needsXValueClosureFieldRuntime )
                xl6OutputPuts(
                    "static bool xlir_xvalue_object_set_literal_closure(xvalue* object, xlir_string_view key, xlir_closure* closure, xrtcallproc entry, uint64_t typeId)\n"
                    "{ xvalue* boxed = closure != NULL ? xlir_closure_to_xvalue_scalar(closure, entry, typeId) : xrtValueRetain(xrtValueNull()); if (boxed == NULL) return false; return xrtValueObjectSetNewPostCommit(object, key, boxed); }\n"
                    "static bool xlir_xvalue_object_get_literal_closure(const xvalue* object, xlir_string_view key, void (*entry)(void), xlir_closure** output)\n"
                    "{\n"
                    "    return xlir_xvalue_project_closure_try_copy_into(xrtValueObjectGet(object, key), entry, true, output);\n"
                    "}\n", out);
            if ( needs->needsXValueClosureCheckedCastRuntime )
                xl6OutputPuts(
                    "static xlir_closure* xlir_xvalue_cast_closure_checked(const xvalue* source, uint64_t typeId, void (*entry)(void))\n"
                    "{\n"
                    "    xlir_closure* closure;\n"
                    "    if (typeId == 0 || entry == NULL) goto runtime_failure;\n"
                    "    if (source == NULL || xrtValueType(source) == 0 || xlir_xvalue_type_id(source) != typeId || !xrtValueIsCallable(source)) goto incompatible;\n"
                    "    closure = xl6ClosureRuntimeV2Get()->CreateValue(xlir_closure_owner, entry, source);\n"
                    "    if (closure == NULL) goto runtime_failure;\n"
                    "    return closure;\n"
                    "incompatible:\n"
                    "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value does not match requested callable target\"));\n"
                    "    return NULL;\n"
                    "runtime_failure:\n"
                    "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"checked callable cast result allocation failed\"));\n"
                    "    return NULL;\n"
                    "}\n", out);
            for ( signatureIndex = 0; signatureIndex < module->types.count;
                  ++signatureIndex ) {
                XL6_IrTypeId signatureType =
                    (XL6_IrTypeId)(signatureIndex + 1);
                if ( xl6IrCgenRuntimeCallableAdapterNeeded(
                         module, signatureType, 0) &&
                     !xl6IrCgenEmitRuntimeCallableEntryPrototype(
                         out, module, signatureType, diags) ) return 0;
                if ( xl6IrCgenRuntimeCallableAdapterNeeded(
                         module, signatureType, 1) &&
                     !xl6IrCgenEmitRuntimeCallableClosureEntryPrototype(
                         out, module, signatureType, diags) ) return 0;
            }
            xl6OutputPuts("\n", out);
            for ( signatureIndex = 0; signatureIndex < module->types.count;
                  ++signatureIndex ) {
                XL6_IrTypeId signatureType =
                    (XL6_IrTypeId)(signatureIndex + 1);
                if ( xl6IrCgenRuntimeCallableAdapterNeeded(
                         module, signatureType, 0) &&
                     !xl6IrCgenEmitRuntimeCallableEntry(
                         out, module, signatureType, diags) ) return 0;
            }
            /* Dynamic invocation is a nested foreign boundary: a callback error
             * caught in language code must not latch the enclosing export. */
            xl6OutputPuts(
                "static xvalue* xlir_xvalue_dynamic_call_owned(const xvalue* callable, size_t argumentCount, xvalue** arguments, size_t keywordCount, const xlir_string_view* keywordNames, xvalue** keywordValues, bool discardResults)\n"
                "{\n"
                "    xrtcallframe frame = {0}; xrtcallresult result = {0}; xvalue* owned = NULL; size_t i;\n"
                "    xl6_foreign_error_scope invocation; xl6_foreign_error_result outcome; bool success;\n"
                "    if (!xl6ForeignCallBegin(&invocation)) {\n"
                "        xerror* error = xrtTakeError();\n"
                "        for (i = 0; i < argumentCount; ++i) xrtValueRelease(arguments[i]);\n"
                "        for (i = 0; i < keywordCount; ++i) xrtValueRelease(keywordValues[i]);\n"
                "        xrtSetErrorTake(error); return NULL;\n"
                "    }\n"
                "    frame.ArgumentCount = argumentCount; frame.Arguments = arguments; frame.KeywordCount = keywordCount; frame.KeywordNames = keywordNames; frame.KeywordValues = keywordValues;\n"
                "    success = xrtValueInvoke(callable, &frame, &result);\n"
                "    if (!success) {\n"
                "        if (!invocation.failed) {\n"
                "            xerror* error = xrtTakeError(); const xerror* cause = xrtErrorCause(error);\n"
                "            xerror* projected = cause != NULL ? xrtErrorRef(cause) : NULL;\n"
                "            if (projected != NULL) { xrtErrorFree(error); error = projected; }\n"
                "            xrtSetErrorTake(error);\n"
                "        }\n"
                "        xl6ErrorScopeCaptureFailure(&invocation);\n"
                "    } else if (discardResults) {\n"
                "        owned = xrtValueRetain(xrtValueNull());\n"
                "        if (owned == NULL) xl6ErrorScopeCaptureFailure(&invocation);\n"
                "    } else if (xrtCallResultCount(&result) == 0) {\n"
                "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic call returned no value\"));\n"
                "        xl6ErrorScopeCaptureFailure(&invocation);\n"
                "    } else {\n"
                "        xvalue* borrowed = xrtCallResultGet(&result, 0);\n"
                "        owned = borrowed != NULL ? xrtValueRetain(borrowed) : NULL;\n"
                "        if (owned == NULL) xl6ErrorScopeCaptureFailure(&invocation);\n"
                "    }\n"
                "    for (i = 0; i < argumentCount; ++i) xrtValueRelease(arguments[i]);\n"
                "    for (i = 0; i < keywordCount; ++i) xrtValueRelease(keywordValues[i]);\n"
                "    xrtCallResultUnit(&result);\n"
                "    if (invocation.failed || xrtGetError() != NULL) { xrtValueRelease(owned); owned = NULL; }\n"
                "    outcome = xl6ForeignCallEnd(&invocation);\n"
                "    if (!outcome.ok) { xrtSetErrorTake(outcome.error); return NULL; }\n"
                "    return owned;\n"
                "}\n", out);
            for ( signatureIndex = 0; signatureIndex < module->types.count;
                  ++signatureIndex ) {
                XL6_IrTypeId signatureType =
                    (XL6_IrTypeId)(signatureIndex + 1);
                if ( xl6IrCgenRuntimeCallableAdapterNeeded(
                         module, signatureType, 1) &&
                     !xl6IrCgenEmitRuntimeCallableClosureEntry(
                         out, module, signatureType, diags) ) return 0;
            }
        }
        xl6OutputPuts("\n", out);
    }
    return !xl6OutputHasFailed(out);
}
