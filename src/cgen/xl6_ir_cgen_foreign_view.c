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
        if(xl6IrForeignViewBorrowPhysical(target->parameterPhysicalAbis[p]))++count;
    if(!count)return 0;
    /* A successful native admission may still have let a language callback
     * escape. Its independent failed bit survives a swallowed/NULL native
     * diagnostic. Keep acquired scopes for End, but admit no later operand
     * or producer after that failure. */
    xl6OutputPrintf(out,"    { bool native_views_ok = !%s.failed;\n"
        "    if (xrtGetError()) { native_views_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",errors,errors);
    for(size_t p=0;p<target->parameterCount;++p){
        if(!xl6IrForeignViewBorrowPhysical(target->parameterPhysicalAbis[p]))continue;
        const XL6_IrType* root=xl6IrForeignRootType(module,target->parameterTypes[p]);
        const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[p]);
        xl6OutputPrintf(out,"    XL6_ForeignViewCallV1 native_view_%lu = {0};\n",(unsigned long)p);
        if(type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
            xl6OutputPuts("    if (native_views_ok",out);
            if(type->kind==XL6_IR_TYPE_NULLABLE){
                xl6OutputPuts(" && ",out);xl6IrCgenForeignViewArgument(out,values,p,receiver);xl6OutputPuts(".hasValue",out);
            }
            xl6OutputPuts(") { if (!xl6ForeignScopeViewGetV2(",out);
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
        xl6OutputPrintf(out,")) { native_views_ok = false; xl6ErrorScopeCaptureFailure(&%s); } }\n"
            "    if (xrtGetError()) { native_views_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
            "    if (%s.failed) native_views_ok=false;\n",errors,errors,errors);
    }
    if(xl6IrForeignManagedPointeeWrite(module,target))
        xl6OutputPrintf(out,"    if (native_views_ok && native_view_1.raw && native_view_0.root != native_view_1.root) {\n"
            "        native_views_ok=false; xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&%s);\n    }\n",errors);
    xl6OutputPuts("    if (native_views_ok) {\n",out);return count;
}
static void xl6IrCgenForeignViewsEnd(XL6_Output* out,const XL6_IrCallTarget* target,const char* errors)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(xl6IrForeignViewBorrowPhysical(target->parameterPhysicalAbis[p-1]))
            xl6OutputPrintf(out,"    if (native_view_%lu.self && !xl6ForeignViewCallEndV1(&native_view_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),errors);
    xl6OutputPuts("    }\n",out);
}
/* Reserve the one ordinary root allocation before any native ownership is
 * acquired. Domain and code-owner admission cannot be postponed until after
 * a producer/Retain has transferred a possibly busy native resource. */
static void xl6IrCgenReserveRootProbeNamed(XL6_Output* out,const XL6_IrType* type,int recordStorage,const char* probe)
{
    xl6OutputPrintf(out,"xl6ForeignResultProbeReserveRootV1(&%s,&%s,",probe,type->foreignPolicy);
    if(type->foreignDomain)xl6OutputPrintf(out,"&%s",type->foreignDomain);else xl6OutputPuts("NULL",out);
    xl6OutputPuts(",xlir_code_owner.size ? &xlir_code_owner : NULL,",out);
    if(type->foreignWatch)xl6OutputPrintf(out,"sizeof(%s),XLIR_NATIVE_ALIGNOF(%s),",type->foreignWatch,type->foreignWatch);
    else xl6OutputPuts("0,1,",out);
    xl6OutputPuts(recordStorage ? "XL6_FOREIGN_RESULT_RECORD_UNADOPTED)":"0)",out);
}
static void xl6IrCgenReserveRootProbe(XL6_Output* out,const XL6_IrType* type,int recordStorage)
{xl6IrCgenReserveRootProbeNamed(out,type,recordStorage,"native_result_probe");}
static int xl6IrCgenFactoryProbeBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* errors)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    if(target->resultConversionAbi!=XL6_IR_CALL_RESULT_CONVERSION_NATIVE_VIEW_ADMIT ||
        (!declaration->foreignCall.outputIndexPlusOne &&
         declaration->foreignCall.borrowedViewResult!=XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE))return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->returnType);
    int owning=declaration->foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE ||
        declaration->foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE;
    xl6OutputPuts("    { XL6_ForeignResultProbeV1 native_result_probe = {0};\n",out);
    if(!owning && type->foreignWatch)xl6OutputPrintf(out,"    %s native_result_watch = {0};\n",type->foreignWatch);
    xl6OutputPrintf(out,"    bool native_probe_ok=!%s.failed;\n"
        "    if(xrtGetError()) { native_probe_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
        "    if(native_probe_ok && !",errors,errors);
    if(owning)xl6IrCgenReserveRootProbe(out,type,xl6IrForeignStorageGet(module,target)!=NULL);
    else{
        xl6OutputPrintf(out,"xl6ForeignResultProbePrepareV1(&native_result_probe, &%s, %s, ",
            type->foreignPolicy,type->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED ? "true" : "false");
        xl6OutputPuts(type->foreignWatch ? "&native_result_watch, sizeof(native_result_watch))" : "NULL, 0)",out);
    }
    xl6OutputPrintf(out,
        ") { native_probe_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
        "    if(xrtGetError()) { native_probe_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n"
        "    if(%s.failed) native_probe_ok=false;\n    if(native_probe_ok) {\n",errors,errors,errors);return 1;
}
static void xl6IrCgenFactoryProbeEnd(XL6_Output* out,const char* errors)
{
    xl6OutputPrintf(out,"    }\n    if (native_result_probe.self && !xl6ForeignResultProbeEndV1(&native_result_probe)) xl6ErrorScopeCaptureFailure(&%s);\n    }\n",errors);
}
/* A closed storage contract selects this one allocation/adoption transaction.
 * The C producer still receives its original exact record pointer, not ** and
 * not a generated per-function forwarding implementation. */
static int xl6IrCgenFactoryStorageBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const char* errors)
{
    const XL6_ForeignStorageContract* storage=xl6IrForeignStorageGet(module,target);
    if(!storage)return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->returnType);
    if(storage->kind==XL6_FOREIGN_STORAGE_NATIVE_ADOPTION)xl6OutputPuts("    bool native_storage_adopted=false;\n",out);
    xl6OutputPrintf(out,"    %s native_raw=(%s)xrtCalloc(1,sizeof(%s));\n",type->cName,type->cName,storage->recordCName);
    if(target->outputResultAbi==XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS)xl6OutputPuts("    int native_status=0;\n",out);
    xl6OutputPrintf(out,"    if(!native_raw) { xl6ErrorScopeCaptureFailure(&%s); }\n"
        "    else if(!xl6ForeignResultProbeOwnRecordV1(&native_result_probe,native_raw)) xl6ErrorScopeCaptureFailure(&%s);\n"
        "    if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n"
        "    if(native_raw && !%s.failed) {\n    ",errors,errors,errors,errors);
    if(target->outputResultAbi==XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS)xl6OutputPuts("native_status = ",out);
    return 1;
}
/* Rollback is a cold direct native call. Latch the primary outcome before
 * cleanup and feed both status-only failures and diagnostics into the same
 * first-failure accumulator; a NULL first diagnostic must remain first. */
static void xl6IrCgenNativeRollback(XL6_Output* out,const XL6_IrCallTarget* helper,
    const char* raw,const char* errors)
{
    xl6OutputPrintf(out,"    { if (xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n",errors);
    if(helper->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_INT_STATUS){
        xl6OutputPrintf(out,"        int native_rollback_status=%s(%s);\n"
            "        if (native_rollback_status!=%d) { xl6ForeignStatusFailureV1(native_rollback_status,\"%s\"); xl6ErrorScopeCaptureFailure(&%s); }\n",
            helper->cName,raw,helper->nativeSuccessCode,helper->cName,errors);
    }else xl6OutputPrintf(out,"        %s(%s);\n",helper->cName,raw);
    xl6OutputPrintf(out,"        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n    }\n",errors);
}
/* Both scalar and tuple borrowed native results acquire exactly one native
 * reference before their logical root becomes visible. The caller prepares
 * and ends native_result_probe around this shared admission transaction. */
