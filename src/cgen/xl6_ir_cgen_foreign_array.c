/* Typed synchronous Array projection. The business producer is called once;
 * all storage, snapshots and element pins are prepared before it is reached. */
static const char* xl6IrCgenNativeSlotTypeExpr(XL6_IrNativeSlotAbi);
static int xl6IrCgenArrayProof(XL6_Output* out,const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(!xl6IrForeignArrayGroupsMatch(m,t))return 0;
    for(size_t g=0;g<m->foreignArrayGroupCount;++g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g];
        if(group->declaration!=t->declaration)continue;
        if(group->recordCName[0])xl6OutputPrintf(out,
            "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s*)0,struct %s*), \"native Array tagged struct\");\n"
            "XLIR_NATIVE_ASSERT(XLIR_NATIVE_ALIGNOF(%s)<=XL6_FOREIGN_RECORD_ALIGNMENT_V1, \"native Array heap alignment\");\n"
            "#ifdef __cplusplus\nXLIR_NATIVE_ASSERT(std::is_trivial<%s>::value && std::is_standard_layout<%s>::value, \"native Array trivial record\");\n#endif\n",
            group->recordCName,group->recordCName,group->recordCName,group->recordCName,group->recordCName);
        for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;const XL6_ForeignArrayFieldContract* field;
            if(!xl6IrForeignArrayColumn(m,group,c,&col,&field))return 0;
            if(field)xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,%s), \"native Array record field ABI\");\n",group->recordCName,field->field,col.elementCName);
            else xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(*((%s)0),%s), \"native Array pointee ABI\");\n",
                col.pointerCName,col.elementCName);
            if(col.kind==XL6_FOREIGN_ARRAY_ELEMENT_VALUE && !xl6IrCgenPackPodProof(out,m,col.elementType,0))return 0;
            if(col.profile==XL6_FOREIGN_ARRAY_PROFILE_VALUE_SNAPSHOT)xl6OutputPrintf(out,
                "XLIR_NATIVE_ASSERT(XLIR_NATIVE_ALIGNOF(%s)<=XL6_FOREIGN_RECORD_ALIGNMENT_V1, \"native Array element heap alignment\");\n",col.elementCName);
            if(col.profile==XL6_FOREIGN_ARRAY_PROFILE_VIEW_CALL_PIN){const XL6_IrType* root=xl6IrForeignRootType(m,col.elementType);
                if(root->foreignScope)xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_ALIGNOF(%s)<=XL6_FOREIGN_RECORD_ALIGNMENT_V1, \"native Array call pin alignment\");\n",root->foreignScope);}
        }
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenArrayArguments(XL6_Output* out,const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t physical)
{
    const XL6_ForeignArrayGroupContract* group=xl6IrForeignArrayGroupGet(m,t,physical);if(!group)return 0;
    if(group->recordCName[0])xl6OutputPrintf(out,"native_array_record_%u",group->physicalIndex);
    else for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;
        if(!xl6IrForeignArrayColumn(m,group,c,&col,NULL))return 0;
        if(c)xl6OutputPuts(", ",out);
        xl6OutputPrintf(out,"(%s)native_array_data_%u_%lu",col.pointerCName,group->physicalIndex,(unsigned long)c);
    }
    xl6OutputPrintf(out,", (%s)native_array_count_%u",group->countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int":"size_t",group->physicalIndex);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenArraysBegin(XL6_Output* out,const XL6_IrModule* m,const XL6_IrCallTarget* t,
    const XL6_IrValueId* values,const char* errors)
{
    int count=0;
    for(size_t g=0;g<m->foreignArrayGroupCount;++g)count+=m->foreignArrayGroups[g].declaration==t->declaration;
    if(!count)return 0;
    xl6OutputPrintf(out,"    { bool native_arrays_ok=!%s.failed;\n"
        "    if(xrtGetError()) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",errors,errors);
    /* Declare every cleanup carrier before any fallible preparation. */
    for(size_t g=0;g<m->foreignArrayGroupCount;++g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g];
        if(group->declaration!=t->declaration)continue;
        unsigned k=group->physicalIndex;
        xl6OutputPrintf(out,"    size_t native_array_count_%u=0; int64_t native_array_size_%u=",k,k);
        xl6IrCgenForeignViewArgument(out,values,group->sizeParameter,NULL);xl6OutputPuts(";\n",out);
        if(group->recordCName[0])xl6OutputPrintf(out,"    %s* native_array_record_%u=NULL;\n",group->recordCName,k);
        for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;
            if(!xl6IrForeignArrayColumn(m,group,c,&col,NULL))return -1;
            xl6OutputPrintf(out,"    XL6_SequenceReadPinV1 native_array_pin_%u_%lu={0};\n",k,(unsigned long)c);
            if(!group->recordCName[0])xl6OutputPrintf(out,"    %s* native_array_data_%u_%lu=NULL;\n",col.elementCName,k,(unsigned long)c);
            if(col.profile==XL6_FOREIGN_ARRAY_PROFILE_VIEW_CALL_PIN){const XL6_IrType* root=xl6IrForeignRootType(m,col.elementType);
                xl6OutputPrintf(out,"    typedef struct { XL6_ForeignView* owner; XL6_ForeignViewCallV1 call; ");
                if(root->foreignScope)xl6OutputPrintf(out,"%s scope; ",root->foreignScope);
                xl6OutputPrintf(out,"} xlir_array_views_%u_%lu; xlir_array_views_%u_%lu* native_array_views_%u_%lu=NULL;\n",k,(unsigned long)c,k,(unsigned long)c,k,(unsigned long)c);
            }
        }
    }
    /* Acquire stable backing before any Native scope/hook can reenter. */
    for(size_t g=0;g<m->foreignArrayGroupCount;++g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g];
        if(group->declaration!=t->declaration)continue;
        unsigned k=group->physicalIndex;
        for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;xl6IrForeignArrayColumn(m,group,c,&col,NULL);
            const XL6_IrType* array=xl6IrTypeGet(&m->types,col.arrayType);const XL6_IrType* element=xl6IrTypeGet(&m->types,col.elementType);
            const char* descriptor=xl6IrCgenNativeSlotTypeExpr(array->sequenceLayout.slotAbi);if(!descriptor)return -1;
            xl6OutputPuts("    if(native_arrays_ok && !xl6SequenceReadPinBeginV1(",out);xl6IrCgenForeignViewArgument(out,values,group->firstParameter+c,NULL);
            xl6OutputPrintf(out,",0x%016llXULL,0x%016llXULL,%u,(%s)->Size,%s,&native_array_pin_%u_%lu)) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",
                (unsigned long long)array->typeValueId,(unsigned long long)element->typeValueId,(unsigned)array->sequenceLayout.transferAbi,descriptor,descriptor,k,(unsigned long)c,errors);
            if(!c)xl6OutputPrintf(out,"    native_array_count_%u=native_array_pin_%u_0.count;\n",k,k);
            else xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_size_%u==-1 && native_array_pin_%u_%lu.count!=native_array_count_%u) { native_arrays_ok=false; xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&%s); }\n"
                "    if(native_array_pin_%u_%lu.count<native_array_count_%u) native_array_count_%u=native_array_pin_%u_%lu.count;\n",k,k,(unsigned long)c,k,errors,k,(unsigned long)c,k,k,k,(unsigned long)c);
        }
        xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_size_%u < -1) { native_arrays_ok=false; xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    if(native_array_size_%u>=0 && (uint64_t)native_array_size_%u<native_array_count_%u) native_array_count_%u=(size_t)native_array_size_%u;\n",k,errors,k,k,k,k,k);
        if(group->countAbi==XL6_FOREIGN_SPAN_COUNT_INT)xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_count_%u>INT_MAX) { native_arrays_ok=false; xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n",k,errors);
        if(group->recordCName[0])xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_count_%u) { if(native_array_count_%u>SIZE_MAX/sizeof(*native_array_record_%u)) xrtSetErrorKind(XERR_RANGE); else native_array_record_%u=(%s*)xrtCalloc(native_array_count_%u,sizeof(*native_array_record_%u)); if(!native_array_record_%u) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",k,k,k,k,group->recordCName,k,k,k,errors);
        for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;xl6IrForeignArrayColumn(m,group,c,&col,NULL);
            if(!group->recordCName[0] && col.profile==XL6_FOREIGN_ARRAY_PROFILE_SCALAR_READ_PIN)
                xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_count_%u) native_array_data_%u_%lu=(%s*)xrtTypedArrayConstData(native_array_pin_%u_%lu.storage);\n",k,k,(unsigned long)c,col.elementCName,k,(unsigned long)c);
            else if(!group->recordCName[0])xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_count_%u) { if(native_array_count_%u>SIZE_MAX/sizeof(*native_array_data_%u_%lu)) xrtSetErrorKind(XERR_RANGE); else native_array_data_%u_%lu=(%s*)xrtCalloc(native_array_count_%u,sizeof(*native_array_data_%u_%lu)); if(!native_array_data_%u_%lu) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",k,k,k,(unsigned long)c,k,(unsigned long)c,col.elementCName,k,k,(unsigned long)c,k,(unsigned long)c,errors);
            if(col.profile==XL6_FOREIGN_ARRAY_PROFILE_VIEW_CALL_PIN)xl6OutputPrintf(out,"    if(native_arrays_ok && native_array_count_%u) { if(native_array_count_%u>SIZE_MAX/sizeof(*native_array_views_%u_%lu)) xrtSetErrorKind(XERR_RANGE); else native_array_views_%u_%lu=(xlir_array_views_%u_%lu*)xrtCalloc(native_array_count_%u,sizeof(*native_array_views_%u_%lu)); if(!native_array_views_%u_%lu) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",k,k,k,(unsigned long)c,k,(unsigned long)c,k,(unsigned long)c,k,k,(unsigned long)c,k,(unsigned long)c,errors);
        }
    }
    /* Snapshot pointer identities and materialize only non-identical layouts. */
    for(size_t g=0;g<m->foreignArrayGroupCount;++g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g];
        if(group->declaration!=t->declaration)continue;
        unsigned k=group->physicalIndex;
        for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;const XL6_ForeignArrayFieldContract* field;xl6IrForeignArrayColumn(m,group,c,&col,&field);
            if(col.profile==XL6_FOREIGN_ARRAY_PROFILE_SCALAR_READ_PIN)continue;
            char destination[256];if(field)snprintf(destination,sizeof(destination),"native_array_record_%u[native_array_i].%s",k,field->field);
            else snprintf(destination,sizeof(destination),"native_array_data_%u_%lu[native_array_i]",k,(unsigned long)c);
            const XL6_IrType* element=xl6IrTypeGet(&m->types,col.elementType);
            xl6OutputPrintf(out,"    for(size_t native_array_i=0; native_arrays_ok && native_array_i<native_array_count_%u; ++native_array_i) { const void* native_array_slot=xl6SequenceReadPinSlotV1(&native_array_pin_%u_%lu,native_array_i); if(!native_array_slot) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); break; }\n",k,k,(unsigned long)c,errors);
            if(col.kind==XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW){
                xl6OutputPrintf(out,"    if(!xl6ForeignViewCopyValueV1(*(XL6_ForeignView* const*)native_array_slot,&native_array_views_%u_%lu[native_array_i].owner)) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",k,(unsigned long)c,errors);
                if(element->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out,"    if(!native_array_views_%u_%lu[native_array_i].owner && native_arrays_ok) { native_arrays_ok=false; xrtSetErrorKind(XERR_TYPE); xl6ErrorScopeCaptureFailure(&%s); }\n",k,(unsigned long)c,errors);
            }else if(col.kind==XL6_FOREIGN_ARRAY_ELEMENT_C_STRING){
                xl6OutputPrintf(out,"    { xstrview native_array_text=*(const xstrview*)native_array_slot; if((native_array_text.Size && !native_array_text.Data) || (native_array_text.Data && memchr(native_array_text.Data,0,native_array_text.Size))%s) { native_arrays_ok=false; xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&%s); } else %s=native_array_text.Data; }\n",element->kind==XL6_IR_TYPE_NULLABLE ? "":" || !native_array_text.Data",errors,destination);
            }else if(element->kind==XL6_IR_TYPE_C_RECORD){
                xl6OutputPrintf(out,"    { const xvalue* native_array_box=*(xvalue* const*)native_array_slot; xbytesview native_array_bytes={0}; if(!native_array_box || xrtValueTypeId(native_array_box)!=0x%016llXULL || !xrtValueGetBytes(native_array_box,&native_array_bytes) || native_array_bytes.Size!=sizeof(%s) || !native_array_bytes.Data) { native_arrays_ok=false; if(!xrtGetError()) xrtSetErrorKind(XERR_TYPE); xl6ErrorScopeCaptureFailure(&%s); } else memcpy(&%s,native_array_bytes.Data,sizeof(%s)); }\n",(unsigned long long)element->typeValueId,destination,errors,destination,destination);
            }else xl6OutputPrintf(out,"    memcpy(&%s,native_array_slot,sizeof(%s));\n",destination,destination);
            xl6OutputPuts("    }\n",out);
        }
    }
    xl6OutputPrintf(out,"    if(xrtGetError()) { xl6ErrorScopeCaptureFailure(&%s); }\n    if(%s.failed) native_arrays_ok=false;\n",errors,errors);
    /* Native call pins start only after every temporary and owner snapshot. */
    for(size_t g=0;g<m->foreignArrayGroupCount;++g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g];if(group->declaration!=t->declaration)continue;unsigned k=group->physicalIndex;
        for(size_t c=0;c<group->arrayCount;++c){XL6_ForeignArrayInputContract col;const XL6_ForeignArrayFieldContract* field;xl6IrForeignArrayColumn(m,group,c,&col,&field);if(col.kind!=XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW)continue;
            const XL6_IrType* root=xl6IrForeignRootType(m,col.elementType);
            xl6OutputPrintf(out,"    for(size_t native_array_i=0; native_arrays_ok && native_array_i<native_array_count_%u; ++native_array_i) { if(native_array_views_%u_%lu[native_array_i].owner) { if(!xl6ForeignViewCallBeginV1(native_array_views_%u_%lu[native_array_i].owner,&native_array_views_%u_%lu[native_array_i].call,",k,k,(unsigned long)c,k,(unsigned long)c,k,(unsigned long)c);
            if(root->foreignScope)xl6OutputPrintf(out,"&native_array_views_%u_%lu[native_array_i].scope,sizeof(native_array_views_%u_%lu[native_array_i].scope)",k,(unsigned long)c,k,(unsigned long)c);else xl6OutputPuts("NULL,0",out);
            xl6OutputPrintf(out,")) { native_arrays_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",errors);
            if(field)xl6OutputPrintf(out,"    native_array_record_%u[native_array_i].%s=",k,field->field);else xl6OutputPrintf(out,"    native_array_data_%u_%lu[native_array_i]=",k,(unsigned long)c);
            xl6OutputPrintf(out,"(%s)native_array_views_%u_%lu[native_array_i].call.raw;\n    if(xrtGetError()) { xl6ErrorScopeCaptureFailure(&%s); }\n    if(%s.failed) native_arrays_ok=false; }\n",col.elementCName,k,(unsigned long)c,errors,errors);
        }
    }
    xl6OutputPrintf(out,"    if(xrtGetError()) { xl6ErrorScopeCaptureFailure(&%s); }\n    if(%s.failed) native_arrays_ok=false;\n    if(native_arrays_ok) {\n",errors,errors);return count;
}
static void xl6IrCgenArraysEnd(XL6_Output* out,const XL6_IrModule* m,const XL6_IrCallTarget* t,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    /* All backing pins remain alive through ordinary input ScopeEnd and every
     * projected element ScopeEnd/Drop, even when a cleanup hook reenters. */
    for(size_t g=m->foreignArrayGroupCount;g;--g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g-1];if(group->declaration!=t->declaration)continue;unsigned k=group->physicalIndex;
        for(size_t c=group->arrayCount;c;--c){XL6_ForeignArrayInputContract col;xl6IrForeignArrayColumn(m,group,c-1,&col,NULL);
            if(col.kind==XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW)xl6OutputPrintf(out,"    if(native_array_views_%u_%lu) { for(size_t native_array_i=native_array_count_%u;native_array_i;--native_array_i) { if(native_array_views_%u_%lu[native_array_i-1].call.self && !xl6ForeignViewCallEndV1(&native_array_views_%u_%lu[native_array_i-1].call)) xl6ErrorScopeCaptureFailure(&%s); if(native_array_views_%u_%lu[native_array_i-1].owner) { xl6ForeignViewDropValueV1(native_array_views_%u_%lu[native_array_i-1].owner); if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s); } } xrtFree(native_array_views_%u_%lu); }\n",k,(unsigned long)c-1,k,k,(unsigned long)c-1,k,(unsigned long)c-1,errors,k,(unsigned long)c-1,k,(unsigned long)c-1,errors,k,(unsigned long)c-1);
            if(!group->recordCName[0] && col.profile!=XL6_FOREIGN_ARRAY_PROFILE_SCALAR_READ_PIN)xl6OutputPrintf(out,"    xrtFree(native_array_data_%u_%lu);\n",k,(unsigned long)c-1);
        }
        if(group->recordCName[0])xl6OutputPrintf(out,"    xrtFree(native_array_record_%u);\n",k);
    }
    for(size_t g=m->foreignArrayGroupCount;g;--g){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[g-1];if(group->declaration!=t->declaration)continue;
        for(size_t c=group->arrayCount;c;--c)xl6OutputPrintf(out,"    if(native_array_pin_%u_%lu.self && !xl6SequenceReadPinEndV1(&native_array_pin_%u_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",group->physicalIndex,(unsigned long)c-1,group->physicalIndex,(unsigned long)c-1,errors);
    }
    xl6OutputPuts("    }\n",out);
}
