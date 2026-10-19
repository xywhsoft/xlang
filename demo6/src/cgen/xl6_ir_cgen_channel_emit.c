/* Mechanical emission from a closed Channel plan. The resident provider owns
 * transport, message boxes, metadata and graph/lifetime synchronization. */
static void xl6IrCgenEmitChannelWaitArguments(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* ins,unsigned mode,bool timed,bool cancelled,size_t timeOperand)
{
    if (mode==3 || cancelled) {
        xl6OutputPrintf(out,"(XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2), "
            "%u, ",mode);
        if (mode==2) xl6OutputPrintf(out,"v%u",(unsigned)module->operands[ins->firstOperand+timeOperand]);
        else xl6OutputPuts("0",out);
        xl6OutputPuts(", ",out);
        if (mode==3) xl6OutputPrintf(out,"v%u",(unsigned)module->operands[ins->firstOperand+timeOperand]);
        else xl6OutputPuts("0",out);
        xl6OutputPuts("}",out);
    } else {
        xl6OutputPrintf(out,"(XL6_ChannelWaitV1)%u, ",mode);
        if (timed) xl6OutputPrintf(out,"v%u",(unsigned)module->operands[ins->firstOperand+timeOperand]);
        else xl6OutputPuts("0",out);
    }
    if (cancelled) xl6OutputPrintf(out,", v%u",(unsigned)module->operands[ins->firstOperand+ins->operandCount-1]);
}
static void xl6IrCgenEmitChannelCallPrefix(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* ins,const char* symbol,bool cancelled,const char* guard)
{
    xl6OutputPuts("      bool channel_ok = ",out);
    if (guard) xl6OutputPrintf(out,"%s && ",guard);
    /* Source requires a Token; the lower-level native ABI also permits NULL
     * to mean no cancellation. A failed/empty physical loan must not silently
     * turn a cancellable wait into an unbounded uncancellable operation. */
    if (cancelled) xl6OutputPrintf(out,"v%u != NULL && ",
        (unsigned)module->operands[ins->firstOperand+ins->operandCount-1]);
    xl6OutputPrintf(out,"%s(",symbol);
}
static int xl6IrCgenEmitChannelRuntime(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* ins)
{
    if (!ins || !xl6IrChannelRuntimeOpIs(ins->runtimeOp)) return 0;
    if (!xl6IrChannelInstructionCompatible(module,ins)) return -1;
    XL6_IrTypeId ownerId=xl6IrChannelInstructionType(module,ins),schemaId=0;
    XL6_IrNominalIdentitySetId set=0;
    if (!xl6IrChannelConstructionRecipe(module,ownerId,&schemaId,&set)) return -1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,ins->payloadType);
    const XL6_IrType* result=xl6IrTypeGet(&module->types,ins->type);
    XL6_IrChannelValuePlan valuePlan={0};
    if (!xl6IrChannelValuePlanBuild(module,ownerId,&valuePlan)) return -1;
    bool encoded=valuePlan.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT;
    const char* descriptor=xl6IrCgenNativeSlotTypeExpr(valuePlan.slotAbi);
    const char* slotType=encoded?"xvalue*":xl6IrCgenType(module,ins->payloadType);
    const char* resultType=xl6IrCgenType(module,ins->type);
    if (!owner || !schema || !payload || !result || !descriptor || !slotType || !resultType) return -1;
    unsigned output=(unsigned)ins->result;
    unsigned first=ins->operandCount ? (unsigned)module->operands[ins->firstOperand] : 0;
    unsigned long long identity=(unsigned long long)owner->typeValueId;
    unsigned long long element=(unsigned long long)payload->typeValueId;
    XL6_IrChannelReceivePlan receive={0};
    const XL6_IrChannelReceiveSpec* spec=xl6IrChannelReceiveSpecRuntime(ins->runtimeOp);
    const XL6_IrChannelSendSpec* send=xl6IrChannelSendSpecRuntime(ins->runtimeOp);
    if (spec && !xl6IrChannelReceiveInstructionPlan(module,ins,&receive)) return -1;
    xl6OutputPrintf(out,"    v%u = (%s){0};\n    bool projection_ok_%u = false;\n"
        "    { xl6_error_scope channel_scope = {0};\n"
        "      if (!xl6ErrorScopeBegin(&channel_scope)) abort();\n",output,resultType,output);
    if (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE) {
        xl6OutputPuts("      xvalue* channel_schema = NULL;\n",out);
        if (xl6IrCgenEmitSequenceCreateDepth(out,module,schemaId,"channel_schema",set,0,NULL,0,false,true)<0) return -1;
        xl6OutputPrintf(out,"      if (channel_schema != NULL) v%u = xl6ChannelCreateTakeV1(0x%016llXULL, channel_schema, ",output,identity);
        if (ins->operandCount) xl6OutputPrintf(out,"v%u);\n",first);
        else xl6OutputPuts("2);\n",out);
        xl6OutputPrintf(out,"      if (v%u == NULL) xl6ErrorScopeCaptureFailure(&channel_scope);\n",output);
    } else if (spec) {
        /* On a failed committed move the caller still owns the physical slot.
         * Keep its optional tag live until cleanup; publish absence only on
         * normal flow. Never address a caller variable or an int64 status as
         * the native enum's four-byte output slot. */
        if (receive.valueTagged) xl6OutputPrintf(out,"      v%u.e1.hasValue = true;\n",output);
        if (encoded) xl6OutputPuts("      xvalue* channel_slot = NULL;\n",out);
        xl6OutputPuts("      XL6_ChannelStatusV1 channel_status = XL6_CHANNEL_EMPTY_V1;\n",out);
        xl6IrCgenEmitChannelCallPrefix(out,module,ins,spec->nativeSymbol,spec->cancelled,NULL);
        xl6OutputPrintf(out,"v%u, 0x%016llXULL, 0x%016llXULL, %s,\n"
            "          (XL6_ChannelSlotV1){",first,identity,element,descriptor);
        if (encoded) xl6OutputPuts("&channel_slot",out);
        else xl6OutputPrintf(out,"&v%u.e1%s",output,receive.valueTagged ? ".value" : "");
        xl6OutputPrintf(out,", sizeof(%s)}, ",slotType);
        xl6IrCgenEmitChannelWaitArguments(out,module,ins,spec->waitMode,spec->timed,spec->cancelled,1);
        xl6OutputPrintf(out,", &channel_status);\n"
            "      if (!channel_ok) xl6ErrorScopeCaptureFailure(&channel_scope);\n"
            "      v%u.e0 = (int64_t)channel_status;\n",output);
        if (encoded) {
            char target[48];snprintf(target,sizeof(target),"v%u.e1%s",output,receive.valueTagged?".value":"");
            xl6OutputPuts("      if (channel_ok && channel_status == XL6_CHANNEL_OK_V1) {\n",out);
            if (xl6IrCgenChannelValueDecode(out,module,&valuePlan,"channel_slot",target,"channel_scope")<0) return -1;
            xl6OutputPuts("      }\n      xrtValueRelease(channel_slot);\n",out);
        }
    } else if (send) {
        unsigned second=(unsigned)module->operands[ins->firstOperand+1];
        xl6OutputPuts("      XL6_ChannelStatusV1 channel_status = XL6_CHANNEL_FULL_V1;\n",out);
        if (encoded) {
            char source[40];snprintf(source,sizeof(source),"v%u",second);
            if (xl6IrCgenChannelValueEncode(out,module,&valuePlan,source,"channel_slot")<0) return -1;
            xl6OutputPuts("      if (!channel_slot) xl6ErrorScopeCaptureFailure(&channel_scope);\n",out);
        }
        /* Encoding failure is a failed operation, never a nullable message. */
        xl6IrCgenEmitChannelCallPrefix(out,module,ins,send->nativeSymbol,send->cancelled,
            encoded?"channel_slot != NULL":NULL);
        xl6OutputPrintf(out,"v%u, 0x%016llXULL, 0x%016llXULL, %s,\n"
            "          (XL6_ChannelSlotV1){",first,identity,element,descriptor);
        if (encoded) xl6OutputPuts("&channel_slot",out);else xl6OutputPrintf(out,"&v%u",second);
        xl6OutputPrintf(out,", sizeof(%s)}, ",slotType);
        xl6IrCgenEmitChannelWaitArguments(out,module,ins,send->waitMode,send->timed,send->cancelled,2);
        xl6OutputPuts(", &channel_status);\n"
            "      if (!channel_ok) xl6ErrorScopeCaptureFailure(&channel_scope);\n",out);
        if (encoded) xl6OutputPuts("      xrtValueRelease(channel_slot);\n",out);
        if (send->cancelled) xl6OutputPrintf(out,"      v%u = (int64_t)channel_status;\n",output);
        else xl6OutputPrintf(out,"      v%u = channel_ok && channel_status == XL6_CHANNEL_OK_V1;\n",output);
    } else if (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CLOSE) {
        xl6OutputPrintf(out,"      v%u = xl6ChannelCloseV1(v%u, 0x%016llXULL);\n"
            "      if (!v%u) xl6ErrorScopeCaptureFailure(&channel_scope);\n",output,first,identity,output);
    } else {
        xl6OutputPrintf(out,"      int64_t channel_query = xl6ChannelQueryV1(v%u, 0x%016llXULL, %s);\n"
            "      if (channel_query < 0) xl6ErrorScopeCaptureFailure(&channel_scope);\n"
            "      v%u = channel_query%s;\n",first,identity,
            ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_COUNT ? "XL6_CHANNEL_QUERY_COUNT_V1" : "XL6_CHANNEL_QUERY_CLOSED_V1",
            output,result->cAbi==XL6_IR_C_ABI_BOOL ? " != 0" : "");
    }
    xl6OutputPuts("      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&channel_scope);\n"
        "      xl6_error_result channel_outcome = xl6ErrorScopeEnd(&channel_scope);\n",out);
    if (spec && receive.valueTagged)
        xl6OutputPrintf(out,"      if (channel_outcome.ok) v%u.e1.hasValue = v%u.e0 == XL6_CHANNEL_OK_V1;\n",output,output);
    if (result->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED) {
        char place[24]; snprintf(place,sizeof(place),"v%u",output);
        /* Cleanup entry latches the ambient failure, including a diagnostic-
         * free failure. Install the original diagnostic before that latch;
         * installing it afterwards cannot replace the first failed outcome. */
        xl6OutputPuts("      if (!channel_outcome.ok) { xl6_error_scope channel_cleanup = {0};\n"
            "        xrtSetErrorTake(channel_outcome.error);\n"
            "        if (!xl6ErrorScopeBeginCleanup(&channel_cleanup,true)) abort();\n",out);
        if (!xl6IrCgenEmitOwnedPlaceRelease(out,module,ins->type,place,NULL)) return -1;
        xl6OutputPuts("        if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&channel_cleanup);\n"
            "        channel_outcome = xl6ErrorScopeEnd(&channel_cleanup); }\n",out);
    }
    xl6OutputPrintf(out,"      projection_ok_%u = channel_outcome.ok;\n"
        "      if (!channel_outcome.ok) xrtSetErrorTake(channel_outcome.error); }\n",output);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
