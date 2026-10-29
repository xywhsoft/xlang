/* Included after the Sequence factory renderer. All choices here are closed
 * IR facts. Native providers own storage, batching, admission and graph edges;
 * generated code has no wire Array loop, family switch or channel fallback. */
static const char* xl6IrCgenConcurrentQueuePrefix(XL6_IrRuntimeResourceAbi family)
{
    switch (family) {
    case XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE: return "xl6SpscQueue";
    case XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE: return "xl6MpscQueue";
    case XL6_IR_RUNTIME_RESOURCE_MPMC_QUEUE: return "xl6MpmcQueue";
    case XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE: return "xl6WaitQueue";
    default: return NULL;
    }
}

static const char* xl6IrCgenConcurrentQueueMethod(XL6_IrRuntimeResourceAbi family,XL6_IrRuntimeOp op)
{
    if(family==XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE && xl6IrWaitQueueReceiveRuntime(op))return "ReceiveV1";
    switch (op) {
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE: return "CreateTakeV1";
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH: return "PushV1";
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_TRY_POP: return "PopV1";
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH: return "PushArrayV2";
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_POP_BATCH: return "PopArrayV2";
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE: return "CloseV1";
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET: return "ResetV1";
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED: return "QueryV1";
    default: return NULL;
    }
}

