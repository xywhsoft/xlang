/* Regions keep one physical pointer slot. All selectors and observer edges
 * arrive closed by XLIR; this emitter never interprets declaration text. */
static void xl6IrCgenRegionPlace(char* place,size_t capacity,const XL6_IrValueId* values,size_t p)
{
    if(values)snprintf(place,capacity,"v%u",(unsigned)values[p]);
    else if(p)snprintf(place,capacity,"a%lu",(unsigned long)(p-1));
    else snprintf(place,capacity,"receiver");
}
static int xl6IrCgenRegionSelector(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,size_t p,const XL6_ForeignRegionSelector* selector)
{
    if(selector->kind==XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD){
        xl6OutputPrintf(out,"native_region_observed_%lu.%s",(unsigned long)p,selector->field);return 1;
    }
    char place[40];xl6IrCgenRegionPlace(place,sizeof(place),values,selector->parameter);
    xl6OutputPuts("(",out);
    if(!xl6IrCgenEmitCallPlace(out,module,target,selector->parameter,place))return 0;
    xl6OutputPuts(")",out);
    if(selector->kind==XL6_FOREIGN_REGION_SELECTOR_INPUT_FIELD)xl6OutputPrintf(out,".%s",selector->field);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenRegionObserver(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,size_t p,const char* errors)
{
    const XL6_ForeignRegionObserverContract* row=xl6IrForeignRegionObserverGet(module,target,p);
    if(!row)return 1;
    const XL6_IrCallTarget* query=xl6IrCallTargetGet(module,row->querySymbol);
    const XL6_IrType* record=xl6IrTypeGet(&module->types,row->recordType);
    const XL6_IrNode* declaration=xl6IrNodeGet(module,row->queryDeclaration);
    const XL6_ForeignFieldCallContract* field=xl6IrForeignFieldCallGet(module,query);
    if(!query || !record || !declaration)return 0;
    xl6OutputPrintf(out,"    %s native_region_observed_%lu = {0};\n    if (native_regions_ok) {\n",record->cName,(unsigned long)p);
    for(size_t i=0;i<module->foreignOutputSizeCount;++i){
        const XL6_ForeignOutputSizeContract* init=&module->foreignOutputSizes[i];
        if(init->declaration==query->declaration)
            xl6OutputPrintf(out,"    native_region_observed_%lu.%s=sizeof(native_region_observed_%lu);\n",(unsigned long)p,init->field,(unsigned long)p);
    }
    if(field){
        const XL6_IrType* receiver=xl6IrTypeGet(&module->types,query->parameterTypes[0]);
        const XL6_ForeignPointerQualifierContract* qualifier=xl6IrForeignPointerQualifierGet(module,query,0);
        char place[40];xl6IrCgenRegionPlace(place,sizeof(place),values,row->parameters[0]);
        xl6OutputPrintf(out,"    %s native_region_query_receiver_%lu=",qualifier ? qualifier->cName:receiver->cName,(unsigned long)p);
        if(!xl6IrCgenEmitCallPlace(out,module,target,row->parameters[0],place))return 0;
        xl6OutputPrintf(out,";\n    xlir_native_callee_s%u native_region_query_%lu=native_region_query_receiver_%lu ? native_region_query_receiver_%lu->%s : NULL;\n"
            "    if (!native_region_query_%lu) { native_regions_ok=false; xrtSetErrorKind(XERR_UNSUPPORTED); xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    if (native_regions_ok) {\n",(unsigned)query->symbol,(unsigned long)p,(unsigned long)p,(unsigned long)p,field->field,(unsigned long)p,errors);
    }
    bool status=query->outputResultAbi==XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS;
    xl6OutputPuts("    ",out);
    if(status)xl6OutputPrintf(out,"int native_region_query_status_%lu=",(unsigned long)p);
    if(field)xl6OutputPrintf(out,"native_region_query_%lu(",(unsigned long)p);
    else xl6OutputPrintf(out,"%s(",query->cName);
    size_t output=declaration->foreignCall.outputIndexPlusOne-1;
    for(size_t physical=0;physical<query->parameterCount+1;++physical){
        if(physical)xl6OutputPuts(",",out);
        if(physical==output)xl6OutputPrintf(out,"&native_region_observed_%lu",(unsigned long)p);
        else{
            size_t source=row->parameters[physical-(physical>output)];char place[40];
            xl6IrCgenRegionPlace(place,sizeof(place),values,source);
            if(!xl6IrCgenEmitCallPlace(out,module,target,source,place))return 0;
        }
    }
    xl6OutputPuts(");\n",out);
    if(status)xl6OutputPrintf(out,"    if (native_region_query_status_%lu!=%d) { native_regions_ok=false; if (!%s.failed && !xrtGetError()) xl6ForeignStatusFailureV1(native_region_query_status_%lu,\"%s\"); xl6ErrorScopeCaptureFailure(&%s); }\n",
        (unsigned long)p,query->nativeSuccessCode,errors,(unsigned long)p,query->cName,errors);
    xl6OutputPrintf(out,"    if (xrtGetError()) { native_regions_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",errors);
    xl6OutputPrintf(out,"    if (%s.failed) native_regions_ok=false;\n",errors);
    if(field)xl6OutputPuts("    }\n",out);
    xl6OutputPuts("    }\n",out);return !xl6OutputHasFailed(out);
}
static int xl6IrCgenForeignRegionsBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,const char* errors)
{
    size_t count=0;
    for(size_t p=0;p<target->parameterCount;++p)count+=xl6IrForeignRegionGet(module,target,p)!=NULL;
    if(!count)return 0;
    xl6OutputPrintf(out,"    { bool native_regions_ok=!%s.failed;\n"
        "    if(xrtGetError()) { native_regions_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",errors,errors);
    /* Query every observed extent before acquiring any writable Buffer. */
    for(size_t p=0;p<target->parameterCount;++p)
        if(!xl6IrCgenRegionObserver(out,module,target,values,p,errors))return -1;
    for(size_t p=0;p<target->parameterCount;++p){
        const XL6_ForeignRegionContract* row=xl6IrForeignRegionGet(module,target,p);if(!row)continue;
        const XL6_IrClassField* buffer=xl6IrForeignRegionBufferField(module,target,p);
        xl6OutputPrintf(out,"    %s native_region_data_%lu=NULL; size_t native_region_bytes_%lu=0; bool native_region_absent_%lu=false;\n",
            row->access==XL6_FOREIGN_REGION_ACCESS_READ ? "const void*":"void*",(unsigned long)p,(unsigned long)p,(unsigned long)p);
        if(buffer)xl6OutputPrintf(out,"    XL6_BufferSpanV1 native_region_%lu={0};\n",(unsigned long)p);
        const XL6_ForeignRegionSelector* selectors[]={&row->width,&row->height,&row->stride};
        const char* names[]={"width","height","stride"};
        for(size_t s=0;s<3;++s){
            xl6OutputPrintf(out,"    %s native_region_%s_%lu=0;\n    if(native_regions_ok) native_region_%s_%lu=",
                xl6IrCgenType(module,selectors[s]->fieldType),names[s],(unsigned long)p,names[s],(unsigned long)p);
            if(!xl6IrCgenRegionSelector(out,module,target,values,p,selectors[s]))return -1;
            xl6OutputPuts(";\n",out);
        }
        xl6OutputPrintf(out,"    size_t native_region_required_%lu=0;\n    if(native_regions_ok) {\n"
            "    uintmax_t w=(uintmax_t)native_region_width_%lu,h=(uintmax_t)native_region_height_%lu;\n"
            "    if(native_region_width_%lu<=0 || native_region_height_%lu<=0 || w>SIZE_MAX/%u || h>SIZE_MAX) native_regions_ok=false;\n"
            "    else { size_t row_bytes=(size_t)w*%u; size_t stride=row_bytes;\n"
            "        if(native_region_stride_%lu>0) { uintmax_t given=(uintmax_t)native_region_stride_%lu; if(given>SIZE_MAX) native_regions_ok=false; else stride=(size_t)given; }\n"
            "        else { uintmax_t limit=UINTMAX_MAX >> (sizeof(uintmax_t)*CHAR_BIT-sizeof(native_region_stride_%lu)*CHAR_BIT+((%s)-1>(%s)0 ? 0:1)); if(row_bytes>limit) native_regions_ok=false; }\n"
            "        if(stride<row_bytes || ((size_t)h-1)>(SIZE_MAX-row_bytes)/stride) native_regions_ok=false;\n"
            "        if(native_regions_ok) native_region_required_%lu=((size_t)h-1)*stride+row_bytes;\n    }\n"
            "    if(!native_regions_ok) { xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); } }\n",
            (unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p,row->unitBytes,row->unitBytes,
            (unsigned long)p,(unsigned long)p,(unsigned long)p,xl6IrCgenType(module,row->stride.fieldType),xl6IrCgenType(module,row->stride.fieldType),(unsigned long)p,errors);
        if(row->nullable){
            xl6OutputPrintf(out,"    native_region_absent_%lu=!(",(unsigned long)p);xl6IrCgenForeignViewArgument(out,values,p,NULL);
            xl6OutputPuts(") || !xrtValueType(",out);xl6IrCgenForeignViewArgument(out,values,p,NULL);xl6OutputPuts(");\n",out);
        }
        if(buffer){
            xl6OutputPrintf(out,"    if(native_regions_ok && !native_region_absent_%lu) { if(!xl6BufferSpanBeginV1(xrtValueObjectGet(",(unsigned long)p);
            xl6IrCgenForeignViewArgument(out,values,p,NULL);
            xl6OutputPuts(",(xstrview){",out);xl6IrCgenEmitCStringLiteral(out,buffer->name);
            xl6OutputPuts(",sizeof(",out);xl6IrCgenEmitCStringLiteral(out,buffer->name);
            xl6OutputPrintf(out,")-1}),%s,&native_region_%lu)) { native_regions_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
                "    else { native_region_data_%lu=native_region_%lu.data; native_region_bytes_%lu=native_region_%lu.bytes; } }\n",
                row->access==XL6_FOREIGN_REGION_ACCESS_READ ? "true":"false",(unsigned long)p,errors,(unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p);
        }else{
            xl6OutputPrintf(out,"    if(native_regions_ok && !native_region_absent_%lu) { xbytesview view={0}; if(!xrtValueGetBytes(",(unsigned long)p);
            xl6IrCgenForeignViewArgument(out,values,p,NULL);
            xl6OutputPrintf(out,",&view)) { native_regions_ok=false; xl6ErrorScopeCaptureFailure(&%s); } else { native_region_data_%lu=view.Data; native_region_bytes_%lu=view.Size; } }\n",errors,(unsigned long)p,(unsigned long)p);
        }
        xl6OutputPrintf(out,"    if(native_regions_ok && !native_region_absent_%lu && (native_region_required_%lu>native_region_bytes_%lu || !native_region_data_%lu)) { native_regions_ok=false; xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n",
            (unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p,errors);
    }
    xl6OutputPuts("    if(native_regions_ok) {\n",out);return (int)count;
}
static void xl6IrCgenForeignRegionsEnd(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(xl6IrForeignRegionBufferField(module,target,p-1))
            xl6OutputPrintf(out,"    if(native_region_%lu.access.self && !xl6BufferSpanEndV1(&native_region_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",(unsigned long)(p-1),(unsigned long)(p-1),errors);
    xl6OutputPuts("    }\n",out);
}
