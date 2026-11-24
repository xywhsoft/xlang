/* Typed native callbacks share physical signature and argument rendering.
 * Retained publishers hold a snapshot through the complete thunk return tail;
 * synchronous publishers use one nonescaping stack context. Logical captures
 * never cross the C API as xvalue. */
static const char* xl6IrCgenCallbackNativeType(const XL6_IrModule* module,XL6_IrTypeId id)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if(type && type->cAbi==XL6_IR_C_ABI_MANAGED_STRING)return "xstrview";
    return type && (type->foreignPolicy || type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) ? type->cName : xl6IrCgenType(module,id);
}
static const char* xl6IrCgenCallbackSpanType(const XL6_ForeignCallbackSpanContract* span,
    const XL6_IrType* view,char* name,size_t capacity)
{
    if(xl6ForeignCallbackRegion(span))
        return xl6ForeignCallbackSpanWritable(span) ? "XL6_WritableRegionViewV1":"XL6_ReadableRegionViewV1";
    if(span->countsElements)
        return xl6ForeignCallbackSpanPointeeName(view->cName,name,capacity) ? name : NULL;
    return xl6ForeignCallbackSpanWritable(span) ? "XL6_WritableBytesViewV1" : "xbytesview";
}
static int xl6IrCgenCallbackParameters(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrType* signature,const XL6_ForeignParameterContract* row,bool names)
{
    bool userFirst=row->callbackUserMode==XL6_FOREIGN_CALLBACK_USER_FIRST;
    if(userFirst)xl6OutputPuts(names ? "void* userdata" : "void*",out);
    for(size_t p=0;p<signature->paramCount;++p) {
        if(p || userFirst)xl6OutputPuts(", ",out);
        const XL6_ForeignCallbackSpanContract* span=xl6IrForeignCallbackSpanGet(module,row,p);
        if(span && !xl6ForeignCallbackOwnedOutput(span)){
            xl6OutputPuts(span->pointerCName,out);
            if(names)xl6OutputPrintf(out," raw%lu",(unsigned long)p);
            if(!span->fixedCount){
                xl6OutputPrintf(out,", %s",span->countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int" : "size_t");
                if(names)xl6OutputPrintf(out," raw%lu_count",(unsigned long)p);
            }
            continue;
        }
        xl6OutputPuts(xl6IrCgenCallbackNativeType(module,signature->params[p]),out);
        if(names)xl6OutputPrintf(out," raw%lu",(unsigned long)p);
    }
    if(row->callbackUserMode==XL6_FOREIGN_CALLBACK_USER_LAST) {
        if(signature->paramCount)xl6OutputPuts(", ",out);
        xl6OutputPuts(names ? "void* userdata" : "void*",out);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitNativeCallbackTypes(XL6_Output* out,const XL6_IrModule* module)
{
    bool regionHeader=false;
    for(size_t i=0;i<module->foreignParameterCount;++i) {
        const XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(row->kind==XL6_FOREIGN_PARAMETER_SPAN)continue;
        const XL6_IrCallTarget* target=NULL;size_t p=0;
        for(size_t t=0;t<module->callTargetCount && !target;++t)
            for(size_t a=0;a<module->callTargets[t].parameterCount;++a)
                if(module->callTargets[t].reachability!=XL6_IR_REACHABILITY_UNREACHABLE &&
                    xl6IrForeignParameterGet(module,&module->callTargets[t],a)==row) {target=&module->callTargets[t];p=a;break;}
        if(!target)continue;
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,p);
        for(size_t a=0;!regionHeader && a<signature->paramCount;++a)
            if(xl6ForeignCallbackRegion(xl6IrForeignCallbackSpanGet(module,row,a))){
                xl6OutputPuts("#include \"__xl_runtime/xl6_foreign_region_view.h\"\n",out);
                regionHeader=true;
            }
        xl6OutputPrintf(out,"typedef %s (*%s)(",signature->returnCount ?
            xl6IrCgenCallbackNativeType(module,signature->callableResultType) : "void",row->callbackTypeCName);
        if(!xl6IrCgenCallbackParameters(out,module,signature,row,false))return 0;
        xl6OutputPuts(");\n",out);
        for(size_t a=0;a<signature->paramCount;++a){
            const XL6_ForeignCallbackSpanContract* span=xl6IrForeignCallbackSpanGet(module,row,a);
            if(!span)continue;
            const XL6_IrType* view=xl6IrTypeGet(&module->types,signature->params[a]);
            char name[128];
            if(xl6ForeignCallbackOwnedOutput(span)){
                if(!xl6ForeignOwnedOutputPointeeName(view->cName,name,sizeof(name)))return 0;
                xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&%s,void (*)(%s*)),"
                    "\"owned callback output needs an exact consuming rollback function\");\n",span->rollbackCName,name);
                continue;
            }
            const char* spanType=xl6IrCgenCallbackSpanType(span,view,name,sizeof(name));
            if(!spanType)return 0;
            if(xl6ForeignCallbackRegion(span)){
                const XL6_ForeignCallbackRegionObserver* observer=&span->regionObserver;
                if(observer->function[0])
                    xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&%s,int (*)(%s,%s,%s*)),"
                        "\"callback region observer must have the exact status-output ABI\");\n",
                        observer->function,
                        xl6IrCgenCallbackNativeType(module,signature->params[observer->inputs[0]]),
                        xl6IrCgenCallbackNativeType(module,signature->params[observer->inputs[1]]),observer->record);
                const XL6_ForeignCallbackRegionExtent* extents[]={&span->regionWidth,&span->regionHeight};
                for(size_t e=0;e<XL6_ARRAY_COUNT(extents);++e){
                    const XL6_ForeignCallbackRegionExtent* extent=extents[e];
                    if(extent->field[0])
                        xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((const %s*)0)->%s,int),"
                            "\"callback region extent requires an int field\");\n",
                            observer->function[0] ? observer->record:
                            xl6IrCgenCallbackNativeType(module,signature->params[extent->argument]),extent->field);
                }
            }
            if(span->countsElements){
                xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((const %s*)0)->Data,%s),"
                    "\"callback element span Data has the native const element pointer type\");\n"
                    "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((const %s*)0)->Count,size_t),"
                    "\"callback element span Count has type size_t\");\n",
                    spanType,span->pointerCName,spanType);
            }else
                xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s)0,const %s*),\"callback span requires const %s pointer\");\n",
                    view->cName,spanType,spanType);
        }
    }
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenPrepareCallbackRegion(XL6_Output* out,const XL6_ForeignCallbackSpanContract* span,size_t argument)
{
    const unsigned long p=(unsigned long)argument;
    const XL6_ForeignCallbackRegionObserver* observer=&span->regionObserver;
    if(!span->regionNullable)
        xl6OutputPrintf(out,"    if (!errors.failed && !raw%lu) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }\n",p);
    if(observer->function[0]){
        xl6OutputPrintf(out,"    %s native_region_extent_%lu={0};\n"
            "    if (!errors.failed && (!raw%u || !raw%u)) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }\n"
            "    if (!errors.failed) { int status=%s(raw%u,raw%u,&native_region_extent_%lu);\n"
            "        if(status!=0 || xrtGetError()) {\n"
            "            if(!xrtGetError()) xl6ForeignStatusFailureV1(status,\"%s\");\n"
            "            xl6ErrorScopeCaptureFailure(&errors);\n        }\n    }\n",
            observer->record,p,observer->inputs[0],observer->inputs[1],observer->function,
            observer->inputs[0],observer->inputs[1],p,observer->function);
    }
    xl6OutputPuts("    if (!errors.failed && !xl6ForeignCallbackRegionLayoutV1(",out);
    const XL6_ForeignCallbackRegionExtent* extents[]={&span->regionWidth,&span->regionHeight};
    for(size_t e=0;e<XL6_ARRAY_COUNT(extents);++e){
        const XL6_ForeignCallbackRegionExtent* extent=extents[e];
        if(e)xl6OutputPuts(",",out);
        if(observer->function[0])xl6OutputPrintf(out,"native_region_extent_%lu.%s",p,extent->field);
        else{
            xl6OutputPrintf(out,"raw%u",extent->argument);
            if(extent->field[0])xl6OutputPrintf(out,".%s",extent->field);
        }
    }
    xl6OutputPrintf(out,",raw%lu_count,%u,&native_span_%lu.RowBytes,&native_span_%lu.RowCount,&native_span_%lu.Stride)) "
        "xl6ErrorScopeCaptureFailure(&errors);\n",p,span->regionUnitBytes,p,p,p);
    xl6OutputPrintf(out,"    if (!errors.failed && raw%lu) { XL6_ForeignScopeViewV1 borrowed={0}; native_span_%lu.Data=(%suint8_t*)raw%lu;\n"
        "        if (!xl6ForeignScopeViewMakeV2(&lifetime,&native_span_%lu,&borrowed)) xl6ErrorScopeCaptureFailure(&errors);\n"
        "        else arg%lu = %s;\n    }\n",p,p,xl6ForeignCallbackSpanWritable(span) ? "":"const ",p,p,p,span->regionNullable ?
        "(XL6_NullableForeignScopeViewV1){true,borrowed}":"borrowed");
}
/* Lifetime expiry and all callback-leave checks precede publication. The
 * consuming rollback runs in its own diagnostic scope. An explicit native
 * status keeps its numeric value when cleanup reports a secondary failure. */
