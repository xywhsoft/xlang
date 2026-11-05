static int xl6IrCgenForeignSpansBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* operands,const char* scope)
{
    int count=0;
    for(size_t p=0;p<target->parameterCount;++p) {
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_SPAN_BORROW)continue;
        const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,p);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[p]);
        const XL6_IrClassField* buffer=xl6IrForeignSpanBufferField(module,target,p);
        if(!row || !type)return -1;
        if(!count++)xl6OutputPuts("    { bool native_spans_ok = true;\n",out);
        xl6OutputPrintf(out,"    const void* native_span_data_%lu = NULL; size_t native_span_bytes_%lu = 0; size_t native_span_count_%lu = 0;\n",
            (unsigned long)p,(unsigned long)p,(unsigned long)p);
        if(buffer) {
            xl6OutputPrintf(out,"    XL6_BufferSpanV1 native_span_%lu = {0};\n"
                "    if (native_spans_ok",(unsigned long)p);
            if(type->kind==XL6_IR_TYPE_NULLABLE){
                xl6OutputPuts(" && (",out);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
                xl6OutputPuts(") && xrtValueType(",out);xl6IrCgenForeignViewArgument(out,operands,p,NULL);xl6OutputPuts(")",out);
            }
            xl6OutputPuts(") { if (!xl6BufferSpanBeginV1(",out);
            xl6OutputPuts("xrtValueObjectGet(",out);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPuts(",(xstrview){",out);xl6IrCgenEmitCStringLiteral(out,buffer->name);
            xl6OutputPuts(",sizeof(",out);xl6IrCgenEmitCStringLiteral(out,buffer->name);
            xl6OutputPrintf(out,")-1}),%s,&native_span_%lu)) { native_spans_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
                "    else { native_span_data_%lu=native_span_%lu.data; native_span_bytes_%lu=native_span_%lu.bytes; } }\n",
                row->readOnly ? "true" : "false",(unsigned long)p,scope,(unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p);
        } else if(type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES) {
            xl6OutputPrintf(out,"    if (native_spans_ok) { xbytesview view={0}; if (!xrtValueGetBytes(");
            xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPrintf(out,",&view)) { native_spans_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
                "    else { native_span_data_%lu=view.Data; native_span_bytes_%lu=view.Size; } }\n",scope,(unsigned long)p,(unsigned long)p);
        } else if(type->cAbi==XL6_IR_C_ABI_MANAGED_STRING) {
            xl6OutputPrintf(out,"    native_span_data_%lu=",(unsigned long)p);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPrintf(out,".Data; native_span_bytes_%lu=",(unsigned long)p);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPuts(".Size;\n",out);
        } else if(type->cAbi==XL6_IR_C_ABI_C_RECORD) {
            xl6OutputPrintf(out,"    native_span_data_%lu=&",(unsigned long)p);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPrintf(out,"; native_span_bytes_%lu=sizeof(",(unsigned long)p);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPuts(");\n",out);
        } else if(type->cAbi==XL6_IR_C_ABI_C_ARRAY) {
            xl6OutputPrintf(out,"    native_span_data_%lu=",(unsigned long)p);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPrintf(out,".Elements; native_span_bytes_%lu=sizeof(",(unsigned long)p);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
            xl6OutputPuts(".Elements);\n",out);
        } else return -1;
        xl6OutputPrintf(out,"    if (native_spans_ok) { size_t unit = ");
        if(!strcmp(row->elementCName,"void"))xl6OutputPuts("1",out);
        else xl6OutputPrintf(out,"sizeof(%s)",row->elementCName);
        xl6OutputPrintf(out,"; size_t alignment = ");
        if(!strcmp(row->elementCName,"void"))xl6OutputPuts("1",out);
        else xl6OutputPrintf(out,"_Alignof(%s)",row->elementCName);
        xl6OutputPrintf(out,"; if ((native_span_bytes_%lu && !native_span_data_%lu) || native_span_bytes_%lu %% unit ||\n"
            "        (native_span_data_%lu && (uintptr_t)native_span_data_%lu %% alignment)",
            (unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p);
        if(row->countAbi==XL6_FOREIGN_SPAN_COUNT_INT)
            xl6OutputPrintf(out," || native_span_bytes_%lu / unit > INT_MAX",(unsigned long)p);
        xl6OutputPrintf(out,") { native_spans_ok=false; xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    else native_span_count_%lu=native_span_bytes_%lu/unit; }\n",scope,(unsigned long)p,(unsigned long)p);
    }
    if(count)xl6OutputPuts("    if (native_spans_ok) {\n",out);
    return count;
}
static void xl6IrCgenForeignSpansEnd(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* scope)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(xl6IrForeignSpanBufferField(module,target,p-1))
            xl6OutputPrintf(out,"    if (native_span_%lu.access.self && !xl6BufferSpanEndV1(&native_span_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),scope);
    xl6OutputPuts("    }\n",out);
}