static void xl6IrCgenRetainedViewAdmission(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId typeId,const XL6_ForeignRetainContract* retained,
    const char* place,const char* raw,const char* errors,const char* probe)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,typeId);
    const XL6_IrCallTarget* retain=xl6IrCallTargetGet(module,retained->retainSymbol);
    xl6OutputPrintf(out,"    bool native_result_retained=false;\n"
        "    if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n"
        "    if(!%s.failed && %s) {\n"
        "        int retained_status=%s(%s); native_result_retained=(retained_status%s);\n"
        "        if(!native_result_retained) { if(!xrtGetError()) xl6ForeignStatusFailureV1(retained_status,\"%s\"); xl6ErrorScopeCaptureFailure(&%s); }\n"
        "        if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n"
        "        if(native_result_retained && !xl6ForeignResultProbeObserveV1(&%s,%s)) xl6ErrorScopeCaptureFailure(&%s);\n    }\n",
        errors,errors,raw,retain->cName,raw,retained->success==XL6_FOREIGN_RETAIN_ZERO ? "==0" : ">0",
        retain->cName,errors,errors,probe,raw,errors);
    xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError()) {\n        if (%s) { %s = ",errors,raw,place);
    xl6OutputPrintf(out,"xl6ForeignResultProbeMaterializeRootV1(&%s)",probe);
    xl6OutputPrintf(out,"; if (!%s) xl6ErrorScopeCaptureFailure(&%s); }",place,errors);
    if(type->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out," else { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&%s); }",errors);
    xl6OutputPuts("\n    }\n",out);
}
static void xl6IrCgenNativeViewAdmission(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* values,const char* receiver,
    const char* place,const char* raw,const char* errors,const char* status)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->returnType);
    const XL6_ForeignStorageContract* storage=xl6IrForeignStorageGet(module,target);
    const XL6_ForeignRetainContract* retained=xl6IrForeignRetainGet(module,target);
    bool factory=target->outputResultAbi!=XL6_IR_CALL_OUTPUT_RESULT_NONE;
    bool borrowedOutput=xl6ForeignBorrowedOwnerOutput(&declaration->foreignCall);
    if(retained){xl6IrCgenRetainedViewAdmission(out,module,target->returnType,retained,place,raw,errors,"native_result_probe");return;}
    if(type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK){
        xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError()==NULL) {\n        if (%s) { XL6_ForeignScopeViewV1 derived={0};\n            if (!xl6ForeignScopeViewDeriveV2(",errors,raw);
        xl6IrCgenForeignViewArgument(out,values,declaration->foreignCall.resultOwnerIndexPlusOne-1,receiver);
        xl6OutputPrintf(out,", %s, &derived)) xl6ErrorScopeCaptureFailure(&%s);\n            else %s = ",raw,errors,place);
        if(type->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("(XL6_NullableForeignScopeViewV1){true,derived}",out);
        else xl6OutputPuts("derived",out);
        xl6OutputPuts(";\n        }",out);
        if(type->kind!=XL6_IR_TYPE_NULLABLE)xl6OutputPrintf(out," else { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&%s); }",errors);
        xl6OutputPrintf(out,"\n    } else xl6ErrorScopeCaptureFailure(&%s);\n",errors);return;
    }
    if(factory){
        xl6OutputPrintf(out,"    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&%s);\n",errors);
        if(status)xl6OutputPrintf(out,"    if (!%s.failed && %s != %d) { xl6ForeignStatusFailureV1(%s,\"%s\"); xl6ErrorScopeCaptureFailure(&%s); }\n",
            errors,status,target->nativeSuccessCode,status,target->cName,errors);
        /* A failed borrowed query has not transferred anything; its output may
         * be unspecified. Never observe/dereference that raw pointer. */
        if(borrowedOutput)xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError()) {\n",errors);
        xl6OutputPrintf(out,"    if (!xl6ForeignResultProbeObserveV1(&native_result_probe, %s)) xl6ErrorScopeCaptureFailure(&%s);\n",raw,errors);
        if(borrowedOutput)xl6OutputPuts("    }\n",out);
    }
    if(storage && storage->kind==XL6_FOREIGN_STORAGE_NATIVE_ADOPTION){
        const XL6_IrCallTarget* adopt=xl6IrCallTargetGet(module,storage->adoptSymbol);
        xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError()==NULL",errors);
        if(status)xl6OutputPrintf(out," && %s==%d",status,target->nativeSuccessCode);
        xl6OutputPrintf(out," && %s) {\n"
            "        int adopted=%s(%s); native_storage_adopted=(adopted==%d);\n"
            "        if(!native_storage_adopted) { if(!xrtGetError()) xl6ForeignStatusFailureV1(adopted,\"%s\"); xl6ErrorScopeCaptureFailure(&%s); }\n"
            "        if(native_storage_adopted && !xl6ForeignResultProbeStorageAdoptedV1(&native_result_probe)) xl6ErrorScopeCaptureFailure(&%s);\n"
            "        if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n    }\n",
            raw,adopt->cName,raw,storage->successCode,adopt->cName,errors,errors,errors);
    }
    else if(storage)xl6OutputPrintf(out,"    if (!%s.failed && !xrtGetError() && %s && !xl6ForeignResultProbeStorageAdoptedV1(&native_result_probe)) xl6ErrorScopeCaptureFailure(&%s);\n",errors,raw,errors);
    xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError() == NULL",errors);
    if(status)xl6OutputPrintf(out," && %s == %d",status,target->nativeSuccessCode);
    xl6OutputPrintf(out,") {\n        if (%s != NULL) { %s = ",raw,place);
    if(declaration->foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE){
        xl6OutputPuts("xl6ForeignResultProbeMaterializeRootV1(&native_result_probe)",out);
    }
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
    if(factory && declaration->foreignCall.borrowedViewResult!=XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE &&
        declaration->foreignCall.rollbackSymbol){
        const XL6_IrCallTarget* rollback=xl6IrCallTargetGet(module,declaration->foreignCall.rollbackSymbol);
        xl6OutputPrintf(out,"    if (%s == NULL && %s != NULL && xl6ForeignResultProbeAliveV1(&native_result_probe)) {\n",place,raw);
        xl6IrCgenNativeRollback(out,rollback,raw,errors);xl6OutputPuts("    }\n",out);
    }
}