static void xl6IrCgenCallbackPublishOutputs(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrType* signature,const XL6_ForeignParameterContract* row,bool sync)
{
    xl6OutputPuts("    if (outcome.ok && result==0",out);
    for(size_t p=0;p<signature->paramCount;++p)
        if(xl6ForeignCallbackOwnedOutput(xl6IrForeignCallbackSpanGet(module,row,p)))
            xl6OutputPrintf(out," && staged%lu",(unsigned long)p);
    xl6OutputPuts(") {\n",out);
    for(size_t p=0;p<signature->paramCount;++p)
        if(xl6ForeignCallbackOwnedOutput(xl6IrForeignCallbackSpanGet(module,row,p)))
            xl6OutputPrintf(out,"        *raw%lu=staged%lu;\n",(unsigned long)p,(unsigned long)p);
    xl6OutputPuts("    } else {\n"
        "        xl6_error_scope cleanup={0};\n"
        "        if (!xl6ErrorScopeBegin(&cleanup)) abort();\n"
        "        if (outcome.ok && result==0) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&cleanup); }\n",out);
    for(size_t p=signature->paramCount;p>0;--p){
        const XL6_ForeignCallbackSpanContract* projection=xl6IrForeignCallbackSpanGet(module,row,p-1);
        if(xl6ForeignCallbackOwnedOutput(projection))
            xl6OutputPrintf(out,"        if (staged%lu) { %s(staged%lu); if(xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup); }\n",
                (unsigned long)(p-1),projection->rollbackCName,(unsigned long)(p-1));
    }
    xl6OutputPuts("        xl6_error_result cleaned=xl6ErrorScopeEnd(&cleanup);\n"
        "        if (outcome.ok && result==0) outcome=cleaned;\n"
        "        else if (outcome.ok && !cleaned.ok) { ",out);
    xl6OutputPuts(sync ? "xl6SyncCallbackReportV1(&record->invocation,cleaned);" : "xl6NativeCallbackReportFailureV1(record,cleaned);",out);
    xl6OutputPuts(" } else xrtErrorFree(cleaned.error);\n    }\n",out);
}

