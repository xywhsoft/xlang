/* Public-XRT adapters selected by verified XLIR operations. Visitor arguments
 * are owned events, the callable is borrowed, and writer scalars/destruction
 * use the canonical XLIR physical ABI. */
static const char xl6XlonView[] =
    "static xstrview xl6_xlon_view(const char* text)\n"
    "{\n"
    "    xstrview view;\n"
    "    view.Data = text != 0 ? text : \"\";\n"
    "    view.Size = text != 0 ? strlen(text) : 0u;\n"
    "    return view;\n"
    "}\n"
    "\n";

static const char xl6XlonReadConfig[] =
    "static xvalue* xl6_xlon_ignore_custom(xstrview tag, xstrview payload, void* data)\n"
    "{\n"
    "    (void)tag; (void)payload; (void)data;\n"
    "    return xrtValueRetain(xrtValueNull());\n"
    "}\n"
    "\n"
    "static xxlonreadconfig xl6_xlon_read_config(bool ignoreUnsupported)\n"
    "{\n"
    "    xxlonreadconfig config;\n"
    "    xrtXlonReadConfigInit(&config);\n"
    "    if ( ignoreUnsupported ) {\n"
    "        config.Flags |= XXLON_READ_CUSTOM;\n"
    "        config.Decode = xl6_xlon_ignore_custom;\n"
    "    }\n"
    "    return config;\n"
    "}\n"
    "\n";

static const char xl6XlonWriteConfig[] =
    "static xxlonwriteconfig xl6_xlon_write_config(bool pretty, bool ignoreUnsupported)\n"
    "{\n"
    "    xxlonwriteconfig config;\n"
    "    xrtXlonWriteConfigInit(&config);\n"
    "    if ( pretty ) { config.Flags |= XXLON_WRITE_PRETTY; }\n"
    "    if ( ignoreUnsupported ) { config.Unsupported = XXLON_UNSUPPORTED_SKIP; }\n"
    "    return config;\n"
    "}\n"
    "\n";

