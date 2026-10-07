/* Configured serialization uses one borrowed dynamic options object and an
 * exact XLIR bridge for each operation. No XRT algorithm is reimplemented. */
void xl6CgenEmitStructuredOptionsRuntime(XL6_Output* out, unsigned operations)
{
    if (out == NULL || operations == 0) return;
    xl6OutputPuts("#include <__xl_runtime/xl6_serialization_options.h>\n"
                 "#include <__xl_runtime/xl6_serialization_read.h>\n", out);
    if (operations & XL6_CGEN_CONFIG_READ_MASK)
        xl6OutputPuts(
            "static xlir_result_xvalue xl6_config_read_finish(xvalue* value, xerror* prior)\n"
            "{ bool ok = xl6SerializeReadFinishV1(value, prior);\n"
            "  return (xlir_result_xvalue){ ok, ok ? value : NULL }; }\n"
            "static xlir_result_xvalue xl6_config_read_option_error(xerror* prior)\n"
            "{ if (xrtGetError() == NULL) xrtSetErrorKind(XERR_ARGUMENT);\n"
            "  xrtErrorFree(prior); return (xlir_result_xvalue){ false, NULL }; }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_WRITE_FILE_MASK)
        xl6OutputPuts(
            "static xlir_result_bool xl6_config_write_finish(bool ok, xerror* prior)\n"
            "{ xrtClearError(); xrtSetErrorTake(prior); return (xlir_result_bool){ true, ok }; }\n"
            "static xlir_result_bool xl6_config_write_option_error(xerror* prior)\n"
            "{ if (xrtGetError() == NULL) xrtSetErrorKind(XERR_ARGUMENT);\n"
            "  xrtErrorFree(prior); return (xlir_result_bool){ false, false }; }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_PARSE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_json_read_configured(xlir_string text, xvalue* options)\n"
            "{ xjsonreadconfig config; xerror* prior = xrtTakeError(); xvalue* value;\n"
            "  if (!xl6JsonReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  value = text.Data != NULL ? xrtJsonRead(text, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_READ_FILE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_json_read_file_configured(xlir_string path, xvalue* options)\n"
            "{ xjsonreadconfig config; xerror* prior = xrtTakeError(); const char* name; xvalue* value;\n"
            "  if (!xl6JsonReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  name = xlir_string_cstr(path); value = name != NULL ? xrtJsonReadFile(name, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_STRINGIFY)
        xl6OutputPuts(
            "void* xl6_xrt_json_stringify_configured(xvalue* value, xvalue* options)\n"
            "{ xjsonwriteconfig config; xbuffer* output;\n"
            "  if (value == NULL || !xl6JsonWriteOptions(options, &config)) return NULL;\n"
            "  output = xrtBufferCreate(); if (output == NULL) return NULL;\n"
            "  if (!xrtJsonWrite(value, &config, xl6SerializeBufferWrite, output)) { xrtBufferDestroy(output); return NULL; }\n"
            "  return xl6SerializeBufferTake(output); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_WRITE_FILE)
        xl6OutputPuts(
            "xlir_result_bool xl6_xrt_json_write_file_configured(xlir_string path, xvalue* value, xvalue* options)\n"
            "{ xjsonwriteconfig config; xerror* prior = xrtTakeError(); const char* name; bool ok;\n"
            "  if (!xl6JsonWriteOptions(options, &config)) return xl6_config_write_option_error(prior);\n"
            "  name = xlir_string_cstr(path); ok = name != NULL && value != NULL && xrtJsonWriteFile(name, value, &config);\n"
            "  return xl6_config_write_finish(ok, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_PARSE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_xlon_read_configured(xlir_string text, xvalue* options)\n"
            "{ xxlonreadconfig config; xerror* prior = xrtTakeError(); xvalue* value;\n"
            "  if (!xl6XlonReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  value = text.Data != NULL ? xrtXlonRead(text, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_READ_FILE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_xlon_read_file_configured(xlir_string path, xvalue* options)\n"
            "{ xxlonreadconfig config; xerror* prior = xrtTakeError(); const char* name; xvalue* value;\n"
            "  if (!xl6XlonReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  name = xlir_string_cstr(path); value = name != NULL ? xrtXlonReadFile(name, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_STRINGIFY)
        xl6OutputPuts(
            "void* xl6_xrt_xlon_stringify_configured(xvalue* value, xvalue* options)\n"
            "{ xxlonwriteconfig config; xbuffer* output;\n"
            "  if (value == NULL || !xl6XlonWriteOptions(options, &config)) return NULL;\n"
            "  output = xrtBufferCreate(); if (output == NULL) return NULL;\n"
            "  if (!xrtXlonWrite(value, &config, xl6SerializeBufferWrite, output)) { xrtBufferDestroy(output); return NULL; }\n"
            "  return xl6SerializeBufferTake(output); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_WRITE_FILE)
        xl6OutputPuts(
            "xlir_result_bool xl6_xrt_xlon_write_file_configured(xlir_string path, xvalue* value, xvalue* options)\n"
            "{ xxlonwriteconfig config; xerror* prior = xrtTakeError(); const char* name; bool ok;\n"
            "  if (!xl6XlonWriteOptions(options, &config)) return xl6_config_write_option_error(prior);\n"
            "  name = xlir_string_cstr(path); ok = name != NULL && value != NULL && xrtXlonWriteFile(name, value, &config);\n"
            "  return xl6_config_write_finish(ok, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSONL_PARSE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_jsonl_read_configured(xlir_string text, xvalue* options)\n"
            "{ xjsonlreadconfig config; xerror* prior = xrtTakeError(); xvalue* value;\n"
            "  if (!xl6JsonlReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  value = text.Data != NULL ? xrtJsonlRead(text, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSONL_READ_FILE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_jsonl_read_file_configured(xlir_string path, xvalue* options)\n"
            "{ xjsonlreadconfig config; xerror* prior = xrtTakeError(); const char* name; xvalue* value;\n"
            "  if (!xl6JsonlReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  name = xlir_string_cstr(path); value = name != NULL ? xrtJsonlReadFile(name, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSONL_STRINGIFY)
        xl6OutputPuts(
            "void* xl6_xrt_jsonl_stringify_configured(xvalue* value, xvalue* options)\n"
            "{ xjsonlwriteconfig config; xbuffer* output;\n"
            "  if (value == NULL || !xl6JsonlWriteOptions(options, &config)) return NULL;\n"
            "  output = xrtBufferCreate(); if (output == NULL) return NULL;\n"
            "  if (!xrtJsonlWrite(value, &config, xl6SerializeBufferWrite, output)) { xrtBufferDestroy(output); return NULL; }\n"
            "  return xl6SerializeBufferTake(output); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSONL_WRITE_FILE)
        xl6OutputPuts(
            "xlir_result_bool xl6_xrt_jsonl_write_file_configured(xlir_string path, xvalue* value, xvalue* options)\n"
            "{ xjsonlwriteconfig config; xerror* prior = xrtTakeError(); const char* name; bool ok;\n"
            "  if (!xl6JsonlWriteOptions(options, &config)) return xl6_config_write_option_error(prior);\n"
            "  name = xlir_string_cstr(path); ok = name != NULL && value != NULL && xrtJsonlWriteFile(name, value, &config);\n"
            "  return xl6_config_write_finish(ok, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLONL_PARSE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_xlonl_read_configured(xlir_string text, xvalue* options)\n"
            "{ xxlonlreadconfig config; xerror* prior = xrtTakeError(); xvalue* value;\n"
            "  if (!xl6XlonlReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  value = text.Data != NULL ? xrtXlonlRead(text, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLONL_READ_FILE)
        xl6OutputPuts(
            "xlir_result_xvalue xl6_xrt_xlonl_read_file_configured(xlir_string path, xvalue* options)\n"
            "{ xxlonlreadconfig config; xerror* prior = xrtTakeError(); const char* name; xvalue* value;\n"
            "  if (!xl6XlonlReadOptions(options, &config)) return xl6_config_read_option_error(prior);\n"
            "  name = xlir_string_cstr(path); value = name != NULL ? xrtXlonlReadFile(name, &config) : NULL;\n"
            "  return xl6_config_read_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLONL_STRINGIFY)
        xl6OutputPuts(
            "void* xl6_xrt_xlonl_stringify_configured(xvalue* value, xvalue* options)\n"
            "{ xxlonlwriteconfig config; xbuffer* output;\n"
            "  if (value == NULL || !xl6XlonlWriteOptions(options, &config)) return NULL;\n"
            "  output = xrtBufferCreate(); if (output == NULL) return NULL;\n"
            "  if (!xrtXlonlWrite(value, &config, xl6SerializeBufferWrite, output)) { xrtBufferDestroy(output); return NULL; }\n"
            "  return xl6SerializeBufferTake(output); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLONL_WRITE_FILE)
        xl6OutputPuts(
            "xlir_result_bool xl6_xrt_xlonl_write_file_configured(xlir_string path, xvalue* value, xvalue* options)\n"
            "{ xxlonlwriteconfig config; xerror* prior = xrtTakeError(); const char* name; bool ok;\n"
            "  if (!xl6XlonlWriteOptions(options, &config)) return xl6_config_write_option_error(prior);\n"
            "  name = xlir_string_cstr(path); ok = name != NULL && value != NULL && xrtXlonlWriteFile(name, value, &config);\n"
            "  return xl6_config_write_finish(ok, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_WRITE_TO_MASK)
        xl6OutputPuts(
            "static bool xl6_serialization_sink_write(xbytesview data, void* user)\n"
            "{ xl6_serialization_visitor_context* context = (xl6_serialization_visitor_context*)user;\n"
            "  xvalue* chunk = xrtValueBytes(data); bool keepGoing = false;\n"
            "  if (chunk == NULL) { xl6_serialization_callback_fail(context); return false; }\n"
            "  if (!xl6_serialization_callback_call(context, chunk, &keepGoing)) return false;\n"
            "  if (!keepGoing) { xrtSetErrorKind(XERR_IO); xl6_serialization_callback_fail(context); return false; }\n"
            "  return true; }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_WRITE_TO)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_json_write_to_configured(xvalue* value, XL6_Function sink, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xjsonwriteconfig config; bool written;\n"
            "  context.Visitor = sink;\n"
            "  if (value == NULL || sink.invoke_var == NULL || !xl6JsonWriteOptions(options, &config))\n"
            "    return xl6_serialization_callback_finish(&context, 0);\n"
            "  written = xrtJsonWrite(value, &config, xl6_serialization_sink_write, &context);\n"
            "  return xl6_serialization_callback_finish(&context, written ? 1 : 0); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_WRITE_TO)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_xlon_write_to_configured(xvalue* value, XL6_Function sink, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xxlonwriteconfig config; bool written;\n"
            "  context.Visitor = sink;\n"
            "  if (value == NULL || sink.invoke_var == NULL || !xl6XlonWriteOptions(options, &config))\n"
            "    return xl6_serialization_callback_finish(&context, 0);\n"
            "  written = xrtXlonWrite(value, &config, xl6_serialization_sink_write, &context);\n"
            "  return xl6_serialization_callback_finish(&context, written ? 1 : 0); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSONL_WRITE_TO)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_jsonl_write_to_configured(xvalue* value, XL6_Function sink, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xjsonlwriteconfig config; bool written;\n"
            "  context.Visitor = sink;\n"
            "  if (value == NULL || sink.invoke_var == NULL || !xl6JsonlWriteOptions(options, &config))\n"
            "    return xl6_serialization_callback_finish(&context, 0);\n"
            "  written = xrtJsonlWrite(value, &config, xl6_serialization_sink_write, &context);\n"
            "  return xl6_serialization_callback_finish(&context, written ? 1 : 0); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLONL_WRITE_TO)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_xlonl_write_to_configured(xvalue* value, XL6_Function sink, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xxlonlwriteconfig config; bool written;\n"
            "  context.Visitor = sink;\n"
            "  if (value == NULL || sink.invoke_var == NULL || !xl6XlonlWriteOptions(options, &config))\n"
            "    return xl6_serialization_callback_finish(&context, 0);\n"
            "  written = xrtXlonlWrite(value, &config, xl6_serialization_sink_write, &context);\n"
            "  return xl6_serialization_callback_finish(&context, written ? 1 : 0); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_QUOTE)
        xl6OutputPuts(
            "void* xl6_xrt_json_quote_configured(xlir_string text, xvalue* options)\n"
            "{ uint32_t flags; xbuffer* output;\n"
            "  if (text.Data == NULL || !xl6JsonQuoteOptions(options, &flags)) return NULL;\n"
            "  output = xrtBufferCreate(); if (output == NULL) return NULL;\n"
            "  if (!xrtJsonQuoteWrite(text, flags, xl6SerializeBufferWrite, output, NULL))\n"
            "    { xrtBufferDestroy(output); return NULL; }\n"
            "  return xl6SerializeBufferTake(output); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_QUOTE_TO)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_json_quote_to_configured(xlir_string text, XL6_Function sink, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; uint32_t flags; bool written;\n"
            "  context.Visitor = sink;\n"
            "  if (text.Data == NULL || sink.invoke_var == NULL || !xl6JsonQuoteOptions(options, &flags))\n"
            "    return xl6_serialization_callback_finish(&context, 0);\n"
            "  written = xrtJsonQuoteWrite(text, flags, xl6_serialization_sink_write, &context, NULL);\n"
            "  return xl6_serialization_callback_finish(&context, written ? 1 : 0); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_VISIT)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_json_visit_configured(xlir_string text, XL6_Function visitor, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xjsonreadconfig config; int status;\n"
            "  context.Visitor = visitor; context.PriorError = xrtTakeError();\n"
            "  if ((visitor.invoke_var == NULL && visitor.invoke_xcall == NULL) || !xl6JsonReadOptions(options, &config)) {\n"
            "      if (xrtGetError() == NULL) xrtSetErrorKind(XERR_ARGUMENT);\n"
            "      xl6_serialization_callback_fail(&context); return xl6_serialization_visit_finish(&context, -1); }\n"
            "  if (text.Data == NULL) return xl6_serialization_visit_finish(&context, -1);\n"
            "  status = xrtJsonVisit(text, &config, xl6_json_stream_visit_proc, &context);\n"
            "  return xl6_serialization_visit_finish(&context, status); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_VISIT)
        xl6OutputPuts(
            "XL6_Serialization_Visit_Result xl6_xrt_xlon_visit_configured(xlir_string text, XL6_Function visitor, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xxlonreadconfig config; int status;\n"
            "  context.Visitor = visitor; context.PriorError = xrtTakeError();\n"
            "  if ((visitor.invoke_var == NULL && visitor.invoke_xcall == NULL) || !xl6XlonReadOptions(options, &config)) {\n"
            "      if (xrtGetError() == NULL) xrtSetErrorKind(XERR_ARGUMENT);\n"
            "      xl6_serialization_callback_fail(&context); return xl6_serialization_visit_finish(&context, -1); }\n"
            "  if (text.Data == NULL) return xl6_serialization_visit_finish(&context, -1);\n"
            "  status = xrtXlonVisit(text, &config, xl6_xlon_visit_proc, &context);\n"
            "  return xl6_serialization_visit_finish(&context, status); }\n\n", out);
    if (operations & (XL6_CGEN_CONFIG_JSON_VALID | XL6_CGEN_CONFIG_XLON_VALID |
                      XL6_CGEN_CONFIG_JSONL_VALID | XL6_CGEN_CONFIG_XLONL_VALID))
        xl6OutputPuts(
            "static xlir_result_c_bool xl6_config_validation_finish(xvalue* value, xerror* prior)\n"
            "{ xerror* failure = xrtTakeError(); const xerror* cause; bool syntax = false;\n"
            "  if (value != NULL) { xrtValueRelease(value); xrtErrorFree(failure); xrtSetErrorTake(prior); return (xlir_result_c_bool){ true, true }; }\n"
            "  if (failure != NULL && xrtErrorKind(failure) == XERR_PROTOCOL) {\n"
            "    syntax = true; for (cause = xrtErrorCause(failure); cause != NULL; cause = xrtErrorCause(cause))\n"
            "      if (xrtErrorKind(cause) != XERR_PROTOCOL && xrtErrorKind(cause) != XERR_VALUE) syntax = false; }\n"
            "  if (syntax || failure == NULL) { xrtErrorFree(failure); xrtSetErrorTake(prior); return (xlir_result_c_bool){ true, false }; }\n"
            "  xrtErrorFree(prior); xrtSetErrorTake(failure); return (xlir_result_c_bool){ false, false }; }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSON_VALID)
        xl6OutputPuts(
            "xlir_result_c_bool xl6_xrt_json_valid_configured(xlir_string text, xvalue* options)\n"
            "{ xjsonreadconfig config; xerror* prior = xrtTakeError(); xvalue* value = NULL;\n"
            "  if (text.Data != NULL && xl6JsonReadOptions(options, &config)) value = xrtJsonRead(text, &config);\n"
            "  return xl6_config_validation_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_VALID)
        xl6OutputPuts(
            "xlir_result_c_bool xl6_xrt_xlon_valid_configured(xlir_string text, xvalue* options)\n"
            "{ xxlonreadconfig config; xerror* prior = xrtTakeError(); xvalue* value = NULL;\n"
            "  if (text.Data != NULL && xl6XlonReadOptions(options, &config)) value = xrtXlonRead(text, &config);\n"
            "  return xl6_config_validation_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_JSONL_VALID)
        xl6OutputPuts(
            "xlir_result_c_bool xl6_xrt_jsonl_valid_configured(xlir_string text, xvalue* options)\n"
            "{ xjsonlreadconfig config; xerror* prior = xrtTakeError(); xvalue* value = NULL;\n"
            "  if (text.Data != NULL && xl6JsonlReadOptions(options, &config)) value = xrtJsonlRead(text, &config);\n"
            "  return xl6_config_validation_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLONL_VALID)
        xl6OutputPuts(
            "xlir_result_c_bool xl6_xrt_xlonl_valid_configured(xlir_string text, xvalue* options)\n"
            "{ xxlonlreadconfig config; xerror* prior = xrtTakeError(); xvalue* value = NULL;\n"
            "  if (text.Data != NULL && xl6XlonlReadOptions(options, &config)) value = xrtXlonlRead(text, &config);\n"
            "  return xl6_config_validation_finish(value, prior); }\n\n", out);
    if (operations & XL6_CGEN_CONFIG_XLON_DECODE)
        xl6OutputPuts(
            "static xvalue* xl6_xlon_decode_callback(xstrview tag, xstrview payload, void* user)\n"
            "{ xl6_serialization_visitor_context* context = (xl6_serialization_visitor_context*)user;\n"
            "  xerror* prior = xrtTakeError(); xvalue* event = xrtValueObject(); xvalue* result; xvalue* args[1];\n"
            "  if (event == NULL || !xrtValueObjectSetNew(event, xlir_literal_view(\"tag\"), xrtValueString(tag)) ||\n"
            "      !xrtValueObjectSetNew(event, xlir_literal_view(\"payload\"), xrtValueString(payload))) {\n"
            "    xrtValueRelease(event); xl6_serialization_callback_fail(context); xrtSetErrorTake(prior); return NULL; }\n"
            "  args[0] = event; result = context->Visitor.invoke_var(context->Visitor.env, 1, args);\n"
            "  if (result == NULL) xl6_serialization_callback_fail(context);\n"
            "  xrtValueRelease(event); xrtSetErrorTake(prior); return result; }\n\n"
            "xlir_result_xvalue xl6_xrt_xlon_decode_configured(xlir_string text, XL6_Function decoder, xvalue* options)\n"
            "{ xl6_serialization_visitor_context context = {0}; xxlonreadconfig config; xerror* prior; xerror* failure; xvalue* value;\n"
            "  const xerror* cause; bool syntax; context.Visitor = decoder; prior = xrtTakeError();\n"
            "  if (text.Data == NULL) { xrtSetErrorTake(prior); return (xlir_result_xvalue){ true, NULL }; }\n"
            "  if (decoder.invoke_var == NULL || !xl6XlonReadOptions(options, &config)) {\n"
            "    failure = xrtTakeError(); xrtErrorFree(prior); xrtSetErrorTake(failure);\n"
            "    return (xlir_result_xvalue){ false, NULL }; }\n"
            "  config.Flags |= XXLON_READ_CUSTOM; config.Decode = xl6_xlon_decode_callback; config.DecodeData = &context;\n"
            "  value = xrtXlonRead(text, &config); failure = xrtTakeError();\n"
            "  if (context.CallbackFailed) { xrtValueRelease(value); xrtErrorFree(failure); xrtErrorFree(prior);\n"
            "    xrtSetErrorTake(context.CallbackError); return (xlir_result_xvalue){ false, NULL }; }\n"
            "  if (value != NULL) { xrtErrorFree(failure); xrtSetErrorTake(prior); return (xlir_result_xvalue){ true, value }; }\n"
            "  syntax = failure != NULL && xrtErrorKind(failure) == XERR_PROTOCOL;\n"
            "  for (cause = syntax ? xrtErrorCause(failure) : NULL; cause != NULL; cause = xrtErrorCause(cause))\n"
            "    if (xrtErrorKind(cause) != XERR_PROTOCOL && xrtErrorKind(cause) != XERR_VALUE) syntax = false;\n"
            "  if (syntax || failure == NULL) { xrtErrorFree(failure); xrtSetErrorTake(prior);\n"
            "    return (xlir_result_xvalue){ true, NULL }; }\n"
            "  xrtErrorFree(prior); xrtSetErrorTake(failure); return (xlir_result_xvalue){ false, NULL }; }\n\n", out);
}
