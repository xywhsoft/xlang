/* Native parsers return status codes, independently of language callback
 * exceptions. Hold an owned callback error across parser unwinding; the XLIR
 * bridge's explicit result carrier, not ambient native diagnostics, signals
 * failure to its caller. Each visit owns its context, including nested visits. */
void xl6CgenEmitSerializationCallbackRuntime(XL6_Output* out, int xlirAbi)
{
    if (out == NULL) return;
    xl6OutputPuts(
        "typedef struct { XL6_Function Visitor; int CallbackFailed; xerror* CallbackError; } xl6_serialization_visitor_context;\n", out);
    if (xlirAbi) {
        xl6OutputPuts(
            "typedef xlir_result_c_int XL6_Serialization_Visit_Result;\n"
            "static void xl6_serialization_callback_fail(xl6_serialization_visitor_context* context)\n"
            "{\n"
            "    if (context->CallbackFailed) return;\n"
            "    context->CallbackFailed = 1;\n"
            "    context->CallbackError = xrtTakeError();\n"
            "    if (context->CallbackError == NULL)\n"
            "        context->CallbackError = xrtErrorCreate(XERR_STATE, \"xlang\", 0, \"visitor callback failed without an error\");\n"
            "}\n"
            "static int xl6_serialization_callback_call(xl6_serialization_visitor_context* context, xvalue* event, bool* keepGoing)\n"
            "{\n"
            "    xerror* prior = xrtTakeError();\n"
            "    xvalue* arguments[1] = { event };\n"
            "    xvalue* result = context->Visitor.invoke_var != NULL ?\n"
            "        context->Visitor.invoke_var(context->Visitor.env, 1, arguments) : NULL;\n"
            "    int ok = result != NULL && xrtValueGetBool(result, keepGoing);\n"
            "    if (!ok) xl6_serialization_callback_fail(context);\n"
            "    xrtValueRelease(result);\n"
            "    xrtValueRelease(event);\n"
            "    xrtSetErrorTake(prior);\n"
            "    return ok;\n"
            "}\n"
            "static XL6_Serialization_Visit_Result xl6_serialization_callback_finish(xl6_serialization_visitor_context* context, int status)\n"
            "{\n"
            "    if (context->CallbackFailed) {\n"
            "        xrtSetErrorTake(context->CallbackError);\n"
            "        context->CallbackError = NULL;\n"
            "        return (XL6_Serialization_Visit_Result){ false, 0 };\n"
            "    }\n"
            "    return (XL6_Serialization_Visit_Result){ true, status };\n"
            "}\n\n", out);
    } else {
        xl6OutputPuts(
            "typedef int XL6_Serialization_Visit_Result;\n"
            "static xvalue* xl6_function_call_var(XL6_Function, int, ...);\n"
            "static void xl6_serialization_callback_fail(xl6_serialization_visitor_context* context) { (void)context; }\n"
            "static int xl6_serialization_callback_call(xl6_serialization_visitor_context* context, xvalue* event, bool* keepGoing)\n"
            "{\n"
            "    xvalue* result = xl6_function_call_var(context->Visitor, 1, event);\n"
            "    int ok = result != NULL && xrtValueGetBool(result, keepGoing);\n"
            "    xrtValueRelease(result);\n"
            "    return ok;\n"
            "}\n"
            "static XL6_Serialization_Visit_Result xl6_serialization_callback_finish(xl6_serialization_visitor_context* context, int status)\n"
            "{ (void)context; return status; }\n\n", out);
    }
}