static const char xl6XlonVisitor[] =
    "static xvalue* xl6_xlon_null(void)\n"
    "{\n"
    "    return xrtValueRetain(xrtValueNull());\n"
    "}\n"
    "\n"
    "static bool xl6_xlon_event_set(xvalue* event, const char* key, xvalue* value)\n"
    "{\n"
    "    if ( event == 0 || value == 0 ) {\n"
    "        xrtValueRelease(value);\n"
    "        return false;\n"
    "    }\n"
    "    return xrtValueObjectSetNew(event, xl6_xlon_view(key), value);\n"
    "}\n"
    "\n"
    "static xvalue* xl6_xlon_event_cstr(const char* text)\n"
    "{\n"
    "    return xrtValueString(xl6_xlon_view(text));\n"
    "}\n"
    "\n"
    "static const char* xl6_xlon_event_type_name(xxloneventtype type)\n"
    "{\n"
    "    switch ( type ) {\n"
    "        case XXLON_EVENT_NULL: return \"null\";\n"
    "        case XXLON_EVENT_BOOL: return \"bool\";\n"
    "        case XXLON_EVENT_INT: return \"int\";\n"
    "        case XXLON_EVENT_UINT: return \"uint\";\n"
    "        case XXLON_EVENT_CHAR: return \"char\";\n"
    "        case XXLON_EVENT_FLOAT: return \"float\";\n"
    "        case XXLON_EVENT_STRING: return \"string\";\n"
    "        case XXLON_EVENT_BYTES: return \"bytes\";\n"
    "        case XXLON_EVENT_TIME: return \"time\";\n"
    "        case XXLON_EVENT_CUSTOM: return \"custom\";\n"
    "        case XXLON_EVENT_ARRAY_BEGIN: return \"arrayBegin\";\n"
    "        case XXLON_EVENT_ARRAY_END: return \"arrayEnd\";\n"
    "        case XXLON_EVENT_INT_MAP_BEGIN: return \"listBegin\";\n"
    "        case XXLON_EVENT_INT_MAP_END: return \"listEnd\";\n"
    "        case XXLON_EVENT_SET_BEGIN: return \"setBegin\";\n"
    "        case XXLON_EVENT_SET_END: return \"setEnd\";\n"
    "        case XXLON_EVENT_OBJECT_BEGIN: return \"dictBegin\";\n"
    "        case XXLON_EVENT_OBJECT_END: return \"dictEnd\";\n"
    "        default: return \"unknown\";\n"
    "    }\n"
    "}\n"
    "\n"
    "static xvalue* xl6_xlon_event_time(long long value)\n"
    "{\n"
    "    char buffer[64];\n"
    "    size_t size = xrtTimeWriteRFC3339(buffer, sizeof(buffer), value, 0);\n"
    "    return size != 0 ? xrtValueString((xstrview){ buffer, size }) : xl6_xlon_null();\n"
    "}\n"
    "\n"
    "static xvalue* xl6_xlon_event_value(const xxlonevent* source)\n"
    "{\n"
    "    switch ( source->Type ) {\n"
    "        case XXLON_EVENT_BOOL: return xrtValueBool(source->Value.Boolean);\n"
    "        case XXLON_EVENT_INT: return xrtValueInt(source->Value.Integer);\n"
    "        case XXLON_EVENT_UINT: return xrtValueUInt(source->Value.Unsigned);\n"
    "        case XXLON_EVENT_CHAR: return xrtValueChar(source->Value.Character);\n"
    "        case XXLON_EVENT_FLOAT: return xrtValueFloat(source->Value.Float);\n"
    "        case XXLON_EVENT_STRING: return xrtValueString(source->Value.String);\n"
    "        case XXLON_EVENT_BYTES: return xrtValueBytes(source->Value.Bytes);\n"
    "        case XXLON_EVENT_TIME: return xl6_xlon_event_time(source->Value.Time);\n"
    "        case XXLON_EVENT_CUSTOM: return xrtValueString(source->Value.Tag.Payload);\n"
    "        default: return xl6_xlon_null();\n"
    "    }\n"
    "}\n"
    "\n"
    "static xvalue* xl6_xlon_event_index(const xxlonevent* source)\n"
    "{\n"
    "    if (source->Key.Type == XVALUE_KEY_INDEX) return xrtValueInt((int64_t)source->Key.Index);\n"
    "    if (source->Key.Type == XVALUE_KEY_INT) return xrtValueInt(source->Key.Integer);\n"
    "    return xl6_xlon_null();\n"
    "}\n"
    "\n"
    "static xxlonvisitaction xl6_xlon_visit_proc(const xxlonevent* source, void* data)\n"
    "{\n"
    "    xl6_serialization_visitor_context* context = (xl6_serialization_visitor_context*)data;\n"
    "    xvalue* event;\n"
    "    bool keepGoing;\n"
    "    if ( source == 0 || context == 0 ) { return XXLON_VISIT_FAIL; }\n"
    "    event = xrtValueObject();\n"
    "    if ( event == 0 ||\n"
    "         !xl6_xlon_event_set(event, \"type\", xl6_xlon_event_cstr(xl6_xlon_event_type_name(source->Type))) ||\n"
    "         !xl6_xlon_event_set(event, \"depth\", xrtValueInt((long long)source->Depth)) ||\n"
    "         !xl6_xlon_event_set(event, \"key\", source->Key.Type == XVALUE_KEY_STRING ?\n"
    "             xrtValueString(source->Key.String) : xl6_xlon_null()) ||\n"
    "         !xl6_xlon_event_set(event, \"index\", xl6_xlon_event_index(source)) ||\n"
    "         !xl6_xlon_event_set(event, \"value\", xl6_xlon_event_value(source)) ||\n"
    "         !xl6_xlon_event_set(event, \"byteOffset\", xrtValueUInt((uint64_t)source->Location.Offset)) ||\n"
    "         !xl6_xlon_event_set(event, \"line\", xrtValueUInt((uint64_t)source->Location.Line)) ||\n"
    "         !xl6_xlon_event_set(event, \"column\", xrtValueUInt((uint64_t)source->Location.Column)) ||\n"
    "         !xl6_xlon_event_set(event, \"raw\", source->Raw.Data != NULL ? xrtValueString(source->Raw) : xl6_xlon_null()) ||\n"
    "         !xl6_xlon_event_set(event, \"tag\", source->Type == XXLON_EVENT_CUSTOM ? xrtValueString(source->Value.Tag.Name) : xl6_xlon_null()) ) {\n"
    "        xl6_serialization_callback_fail(context);\n"
    "        xrtValueRelease(event);\n"
    "        return XXLON_VISIT_FAIL;\n"
    "    }\n"
    "    keepGoing = false;\n"
    "    if ( !xl6_serialization_callback_call(context, event, &keepGoing) ) {\n"
    "        return XXLON_VISIT_FAIL;\n"
    "    }\n"
    "    return keepGoing ? XXLON_VISIT_NEXT : XXLON_VISIT_STOP;\n"
    "}\n"
    "\n"
    "XL6_Serialization_Visit_Result xl6_xrt_xlon_visit_options(xlir_string text, XL6_Function visitor, bool pretty, bool ignoreEncode, bool ignoreDecode)\n"
    "{\n"
    "    xl6_serialization_visitor_context context = {0};\n"
    "    xxlonreadconfig config;\n"
    "    int status;\n"
    "    (void)pretty; (void)ignoreEncode;\n"
    "    if ( text.Data == 0 || (visitor.invoke_var == 0 && visitor.invoke_xcall == 0) ) { return xl6_serialization_callback_finish(&context, -1); }\n"
    "    context.Visitor = visitor;\n"
    "    config = xl6_xlon_read_config(ignoreDecode);\n"
    "    status = xrtXlonVisit(text, &config, xl6_xlon_visit_proc, &context);\n"
    "    return xl6_serialization_callback_finish(&context, status);\n"
    "}\n"
    "\n";

