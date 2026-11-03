/* Typed retained native callbacks. The publisher owns one snapshot through
 * the thunk's complete C return tail; logical captures never cross as xvalue. */
static const char* xl6IrCgenRetainedNativeType(const XL6_IrModule* module,XL6_IrTypeId id)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    return type && (type->foreignPolicy || type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) ? type->cName : xl6IrCgenType(module,id);
}
static int xl6IrCgenRetainedParameters(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrType* signature,const XL6_ForeignParameterContract* row,bool names)
{
    if(row->callbackUserFirst)xl6OutputPuts(names ? "void* userdata" : "void*",out);
    for(size_t p=0;p<signature->paramCount;++p) {
        if(p || row->callbackUserFirst)xl6OutputPuts(", ",out);
        xl6OutputPuts(xl6IrCgenRetainedNativeType(module,signature->params[p]),out);
        if(names)xl6OutputPrintf(out," raw%lu",(unsigned long)p);
    }
    if(!row->callbackUserFirst) {
        if(signature->paramCount)xl6OutputPuts(", ",out);
        xl6OutputPuts(names ? "void* userdata" : "void*",out);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitRetainedCallbackTypes(XL6_Output* out,const XL6_IrModule* module)
{
    for(size_t i=0;i<module->foreignParameterCount;++i) {
        const XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(row->kind!=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK)continue;
        const XL6_IrCallTarget* target=NULL;size_t p=0;
        for(size_t t=0;t<module->callTargetCount && !target;++t)
            for(size_t a=0;a<module->callTargets[t].parameterCount;++a)
                if(module->callTargets[t].reachability!=XL6_IR_REACHABILITY_UNREACHABLE &&
                    xl6IrForeignParameterGet(module,&module->callTargets[t],a)==row) {target=&module->callTargets[t];p=a;break;}
        if(!target)continue;
        const XL6_IrType* signature=xl6IrForeignRetainedSignature(module,target,p);
        xl6OutputPrintf(out,"typedef %s (*%s)(",signature->returnCount ?
            xl6IrCgenRetainedNativeType(module,signature->callableResultType) : "void",row->callbackTypeCName);
        if(!xl6IrCgenRetainedParameters(out,module,signature,row,false))return 0;
        xl6OutputPuts(");\n",out);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitRetainedCallbackThunks(XL6_Output* out,const XL6_IrModule* module)
{
    for(size_t i=0;i<module->foreignParameterCount;++i) {
        const XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(row->kind!=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK)continue;
        const XL6_IrCallTarget* target=NULL;size_t parameter=0;
        for(size_t t=0;t<module->callTargetCount && !target;++t)
            for(size_t p=0;p<module->callTargets[t].parameterCount;++p)
                if(module->callTargets[t].reachability!=XL6_IR_REACHABILITY_UNREACHABLE &&
                    xl6IrForeignParameterGet(module,&module->callTargets[t],p)==row) {target=&module->callTargets[t];parameter=p;break;}
        if(!target)continue;
        const XL6_IrType* signature=xl6IrForeignRetainedSignature(module,target,parameter);
        const XL6_IrType* root=xl6IrForeignRootType(module,target->parameterTypes[row->callbackOwnerIndexPlusOne-1]);
        const char* result=signature->returnCount ? xl6IrCgenType(module,signature->callableResultType) : "void";
        xl6OutputPrintf(out,"static %s %s_thunk(",result,row->callbackTypeCName);
        if(!xl6IrCgenRetainedParameters(out,module,signature,row,true))return 0;
        xl6OutputPuts(")\n{\n    XL6_NativeCallback* record = userdata;\n"
            "    XL6_NativeCallbackScopeV1 scope = {0}; xl6_error_scope errors = {0};\n"
            "    if (!xl6ErrorScopeBegin(&errors)) abort();\n",out);
        if(signature->returnCount)xl6OutputPrintf(out,"    %s result = {%d};\n",result,row->callbackFailureCode);
        bool scoped=false;
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            if(item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
                scoped=true;xl6OutputPrintf(out,"    %s arg%lu = {0};\n",xl6IrCgenType(module,signature->params[p]),(unsigned long)p);
            }else if(item->foreignPolicy)xl6OutputPrintf(out,"    XL6_ForeignView* arg%lu = NULL;\n",(unsigned long)p);
        }
        if(scoped)xl6OutputPuts("    XL6_ForeignScopeLifetimeV1 lifetime={0};\n",out);
        if(root->foreignScope)xl6OutputPrintf(out,"    %s native_scope = {0};\n",root->foreignScope);
        xl6OutputPrintf(out,"    if (!xl6NativeCallbackRuntimeV1Get()->Enter(record,0x%016llXULL,&scope,%s)) xl6ErrorScopeCaptureFailure(&errors);\n"
            "    else {\n",(unsigned long long)signature->typeValueId,
            root->foreignScope ? "&native_scope,sizeof(native_scope)" : "NULL,0");
        if(scoped)xl6OutputPuts("    if (!xl6ForeignScopeBeginV1(&lifetime,&scope.owner)) xl6ErrorScopeCaptureFailure(&errors);\n",out);
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            if(item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
                xl6OutputPrintf(out,"    if (!errors.failed && raw%lu) { XL6_ForeignScopeViewV1 borrowed={0};\n"
                    "        if (!xl6ForeignScopeViewMakeV1(&lifetime,raw%lu,&borrowed)) xl6ErrorScopeCaptureFailure(&errors);\n        else arg%lu = ",
                    (unsigned long)p,(unsigned long)p,(unsigned long)p);
                if(item->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("(XL6_NullableForeignScopeViewV1){true,borrowed}",out);
                else xl6OutputPuts("borrowed",out);
                xl6OutputPuts("; }\n",out);
                if(item->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out,"    if (!errors.failed && !raw%lu) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&errors); }\n",(unsigned long)p);
                continue;
            }
            if(!item->foreignPolicy)continue;
            xl6OutputPrintf(out,"    if (!errors.failed && raw%lu) { arg%lu = %s(raw%lu,scope.owner.root",
                (unsigned long)p,(unsigned long)p,item->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED ?
                    "xl6ForeignViewBorrowRootV1" : "xl6ForeignViewBorrowChildV1",(unsigned long)p);
            if(item->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNED)
                xl6OutputPrintf(out,",&%s,xlir_closure_owner",item->foreignPolicy);
            xl6OutputPrintf(out,"); if (!arg%lu) xl6ErrorScopeCaptureFailure(&errors); }\n",(unsigned long)p);
            if(item->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out,
                "    if (!errors.failed && !raw%lu) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&errors); }\n",(unsigned long)p);
        }
        xl6OutputPrintf(out,"    if (!errors.failed && !((bool(*)(void*%s",signature->returnCount ? ", " : "");
        if(signature->returnCount)xl6OutputPrintf(out,"%s*",result);
        for(size_t p=0;p<signature->paramCount;++p)xl6OutputPrintf(out,", %s",xl6IrCgenType(module,signature->params[p]));
        xl6OutputPuts("))scope.closure.closure->invoke)(scope.closure.closure->environment",out);
        if(signature->returnCount)xl6OutputPuts(", &result",out);
        for(size_t p=0;p<signature->paramCount;++p) {
            const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
            xl6OutputPrintf(out,", %s%lu",(item->foreignPolicy || item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) ? "arg" : "raw",(unsigned long)p);
        }
        xl6OutputPuts(")) xl6ErrorScopeCaptureFailure(&errors);\n    }\n",out);
        for(size_t p=signature->paramCount;p>0;--p)
            if(xl6IrTypeGet(&module->types,signature->params[p-1])->foreignPolicy)
                xl6OutputPrintf(out,"    if (arg%lu && !xl6ForeignViewReleaseV1(arg%lu)) xl6ErrorScopeCaptureFailure(&errors);\n",(unsigned long)(p-1),(unsigned long)(p-1));
        if(scoped)xl6OutputPuts("    if (lifetime.self && !xl6ForeignScopeEndV1(&lifetime)) xl6ErrorScopeCaptureFailure(&errors);\n",out);
        xl6OutputPuts("    xl6_error_result outcome = xl6ErrorScopeEnd(&errors);\n"
            "    if (scope.self) outcome=xl6NativeCallbackRuntimeV1Get()->Leave(&scope,outcome);\n"
            "    if (!outcome.ok) { xl6NativeCallbackReportFailureV1(record,outcome);",out);
        if(signature->returnCount)xl6OutputPrintf(out," result=(%s){%d};",result,row->callbackFailureCode);
        xl6OutputPuts(" }\n",out);
        if(signature->returnCount)xl6OutputPuts("    return result;\n",out);
        xl6OutputPuts("}\n",out);
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenRetainedCallbacksBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,const char* errors)
{
    int count=0;
    for(size_t p=0;p<target->parameterCount;++p) {
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK)continue;
        const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,p);
        const XL6_IrType* signature=xl6IrForeignRetainedSignature(module,target,p);
        if(!row || !signature)return -1;
        if(!count++)xl6OutputPuts("    { bool native_callbacks_ok=true;\n",out);
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
static void xl6IrCgenRetainedCallbacksEnd(XL6_Output* out,const XL6_IrCallTarget* target,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK)
            xl6OutputPrintf(out,"    if (native_callback_%lu && !xl6NativeCallbackRuntimeV1Get()->Release(native_callback_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),errors);
    xl6OutputPuts("    }\n",out);
}
