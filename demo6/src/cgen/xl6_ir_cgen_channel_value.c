/* Representation calls from the closed Channel value plan. No transport,
 * ownership-policy discovery or type-name inference is generated here. */
static int xl6IrCgenChannelValueEncode(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrChannelValuePlan* plan,const char* source,const char* slot)
{
    const XL6_IrType* base=xl6IrTypeGet(&module->types,plan->baseElementType);
    if (!base || plan->codecAbi==XL6_IR_SEQUENCE_CODEC_DIRECT) return -1;
    if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1)
        xl6OutputPrintf(out,"      xvalue* %s = %s != NULL ? xlir_closure_to_xvalue_scalar(%s, xlir_xcall_entry_t%u, 0x%016llXULL) : xrtValueRetain(xrtValueNull());\n",
            slot,source,source,(unsigned)plan->baseElementType,(unsigned long long)base->typeValueId);
    else if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_ERROR_VALUE_V1)
        xl6OutputPrintf(out,"      xvalue* %s = xl6ErrorValueRefV1(%s, 0x%016llXULL);\n",
            slot,source,(unsigned long long)xl6IrTypeGet(&module->types,plan->elementType)->typeValueId);
    else if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1) {
        if (plan->logicalTagged) xl6OutputPrintf(out,
            "      xvalue* %s = %s.hasValue ? xlir_xvalue_union_box(&%s.value, sizeof(%s.value), 0x%016llXULL) : xrtValueRetain(xrtValueNull());\n",
            slot,source,source,source,(unsigned long long)base->typeValueId);
        else xl6OutputPrintf(out,"      xvalue* %s = xlir_xvalue_union_box(&%s, sizeof(%s), 0x%016llXULL);\n",
            slot,source,source,(unsigned long long)base->typeValueId);
    } else return -1;
    return xl6OutputHasFailed(out)?-1:1;
}

static int xl6IrCgenChannelValueDecode(XL6_Output*,const XL6_IrModule*,
    const XL6_IrChannelValuePlan*,const char*,const char*,const char*);
static bool xl6IrCgenNativeCallValueEncoded(const XL6_IrNativeCallValuePlan* plan)
{ return plan->value.schemaType && plan->value.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT; }

/* Ordinary SSA calls and dynamic witnesses share one preparation/publication
 * boundary. Fixed names are local to the call's foreign scope. No callback,
 * transport, allocation algorithm or logical-slot reinterpretation is emitted. */
static int xl6IrCgenNativeCallValueBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrNativeCallValuePlan* plan,const char* input,const char* scope)
{
    if (!xl6IrCgenNativeCallValueEncoded(plan)) return 1;
    if (plan->output) xl6OutputPuts("      xvalue* native_value_output = NULL;\n",out);
    if (plan->inputParameter) {
        if (!input || xl6IrCgenChannelValueEncode(out,module,&plan->value,input,"native_value_input")<0) return 0;
        xl6OutputPrintf(out,"      if (!native_value_input) xl6ErrorScopeCaptureFailure(&%s);\n"
            "      if (native_value_input) {\n",scope);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenNativeCallValueEnd(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrNativeCallValuePlan* plan,const char* result,const char* scope)
{
    if (!xl6IrCgenNativeCallValueEncoded(plan)) return 1;
    if (plan->output) {
        xl6OutputPrintf(out,"      if (!%s.failed && xrtGetError() == NULL) {\n",scope);
        if (!result || xl6IrCgenChannelValueDecode(out,module,&plan->value,"native_value_output",result,scope)<0) return 0;
        xl6OutputPuts("      }\n",out);
    }
    if (plan->inputParameter) xl6OutputPuts("      }\n      xrtValueRelease(native_value_input);\n",out);
    if (plan->output) xl6OutputPuts("      xrtValueRelease(native_value_output);\n",out);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenChannelValueDecode(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrChannelValuePlan* plan,const char* slot,const char* result,const char* scope)
{
    const XL6_IrType* base=xl6IrTypeGet(&module->types,plan->baseElementType);
    if (!base) return -1;
    if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1)
        xl6OutputPrintf(out,"        if (!xlir_xvalue_project_closure_try_copy_into(%s, (void (*)(void))xlir_xvalue_closure_entry_t%u, %s, &%s)) xl6ErrorScopeCaptureFailure(&%s);\n",
            slot,(unsigned)plan->baseElementType,plan->nullable?"true":"false",result,scope);
    else if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_ERROR_VALUE_V1) {
        xl6OutputPrintf(out,"        if (!xl6ErrorValueReadRefV1(%s",slot);
        if (plan->nullable) xl6OutputPrintf(out," ? %s : xrtValueNull()",slot);
        xl6OutputPrintf(out,", 0x%016llXULL, &%s)) xl6ErrorScopeCaptureFailure(&%s);\n",
            (unsigned long long)xl6IrTypeGet(&module->types,plan->elementType)->typeValueId,result,scope);
    }
    else if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1) {
        if (plan->logicalTagged) xl6OutputPrintf(out,
            "        %s.hasValue = xrtValueType(%s) != 0;\n"
            "        if (%s.hasValue) xlir_xvalue_union_unbox(%s, 0x%016llXULL, &%s.value, sizeof(%s.value));\n",
            result,slot,result,slot,(unsigned long long)base->typeValueId,result,result);
        else xl6OutputPrintf(out,"        xlir_xvalue_union_unbox(%s, 0x%016llXULL, &%s, sizeof(%s));\n",
            slot,(unsigned long long)base->typeValueId,result,result);
    } else return -1;
    return xl6OutputHasFailed(out)?-1:1;
}