static const char* xl6IrCgenConcurrentQueueAdmissionFactory(XL6_IrRuntimeResourceAbi family)
{
    switch (family) {
    case XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE: return "xl6SequenceSpscQueueCreateTakeV1";
    case XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE: return "xl6SequenceMpscQueueCreateTakeV1";
    case XL6_IR_RUNTIME_RESOURCE_MPMC_QUEUE: return "xl6SequenceMpmcQueueCreateTakeV1";
    case XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE: return "xl6SequenceWaitQueueCreateTakeV1";
    default: return NULL;
    }
}
static int xl6IrCgenEmitQueueReferenceCreateDepth(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId sequenceId,const char* result,size_t depth)
{
    const XL6_IrType* sequence=xl6IrTypeGet(&module->types,sequenceId);
    if (depth>64 || !sequence || sequence->sequenceLayout.abi!=XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1) return -1;
    const XL6_IrSequenceLayout* plan=&sequence->sequenceLayout;
    XL6_IrRuntimeResourceAbi family=XL6_IR_RUNTIME_RESOURCE_NONE;
    XL6_IrTypeId schemaId=0; XL6_IrNominalIdentitySetId set=0;
    if (!xl6IrConcurrentQueueConstructionRecipe(module,plan->baseElementType,&family,&schemaId,&set)) return -1;
    const char* factory=xl6IrCgenConcurrentQueueAdmissionFactory(family);
    if (!factory) return -1;
    char seed[64],scope[64],outcome[64];
    snprintf(seed,sizeof(seed),"xlir_queue_schema_seed_%u",(unsigned)depth);
    snprintf(scope,sizeof(scope),"xlir_queue_schema_scope_%u",(unsigned)depth);
    snprintf(outcome,sizeof(outcome),"xlir_queue_schema_outcome_%u",(unsigned)depth);
    xl6OutputPrintf(out,"    { xvalue* %s = NULL; xl6_error_scope %s = {0};\n"
        "      if (xl6ErrorScopeBegin(&%s)) {\n",seed,scope,scope);
    if (xl6IrCgenEmitSequenceCreateDepth(out,module,schemaId,seed,set,depth+1,NULL,0,false,true)<0) return -1;
    xl6OutputPrintf(out,"      if (!%s) xl6ErrorScopeCaptureFailure(&%s);\n"
        "      %s = %s(%u, 0x%016llXULL, 0x%016llXULL, 0x%016llXULL, %s, %s);\n"
        "      if (!%s) xl6ErrorScopeCaptureFailure(&%s);\n"
        "      if (%s.failed) { xrtValueRelease(%s); %s = NULL; }\n"
        "      xl6_error_result %s = xl6ErrorScopeEnd(&%s);\n"
        "      if (!%s.ok) xrtSetErrorTake(%s.error);\n"
        "      } else { %s = NULL; } }\n",seed,scope,result,factory,
        (unsigned)plan->role,(unsigned long long)sequence->typeValueId,
        (unsigned long long)xl6IrTypeGet(&module->types,plan->elementType)->typeValueId,
        (unsigned long long)xl6IrTypeGet(&module->types,plan->baseElementType)->typeValueId,
        seed,plan->nullable ? "true" : "false",result,scope,scope,result,result,
        outcome,scope,outcome,outcome,result);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenEmitOwnedPlaceRelease(XL6_Output*,const XL6_IrModule*,XL6_IrTypeId,const char*,XL6_DiagList*);
static int xl6IrCgenEmitConcurrentQueueRuntime(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (!xl6IrConcurrentQueueInstructionCompatible(module,instruction)) return -1;
    XL6_IrTypeId ownerId=xl6IrConcurrentQueueInstructionType(module,instruction),schemaId=0;
    XL6_IrRuntimeResourceAbi family=XL6_IR_RUNTIME_RESOURCE_NONE;
    XL6_IrNominalIdentitySetId set=0;
    if (!xl6IrConcurrentQueueConstructionRecipe(module,ownerId,&family,&schemaId,&set)) return -1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,instruction->payloadType);
    const XL6_IrType* result=xl6IrTypeGet(&module->types,instruction->type);
    const XL6_IrSequenceLayout* plan=&schema->sequenceLayout;
    const char* prefix=xl6IrCgenConcurrentQueuePrefix(family);
    const char* method=xl6IrCgenConcurrentQueueMethod(family,instruction->runtimeOp);
    const char* descriptor=xl6IrCgenNativeSlotTypeExpr(plan->slotAbi);
    XL6_IrChannelValuePlan valuePlan={0};
    if (!xl6IrChannelValuePlanFromSchema(module,schemaId,&valuePlan)) return -1;
    bool encoded=plan->codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT;
    const char* slotType=encoded ? "xvalue*" : xl6IrCgenType(module,instruction->payloadType);
    if (!prefix || !method || !descriptor || !slotType || !payload || !result) return -1;
    unsigned output=(unsigned)instruction->result;
    unsigned first=(unsigned)module->operands[instruction->firstOperand];
    unsigned second=instruction->operandCount==2 ?
        (unsigned)module->operands[instruction->firstOperand+1] : 0;
    unsigned long long ownerIdentity=(unsigned long long)owner->typeValueId;
    unsigned long long elementIdentity=(unsigned long long)payload->typeValueId;
    xl6OutputPrintf(out,
        "    v%u = (%s){0};\n"
        "    bool projection_ok_%u = false;\n"
        "    { xl6_error_scope queue_scope = {0};\n"
        "      if (!xl6ErrorScopeBegin(&queue_scope)) abort();\n",
        output,xl6IrCgenType(module,instruction->type),output);
    switch (instruction->runtimeOp) {
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE:
        xl6OutputPuts("      xvalue* queue_schema = NULL;\n",out);
        if (xl6IrCgenEmitSequenceCreateDepth(out,module,schemaId,"queue_schema",set,0,NULL,0,false,true)<0) return -1;
        xl6OutputPrintf(out,
            "      v%u = queue_schema ? %s%s(0x%016llXULL, queue_schema, v%u) : NULL;\n"
            "      if (v%u == NULL) xl6ErrorScopeCaptureFailure(&queue_scope);\n",
            output,prefix,method,ownerIdentity,first,output);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH:
        if (encoded) {
            char source[24];snprintf(source,sizeof(source),"v%u",second);
            if (xl6IrCgenChannelValueEncode(out,module,&valuePlan,source,"queue_slot")<0) return -1;
            xl6OutputPrintf(out,
                "      if (queue_slot == NULL) { v%u = XQUEUE_ERROR; xl6ErrorScopeCaptureFailure(&queue_scope); }\n"
                "      else v%u = %s%s(v%u, 0x%016llXULL, 0x%016llXULL, sizeof(%s), %s, &queue_slot);\n"
                "      xrtValueRelease(queue_slot);\n",
                output,output,prefix,method,first,ownerIdentity,elementIdentity,slotType,descriptor);
        } else xl6OutputPrintf(out,
            "      v%u = %s%s(v%u, 0x%016llXULL, 0x%016llXULL, sizeof(%s), %s, &v%u);\n",
            output,prefix,method,first,ownerIdentity,elementIdentity,slotType,descriptor,second);
        xl6OutputPrintf(out,"      if (v%u == XQUEUE_ERROR) xl6ErrorScopeCaptureFailure(&queue_scope);\n",output);
        break;
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_TRY_POP:
    case XL6_IR_RUNTIME_WAIT_QUEUE_POP:
    case XL6_IR_RUNTIME_WAIT_QUEUE_POP_TIMEOUT: {
        bool tagged=xl6IrOptionalResultPayloadTagged(module,result->params[1],instruction->payloadType)!=0;
        if(tagged)xl6OutputPrintf(out,"      v%u.e1.hasValue = true;\n",output);
        if (encoded) xl6OutputPuts("      xvalue* queue_slot = NULL;\n",out);
        if(family==XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE) {
            const XL6_IrWaitQueueReceiveSpec* spec=xl6IrWaitQueueReceiveRuntime(instruction->runtimeOp);
            if(!spec)return -1;
            xl6OutputPrintf(out,"      XL6_ChannelStatusV1 queue_status = XL6_CHANNEL_EMPTY_V1;\n"
                "      bool queue_ok = xl6WaitQueueReceiveV1(v%u, 0x%016llXULL, 0x%016llXULL, %s,\n"
                "          (XL6_ChannelSlotV1){",first,ownerIdentity,elementIdentity,descriptor);
            if(encoded)xl6OutputPuts("&queue_slot",out);
            else xl6OutputPrintf(out,"&v%u.e1%s",output,tagged?".value":"");
            xl6OutputPrintf(out,", sizeof(%s)}, (XL6_ChannelWaitV1)%u, ",slotType,spec->waitMode);
            if(spec->timed)xl6OutputPrintf(out,"v%u",second);else xl6OutputPuts("0",out);
            xl6OutputPrintf(out,", &queue_status);\n"
                "      if (!queue_ok) xl6ErrorScopeCaptureFailure(&queue_scope);\n"
                "      v%u.e0 = (int64_t)queue_status;\n",output);
        } else {
            xl6OutputPrintf(out,"      v%u.e0 = %s%s(v%u, 0x%016llXULL, 0x%016llXULL, sizeof(%s), %s, ",
            output,prefix,method,first,ownerIdentity,elementIdentity,slotType,descriptor);
        if (encoded) xl6OutputPuts("&queue_slot);\n",out);
        else xl6OutputPrintf(out,"&v%u.e1%s);\n",output,tagged ? ".value" : "");
        xl6OutputPrintf(out,"      if (v%u.e0 == XQUEUE_ERROR) xl6ErrorScopeCaptureFailure(&queue_scope);\n",output);
        }
        if (encoded) {
            char target[32];snprintf(target,sizeof(target),"v%u.e1%s",output,tagged ? ".value" : "");
            xl6OutputPrintf(out,"      if (v%u.e0 == XQUEUE_OK) {\n",output);
            if (xl6IrCgenChannelValueDecode(out,module,&valuePlan,"queue_slot",target,"queue_scope")<0) return -1;
            xl6OutputPuts("      }\n      xrtValueRelease(queue_slot);\n",out);
        }
        break;
    }
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH:
        xl6OutputPrintf(out,
            "      xqueuebatchresult queue_batch = %s%s(v%u, 0x%016llXULL, v%u, 0x%016llXULL);\n"
            "      v%u.e0 = queue_batch.Result; v%u.e1 = (int64_t)queue_batch.Count;\n"
            "      if (queue_batch.Result == XQUEUE_ERROR) xl6ErrorScopeCaptureFailure(&queue_scope);\n",
            prefix,method,first,ownerIdentity,second,(unsigned long long)schema->typeValueId,output,output);
        break;
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_POP_BATCH:
        xl6OutputPrintf(out,
            "      XL6_QueueArrayResultV2 queue_batch = %s%s(v%u, 0x%016llXULL, 0x%016llXULL, v%u, %s);\n"
            "      v%u.e0 = queue_batch.Result; v%u.e1 = queue_batch.Items;\n"
            "      if (queue_batch.Result == XQUEUE_ERROR) xl6ErrorScopeCaptureFailure(&queue_scope);\n",
            prefix,method,first,ownerIdentity,(unsigned long long)schema->typeValueId,second,
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ? "NULL" :
                xl6IrCgenNativeValueConverterExpr(module,instruction->payloadType),output,output);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET:
        xl6OutputPrintf(out,
            "      v%u = %s%s(v%u, 0x%016llXULL);\n"
            "      if (!v%u) xl6ErrorScopeCaptureFailure(&queue_scope);\n",
            output,prefix,method,first,ownerIdentity,output);
        break;
    default: {
        unsigned query=instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT ? 1u :
            instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY ? 2u :
            instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED ? 3u : 4u;
        xl6OutputPrintf(out,
            "      int64_t queue_query = %s%s(v%u, 0x%016llXULL, %u);\n"
            "      if (queue_query < 0) xl6ErrorScopeCaptureFailure(&queue_scope);\n"
            "      v%u = queue_query%s;\n",
            prefix,method,first,ownerIdentity,query,output,result->cAbi==XL6_IR_C_ABI_BOOL ? " != 0" : "");
        break;
    }
    }
    xl6OutputPuts(
        "      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&queue_scope);\n"
        "      xl6_error_result queue_outcome = xl6ErrorScopeEnd(&queue_scope);\n",out);
    if((instruction->runtimeOp==XL6_IR_RUNTIME_CONCURRENT_QUEUE_TRY_POP ||
        xl6IrWaitQueueReceiveRuntime(instruction->runtimeOp)) && result->kind==XL6_IR_TYPE_TUPLE &&
        xl6IrOptionalResultPayloadTagged(module,result->params[1],instruction->payloadType))
        xl6OutputPrintf(out,"      if (queue_outcome.ok) v%u.e1.hasValue = v%u.e0 == XQUEUE_OK;\n",output,output);
    /* STATUS_OUTPUT promises an empty owner on failure. Native pop can still
     * return an owned prefix after a cleanup failure, so retire it exactly
     * once at this language invocation boundary, preserving the first error. */
    if (result->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED) {
        char place[24]; snprintf(place,sizeof(place),"v%u",output);
        xl6OutputPuts("      if (!queue_outcome.ok) { xl6_error_scope queue_cleanup = {0};\n"
            "        xrtSetErrorTake(queue_outcome.error);\n"
            "        if (!xl6ErrorScopeBeginCleanup(&queue_cleanup,true)) abort();\n"
            "        xl6ErrorScopeCaptureFailure(&queue_cleanup);\n",out);
        if (!xl6IrCgenEmitOwnedPlaceRelease(out,module,instruction->type,place,NULL)) return -1;
        xl6OutputPuts("        if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&queue_cleanup);\n"
            "        queue_outcome = xl6ErrorScopeEnd(&queue_cleanup); }\n",out);
    }
    xl6OutputPrintf(out,
        "      projection_ok_%u = queue_outcome.ok;\n"
        "      if (!queue_outcome.ok) xrtSetErrorTake(queue_outcome.error); }\n",output);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
