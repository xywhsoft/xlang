/* Public XRT stream adapters. Visitor delivery borrows the callable and
 * consumes one owned event; the callback result is owned and released once.
 * Writer scalars and destruction use the canonical XLIR physical ABI. */
static const char xl6JsonStreamView[] =
    "static xstrview xl6_json_stream_view(const char* text)\n"
    "{\n"
    "    xstrview view;\n"
    "    view.Data = text != 0 ? text : \"\";\n"
    "    view.Size = text != 0 ? strlen(text) : 0u;\n"
    "    return view;\n"
    "}\n\n";

static const char xl6JsonStreamVisitor[] =
    "static xvalue* xl6_json_stream_null(void)\n"
    "{\n"
    "    return xrtValueRetain(xrtValueNull());\n"
    "}\n\n"
    "static bool xl6_json_stream_event_set(xvalue* event, const char* key, xvalue* value)\n"
    "{\n"
    "    if ( event == 0 || value == 0 ) {\n"
    "        xrtValueRelease(value);\n"
    "        return false;\n"
    "    }\n"
    "    return xrtValueObjectSetNew(event, xl6_json_stream_view(key), value);\n"
    "}\n\n"
    "static xvalue* xl6_json_stream_event_string(xstrview text)\n"
    "{\n"
    "    return xrtValueString(text);\n"
    "}\n\n"
    "static xvalue* xl6_json_stream_event_cstr(const char* text)\n"
    "{\n"
    "    return xrtValueString(xl6_json_stream_view(text));\n"
    "}\n\n"
    "static const char* xl6_json_stream_event_type_name(xjsoneventtype type)\n"
    "{\n"
    "    switch ( type ) {\n"
    "        case XJSON_EVENT_NULL: return \"null\";\n"
    "        case XJSON_EVENT_BOOL: return \"bool\";\n"
    "        case XJSON_EVENT_INT: return \"int\";\n"
    "        case XJSON_EVENT_UINT: return \"uint\";\n"
    "        case XJSON_EVENT_FLOAT: return \"float\";\n"
    "        case XJSON_EVENT_STRING: return \"string\";\n"
    "        case XJSON_EVENT_ARRAY_BEGIN: return \"arrayBegin\";\n"
    "        case XJSON_EVENT_ARRAY_END: return \"arrayEnd\";\n"
    "        case XJSON_EVENT_OBJECT_BEGIN: return \"objectBegin\";\n"
    "        case XJSON_EVENT_OBJECT_END: return \"objectEnd\";\n"
    "        default: return \"unknown\";\n"
    "    }\n"
    "}\n\n"
    "static xvalue* xl6_json_stream_event_value(const xjsonevent* source)\n"
    "{\n"
    "    switch ( source->Type ) {\n"
    "        case XJSON_EVENT_BOOL: return xrtValueBool(source->Value.Boolean);\n"
    "        case XJSON_EVENT_INT: return xrtValueInt(source->Value.Integer);\n"
    "        case XJSON_EVENT_UINT: return xrtValueUInt(source->Value.Unsigned);\n"
    "        case XJSON_EVENT_FLOAT: return xrtValueFloat(source->Value.Float);\n"
    "        case XJSON_EVENT_STRING: return xl6_json_stream_event_string(source->Value.String);\n"
    "        default: return xl6_json_stream_null();\n"
    "    }\n"
    "}\n\n"
    "static xjsonvisitaction xl6_json_stream_visit_proc(const xjsonevent* source, void* data)\n"
    "{\n"
    "    xl6_serialization_visitor_context* context = (xl6_serialization_visitor_context*)data;\n"
    "    xvalue* event;\n"
    "    bool keepGoing;\n"
    "    if ( source == 0 || context == 0 ) { return XJSON_VISIT_FAIL; }\n"
    "    event = xrtValueObject();\n"
    "    if ( event == 0 ||\n"
    "         !xl6_json_stream_event_set(event, \"type\", xl6_json_stream_event_cstr(xl6_json_stream_event_type_name(source->Type))) ||\n"
    "         !xl6_json_stream_event_set(event, \"depth\", xrtValueInt((long long)source->Depth)) ||\n"
    "         !xl6_json_stream_event_set(event, \"key\", source->HasName ? xl6_json_stream_event_string(source->Name) : xl6_json_stream_null()) ||\n"
    "         !xl6_json_stream_event_set(event, \"index\", source->HasName ? xl6_json_stream_null() : xrtValueInt((long long)source->Index)) ||\n"
    "         !xl6_json_stream_event_set(event, \"value\", xl6_json_stream_event_value(source)) ||\n"
    "         !xl6_json_stream_event_set(event, \"byteOffset\", xrtValueUInt((uint64_t)source->Location.Offset)) ||\n"
    "         !xl6_json_stream_event_set(event, \"line\", xrtValueUInt((uint64_t)source->Location.Line)) ||\n"
    "         !xl6_json_stream_event_set(event, \"column\", xrtValueUInt((uint64_t)source->Location.Column)) ||\n"
    "         !xl6_json_stream_event_set(event, \"raw\", source->Raw.Data != NULL ? xrtValueString(source->Raw) : xl6_json_stream_null()) ) {\n"
    "        xl6_serialization_callback_fail(context);\n"
    "        xrtValueRelease(event);\n"
    "        return XJSON_VISIT_FAIL;\n"
    "    }\n"
    "    keepGoing = false;\n"
    "    if ( !xl6_serialization_callback_call(context, event, &keepGoing) ) {\n"
    "        return XJSON_VISIT_FAIL;\n"
    "    }\n"
    "    return keepGoing ? XJSON_VISIT_NEXT : XJSON_VISIT_STOP;\n"
    "}\n\n"
    "XL6_Serialization_Visit_Result xl6_xrt_json_visit(xlir_string text, XL6_Function visitor)\n"
    "{\n"
    "    xl6_serialization_visitor_context context = {0};\n"
    "    xjsonreadconfig config;\n"
    "    int status;\n"
    "    context.PriorError = xrtTakeError();\n"
    "    if (visitor.invoke_var == 0 && visitor.invoke_xcall == 0) {\n"
    "        xrtSetErrorKind(XERR_ARGUMENT); xl6_serialization_callback_fail(&context);\n"
    "        return xl6_serialization_visit_finish(&context, -1); }\n"
    "    if (text.Data == 0) return xl6_serialization_visit_finish(&context, -1);\n"
    "    xrtJsonReadConfigInit(&config);\n"
    "    context.Visitor = visitor;\n"
    "    status = xrtJsonVisit(text, &config, xl6_json_stream_visit_proc, &context);\n"
    "    return xl6_serialization_visit_finish(&context, status);\n"
    "}\n\n";


void xl6CgenEmitJsonStreamRuntime(
    XL6_Output* out, int hasVisitor, int hasWriter)
{
    if ( out == NULL || (!hasVisitor && !hasWriter) ) return;
    if ( hasVisitor ) {
        xl6OutputPuts("extern size_t strlen(const char*);\n", out);
        xl6OutputPuts(xl6JsonStreamView, out);
        xl6OutputPuts(xl6JsonStreamVisitor, out);
    }
    if ( hasWriter ) {
        xl6OutputPuts("#include <__xl_runtime/xl6_json_writer.h>\n", out);
    }
}
