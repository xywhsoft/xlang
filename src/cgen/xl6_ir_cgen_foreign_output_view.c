/* Each raw output has its exact physical type, separate from owned results. */
static void xl6IrCgenRetainedViewAdmission(XL6_Output*,const XL6_IrModule*,XL6_IrTypeId,
    const XL6_ForeignRetainContract*,const char*,const char*,const char*,const char*);
static void xl6IrCgenReserveRootProbeNamed(XL6_Output*,const XL6_IrType*,int,const char*);
static const char* xl6IrCgenOutputRawType(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    if(target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_NATIVE_VIEW_ADMIT)
        return xl6IrTypeGet(&module->types,target->returnType)->cName;
    const XL6_ForeignNativeSequenceContract* sequence=xl6IrForeignNativeSequenceGet(module,target);
    if(sequence)return ordinal ? (sequence->countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int":"size_t"):sequence->dataCName;
    const XL6_ForeignOutputRecordContract* record=xl6IrForeignOutputRecordGet(module,target);
    if(record)return record->recordCName;
    const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
    return row ? row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? "xstrview":
        row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_BYTES ? "xbytesview":
        row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING ? "const char*":xl6IrTypeGet(&module->types,row->logicalType)->cName:
        xl6IrCgenType(module,xl6IrForeignOutputType(module,target,ordinal));
}
/* Consume only the verified output type. Fixed values retain their complete
 * inline wrapper for storage/admission, but decay to an element pointer here. */
static const XL6_IrType* xl6IrCgenOutputFixedArray(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    if(xl6IrForeignNativeSequenceGet(module,target) || xl6IrForeignOutputRecordGet(module,target) ||
        xl6IrForeignOutputViewGet(module,target,ordinal))return NULL;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,xl6IrForeignOutputType(module,target,ordinal));
    return type && type->kind==XL6_IR_TYPE_C_ARRAY ? type:NULL;
}
static const char* xl6IrCgenOutputPointeeType(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    const XL6_IrType* array=xl6IrCgenOutputFixedArray(module,target,ordinal);
    return array ? xl6IrCgenType(module,array->params[0]):xl6IrCgenOutputRawType(module,target,ordinal);
}
static void xl6IrCgenOutputAddress(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t ordinal)
{
    int array=xl6IrCgenOutputFixedArray(module,target,ordinal)!=NULL;
    int field=xl6IrForeignNativeSequenceGet(module,target) ||
        (!xl6IrForeignOutputRecordGet(module,target) && xl6IrForeignOutputCount(module,target)>1);
    if(!array)xl6OutputPutc('&',out);
    xl6OutputPuts("native_raw",out);
    if(field)xl6OutputPrintf(out,".e%lu",(unsigned long)ordinal);
    if(array)xl6OutputPuts(".Elements",out);
}
static int xl6IrCgenOutputRecordProof(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target)
{
    const XL6_ForeignOutputRecordContract* record=xl6IrForeignOutputRecordGet(module,target);
    if(!record)return 1;
    if(!xl6IrForeignOutputRecordsMatch(module,target))return 0;
    xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s*)0,struct %s*), \"borrowed output tagged struct\");\n"
        "XLIR_NATIVE_ASSERT(sizeof(%s)>0, \"borrowed output record completeness\");\n"
        "#ifdef __cplusplus\nXLIR_NATIVE_ASSERT(std::is_trivial<%s>::value && std::is_standard_layout<%s>::value, \"borrowed output trivial C layout\");\n#endif\n",
        record->recordCName,record->recordCName,record->recordCName,record->recordCName,record->recordCName);
    const XL6_ForeignOutputSizeContract* init=xl6IrForeignOutputSizeGet(module,target,0);
    if(init){
        if(!xl6IrForeignOutputSizesMatch(module,target))return 0;
        xl6OutputPrintf(out,
            "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,unsigned char) || "
            "XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,unsigned short) || "
            "XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,unsigned int) || "
            "XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,unsigned long) || "
            "XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,unsigned long long), \"C output size unsigned member\");\n"
            "XLIR_NATIVE_ASSERT(sizeof(%s) <= (~0ULL >> ((sizeof(unsigned long long)-sizeof(((%s*)0)->%s))*CHAR_BIT)), \"C output size field range\");\n",
            record->recordCName,init->field,record->recordCName,init->field,record->recordCName,init->field,
            record->recordCName,init->field,record->recordCName,init->field,
            record->recordCName,record->recordCName,init->field);
    }
    for(size_t o=0;o<xl6IrForeignOutputCount(module,target);++o){
        const XL6_ForeignOutputFieldContract* field=xl6IrForeignOutputFieldGet(module,target,o);
        const XL6_ForeignOutputViewContract* view=xl6IrForeignOutputViewGet(module,target,o);
        const XL6_IrType* type=field ? xl6IrTypeGet(&module->types,field->logicalType):NULL;
        if(!field || !type)return 0;
        const char* physical=view ? type->cName:xl6IrCgenType(module,field->logicalType);
        xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,%s), \"borrowed output record field type\");\n",
            record->recordCName,field->field,physical);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenOutputRawDeclare(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    size_t count=xl6IrForeignOutputCount(module,target);
    if(!xl6IrForeignOutputViewsMatch(module,target))return 0;
    const XL6_ForeignNativeSequenceContract* sequence=xl6IrForeignNativeSequenceGet(module,target);
    if(sequence){
        xl6OutputPrintf(out,"    struct { %s e0; %s e1; } native_raw={0};\n",sequence->dataCName,sequence->countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int":"size_t");return !xl6OutputHasFailed(out);
    }
    const XL6_ForeignOutputRecordContract* record=xl6IrForeignOutputRecordGet(module,target);
    if(record){
        if(!xl6IrForeignOutputRecordsMatch(module,target))return 0;
        xl6OutputPrintf(out,"    %s native_raw = {0};\n",record->recordCName);
        return xl6IrCgenOutputSizeInitialize(out,module,target) && !xl6OutputHasFailed(out);
    }else if(!xl6ForeignValueOutputHasView(declaration->foreignCall.valueOutput))
        xl6OutputPrintf(out,"    %s native_raw = {0};\n",count==1 ?
            xl6IrCgenOutputRawType(module,target,0):xl6IrCgenType(module,target->returnType));
    else if(count==1)xl6OutputPrintf(out,"    %s native_raw = {0};\n",xl6IrCgenOutputRawType(module,target,0));
    else{
        xl6OutputPuts("    struct {",out);
        for(size_t ordinal=0;ordinal<count;++ordinal)
            xl6OutputPrintf(out," %s e%lu;",xl6IrCgenOutputRawType(module,target,ordinal),(unsigned long)ordinal);
        xl6OutputPuts(" } native_raw = {0};\n",out);
    }
    return xl6IrCgenOutputSizeInitialize(out,module,target);
}
/* Reserve every retained output before the producer can have side effects.
 * A tuple failure does not let a later field force allocation after an earlier
 * native session was already acquired. No output-specific wrapper is emitted. */
static int xl6IrCgenOutputProbesBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* errors)
{
    if(xl6IrForeignNativeSequenceGet(module,target))return xl6IrCgenNativeSequenceOutputBegin(out,module,target,errors);
    size_t count=xl6IrForeignOutputCount(module,target);int opened=0;
    for(size_t ordinal=0;ordinal<count;++ordinal){
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
        if(!row || row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)continue;
        if(!opened){xl6OutputPuts("    {\n",out);opened=1;}
        xl6OutputPrintf(out,"    XL6_ForeignResultProbeV1 native_output_probe_%lu={0};\n",(unsigned long)ordinal);
    }
    if(!opened)return 0;
    for(size_t ordinal=0;ordinal<count;++ordinal){
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
        if(!row || row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)continue;
        char probe[48];snprintf(probe,sizeof(probe),"native_output_probe_%lu",(unsigned long)ordinal);
        xl6OutputPrintf(out,"    if(!%s.failed && !",errors);
        xl6IrCgenReserveRootProbeNamed(out,xl6IrTypeGet(&module->types,row->logicalType),0,probe);
        xl6OutputPrintf(out,") xl6ErrorScopeCaptureFailure(&%s);\n",errors);
    }
    xl6OutputPrintf(out,"    if(!%s.failed && !xrtGetError()) {\n",errors);return 1;
}
static void xl6IrCgenOutputProbesEnd(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* errors)
{
    if(xl6IrForeignNativeSequenceGet(module,target)){
        xl6OutputPrintf(out,"    }\n    if(native_sequence_result.self && !xl6NativeSequenceResultEndV1(&native_sequence_result))xl6ErrorScopeCaptureFailure(&%s);\n    }\n",errors);return;
    }
    xl6OutputPuts("    }\n",out);
    for(size_t ordinal=xl6IrForeignOutputCount(module,target);ordinal>0;--ordinal){
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal-1);
        if(!row || row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)continue;
        xl6OutputPrintf(out,"    if(native_output_probe_%lu.self && !xl6ForeignResultProbeEndV1(&native_output_probe_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
            (unsigned long)(ordinal-1),(unsigned long)(ordinal-1),errors);
    }
    xl6OutputPuts("    }\n",out);
}
static int xl6IrCgenOutputValuesAdmit(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* place,const char* scope)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    if(xl6IrForeignNativeSequenceGet(module,target)){xl6IrCgenNativeSequenceOutputAdmit(out,module,target,place,scope);return !xl6OutputHasFailed(out);}
    const XL6_ForeignOutputRecordContract* record=xl6IrForeignOutputRecordGet(module,target);
    if(!record && !xl6ForeignValueOutputHasView(declaration->foreignCall.valueOutput)){
        xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError()) %s = native_raw;\n",scope,place);return 1;
    }
    size_t count=xl6IrForeignOutputCount(module,target);
    xl6OutputPrintf(out,"    %s native_admitted = {0};\n",xl6IrCgenType(module,target->returnType));
    for(size_t ordinal=0;ordinal<count;++ordinal){
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
        char raw[128],admitted[48];
        if(count>1){snprintf(raw,sizeof(raw),"native_raw.e%lu",(unsigned long)ordinal);
            snprintf(admitted,sizeof(admitted),"native_admitted.e%lu",(unsigned long)ordinal);}
        else{strcpy(raw,"native_raw");strcpy(admitted,"native_admitted");}
        if(record){
            const XL6_ForeignOutputFieldContract* field=xl6IrForeignOutputFieldGet(module,target,ordinal);
            if(!field)return 0;
            snprintf(raw,sizeof(raw),"native_raw.%s",field->field);
        }
        if(row && row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE){
            XL6_ForeignRetainContract retain={0};retain.retainSymbol=row->retainSymbol;retain.rollbackSymbol=row->rollbackSymbol;retain.success=row->retainSuccess;
            char probe[48];snprintf(probe,sizeof(probe),"native_output_probe_%lu",(unsigned long)ordinal);
            xl6OutputPuts("    {\n",out);
            xl6IrCgenRetainedViewAdmission(out,module,row->logicalType,&retain,admitted,raw,scope,probe);
            xl6OutputPuts("    }\n",out);
        }else if(row)xl6IrCgenEmitBorrowedOwnedAdmission(out,module,row->logicalType,
            row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW:
            row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_BYTES ? XL6_IR_CALL_RESULT_C_ABI_BYTES_VIEW:XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER,
            admitted,raw,scope);
        else xl6OutputPrintf(out,"    %s = %s;\n",admitted,raw);
    }
    xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError()) %s = native_admitted;\n"
        "    else { xerror* output_view_error = xrtTakeError();\n",scope,place);
    for(size_t reverse=count;reverse>0;--reverse){
        size_t ordinal=reverse-1;
        const XL6_ForeignOutputViewContract* row=xl6IrForeignOutputViewGet(module,target,ordinal);
        if(!row)continue;
        if(row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE){
            char admitted[48];if(count>1)snprintf(admitted,sizeof(admitted),"native_admitted.e%lu",(unsigned long)ordinal);else strcpy(admitted,"native_admitted");
            xl6OutputPrintf(out,"        if (%s && !xl6ForeignViewReleaseV1(%s)) xl6ErrorScopeCaptureFailure(&%s);\n",admitted,admitted,scope);
            continue;
        }
        const char* drop=row->conversion==XL6_FOREIGN_OUTPUT_CONVERSION_BYTES ? "xrtValueRelease":"xlir_string_drop";
        if(count>1)xl6OutputPrintf(out,"        %s(native_admitted.e%lu);\n",drop,(unsigned long)ordinal);
        else xl6OutputPrintf(out,"        %s(native_admitted);\n",drop);
    }
    xl6OutputPuts("        xrtSetErrorTake(output_view_error);\n    }\n",out);return !xl6OutputHasFailed(out);
}