static const char xl6XlonParse[] =
    "#include <__xl_runtime/xl6_serialization_read.h>\n"
    "xlir_result_xvalue xl6_xrt_xlon_parse_options(xlir_string text, bool pretty, bool ignoreEncode, bool ignoreDecode)\n"
    "{\n"
    "    xxlonreadconfig config;\n"
    "    xvalue* value;\n"
    "    xerror* prior = xrtTakeError();\n"
    "    (void)pretty; (void)ignoreEncode;\n"
    "    if ( text.Data == 0 ) { xrtSetErrorTake(prior); return (xlir_result_xvalue){ true, NULL }; }\n"
    "    if ( !ignoreDecode ) value = xrtXlonParse(text);\n"
    "    else { config = xl6_xlon_read_config(true); value = xrtXlonRead(text, &config); }\n"
    "    bool ok = xl6SerializeReadFinishV1(value, prior);\n"
    "    return (xlir_result_xvalue){ ok, ok ? value : NULL };\n"
    "}\n"
    "\n";

static const char xl6XlonValid[] =
    "static xxlonvisitaction xl6_xlon_validate_proc(const xxlonevent* source, void* data)\n"
    "{\n"
    "    (void)source; (void)data;\n"
    "    return XXLON_VISIT_NEXT;\n"
    "}\n"
    "\n"
    "bool xl6_xrt_xlon_valid_options(xlir_string text, bool pretty, bool ignoreEncode, bool ignoreDecode)\n"
    "{\n"
    "    xxlonreadconfig config;\n"
    "    bool valid;\n"
    "    (void)pretty; (void)ignoreEncode;\n"
    "    if ( text.Data == 0 ) { return false; }\n"
    "    if ( !ignoreDecode ) valid = xrtXlonValid(text);\n"
    "    else { config = xl6_xlon_read_config(true); valid = xrtXlonVisit(text, &config, xl6_xlon_validate_proc, 0) == 0; }\n"
    "    if (!valid) xrtClearError();\n"
    "    return valid;\n"
    "}\n"
    "\n";

static const char xl6XlonRead[] =
    "#include <__xl_runtime/xl6_serialization_read.h>\n"
    "xlir_result_xvalue xl6_xrt_xlon_read_options(xlir_string path, bool pretty, bool ignoreEncode, bool ignoreDecode)\n"
    "{\n"
    "    xxlonreadconfig config;\n"
    "    xvalue* value;\n"
    "    xerror* prior = xrtTakeError();\n"
    "    (void)pretty; (void)ignoreEncode;\n"
    "    const char* cpath = xlir_string_cstr(path);\n"
    "    if ( cpath == 0 ) value = NULL;\n"
    "    else if ( !ignoreDecode ) value = xrtXlonParseFile(cpath);\n"
    "    else { config = xl6_xlon_read_config(true); value = xrtXlonReadFile(cpath, &config); }\n"
    "    bool ok = xl6SerializeReadFinishV1(value, prior);\n"
    "    return (xlir_result_xvalue){ ok, ok ? value : NULL };\n"
    "}\n"
    "\n";

