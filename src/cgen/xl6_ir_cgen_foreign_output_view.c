/* Each raw output has its exact physical type, separate from owned results. */
static const char* xl6IrCgenOutputRawType(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
    return row ? row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? "xstrview":"xbytesview":
        xl6IrCgenType(module,xl6IrForeignOutputType(module,target,ordinal));
}
static int xl6IrCgenOutputRawDeclare(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    size_t count=xl6IrForeignOutputCount(module,target);
    if(!xl6IrForeignOutputViewsMatch(module,target))return 0;
    if(!xl6ForeignValueOutputHasView(declaration->foreignCall.valueOutput))
        xl6OutputPrintf(out,"    %s native_raw = {0};\n",xl6IrCgenType(module,target->returnType));
    else if(count==1)xl6OutputPrintf(out,"    %s native_raw = {0};\n",xl6IrCgenOutputRawType(module,target,0));
    else{
        xl6OutputPuts("    struct {",out);
        for(size_t ordinal=0;ordinal<count;++ordinal)
            xl6OutputPrintf(out," %s e%lu;",xl6IrCgenOutputRawType(module,target,ordinal),(unsigned long)ordinal);
        xl6OutputPuts(" } native_raw = {0};\n",out);
    }
    return xl6IrCgenOutputSizeInitialize(out,module,target);
}
static int xl6IrCgenOutputValuesAdmit(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* place,const char* scope)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    if(!xl6ForeignValueOutputHasView(declaration->foreignCall.valueOutput)){
        xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError()) %s = native_raw;\n",scope,place);return 1;
    }
    size_t count=xl6IrForeignOutputCount(module,target);
    xl6OutputPrintf(out,"    %s native_admitted = {0};\n",xl6IrCgenType(module,target->returnType));
    for(size_t ordinal=0;ordinal<count;++ordinal){
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
        char raw[48],admitted[48];
        if(count>1){snprintf(raw,sizeof(raw),"native_raw.e%lu",(unsigned long)ordinal);
            snprintf(admitted,sizeof(admitted),"native_admitted.e%lu",(unsigned long)ordinal);}
        else{strcpy(raw,"native_raw");strcpy(admitted,"native_admitted");}
        if(row)xl6IrCgenEmitBorrowedOwnedAdmission(out,module,row->logicalType,
            row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW:XL6_IR_CALL_RESULT_C_ABI_BYTES_VIEW,
            admitted,raw,scope);
        else xl6OutputPrintf(out,"    %s = %s;\n",admitted,raw);
    }
    xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError()) %s = native_admitted;\n"
        "    else { xerror* output_view_error = xrtTakeError();\n",scope,place);
    for(size_t ordinal=0;ordinal<count;++ordinal){
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
        if(!row)continue;
        const char* drop=row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? "xlir_string_drop":"xrtValueRelease";
        if(count>1)xl6OutputPrintf(out,"        %s(native_admitted.e%lu);\n",drop,(unsigned long)ordinal);
        else xl6OutputPrintf(out,"        %s(native_admitted);\n",drop);
    }
    xl6OutputPuts("        xrtSetErrorTake(output_view_error);\n    }\n",out);return !xl6OutputHasFailed(out);
}
