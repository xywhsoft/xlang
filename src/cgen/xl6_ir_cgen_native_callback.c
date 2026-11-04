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
static int xl6IrCgenCallbackParameters(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrType* signature,const XL6_ForeignParameterContract* row,bool names)
{
    if(row->callbackUserFirst)xl6OutputPuts(names ? "void* userdata" : "void*",out);
    for(size_t p=0;p<signature->paramCount;++p) {
        if(p || row->callbackUserFirst)xl6OutputPuts(", ",out);
        const XL6_ForeignCallbackSpanContract* span=xl6IrForeignCallbackSpanGet(module,row,p);
        if(span){
            xl6OutputPuts(span->pointerCName,out);
            if(names)xl6OutputPrintf(out," raw%lu",(unsigned long)p);
            xl6OutputPrintf(out,", %s",span->countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int" : "size_t");
            if(names)xl6OutputPrintf(out," raw%lu_count",(unsigned long)p);
            continue;
        }
        xl6OutputPuts(xl6IrCgenCallbackNativeType(module,signature->params[p]),out);
        if(names)xl6OutputPrintf(out," raw%lu",(unsigned long)p);
    }
    if(!row->callbackUserFirst) {
        if(signature->paramCount)xl6OutputPuts(", ",out);
        xl6OutputPuts(names ? "void* userdata" : "void*",out);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitNativeCallbackTypes(XL6_Output* out,const XL6_IrModule* module)
{
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
        xl6OutputPrintf(out,"typedef %s (*%s)(",signature->returnCount ?
            xl6IrCgenCallbackNativeType(module,signature->callableResultType) : "void",row->callbackTypeCName);
        if(!xl6IrCgenCallbackParameters(out,module,signature,row,false))return 0;
        xl6OutputPuts(");\n",out);
        for(size_t a=0;a<signature->paramCount;++a)if(xl6IrForeignCallbackSpanGet(module,row,a)){
            const XL6_IrType* view=xl6IrTypeGet(&module->types,signature->params[a]);
            xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s)0,const xbytesview*),\"callback span requires const xbytesview pointer\");\n",view->cName);
        }
    }
    return !xl6OutputHasFailed(out);
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
        }else xl6OutputPuts("    XL6_NativeCallback* record=userdata;\n    XL6_NativeCallbackScopeV1 scope={0};\n",out);
        xl6OutputPuts("    xl6_error_scope errors={0};\n    if (!xl6ErrorScopeBegin(&errors)) abort();\n",out);
        if(signature->returnCount)xl6OutputPrintf(out,"    %s result = {%d};\n",result,row->callbackFailureCode);
        bool scoped=false;
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            if(item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
                scoped=true;xl6OutputPrintf(out,"    %s arg%lu = {0};\n",xl6IrCgenType(module,signature->params[p]),(unsigned long)p);
                if(xl6IrForeignCallbackSpanGet(module,row,p))xl6OutputPrintf(out,"    xbytesview native_span_%lu={0};\n",(unsigned long)p);
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
                if(span){
                    xl6OutputPuts("    if(!errors.failed && (",out);
                    if(span->countAbi==XL6_FOREIGN_SPAN_COUNT_INT)xl6OutputPrintf(out,"raw%lu_count<0 || ",(unsigned long)p);
                    xl6OutputPrintf(out,"(uintmax_t)raw%lu_count>INT64_MAX || (raw%lu_count && !raw%lu))) { xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors); }\n"
                        "    if(!errors.failed) { XL6_ForeignScopeViewV1 borrowed={0};\n"
                        "        native_span_%lu=(xbytesview){raw%lu ? (const uint8_t*)raw%lu : (const uint8_t*)\"\",(size_t)raw%lu_count};\n"
                        "        if(!xl6ForeignScopeViewMakeV2(&lifetime,&native_span_%lu,&borrowed)) xl6ErrorScopeCaptureFailure(&errors);\n"
                        "        else arg%lu=borrowed;\n    }\n",
                        (unsigned long)p,(unsigned long)p,(unsigned long)p,
                        (unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p,(unsigned long)p);
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
        xl6OutputPrintf(out,"    if (scope.self) outcome=%s;\n    if (!outcome.ok) {",sync ? "xl6SyncCallbackLeaveV1(&scope,outcome)" : "xl6NativeCallbackRuntimeV1Get()->Leave(&scope,outcome)");
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
    for(size_t p=0;p<target->parameterCount;++p) {
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK &&
            target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK)continue;
        const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,p);
        const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,p);
        if(!row || !signature)return -1;
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
        xl6IrCgenForeignViewArgument(out,values,row->callbackOwnerIndexPlusOne-1,NULL);
        xl6OutputPrintf(out,",0x%016llXULL); if (!native_callback_%lu) { native_callbacks_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    else native_callback_owner_%lu=(%s){xl6NativeCallbackRuntimeV1Get()->Ownership(native_callback_%lu),"
            "xl6NativeCallbackRuntimeV1Get()->HoldNative,xl6NativeCallbackRuntimeV1Get()->DropNative}; }\n",
            (unsigned long long)signature->typeValueId,(unsigned long)p,errors,(unsigned long)p,row->callbackOwnerCName,(unsigned long)p);
    }
    if(count)xl6OutputPuts("    if (native_callbacks_ok) {\n",out);
    return count;
}
static void xl6IrCgenNativeCallbacksObserve(XL6_Output* out,const XL6_IrCallTarget* target,const char* errors)
{
    for(size_t p=0;p<target->parameterCount;++p)
        if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK)
            xl6OutputPrintf(out,"    if(native_callback_%lu.invocation.self) { xl6_error_result callback_result=xl6SyncCallbackCompleteV1(&native_callback_%lu.invocation); if(!callback_result.ok) { xl6ErrorScopePropagateFailure(callback_result); xl6ErrorScopeCaptureFailure(&%s); } }\n",(unsigned long)p,(unsigned long)p,errors);
}
static void xl6IrCgenNativeCallbacksEnd(XL6_Output* out,const XL6_IrCallTarget* target,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK)
            xl6OutputPrintf(out,"    if(native_callback_%lu.invocation.self) { xl6_error_result callback_cleanup=xl6SyncCallbackEndV1(&native_callback_%lu.invocation); if(!callback_cleanup.ok) { xl6ErrorScopePropagateFailure(callback_cleanup); xl6ErrorScopeCaptureFailure(&%s); } }\n",(unsigned long)(p-1),(unsigned long)(p-1),errors);
        else if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK)
            xl6OutputPrintf(out,"    if (native_callback_%lu && !xl6NativeCallbackRuntimeV1Get()->Release(native_callback_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),errors);
    xl6OutputPuts("    }\n",out);
}
