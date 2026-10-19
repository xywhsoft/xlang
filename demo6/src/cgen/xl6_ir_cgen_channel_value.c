/* Representation calls from the closed Channel value plan. No transport,
 * ownership-policy discovery or type-name inference is generated here. */
static int xl6IrCgenChannelValueEncode(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrChannelValuePlan* plan,unsigned source,const char* slot)
{
    const XL6_IrType* base=xl6IrTypeGet(&module->types,plan->baseElementType);
    if (!base || plan->codecAbi==XL6_IR_SEQUENCE_CODEC_DIRECT) return -1;
    if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1)
        xl6OutputPrintf(out,"      xvalue* %s = v%u != NULL ? xlir_closure_to_xvalue_scalar(v%u, xlir_xcall_entry_t%u, 0x%016llXULL) : xrtValueRetain(xrtValueNull());\n",
            slot,source,source,(unsigned)plan->baseElementType,(unsigned long long)base->typeValueId);
    else if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1) {
        if (plan->logicalTagged) xl6OutputPrintf(out,
            "      xvalue* %s = v%u.hasValue ? xlir_xvalue_union_box(&v%u.value, sizeof(v%u.value), 0x%016llXULL) : xrtValueRetain(xrtValueNull());\n",
            slot,source,source,source,(unsigned long long)base->typeValueId);
        else xl6OutputPrintf(out,"      xvalue* %s = xlir_xvalue_union_box(&v%u, sizeof(v%u), 0x%016llXULL);\n",
            slot,source,source,(unsigned long long)base->typeValueId);
    } else return -1;
    return xl6OutputHasFailed(out)?-1:1;
}
static int xl6IrCgenChannelValueDecode(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrChannelValuePlan* plan,const char* slot,const char* result,const char* scope)
{
    const XL6_IrType* base=xl6IrTypeGet(&module->types,plan->baseElementType);
    if (!base) return -1;
    if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1)
        xl6OutputPrintf(out,"        if (!xlir_xvalue_project_closure_try_copy_into(%s, (void (*)(void))xlir_xvalue_closure_entry_t%u, %s, &%s)) xl6ErrorScopeCaptureFailure(&%s);\n",
            slot,(unsigned)plan->baseElementType,plan->nullable?"true":"false",result,scope);
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