static const char xl6XlonStringify[] =
    "void* xl6_xrt_xlon_stringify_options(xvalue* value, bool pretty, bool ignoreEncode, bool ignoreDecode)\n"
    "{\n"
    "    xxlonwriteconfig config;\n"
    "    xxlonwriter* writer;\n"
    "    void* text;\n"
    "    (void)ignoreDecode;\n"
    "    if ( value == 0 ) { xrtClearError(); return 0; }\n"
    "    if ( !ignoreEncode ) { text = xrtXlonStringify(value, pretty, 0);\n"
    "        xrtClearError();\n"
    "        return text; }\n"
    "    config = xl6_xlon_write_config(pretty, true);\n"
    "    writer = xrtXlonWriterCreate(&config);\n"
    "    if ( writer == 0 ) { xrtClearError(); return 0; }\n"
    "    text = xrtXlonWriterValue(writer, value) && xrtXlonWriterFinish(writer) ?\n"
    "        xrtXlonWriterTake(writer, 0) : 0;\n"
    "    xrtXlonWriterFree(writer);\n"
    "    xrtClearError();\n"
    "    return text;\n"
    "}\n"
    "\n";

static const char xl6XlonWrite[] =
    "bool xl6_xrt_xlon_write_options(xlir_string path, xvalue* value, bool pretty, bool ignoreEncode, bool ignoreDecode)\n"
    "{\n"
    "    xxlonwriteconfig config;\n"
    "    (void)ignoreDecode;\n"
    "    const char* cpath = xlir_string_cstr(path);\n"
    "    bool ok;\n"
    "    if ( cpath == 0 || value == 0 ) { xrtClearError(); return false; }\n"
    "    if ( !ignoreEncode ) ok = xrtXlonStringifyFile(cpath, value, pretty);\n"
    "    else { config = xl6_xlon_write_config(pretty, true); ok = xrtXlonWriteFile(cpath, value, &config); }\n"
    "    if (!ok) xrtClearError();\n"
    "    return ok;\n"
    "}\n"
    "\n";


void xl6CgenEmitXlonRuntime(XL6_Output* out, unsigned operations)
{
    if (out == NULL || operations == 0) return;
    if (operations & ~XL6_CGEN_XLON_WRITER) {
        xl6OutputPuts("extern size_t strlen(const char*);\n", out);
        xl6OutputPuts(xl6XlonView, out);
    }
    if (operations & (XL6_CGEN_XLON_PARSE | XL6_CGEN_XLON_VALID |
                      XL6_CGEN_XLON_READ | XL6_CGEN_XLON_VISIT))
        xl6OutputPuts(xl6XlonReadConfig, out);
    if (operations & (XL6_CGEN_XLON_STRINGIFY | XL6_CGEN_XLON_WRITE))
        xl6OutputPuts(xl6XlonWriteConfig, out);
    if (operations & XL6_CGEN_XLON_PARSE) xl6OutputPuts(xl6XlonParse, out);
    if (operations & XL6_CGEN_XLON_VALID) xl6OutputPuts(xl6XlonValid, out);
    if (operations & XL6_CGEN_XLON_READ) xl6OutputPuts(xl6XlonRead, out);
    if (operations & XL6_CGEN_XLON_STRINGIFY) xl6OutputPuts(xl6XlonStringify, out);
    if (operations & XL6_CGEN_XLON_WRITE) xl6OutputPuts(xl6XlonWrite, out);
    if (operations & XL6_CGEN_XLON_VISIT) {
        xl6OutputPuts(xl6XlonVisitor, out);
    }
    if (operations & XL6_CGEN_XLON_WRITER) {
        xl6OutputPuts("#include <__xl_runtime/xl6_xlon_writer.h>\n", out);
    }
}