static int xl6IrCgenEmitNativeCallbackThunks(XL6_Output* out,const XL6_IrModule* module)
{
    for(size_t i=0;i<module->foreignParameterCount;++i) {
        const XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(row->kind==XL6_FOREIGN_PARAMETER_SPAN)continue;
        const XL6_IrCallTarget* target=NULL;size_t parameter=0;
        for(size_t t=0;t<module->callTargetCount && !target;++t)
            for(size_t p=0;p<module->callTargets[t].parameterCount;++p)
                if(module->callTargets[t].reachability!=XL6_IR_REACHABILITY_UNREACHABLE &&
                    xl6IrForeignParameterGet(module,&module->callTargets[t],p)==row) {target=&module->callTargets[t];parameter=p;break;}
        if(!target)continue;
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,parameter);
        bool sync=row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK;
        size_t member=0;
        const XL6_ForeignPackContract* group=sync ? NULL:xl6IrForeignPackCallbackMember(module,target,parameter,&member,NULL);
        if(row->callbackUserMode==XL6_FOREIGN_CALLBACK_USER_RECEIVER){
            if(!group || !signature->paramCount)return 0;
            const char* receiver=xl6IrCgenCallbackNativeType(module,signature->params[0]);
            xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s)0,%s*) || XLIR_NATIVE_IS_TYPE((%s)0,const %s*),"
                "\"native callback receiver matches its descriptor\");\n",
                receiver,group->recordCName,receiver,group->recordCName);
        }
        const XL6_IrType* root=row->callbackOwnerIndexPlusOne ? xl6IrForeignRootType(module,target->parameterTypes[row->callbackOwnerIndexPlusOne-1]) : NULL;
        const char* ownerScope=sync ? "native_owner" : "scope.owner";
        if(row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK){
            xl6OutputPrintf(out,"typedef struct %s_context { XL6_SyncCallbackContextV1 invocation;",row->callbackTypeCName);
            if(row->callbackOwnerIndexPlusOne)xl6OutputPuts(" XL6_ForeignView* owner;",out);
            xl6OutputPrintf(out," } %s_context;\n",row->callbackTypeCName);
        }
        const char* result=signature->returnCount ? xl6IrCgenCallbackNativeType(module,signature->callableResultType) : "void";
        xl6OutputPrintf(out,"static %s %s_thunk(",result,row->callbackTypeCName);
        if(!xl6IrCgenCallbackParameters(out,module,signature,row,true))return 0;
        xl6OutputPuts(")\n{\n",out);
        if(sync){
            xl6OutputPrintf(out,"    %s_context* record=userdata;\n    XL6_SyncCallbackScopeV1 scope={0};\n",row->callbackTypeCName);
            if(root)xl6OutputPuts("    XL6_ForeignViewCallV1 native_owner={0};\n",out);
        }else {
            if(row->callbackUserMode==XL6_FOREIGN_CALLBACK_USER_RECEIVER)
                xl6OutputPrintf(out,"    void* userdata=raw0 ? raw0->%s : NULL;\n",group->callbackUserField);
            if(group)xl6OutputPrintf(out,"    const XL6_NativeCallbackGroupV1* group=userdata;\n"
                "    XL6_NativeCallback* record=group && group->Members && group->Count>%lu ? group->Members[%lu]:NULL;\n",
                (unsigned long)member,(unsigned long)member);
            else xl6OutputPuts("    XL6_NativeCallback* record=userdata;\n",out);
            xl6OutputPuts("    XL6_NativeCallbackScopeV1 scope={0};\n",out);
        }
        xl6OutputPuts("    xl6_error_scope errors={0};\n    if (!xl6ErrorScopeBegin(&errors)) abort();\n",out);
        if(signature->returnCount)xl6OutputPrintf(out,"    %s result = {%d};\n",result,row->callbackFailureCode);
        bool scoped=false,ownedOutputs=false;
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            if(item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
                scoped=true;xl6OutputPrintf(out,"    %s arg%lu = {0};\n",xl6IrCgenType(module,signature->params[p]),(unsigned long)p);
                const XL6_ForeignCallbackSpanContract* span=xl6IrForeignCallbackSpanGet(module,row,p);
                if(xl6ForeignCallbackOwnedOutput(span)){
                    char name[128];
                    if(!xl6ForeignOwnedOutputPointeeName(item->cName,name,sizeof(name)))return 0;
                    ownedOutputs=true;
                    xl6OutputPrintf(out,"    %s* staged%lu=NULL;\n    if (raw%lu) *raw%lu=NULL;\n",name,
                        (unsigned long)p,(unsigned long)p,(unsigned long)p);
                }else if(span){
                    char name[64];
                    const char* spanType=xl6IrCgenCallbackSpanType(span,item,name,sizeof(name));
                    if(!spanType)return 0;
                    xl6OutputPrintf(out,"    %s native_span_%lu={0};\n",spanType,(unsigned long)p);
                }
            }else if(item->foreignPolicy)xl6OutputPrintf(out,"    XL6_ForeignView* arg%lu = NULL;\n",(unsigned long)p);
        }
        if(scoped)xl6OutputPuts("    XL6_ForeignScopeLifetimeV2 lifetime={0};\n",out);
        if(root && root->foreignScope)xl6OutputPrintf(out,"    %s native_scope = {0};\n",root->foreignScope);
        if(sync){
            xl6OutputPrintf(out,"    if (!xl6SyncCallbackEnterV1(record ? &record->invocation : NULL,0x%016llXULL,&scope)) xl6ErrorScopeCaptureFailure(&errors);\n    else {\n",(unsigned long long)signature->typeValueId);
            if(root)xl6OutputPrintf(out,"    if (!xl6ForeignViewCallBeginV1(record->owner,&native_owner,%s)) xl6ErrorScopeCaptureFailure(&errors);\n",root->foreignScope ? "&native_scope,sizeof(native_scope)" : "NULL,0");
        }else xl6OutputPrintf(out,"    if (!xl6NativeCallbackRuntimeV1Get()->Enter(record,0x%016llXULL,&scope,%s)) xl6ErrorScopeCaptureFailure(&errors);\n"
            "    else {\n",(unsigned long long)signature->typeValueId,
            root && root->foreignScope ? "&native_scope,sizeof(native_scope)" : "NULL,0");
        if(scoped)xl6OutputPrintf(out,"    if (!errors.failed && !xl6ForeignScopeBeginV2(&lifetime,%s%s)) xl6ErrorScopeCaptureFailure(&errors);\n",root ? "&" : "",root ? ownerScope : "NULL");
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            if(item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
                const XL6_ForeignCallbackSpanContract* span=xl6IrForeignCallbackSpanGet(module,row,p);
                if(xl6ForeignCallbackOwnedOutput(span)){
                    xl6OutputPrintf(out,"    if (!errors.failed && !raw%lu) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }\n"
                        "    if (!errors.failed && !xl6ForeignScopeViewMakeV2(&lifetime,&staged%lu,&arg%lu)) xl6ErrorScopeCaptureFailure(&errors);\n",
                        (unsigned long)p,(unsigned long)p,(unsigned long)p);
                    continue;
                }
                if(xl6ForeignCallbackRegion(span)){
                    xl6IrCgenPrepareCallbackRegion(out,span,p);
                    continue;
                }
                if(span){
                    if(span->fixedCount)
                        xl6OutputPrintf(out,"    const size_t raw%lu_count=%u;\n",(unsigned long)p,span->fixedCount);
                    xl6OutputPuts("    if(!errors.failed && (",out);
                    if(span->countAbi==XL6_FOREIGN_SPAN_COUNT_INT)xl6OutputPrintf(out,"raw%lu_count<0 || ",(unsigned long)p);
                    if(span->countsElements){
                        char element[128];
                        if(!xl6ForeignCallbackSpanPointeeName(span->pointerCName,element,sizeof(element)))return 0;
                        xl6OutputPrintf(out,"(uintmax_t)raw%lu_count>SIZE_MAX/sizeof(%s) || "
                            "(raw%lu_count && (uintptr_t)raw%lu %% XLIR_NATIVE_ALIGNOF(%s)) || ",
                            (unsigned long)p,element,(unsigned long)p,(unsigned long)p,element);
                    }
                    xl6OutputPrintf(out,"(uintmax_t)raw%lu_count>INT64_MAX || (raw%lu_count && !raw%lu))) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }\n"
                        "    if(!errors.failed) { XL6_ForeignScopeViewV1 borrowed={0};\n",
                        (unsigned long)p,(unsigned long)p,(unsigned long)p);
                    if(span->countsElements)
                        xl6OutputPrintf(out,"        native_span_%lu.Data=raw%lu; native_span_%lu.Count=(size_t)raw%lu_count;\n",
                            (unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p);
                    else if(xl6ForeignCallbackSpanWritable(span))
                        xl6OutputPrintf(out,"        native_span_%lu=(XL6_WritableBytesViewV1){(uint8_t*)raw%lu,(size_t)raw%lu_count};\n",
                            (unsigned long)p,(unsigned long)p,(unsigned long)p);
                    else
                        xl6OutputPrintf(out,"        native_span_%lu=(xbytesview){raw%lu ? (const uint8_t*)raw%lu : (const uint8_t*)\"\",(size_t)raw%lu_count};\n",
                            (unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p);
                    xl6OutputPrintf(out,
                        "        if(!xl6ForeignScopeViewMakeV2(&lifetime,&native_span_%lu,&borrowed)) xl6ErrorScopeCaptureFailure(&errors);\n"
                        "        else arg%lu=borrowed;\n    }\n",
                        (unsigned long)p,(unsigned long)p);
                    continue;
                }
                xl6OutputPrintf(out,"    if (!errors.failed && raw%lu) { XL6_ForeignScopeViewV1 borrowed={0};\n"
                    "        if (!xl6ForeignScopeViewMakeV2(&lifetime,raw%lu,&borrowed)) xl6ErrorScopeCaptureFailure(&errors);\n        else arg%lu = ",
                    (unsigned long)p,(unsigned long)p,(unsigned long)p);
                if(item->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("(XL6_NullableForeignScopeViewV1){true,borrowed}",out);
                else xl6OutputPuts("borrowed",out);
                xl6OutputPuts("; }\n",out);
                if(item->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out,"    if (!errors.failed && !raw%lu) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&errors); }\n",(unsigned long)p);
                continue;
            }
            if(!item->foreignPolicy)continue;
            xl6OutputPrintf(out,"    if (!errors.failed && raw%lu) { arg%lu = %s(raw%lu,%s.root",
                (unsigned long)p,(unsigned long)p,item->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED ?
                    "xl6ForeignViewBorrowRootV1" : "xl6ForeignViewBorrowChildV1",(unsigned long)p,ownerScope);
            if(item->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNED)
                xl6OutputPrintf(out,",&%s,xlir_closure_owner",item->foreignPolicy);
            xl6OutputPrintf(out,"); if (!arg%lu) xl6ErrorScopeCaptureFailure(&errors); }\n",(unsigned long)p);
            if(item->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out,
                "    if (!errors.failed && !raw%lu) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&errors); }\n",(unsigned long)p);
        }
        xl6OutputPrintf(out,"    if (!errors.failed && !((bool(*)(void*%s",signature->returnCount ? ", " : "");
        if(signature->returnCount)xl6OutputPrintf(out,"%s*",result);
        for(size_t p=0;p<signature->paramCount;++p)xl6OutputPrintf(out,", %s",xl6IrCgenType(module,signature->params[p]));
        xl6OutputPrintf(out,"))%s->invoke)(%s->environment",sync ? "scope.closure" : "scope.closure.closure",sync ? "scope.closure" : "scope.closure.closure");
        if(signature->returnCount)xl6OutputPuts(", &result",out);
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            xl6OutputPrintf(out,", %s%lu",(item->foreignPolicy || item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) ? "arg" : "raw",(unsigned long)p);
        }
        xl6OutputPuts(")) xl6ErrorScopeCaptureFailure(&errors);\n    }\n",out);
        for(size_t p=signature->paramCount;p>0;--p)
            if(xl6IrTypeGet(&module->types,signature->params[p-1])->foreignPolicy)
                xl6OutputPrintf(out,"    if (arg%lu && !xl6ForeignViewReleaseV1(arg%lu)) xl6ErrorScopeCaptureFailure(&errors);\n",(unsigned long)(p-1),(unsigned long)(p-1));
        if(scoped)xl6OutputPuts("    if (lifetime.self && !xl6ForeignScopeEndV2(&lifetime)) xl6ErrorScopeCaptureFailure(&errors);\n",out);
        if(sync && root)xl6OutputPuts("    if(native_owner.self && !xl6ForeignViewCallEndV1(&native_owner)) xl6ErrorScopeCaptureFailure(&errors);\n",out);
        xl6OutputPuts("    xl6_error_result outcome = xl6ErrorScopeEnd(&errors);\n",out);
        xl6OutputPrintf(out,"    if (scope.self) outcome=%s;\n",sync ? "xl6SyncCallbackLeaveV1(&scope,outcome)" : "xl6NativeCallbackRuntimeV1Get()->Leave(&scope,outcome)");
        if(ownedOutputs)xl6IrCgenCallbackPublishOutputs(out,module,signature,row,sync);
        xl6OutputPuts("    if (!outcome.ok) {",out);
        if(row->callbackResultKind==XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT)xl6OutputPuts(" xrtFree((void*)result.Data);",out);
        xl6OutputPuts(sync ? " xl6SyncCallbackReportV1(&record->invocation,outcome);" : " xl6NativeCallbackReportFailureV1(record,outcome);",out);
        if(signature->returnCount)xl6OutputPrintf(out," result=(%s){%d};",result,row->callbackFailureCode);
        xl6OutputPuts(" }\n",out);
        if(signature->returnCount)xl6OutputPuts("    return result;\n",out);
        xl6OutputPuts("}\n",out);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenNativeCallbacksBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,const char* errors)
{
    int count=0;
    for(size_t i=0;i<module->foreignPackCount;++i){const XL6_ForeignPackContract* pack=&module->foreignPacks[i];
        if(pack->declaration!=target->declaration || !pack->callbackOwnerPhysicalIndexPlusOne)continue;
        size_t members=0;
        for(size_t p=pack->firstParameter;p<pack->firstParameter+pack->parameterCount;++p)
            if(xl6IrForeignPackCallbackMember(module,target,p,NULL,NULL))++members;
        if(!members || members>pack->parameterCount)return -1;
        if(!count++)xl6OutputPuts("    { bool native_callbacks_ok=true;\n",out);
        xl6OutputPrintf(out,"    XL6_NativeCallbackGroupV1* native_callback_group_%u=NULL;\n"
            "    %s native_callback_group_owner_%u={0};\n"
            "    XL6_Closure* native_callback_group_closures_%u[%lu]={",
            pack->physicalIndex,pack->callbackGroupOwnerCName,pack->physicalIndex,pack->physicalIndex,(unsigned long)members);
        size_t n=0;
        for(size_t p=pack->firstParameter;p<pack->firstParameter+pack->parameterCount;++p)
            if(xl6IrForeignPackCallbackMember(module,target,p,NULL,NULL)){
                if(n++)xl6OutputPuts(",",out);
                xl6IrCgenForeignViewArgument(out,values,p,NULL);
            }
        xl6OutputPrintf(out,"};\n    XL6_ForeignView* native_callback_group_contexts_%u[%lu]={",pack->physicalIndex,(unsigned long)members);n=0;
        for(size_t p=pack->firstParameter;p<pack->firstParameter+pack->parameterCount;++p){
            if(!xl6IrForeignPackCallbackMember(module,target,p,NULL,NULL))continue;
            const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,p);
            if(n++)xl6OutputPuts(",",out);
            if(row->callbackOwnerIndexPlusOne)xl6IrCgenForeignViewArgument(out,values,row->callbackOwnerIndexPlusOne-1,NULL);
            else xl6OutputPuts("NULL",out);
        }
        xl6OutputPrintf(out,"};\n    const uint64_t native_callback_group_signatures_%u[%lu]={",pack->physicalIndex,(unsigned long)members);n=0;
        for(size_t p=pack->firstParameter;p<pack->firstParameter+pack->parameterCount;++p){
            if(!xl6IrForeignPackCallbackMember(module,target,p,NULL,NULL))continue;
            const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,p);if(!signature)return -1;
            if(n++)xl6OutputPuts(",",out);
            xl6OutputPrintf(out,"0x%016llXULL",(unsigned long long)signature->typeValueId);
        }
        xl6OutputPuts("};\n    if (native_callbacks_ok",out);
        if(pack->callbackGroupOptional){
            xl6OutputPuts(" && (",out);
            for(size_t member=0;member<members;++member)
                xl6OutputPrintf(out,"%snative_callback_group_closures_%u[%lu]",
                    member ? " || ":"",pack->physicalIndex,(unsigned long)member);
            xl6OutputPuts(")",out);
        }
        xl6OutputPrintf(out,") { native_callback_group_%u=xl6NativeCallbackGroupRuntimeV1Get()->Create("
            "xlir_closure_owner,native_callback_group_closures_%u,native_callback_group_contexts_%u,native_callback_group_signatures_%u,%lu);\n"
            "    if (!native_callback_group_%u) { native_callbacks_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    else native_callback_group_owner_%u=(%s){xl6NativeCallbackGroupRuntimeV1Get()->Ownership(native_callback_group_%u),"
            "xl6NativeCallbackGroupRuntimeV1Get()->HoldNative,xl6NativeCallbackGroupRuntimeV1Get()->DropNative}; }\n",
            pack->physicalIndex,pack->physicalIndex,pack->physicalIndex,pack->physicalIndex,(unsigned long)members,
            pack->physicalIndex,errors,pack->physicalIndex,pack->callbackGroupOwnerCName,pack->physicalIndex);
    }
    for(size_t p=0;p<target->parameterCount;++p) {
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK &&
            target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK)continue;
        const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,p);
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,p);
        if(!row || !signature)return -1;
        if(xl6IrForeignPackCallbackMember(module,target,p,NULL,NULL))continue;
        if(!count++)xl6OutputPuts("    { bool native_callbacks_ok=true;\n",out);
        if(row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK){
            xl6OutputPrintf(out,"    %s_context native_callback_%lu={0};\n",row->callbackTypeCName,(unsigned long)p);
            if(row->callbackOwnerIndexPlusOne)xl6OutputPrintf(out,"    native_callback_%lu.owner=native_view_%lu.root;\n",(unsigned long)p,(unsigned long)(row->callbackOwnerIndexPlusOne-1));
            xl6OutputPuts("    if(native_callbacks_ok && ",out);xl6IrCgenForeignViewArgument(out,values,p,NULL);
            xl6OutputPrintf(out," && !xl6SyncCallbackBeginV1(&native_callback_%lu.invocation,xlir_closure_owner,",(unsigned long)p);
            xl6IrCgenForeignViewArgument(out,values,p,NULL);
            xl6OutputPrintf(out,",0x%016llXULL)) { native_callbacks_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",(unsigned long long)signature->typeValueId,errors);
            continue;
        }
        xl6OutputPrintf(out,"    XL6_NativeCallback* native_callback_%lu=NULL; %s native_callback_owner_%lu={0};\n"
            "    if (native_callbacks_ok && ",(unsigned long)p,row->callbackOwnerCName,(unsigned long)p);
        xl6IrCgenForeignViewArgument(out,values,p,NULL);
        xl6OutputPrintf(out,") { native_callback_%lu=xl6NativeCallbackRuntimeV1Get()->Create(xlir_closure_owner,",(unsigned long)p);
        xl6IrCgenForeignViewArgument(out,values,p,NULL);xl6OutputPuts(",",out);
        if(row->callbackOwnerIndexPlusOne)xl6IrCgenForeignViewArgument(out,values,row->callbackOwnerIndexPlusOne-1,NULL);
        else xl6OutputPuts("NULL",out);
        xl6OutputPrintf(out,",0x%016llXULL); if (!native_callback_%lu) { native_callbacks_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    else native_callback_owner_%lu=(%s){xl6NativeCallbackRuntimeV1Get()->Ownership(native_callback_%lu),"
            "xl6NativeCallbackRuntimeV1Get()->HoldNative,xl6NativeCallbackRuntimeV1Get()->DropNative}; }\n",
            (unsigned long long)signature->typeValueId,(unsigned long)p,errors,(unsigned long)p,row->callbackOwnerCName,(unsigned long)p);
    }
    if(count)xl6OutputPuts("    if (native_callbacks_ok) {\n",out);
    return count;
}
static int xl6IrCgenCallbackGroupOwnerArgument(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,uint32 packIndex)
{
    const XL6_ForeignPackContract* pack=xl6IrForeignPackGet(module,target,packIndex);
    if(!pack)return 0;
    if(pack->callbackGroupOptional)
        xl6OutputPrintf(out,"(native_callback_group_%u ? &native_callback_group_owner_%u : NULL)",packIndex,packIndex);
    else xl6OutputPrintf(out,"&native_callback_group_owner_%u",packIndex);
    return 1;
}
static void xl6IrCgenNativeCallbacksObserve(XL6_Output* out,const XL6_IrCallTarget* target,const char* errors)
{
    for(size_t p=0;p<target->parameterCount;++p)
        if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK)
            xl6OutputPrintf(out,"    if(native_callback_%lu.invocation.self) { xl6_error_result callback_result=xl6SyncCallbackCompleteV1(&native_callback_%lu.invocation); if(!callback_result.ok) { xl6ErrorScopePropagateFailure(callback_result); xl6ErrorScopeCaptureFailure(&%s); } }\n",(unsigned long)p,(unsigned long)p,errors);
}
static void xl6IrCgenNativeCallbacksEnd(XL6_Output* out,const XL6_IrModule* module,const XL6_IrCallTarget* target,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK)
            xl6OutputPrintf(out,"    if(native_callback_%lu.invocation.self) { xl6_error_result callback_cleanup=xl6SyncCallbackEndV1(&native_callback_%lu.invocation); if(!callback_cleanup.ok) { xl6ErrorScopePropagateFailure(callback_cleanup); xl6ErrorScopeCaptureFailure(&%s); } }\n",(unsigned long)(p-1),(unsigned long)(p-1),errors);
        else if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK &&
            !xl6IrForeignPackCallbackMember(module,target,p-1,NULL,NULL))
            xl6OutputPrintf(out,"    if (native_callback_%lu && !xl6NativeCallbackRuntimeV1Get()->Release(native_callback_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),errors);
    for(size_t i=module->foreignPackCount;i>0;--i){const XL6_ForeignPackContract* pack=&module->foreignPacks[i-1];
        if(pack->declaration==target->declaration && pack->callbackOwnerPhysicalIndexPlusOne)
            xl6OutputPrintf(out,"    if (native_callback_group_%u && !xl6NativeCallbackGroupRuntimeV1Get()->Release(native_callback_group_%u)) xl6ErrorScopeCaptureFailure(&%s);\n",
                pack->physicalIndex,pack->physicalIndex,errors);
    }
    xl6OutputPuts("    }\n",out);
}
