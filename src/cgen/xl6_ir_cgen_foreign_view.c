/* Mechanical rendering of a verified native-view call transaction. Typed C
 * scope records, logical controls and physical native pointers stay distinct. */
static void xl6IrCgenForeignViewArgument(XL6_Output* out,const XL6_IrValueId* values,
    size_t p,const char* receiver)
{
    if(values)xl6OutputPrintf(out,"v%u",(unsigned)values[p]);
    else if(!p)xl6OutputPuts(receiver ? receiver : "receiver",out);
    else xl6OutputPrintf(out,"a%lu",(unsigned long)(p-1));
}
static int xl6IrCgenForeignViewsBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,const char* receiver,const char* errors)
{
    int count=0;
    for(size_t p=0;p<target->parameterCount;++p)
        if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)++count;
    if(!count)return 0;
    xl6OutputPuts("    { bool native_views_ok = true;\n",out);
    for(size_t p=0;p<target->parameterCount;++p){
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)continue;
        const XL6_IrType* root=xl6IrForeignRootType(module,target->parameterTypes[p]);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[p]);
        xl6OutputPrintf(out,"    XL6_ForeignViewCallV1 native_view_%lu = {0};\n",(unsigned long)p);
        if(type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
            xl6OutputPuts("    if (native_views_ok",out);
            if(type->kind==XL6_IR_TYPE_NULLABLE){
                xl6OutputPuts(" && ",out);xl6IrCgenForeignViewArgument(out,values,p,receiver);xl6OutputPuts(".hasValue",out);
            }
            xl6OutputPuts(") { if (!xl6ForeignScopeViewGetV1(",out);
            xl6IrCgenForeignViewArgument(out,values,p,receiver);
            if(type->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts(".value",out);
            xl6OutputPrintf(out,", &native_view_%lu.raw, &native_view_%lu.root)) { native_views_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",
                (unsigned long)p,(unsigned long)p,errors);
            continue;
        }
        if(root->foreignScope)xl6OutputPrintf(out,"    %s native_scope_%lu = {0};\n",root->foreignScope,(unsigned long)p);
        xl6OutputPuts("    if (native_views_ok",out);
        if(type->kind==XL6_IR_TYPE_NULLABLE){
            xl6OutputPuts(" && ",out);xl6IrCgenForeignViewArgument(out,values,p,receiver);xl6OutputPuts(" != NULL",out);
        }
        xl6OutputPuts(") { if (!xl6ForeignViewCallBeginV1(",out);
        xl6IrCgenForeignViewArgument(out,values,p,receiver);
        xl6OutputPrintf(out,", &native_view_%lu, ",(unsigned long)p);
        if(root->foreignScope)xl6OutputPrintf(out,"&native_scope_%lu, sizeof(native_scope_%lu)",(unsigned long)p,(unsigned long)p);
        else xl6OutputPuts("NULL, 0",out);
        xl6OutputPrintf(out,")) { native_views_ok = false; xl6ErrorScopeCaptureFailure(&%s); } }\n",errors);
    }
    xl6OutputPuts("    if (native_views_ok) {\n",out);return count;
}
static void xl6IrCgenForeignViewsEnd(XL6_Output* out,const XL6_IrCallTarget* target,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)
            xl6OutputPrintf(out,"    if (native_view_%lu.self && !xl6ForeignViewCallEndV1(&native_view_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),errors);
    xl6OutputPuts("    }\n",out);
}
static int xl6IrCgenFactoryProbeBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* errors)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    if(target->resultConversionAbi!=XL6_IR_CALL_RESULT_CONVERSION_NATIVE_VIEW_ADMIT ||
        !declaration->foreignCall.outputIndexPlusOne)return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->returnType);
    xl6OutputPuts("    { XL6_ForeignResultProbeV1 native_result_probe = {0};\n",out);
    if(type->foreignWatch)xl6OutputPrintf(out,"    %s native_result_watch = {0};\n",type->foreignWatch);
    xl6OutputPrintf(out,"    if (!xl6ForeignResultProbePrepareV1(&native_result_probe, &%s, %s, ",
        type->foreignPolicy,type->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED ? "true" : "false");
    xl6OutputPuts(type->foreignWatch ? "&native_result_watch, sizeof(native_result_watch)" : "NULL, 0",out);
    xl6OutputPrintf(out,")) { xl6ErrorScopeCaptureFailure(&%s); } else {\n",errors);return 1;
}
static void xl6IrCgenFactoryProbeEnd(XL6_Output* out,const char* errors)
{
    xl6OutputPrintf(out,"    }\n    if (native_result_probe.self && !xl6ForeignResultProbeEndV1(&native_result_probe)) xl6ErrorScopeCaptureFailure(&%s);\n    }\n",errors);
}
static void xl6IrCgenNativeViewAdmission(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,const char* receiver,
    const char* place,const char* raw,const char* errors,const char* status)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->returnType);
    if(type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
        xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError()==NULL) {\n        if (%s) { XL6_ForeignScopeViewV1 derived={0};\n            if (!xl6ForeignScopeViewDeriveV1(",errors,raw);
        xl6IrCgenForeignViewArgument(out,values,declaration->foreignCall.resultOwnerIndexPlusOne-1,receiver);
        xl6OutputPrintf(out,", %s, &derived)) xl6ErrorScopeCaptureFailure(&%s);\n            else %s = ",raw,errors,place);
        if(type->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("(XL6_NullableForeignScopeViewV1){true,derived}",out);
        else xl6OutputPuts("derived",out);
        xl6OutputPuts(";\n        }",out);
        if(type->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out," else { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&%s); }",errors);
        xl6OutputPrintf(out,"\n    } else xl6ErrorScopeCaptureFailure(&%s);\n",errors);return;
    }
    if(status){
        xl6OutputPrintf(out,"    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&%s);\n"
            "    if (!xl6ForeignResultProbeObserveV1(&native_result_probe, %s)) xl6ErrorScopeCaptureFailure(&%s);\n",errors,raw,errors);
    }
    xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError() == NULL",errors);
    if(status)xl6OutputPrintf(out," && %s == %d",status,declaration->foreignCall.successCode);
    xl6OutputPrintf(out,") {\n        if (%s != NULL) { %s = ",raw,place);
    if(declaration->foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE)
        xl6OutputPrintf(out,"xl6ForeignViewCreateRootProbedV1(%s, &native_result_probe, xlir_code_owner.size ? &xlir_code_owner : NULL)",raw);
    else {
        int root=type->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED;
        xl6OutputPrintf(out,"%s(%s, ",root ? "xl6ForeignViewBorrowRootV1" : "xl6ForeignViewBorrowChildV1",raw);
        size_t source=declaration->foreignCall.resultOwnerIndexPlusOne-1;
        const XL6_IrType* sourceType=xl6IrTypeGet(&module->types,target->parameterTypes[source]);
        if(sourceType->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK)xl6OutputPrintf(out,"native_view_%lu.root",(unsigned long)source);
        else xl6IrCgenForeignViewArgument(out,values,source,receiver);
        if(!root)xl6OutputPrintf(out,", &%s, xlir_code_owner.size ? &xlir_code_owner : NULL",type->foreignPolicy);
        xl6OutputPuts(")",out);
    }
    xl6OutputPrintf(out,"; if (%s == NULL) xl6ErrorScopeCaptureFailure(&%s); }\n",place,errors);
    if(type->kind!=XL6_IR_TYPE_NULLABLE)
        xl6OutputPrintf(out,"        else { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&%s); }\n",errors);
    xl6OutputPrintf(out,"    } else { xl6ErrorScopeCaptureFailure(&%s); }\n",errors);
    if(status){
        const XL6_IrCallTarget* rollback=xl6IrCallTargetGet(module,declaration->foreignCall.rollbackSymbol);
        xl6OutputPrintf(out,"    if (%s == NULL && %s != NULL && xl6ForeignResultProbeAliveV1(&native_result_probe)) { %s(%s); }\n",place,raw,rollback->cName,raw);
    }
}
