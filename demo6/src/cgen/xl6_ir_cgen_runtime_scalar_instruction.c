/* Scalar, identity, arithmetic, error and cast RuntimeOp emission. */
static int xl6IrCgenRuntimeEmitDiag(
    XL6_DiagList* diags, const XL6_IrModule* module,
    XL6_IrNodeId origin, const char* message)
{
    xl6IrCgenDiag(diags, module, origin, message);
    return -1;
}

static void xl6IrCgenEmitImmediateBytes(
    XL6_Output* out, const unsigned char* data, size_t size)
{
    size_t i;
    xl6OutputPuts("(const unsigned char[]){", out);
    for ( i = 0; i < size; ++i ) {
        if ( i != 0 ) xl6OutputPuts(",", out);
        xl6OutputPrintf(out, "0x%02X", (unsigned int)data[i]);
    }
    if ( size != 0 ) xl6OutputPuts(",", out);
    xl6OutputPuts("0x00}", out);
}

static int xl6IrCgenEmitScalarRuntimeInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
                int sharedRuntime = xl6IrCgenEmitSharedRuntime(
                    out, module, instruction);
                if ( sharedRuntime < 0 )
                    return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                        "XLIR C emitter shared runtime ABI is unresolved");
                if ( sharedRuntime > 0 ) {
                } else if (instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE) {
                    return xl6IrCgenEmitArrayCreate(out,module,instruction)>0;
                } else if (instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN) {
                    char source[24];
                    if (!xl6IrArrayImportRuntimeInstructionCompatible(module,instruction))
                        return xl6IrCgenRuntimeEmitDiag(diags,module,instruction->origin,"XLIR Array import contract is unresolved");
                    snprintf(source,sizeof(source),"v%u",(unsigned)module->operands[instruction->firstOperand]);
                    return xl6IrCgenEmitArrayImportChain(out,module,instruction->type,instruction->result,source,true);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TYPE_ID ) {
                    if ( instruction->immediateText != NULL ||
                         instruction->operandCount != 1 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime type query ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_type_id(v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_FROM_UTF8 ) {
                    if ( instruction->immediateText != NULL ||
                         (instruction->immediateData == NULL && instruction->immediateSize != 0) ||
                         instruction->operandCount != 0 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter explicit UTF-8 literal ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_string_from_utf8((const char*)",
                        (unsigned int)instruction->result);
                    xl6IrCgenEmitImmediateBytes(out, instruction->immediateData,
                                                instruction->immediateSize);
                    xl6OutputPrintf(out, ", %lluU);\n",
                        (unsigned long long)instruction->immediateSize);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_FROM_DATA ) {
                    if ( instruction->immediateText != NULL ||
                         (instruction->immediateData == NULL && instruction->immediateSize != 0) ||
                         instruction->operandCount != 0 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter explicit bytes literal ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xrtValueBytes((xbytesview){ ",
                        (unsigned int)instruction->result);
                    xl6IrCgenEmitImmediateBytes(out, instruction->immediateData,
                                                instruction->immediateSize);
                    xl6OutputPrintf(out, ", %lluU });\n",
                        (unsigned long long)instruction->immediateSize);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_CHAR_TO_STRING ) {
                    if ( instruction->immediateText != NULL || instruction->operandCount != 1 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter char-to-string ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_char_to_string(v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_CONCAT ) {
                    xl6OutputPrintf(out, "    v%u = xlir_bytes_concat(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_EQUAL ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LT ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LE ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GT ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GE ) {
                    const char* relation = instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_EQUAL ? "==" :
                        instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LT ? "<" :
                        instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LE ? "<=" :
                        instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GT ? ">" : ">=";
                    xl6OutputPrintf(out, "    v%u = xlir_bytes_compare(v%u, v%u) %s 0;\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1], relation);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_LENGTH ) {
                    xl6OutputPrintf(out, "    v%u = xlir_bytes_length(v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_AT ) {
                    xl6OutputPrintf(out,
                        "    bool projection_ok_%u = xlir_bytes_at(v%u, v%u, &v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_SET_COPY ) {
                    xl6OutputPrintf(out, "    v%u = xlir_bytes_set_copy(v%u, v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)module->operands[instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_SCALAR_AT ) {
                    xl6OutputPrintf(out,
                        "    bool projection_ok_%u = xlir_string_scalar_at(v%u, v%u, &v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_string_scalar_set_copy(v%u, v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)module->operands[instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_string_concat_char_right(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_string_concat_char_left(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_DUP ) {
                    if ( instruction->immediateText == NULL || instruction->operandCount != 0 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter runtime operation ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_string_from_utf8(%s, sizeof(%s) - 1u);\n",
                                    (unsigned int)instruction->result, instruction->immediateText,
                                    instruction->immediateText);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_COPY ) {
                    if ( instruction->immediateText != NULL || instruction->operandCount != 1 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter runtime operation ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_string_copy(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_STRING_NULLABLE_COPY ) {
                    if ( !xl6IrCgenRuntimeNullableStringCopyAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter nullable string copy ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_string_copy(v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand]);
                    xl6OutputPrintf(out,
                        "    bool projection_ok_%u = v%u.Data == NULL || v%u.Data != NULL;\n"
                        "    if (!projection_ok_%u && xrtGetError() == NULL) xrtSetErrorKind(XERR_MEMORY);\n",
                        (unsigned int)instruction->result, (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)instruction->result, (unsigned int)instruction->result);
                } else if ( xl6IrCgenCheckedIntegerRuntimeOpIs(
                                instruction->runtimeOp) ) {
                    if ( !xl6IrCgenEmitCheckedIntegerArithmetic(
                             out, module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter checked integer arithmetic ABI is unresolved");
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_I64_POW ) {
                    if ( !xl6IrCgenRuntimeI64PowAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter i64 power ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_i64_pow(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned int)module->operands[
                            instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_TIME_APPROX ) {
                    if ( !xl6IrCgenRuntimeTimeApproxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter time approximate ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xrtTimeNear(v%u, v%u, UINT64_C(10000000));\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned int)module->operands[
                            instruction->firstOperand + 1]);
                } else if (instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT ||
                           instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT) {
                    if (!xl6IrPersistenceProjectInstructionAbi(module, instruction))
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter persistence projection ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xl6PersistenceProjectV1(v%u, &xlir_persist_plan_%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)xl6IrCgenPersistenceRuntimeFormat(instruction->runtimeOp));
                } else if (xl6IrPersistenceInvocationRuntime(instruction->runtimeOp) != NULL) {
                    const XL6_IrPersistenceInvocationContract* configured =
                        xl6IrPersistenceInvocationRuntime(instruction->runtimeOp);
                    if (!xl6IrPersistenceInvocationInstructionAbi(module, instruction))
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter persistence invocation ABI is unresolved");
                    if (configured->action == XL6_IR_PERSISTENCE_INVOKE_ENCODE)
                        xl6OutputPrintf(out, "    v%u = %s(v%u, v%u);\n",
                            (unsigned int)instruction->result, configured->entry,
                            (unsigned int)module->operands[instruction->firstOperand],
                            (unsigned int)module->operands[instruction->firstOperand + 1]);
                    else {
                        xl6OutputPrintf(out, "    bool projection_ok_%u = %s(v%u, v%u",
                            (unsigned int)instruction->result, configured->entry,
                            (unsigned int)module->operands[instruction->firstOperand],
                            (unsigned int)module->operands[instruction->firstOperand + 1]);
                        if (instruction->operandCount == 3)
                            xl6OutputPrintf(out, ", v%u", (unsigned int)module->operands[instruction->firstOperand + 2]);
                        xl6OutputPrintf(out, ", &v%u);\n", (unsigned int)instruction->result);
                    }
                } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY ) {
                    const char* format = instruction->runtimeOp ==
                            XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY
                        ? "json" : "xlon";
                    if ( !xl6IrCgenPersistenceStringifyAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter persistence stringify ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    { size_t size = 0; char* text = %s(v%u, v%u, &size);\n"
                        "      v%u = (xlir_string){text, text != NULL ? size : 0}; }\n",
                        strcmp(format, "json") == 0 ? "xrtJsonStringify" : "xrtXlonStringify",
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned int)module->operands[
                            instruction->firstOperand + 1],
                        (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_PERSISTENCE_XLON_READ ) {
                    const int isRead = instruction->runtimeOp ==
                            XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                        instruction->runtimeOp ==
                            XL6_IR_RUNTIME_PERSISTENCE_XLON_READ;
                    const char* format =
                        instruction->runtimeOp ==
                            XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE ||
                        instruction->runtimeOp ==
                            XL6_IR_RUNTIME_PERSISTENCE_JSON_READ
                        ? "json" : "xlon";
                    if ( !xl6IrCgenPersistenceParseAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter persistence parse ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    bool projection_ok_%u = xlir_persist_%s_%s_t%u(v%u, v%u, &v%u);\n",
                        (unsigned int)instruction->result,
                        format,
                        isRead ? "read" : "parse",
                        (unsigned int)instruction->payloadType,
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned int)module->operands[
                            instruction->firstOperand + 1],
                        (unsigned int)instruction->result);
                } else if ( xl6IrCollectionAlgebraRuntimeOpIs(
                                instruction->runtimeOp) ) {
                    if ( !xl6IrCgenEmitCollectionAlgebraInstruction(
                             out, module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter collection algebra ABI is unresolved");
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_ADD ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SUB ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MUL ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_DIV ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MOD ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHL ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHR ) {
                    int mode = instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_ADD
                        ? 0 : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SUB ? 1
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MUL ? 2
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_DIV ? 3
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MOD ? 4
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND ? 5
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR ? 6
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR ? 7
                        : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHL ? 8 : 9;
                    if ( instruction->immediateText != NULL || instruction->operandCount != 2 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter dynamic arithmetic ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_binary(v%u, v%u, %d);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1],
                                    mode);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_POS ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_NEG ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT ) {
                    int mode = instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_POS
                        ? 0 : instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_NEG ? 1 : 2;
                    if ( instruction->immediateText != NULL || instruction->operandCount != 1 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter dynamic unary ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_unary(v%u, %d);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand], mode);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_I64_APPROX ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_U64_APPROX ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_F64_APPROX ) {
                    const char* format = instruction->runtimeOp == XL6_IR_RUNTIME_I64_APPROX
                        ? "    v%u = xlir_i64_approx(v%u, v%u);\n"
                        : instruction->runtimeOp == XL6_IR_RUNTIME_U64_APPROX
                            ? "    v%u = xlir_u64_approx(v%u, v%u);\n"
                            : "    v%u = xlir_f64_approx((double)v%u, (double)v%u);\n";
                    if ( instruction->immediateText != NULL || instruction->operandCount != 2 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter numeric approximate ABI is unresolved");
                    xl6OutputPrintf(out, format,
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_APPROX ) {
                    if ( instruction->immediateText != NULL || instruction->operandCount != 2 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter string approximate ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xrtUtf8Similarity(v%u, v%u) >= 0.95;\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TRUTHY ) {
                    if ( instruction->immediateText != NULL || instruction->operandCount != 1 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter dynamic truth predicate ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xrtValueTruthy(v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_IS ) {
                    if ( instruction->operandCount != 1 || instruction->immediateText == NULL )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter class membership ABI is unresolved");
                    xl6OutputPrintf(out,
                                    "    v%u = xlir_xvalue_object_get_literal_i64(v%u, xrtStrView(\"$xlir.class.runtime-type\")) == (int64_t)%s;\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    instruction->immediateText);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_AS ) {
                    if ( instruction->operandCount != 1 || instruction->immediateText == NULL )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter checked class cast ABI is unresolved");
                    xl6OutputPrintf(out,
                                    "    if (xlir_xvalue_object_get_literal_i64(v%u, xrtStrView(\"$xlir.class.runtime-type\")) != (int64_t)%s) abort();\n"
                                    "    v%u = xrtValueRetain(v%u);\n",
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    instruction->immediateText,
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT ) {
                    if ( instruction->operandCount != 2 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter runtime operation ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_string_concat(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_COUNT ) {
                    xl6OutputPrintf(out,
                                    "    v%u = (int64_t)xrtUtf8Count(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_AT ) {
                    xl6OutputPrintf(out,
                                    "    v%u = xlir_string_utf8_at(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_STRING_CONTAINS ) {
                    xl6OutputPrintf(out,
                                    "    v%u = xrtStrContains(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_I64_RANGE_CONTAINS ) {
                    xl6OutputPrintf(out,
                                    "    v%u = xlir_i64_range_contains(v%u, v%u, v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1],
                                    (unsigned int)module->operands[instruction->firstOperand + 2],
                                    (unsigned int)module->operands[instruction->firstOperand + 3]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_TIME_TO_STRING ) {
                    if ( !xl6IrCgenRuntimeTimeToStringAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter time-to-string ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_time_to_string(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_TYPE_ID_TO_STRING ) {
                    if ( !xl6IrCgenRuntimeTypeIdToStringAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter type-id-to-string ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_string_take_cstr(xlir_type_id_to_string(v%u));\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[
                            instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING ) {
                    if ( !xl6IrCgenRuntimeClassDerivedStringAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter derived class string ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xl6ClassValueTextV1(v%u, &xlir_class_string_schema_t%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[
                             instruction->firstOperand],
                        (unsigned int)instruction->payloadType);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL ) {
                    if ( !xl6IrCgenRuntimeClassDerivedEqualAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter derived class equality ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    bool projection_ok_%u = xl6ClassRuntimeV3Get()->Equal(v%u, v%u, &v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned int)module->operands[
                            instruction->firstOperand + 1],
                        (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NULL ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueRetain(xrtValueNull());\n", (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BOOL ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueRetain(xrtValueBool(v%u));\n",
                                    (unsigned int)instruction->result,
                                     (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CHAR ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueChar(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_U64 ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueUInt(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_UNION_BOX ) {
                    const XL6_IrType* payload = xl6IrTypeGet(
                        &module->types, instruction->payloadType);
                    if ( payload == NULL ||
                         !xl6IrCgenRuntimeUnionBoxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter dynamic union box ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_union_box(&v%u, sizeof(v%u), 0x%016llXULL);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned long long)payload->typeValueId);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_UNION_UNBOX ) {
                    const XL6_IrType* payload = xl6IrTypeGet(
                        &module->types, instruction->payloadType);
                    if ( payload == NULL ||
                         !xl6IrCgenRuntimeUnionUnboxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter dynamic union unbox ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    xlir_xvalue_union_unbox(v%u, 0x%016llXULL, &v%u, sizeof(v%u));\n",
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned long long)payload->typeValueId,
                        (unsigned int)instruction->result,
                        (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX ) {
                    const XL6_IrInstruction* source = xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL);
                    const XL6_IrType* tagged = source != NULL
                        ? xl6IrTypeGet(&module->types, source->type) : NULL;
                    if ( tagged == NULL ||
                         !xl6IrCgenRuntimeTaggedNullableXValueAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter tagged nullable dynamic box ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_box_%s(v%u);\n",
                        (unsigned int)instruction->result, tagged->cName,
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_F64 ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueFloat(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR ) {
                    const XL6_IrInstruction* source = xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL);
                    const XL6_IrType* sourceType = source != NULL
                        ? xl6IrTypeGet(&module->types, source->type) : NULL;
                    if ( sourceType == NULL ||
                         !xl6IrCgenRuntimeForeignScalarBoxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                              "XLIR C emitter native scalar box ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xl6ValueBoxNativeScalarV1(&v%u, %u, 0x%016llXULL);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned)xl6IrNativeScalarProfileForCAbi(sourceType->cAbi),
                        (unsigned long long)xl6IrTypeGet(&module->types, instruction->runtimeIdentityType)->typeValueId);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_ERROR_REF ) {
                    const XL6_IrInstruction* source = xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL);
                    const XL6_IrType* sourceType = source != NULL
                        ? xl6IrTypeGet(&module->types, source->type) : NULL;
                    if ( sourceType == NULL ||
                         !xl6IrCgenRuntimeErrorXValueRefAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter dynamic error reference box ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xl6ErrorValueRefV1(v%u, 0x%016llXULL);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned long long)sourceType->typeValueId);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TIME ) {
                    if ( !xl6IrCgenRuntimeTimeBoxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter dynamic time boxing ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xrtValueTime(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_FUTURE_REF ) {
                    if ( !xl6IrCgenRuntimeFutureXValueRefAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter Future dynamic reference box ABI is unresolved");
                    xl6OutputPrintf(out,
                                    "    v%u = xl6PayloadBoxFutureV1(v%u, 0x%016llXULL);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned long long)xl6IrTypeGet(&module->types,instruction->runtimeIdentityType)->typeValueId);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX ) {
                    if ( !xl6IrCgenRuntimeNullableStringBoxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter nullable string dynamic box ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = v%u.Data != NULL ? xrtValueString(v%u) : xrtValueRetain(xrtValueNull());\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW ) {
                    if ( instruction->immediateText != NULL || instruction->operandCount != 1 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                             "XLIR C emitter runtime operation ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xrtValuePointer(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW ) {
                    xl6OutputPrintf(out, "    v%u = xrtValuePointer(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLONE ) {
                    xl6OutputPrintf(out, "    v%u = xl6ValueCloneV1(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_DEEP_CLONE ) {
                    xl6OutputPrintf(out, "    v%u = xl6ValueDeepCloneV1(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST ) {
                    int collectionTag;
                    if ( !xl6IrCgenRuntimeCollectionCheckedCastAbi(
                             module, instruction) ||
                         (collectionTag = xl6IrCgenCollectionXValueTag(
                              instruction->collectionAbi)) == 0 )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter checked collection cast ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xl6CollectionCheckedTakeV1(v%u, %d, 0, false, 0);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        collectionTag);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND ) {
                    const XL6_IrType* resultType =
                        xl6IrTypeGet(&module->types, instruction->type);
                    const XL6_IrType* payloadType =
                        xl6IrTypeGet(&module->types, instruction->payloadType);
                    int collectionTag;
                    if ( !xl6IrCgenRuntimeCollectionCheckedRebindAbi(
                             module, instruction) || resultType == NULL ||
                        (collectionTag = xl6IrCgenCollectionXValueTag(
                              instruction->collectionAbi)) == 0 || payloadType == NULL ||
                         instruction->collectionAdmissionAbi == XL6_IR_COLLECTION_ADMISSION_NONE )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter checked collection projection ABI is unresolved");
                    if (instruction->collectionAdmissionAbi==XL6_IR_COLLECTION_ADMISSION_NOMINAL) {
                        xl6OutputPrintf(out,
                            "    v%u = xl6CollectionCheckedNominalTakeV1(v%u, %d, &xlir_nominal_set_%u, %s);\n",
                            (unsigned int)instruction->result,
                            (unsigned int)module->operands[instruction->firstOperand],collectionTag,
                            (unsigned int)instruction->collectionAdmissionIdentitySet,
                            instruction->collectionAdmissionNullable ? "true" : "false");
                        return !xl6OutputHasFailed(out);
                    }
                    xl6OutputPrintf(out,
                        "    v%u = xl6CollectionCheckedTakeV1(v%u, %d, %u, %s, 0x%016llXULL);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        collectionTag, (unsigned int)payloadType->xvalueAdmissionAbi,
                        payloadType->xvalueAdmissionNullable ? "true" : "false",
                        (unsigned long long)payloadType->xvalueAdmissionTypeValueId);
                } else return 0;
    return 1;
}
