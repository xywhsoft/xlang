/* One RuntimeOp renderer is shared by sync, async and generator functions. */
static int xl6IrCgenEmitRuntimeCallInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags);

/* Registration is a transaction over already verified physical-slot actions.
 * Failure returns to the AWAIT's existing cleanup/error CFG, in this activation. */
static void xl6IrCgenEmitAsyncSetupFailure(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrAsyncSuspension* suspension)
{
    const XL6_IrAsyncMachineEntry* entry = &module->asyncMachineEntries[suspension->setupFailureEntry];
    xl6OutputPrintf(out, "    await_ok_%u = false; goto ap%lu;\n",
        (unsigned int)suspension->resultValue, (unsigned long)entry->programBlock);
}

static int xl6IrCgenEmitAsyncFrameHandoff(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    size_t first, size_t count, int rollback)
{
    size_t i;
    const char* to=rollback?"frame":"next";
    const char* from=rollback?"next":"frame";
    char targetOuter[32],sourceOuter[32];
    snprintf(targetOuter,sizeof(targetOuter),"%s->outer",to);
    snprintf(sourceOuter,sizeof(sourceOuter),"%s->outer",from);
    xl6OutputPuts("    {\n",out);
    xl6IrCgenSlotEmpty(out,"xpromise*","xlir_empty_outer");
    for (i = 0; i < count; ++i) {
        const XL6_IrAsyncFrameAction* action = &module->asyncFrameActions[first + i];
        const XL6_IrAsyncMachineFrameSlot* frame = &module->asyncMachineFrameSlots[action->frameSlot];
        const char* type = xl6IrCgenType(module,frame->type);
        char empty[64];
        if (!type || action->kind != (rollback?XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT:
            XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT)) return 0;
        if (frame->initBit == SIZE_MAX) continue;
        snprintf(empty,sizeof(empty),"xlir_empty_%lu",(unsigned long)i);
        xl6IrCgenSlotEmpty(out,type,empty);
    }
    xl6IrCgenSlotPlanBegin(out);
    xl6IrCgenSlotWrite(out,targetOuter,sourceOuter); xl6IrCgenSlotReset(out,"xlir_empty_outer",sourceOuter,NULL);
    for (i = 0; i < count; ++i) {
        const XL6_IrAsyncFrameAction* action = &module->asyncFrameActions[first + i];
        const XL6_IrAsyncMachineFrameSlot* frame = &module->asyncMachineFrameSlots[action->frameSlot];
        size_t field = action->frameSlot - machine->firstFrameSlot;
        if (action->kind != (rollback?XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT:
            XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT)) return 0;
        {
            char target[64],source[64],targetBit[64],sourceBit[64],empty[64];
            const char* type=xl6IrCgenType(module,frame->type); if (!type) return 0;
            snprintf(target,sizeof(target),"%s->f%lu",to,(unsigned long)field);
            snprintf(source,sizeof(source),"%s->f%lu",from,(unsigned long)field);
            xl6IrCgenSlotWrite(out,target,source);
            if (frame->initBit!=SIZE_MAX) {
                snprintf(targetBit,sizeof(targetBit),"%s->i%lu",to,(unsigned long)frame->initBit);
                snprintf(sourceBit,sizeof(sourceBit),"%s->i%lu",from,(unsigned long)frame->initBit);
                snprintf(empty,sizeof(empty),"xlir_empty_%lu",(unsigned long)i);
                xl6IrCgenSlotWrite(out,targetBit,sourceBit); xl6IrCgenSlotReset(out,empty,source,sourceBit);
            }
        }
    }
    xl6IrCgenSlotPlanEnd(out);
    xl6OutputPuts("    }\n",out);
    return !xl6OutputHasFailed(out);
}

/* The payload ABI fixes physical drop glue; it is not inferred from a source
 * type spelling or an expression producer. Both launch and resume use the same
 * allocation/acceptance/rollback protocol. */
static int xl6IrCgenAsyncResultDrop(XL6_IrAsyncResultAbi abi, const char** release, const char** destroy)
{
    *release = NULL; *destroy = xl6IrCgenAsyncPayloadDestroy(abi);
    switch (abi) {
    case XL6_IR_ASYNC_RESULT_COPY_BOOL: case XL6_IR_ASYNC_RESULT_COPY_CHAR:
    case XL6_IR_ASYNC_RESULT_COPY_I64:
    case XL6_IR_ASYNC_RESULT_COPY_U64: case XL6_IR_ASYNC_RESULT_COPY_F64:
    case XL6_IR_ASYNC_RESULT_COPY_F32: case XL6_IR_ASYNC_RESULT_COPY_TIME:
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID: case XL6_IR_ASYNC_RESULT_COPY_TAGGED:
    case XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER: return 1;
    case XL6_IR_ASYNC_RESULT_OWNED_STRING: *release = "xlir_string_drop"; return 1;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE: *release = "xrtValueRelease"; return 1;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE: *release = "xlir_closure_release"; return 1;
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR: *release = "xrtErrorFree"; return 1;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE: *release = "xrtFutureDestroy"; return 1;
    case XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR: *release = "xlir_generator_destroy"; return 1;
    case XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE: return 1;
    default: return 0;
    }
}

static void xl6IrCgenEmitAsyncResultRollback(
    XL6_Output* out, const XL6_IrCallTarget* target, int launch, const char* drop)
{
    xl6OutputPuts("    { xl6_error_result capture_outcome = {false, xrtTakeError()};\n"
        "      xl6_error_scope capture_cleanup = {0};\n"
        "      if (!xl6ErrorScopeBegin(&capture_cleanup)) abort();\n"
        "      xrtSetErrorTake(capture_outcome.error); xl6ErrorScopeCaptureFailure(&capture_cleanup);\n", out);
    if (*drop) xl6OutputPrintf(out, "    %s\n", drop);
    xl6IrCgenEmitActivationCleanupObserve(out);
    xl6OutputPrintf(out, "    capture_outcome = xl6ErrorScopeEnd(&capture_cleanup);\n"
        "    capture_outcome = xlir_async_machine_frame_retire_%u(frame, capture_outcome);\n",
        (unsigned int)target->symbol);
    if (launch) {
        xl6OutputPrintf(out, "    xlir_async_machine_frame_free_%u(frame, NULL);\n", (unsigned int)target->symbol);
    }
    /* This failure belongs to the returned Future, not to a producer's outer
     * foreign-call scope on which the continuation happens to execute. */
    xl6OutputPuts("    xrtSetErrorTake(capture_outcome.error); }\n", out);
}

/* A resumed activation publishes its terminal Future only after lexical
 * cleanup and after committing that no successor can be created. Checked
 * module teardown may then wait for the physical queue/release tail without
 * ever waiting on an activation still suspended on an external Future. */
static void xl6IrCgenEmitAsyncCompletionBarrier(XL6_Output* out, int launch)
{
    if (!launch)
        xl6OutputPuts("      if (!xl6ContinuationComplete(&frame->ticket)) abort();\n", out);
}

static int xl6IrCgenEmitAsyncResultCommit(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrAsyncMachine* machine, const XL6_IrInstruction* instruction,
    int launch, XL6_DiagList* diags)
{
    const char *type = xl6IrCgenType(module, target->workerReturnType);
    const char *release = NULL, *destroy = NULL;
    int hasPayload = target->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID;
    const char* promise = launch ? "promise" : "destination";
    char unboxedDrop[96] = "", boxedDrop[96] = "";
    if (machine->resultCommitAbi != XL6_IR_ASYNC_RESULT_COMMIT_OWNED_TRANSACTION ||
        machine->completionAbi != XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH ||
        instruction->operandCount != (hasPayload ? 1u : 0u) ||
        (hasPayload && (type == NULL ||
            !xl6IrCgenAsyncResultDrop(target->asyncResultAbi, &release, &destroy))) ||
        (!hasPayload && target->workerReturnType != XL6_IR_TYPE_INVALID))
        return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR async result commit transaction is unresolved");
    if (release != NULL) snprintf(unboxedDrop, sizeof(unboxedDrop), "%s(v%u);", release,
        (unsigned int)module->operands[instruction->firstOperand]);
    if(target->asyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE) {
        const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,target->workerReturnType));
        if(!native)return 0;
        snprintf(unboxedDrop,sizeof(unboxedDrop),"%s(v%u);",native->dropSymbol,
            (unsigned int)module->operands[instruction->firstOperand]);
    }
    if (hasPayload) snprintf(boxedDrop, sizeof(boxedDrop), "%s(value, NULL);", destroy);
    xl6OutputPuts("    { xl6_error_result retired = {true, NULL};\n", out);
    if (hasPayload) xl6OutputPrintf(out, "      %s* value;\n", type);
    if (launch) {
        xl6OutputPuts("      xfuture* ready = NULL; xpromise* promise = xrtPromiseCreate(&ready, NULL);\n"
            "      if (promise == NULL) {\n"
            "        if (xrtGetError() == NULL) xrtSetErrorKind(XERR_MEMORY);\n", out);
        xl6IrCgenEmitAsyncResultRollback(out, target, 1, unboxedDrop);
        xl6OutputPuts("        if (ready != NULL) { xrtFutureDestroy(ready); }\n        return NULL;\n      }\n", out);
    }
    // 完成信号没有数据分配，仍须先清理帧，再发布成功或失败。
    if (hasPayload) {
      if(!xl6IrCgenEmitAsyncPayloadAllocation(out,module,target->workerReturnType,target->asyncResultAbi,"value"))return 0;
      xl6OutputPuts("      if (value == NULL) {\n"
        "        if (xrtGetError() == NULL) xrtSetErrorKind(XERR_MEMORY);\n",out);
      xl6IrCgenEmitAsyncResultRollback(out, target, launch, unboxedDrop);
      xl6IrCgenEmitAsyncCompletionBarrier(out, launch);
      xl6OutputPrintf(out, "        xlir_async_result_fail(%s);\n", promise);
      if (launch) xl6OutputPuts("        xrtPromiseDestroy(promise); return ready;\n", out);
      else xl6OutputPuts("        return;\n", out);
      xl6OutputPrintf(out, "      }\n      *value = v%u;\n",
          (unsigned int)module->operands[instruction->firstOperand]);
    }
    xl6OutputPrintf(out, "      retired = xlir_async_machine_frame_retire_%u(frame, retired);\n"
        "      if (!retired.ok) {\n        xrtSetErrorTake(retired.error);\n",
        (unsigned int)target->symbol);
    xl6IrCgenEmitAsyncResultRollback(out, target, launch, boxedDrop);
    xl6IrCgenEmitAsyncCompletionBarrier(out, launch);
    xl6OutputPrintf(out, "        xlir_async_result_fail(%s);\n", promise);
    if (launch) xl6OutputPuts("        xrtPromiseDestroy(promise); return ready;\n", out);
    else xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("      }\n", out);
    xl6IrCgenEmitAsyncCompletionBarrier(out, launch);
    xl6OutputPuts("      if (!", out);
    if (hasPayload) {
        if (!xl6IrCgenEmitAsyncPayloadPublish(out, target->asyncResultAbi, promise, "value")) return 0;
    } else xl6OutputPrintf(out, "xrtPromiseResolve(%s, NULL)", promise);
    xl6OutputPuts(") {\n        if (xrtGetError() == NULL) xrtSetErrorKind(XERR_STATE);\n", out);
    xl6IrCgenEmitAsyncResultRollback(out, target, launch, boxedDrop);
    xl6OutputPrintf(out, "        xlir_async_result_fail(%s);\n", promise);
    if (launch) xl6OutputPuts("        xrtPromiseDestroy(promise); return ready;\n", out);
    else xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("      }\n", out);
    if (launch)
        xl6OutputPrintf(out, "      xrtPromiseDestroy(promise); xlir_async_machine_frame_free_%u(frame, NULL); return ready; }\n",
            (unsigned int)target->symbol);
    else xl6OutputPuts("      return; }\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitAsyncMachineInstruction(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrAsyncMachine* machine, const XL6_IrFunction* function,
    const XL6_IrAsyncMachineBlock* block, const XL6_IrInstruction* instruction,
    size_t resumeState, XL6_DiagList* diags)
{
    int launch = resumeState == SIZE_MAX;
    if (!xl6IrCgenEmitContinuationTransfers(out, module, 0, (size_t)(machine - module->asyncMachines),
            instruction, XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION)) return 0;
    switch ( instruction->kind ) {
    case XL6_IR_INSTR_PRODUCT_MAKE:
    case XL6_IR_INSTR_PRODUCT_GET:
    case XL6_IR_INSTR_PRODUCT_TAKE:
    case XL6_IR_INSTR_PRODUCT_DROP:
        if (!xl6IrCgenEmitProductInstruction(out, module, instruction, diags)) return 0;
        break;
    case XL6_IR_INSTR_CONST:
        if (xl6IrCgenResultStorageOmitted(module, instruction)) break;
        if ( instruction->immediateText == NULL )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR async machine constant has no payload");
        xl6OutputPrintf(out, "    v%u = %s;\n",
                        (unsigned int)instruction->result, instruction->immediateText);
        break;
    case XL6_IR_INSTR_NULL:
        if ( !xl6IrCgenEmitNullInstruction(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_NULL_TEST:
    case XL6_IR_INSTR_NULLABLE_WRAP:
    case XL6_IR_INSTR_NULLABLE_UNWRAP:
        if ( !xl6IrCgenEmitNullableInstruction(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_UNION_ZERO:
        xl6OutputPrintf(out, "    v%u = (%s){0};\n",
            (unsigned int)instruction->result,
            xl6IrCgenType(module, instruction->type));
        break;
    case XL6_IR_INSTR_UNION_GET: {
        const XL6_IrUnionField* field =
            instruction->elementIndex < module->unionFieldCount
                ? &module->unionFields[instruction->elementIndex] : NULL;
        if ( field == NULL || field->cName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR async union read has no exact C member ABI");
        xl6OutputPrintf(out, "    v%u = v%u.%s;\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            field->cName);
        break;
    }
    case XL6_IR_INSTR_UNION_SET: {
        const XL6_IrUnionField* field =
            instruction->elementIndex < module->unionFieldCount
                ? &module->unionFields[instruction->elementIndex] : NULL;
        if ( field == NULL || field->cName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR async union write has no exact C member ABI");
        xl6OutputPrintf(out, "    v%u = v%u;\n    v%u.%s = v%u;\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)instruction->result,
            field->cName,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    }
    case XL6_IR_INSTR_RUNTIME_CALL:
        if (!xl6IrCgenEmitRuntimeCallInstruction(out, module, instruction,
                (size_t)(instruction - module->instructions), diags)) return 0;
        break;
    case XL6_IR_INSTR_CELL_NEW:
    case XL6_IR_INSTR_CELL_INIT:
    case XL6_IR_INSTR_CELL_LOAD:
    case XL6_IR_INSTR_CELL_STORE:
    case XL6_IR_INSTR_CELL_EXCHANGE:
    case XL6_IR_INSTR_CELL_RETAIN:
    case XL6_IR_INSTR_CELL_RELEASE:
        {
            size_t frameSlot = xl6IrCgenAsyncFrameStorageSlot(module, machine, instruction->targetSymbol);
            const XL6_IrAsyncMachineFrameSlot* frame = frameSlot != SIZE_MAX
                ? &module->asyncMachineFrameSlots[frameSlot] : NULL;
            if (!xl6IrCgenEmitCellInstruction(out, module, instruction, NULL,
                    frame != NULL ? frameSlot - machine->firstFrameSlot : SIZE_MAX,
                    frame != NULL ? frame->initBit : SIZE_MAX, diags)) return 0;
        }
        break;
    case XL6_IR_INSTR_CLOSURE_MAKE:
        if ( !xl6IrCgenEmitClosureMakeInstruction(
                out, module, instruction,
                (size_t)(instruction - module->instructions), diags) ) return 0;
        break;
    case XL6_IR_INSTR_CLOSURE_CALL:
        if ( !xl6IrCgenEmitClosureCallInstruction(
                out, module, instruction,
                (size_t)(instruction - module->instructions), diags) ) return 0;
        break;
    case XL6_IR_INSTR_RETAIN:
        if ( !xl6IrCgenEmitRetainInstruction(
                out, module, instruction) )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR async machine retain ABI is unresolved");
        break;
    case XL6_IR_INSTR_ASYNC_START:
    case XL6_IR_INSTR_GENERATOR_START:
        if (!xl6IrCgenEmitLaunchCall(out, module, instruction, diags)) return 0;
        break;
    case XL6_IR_INSTR_GENERATOR_NEXT:
    case XL6_IR_INSTR_GENERATOR_ITEM_GET:
    case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
        if (!xl6IrCgenEmitGeneratorConsumerInstruction(out, module, instruction)) return 0;
        break;
    case XL6_IR_INSTR_LOAD:
        if (xl6IrCgenResultStorageOmitted(module, instruction)) break;
        {
            size_t parameter = xl6IrCgenAsyncParameterIndex(
                module, function, instruction->targetSymbol);
            size_t frameSlot = xl6IrCgenAsyncFrameStorageSlot(
                module, machine, instruction->targetSymbol);
            if ( frameSlot != SIZE_MAX )
                xl6OutputPrintf(out, "    v%u = frame->f%lu;\n",
                    (unsigned int)instruction->result,
                    (unsigned long)(frameSlot - machine->firstFrameSlot));
            else if ( launch && parameter != SIZE_MAX )
                xl6OutputPrintf(out, "    v%u = a%lu;\n",
                    (unsigned int)instruction->result, (unsigned long)parameter);
            else if ( xl6IrCgenAsyncLocalStorage(
                          module, machine, function,
                          instruction->targetSymbol) != NULL )
                xl6OutputPrintf(out, "    v%u = s%u;\n",
                    (unsigned int)instruction->result,
                    (unsigned int)instruction->targetSymbol);
            else if ( xl6IrCgenSharedStorage(
                          module, instruction->targetSymbol) != NULL ) {
                xl6OutputPrintf(out, "    v%u = ",
                    (unsigned int)instruction->result);
                xl6IrCgenPutStorageName(
                    out, module, instruction->targetSymbol);
                xl6OutputPuts(";\n", out);
            }
            else
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine load has no launch parameter binding");
        }
        break;
    case XL6_IR_INSTR_STORAGE_ADDRESS:
        {
            size_t frameSlot = xl6IrCgenAsyncFrameStorageSlot(
                module, machine, instruction->targetSymbol);
            const XL6_IrStorage* local = xl6IrCgenAsyncLocalStorage(
                module, machine, function, instruction->targetSymbol);
            const XL6_IrStorage* shared = xl6IrCgenSharedStorage(
                module, instruction->targetSymbol);
            const XL6_IrStorage* storage = xl6IrStorageGet(
                module, instruction->targetSymbol);
            if ( storage == NULL || instruction->operandCount != 0 ||
                 instruction->result == XL6_IR_VALUE_INVALID ||
                 !xl6IrCgenStorageRefTo(module, instruction, storage->type) ||
                 (frameSlot == SIZE_MAX && local == NULL && shared == NULL) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR async machine storage address has no exact binding");
            xl6OutputPrintf(out, "    v%u = (void*)&",
                (unsigned int)instruction->result);
            if ( frameSlot != SIZE_MAX )
                xl6OutputPrintf(out, "frame->f%lu",
                    (unsigned long)(frameSlot - machine->firstFrameSlot));
            else if ( local != NULL )
                xl6OutputPrintf(out, "s%u",
                    (unsigned int)instruction->targetSymbol);
            else
                xl6IrCgenPutStorageName(
                    out, module, instruction->targetSymbol);
            xl6OutputPuts(";\n", out);
        }
        break;
    case XL6_IR_INSTR_STORE:
    case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
        {
            size_t frameSlot = xl6IrCgenAsyncFrameStorageSlot(
                module, machine, instruction->targetSymbol);
            const XL6_IrAsyncMachineFrameSlot* frame = frameSlot != SIZE_MAX
                ? &module->asyncMachineFrameSlots[frameSlot] : NULL;
            const XL6_IrStorage* storage = xl6IrCgenAsyncLocalStorage(
                module, machine, function, instruction->targetSymbol);
            if (storage == NULL) storage = xl6IrCgenSharedStorage(module, instruction->targetSymbol);
            if ( instruction->operandCount != 1 || (frame == NULL && storage == NULL) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine store has no frame binding");
            if (!xl6IrCgenEmitStorageCommit(out,module,instruction,
                    frame?frameSlot-machine->firstFrameSlot:SIZE_MAX,frame?frame->initBit:SIZE_MAX,diags)) return 0;
        }
        break;
    case XL6_IR_INSTR_MOVE:
    case XL6_IR_INSTR_STORAGE_TAKE:
        {
            size_t frameSlot = xl6IrCgenAsyncFrameStorageSlot(
                module, machine, instruction->targetSymbol);
            const XL6_IrAsyncMachineFrameSlot* frame = frameSlot != SIZE_MAX
                ? &module->asyncMachineFrameSlots[frameSlot] : NULL;
            if (!xl6IrCgenEmitMoveInstruction(out, module, instruction,
                    frame != NULL ? frameSlot - machine->firstFrameSlot : SIZE_MAX,
                    frame != NULL ? frame->initBit : SIZE_MAX, diags)) return 0;
        }
        break;
    case XL6_IR_INSTR_RELEASE:
    case XL6_IR_INSTR_VALUE_RELEASE:
        if (instruction->kind == XL6_IR_INSTR_VALUE_RELEASE)
            return xl6IrCgenEmitValueRelease(out, module, instruction, diags);
        {
            size_t frameSlot = xl6IrCgenAsyncFrameStorageSlot(
                module, machine, instruction->targetSymbol);
            const XL6_IrAsyncMachineFrameSlot* frame = frameSlot != SIZE_MAX
                ? &module->asyncMachineFrameSlots[frameSlot] : NULL;
            const XL6_IrStorage* storage = xl6IrCgenAsyncLocalStorage(
                module, machine, function, instruction->targetSymbol);
            const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->type);
            if (storage == NULL) storage = xl6IrCgenSharedStorage(module, instruction->targetSymbol);
            if ( frame != NULL ) {
                if ( !xl6IrCgenEmitFrameRelease(out, module, frame->type,
                         frame->destroyAbi, frameSlot - machine->firstFrameSlot,
                         frame->initBit) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR async machine release has no frame lifetime ABI");
            } else if ( storage == NULL || type == NULL ||
                        !xl6IrCgenEmitSymbolRelease(out, module, instruction->type,
                            instruction->targetSymbol, 0) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR async machine release has no local lifetime ABI");
        }
        break;
    case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
        if (!xl6IrCgenEmitForeignResultRollback(out, module, instruction)) return 0;
        break;
    case XL6_IR_INSTR_CALL:
        {
            size_t instructionIndex = (size_t)(instruction - module->instructions);
            if ( !xl6IrCgenEmitDirectCall(
                    out, module, instruction, instructionIndex, diags,
                    "XLIR async machine call target is unresolved") )
                return 0;
        }
        break;
    case XL6_IR_INSTR_VIRTUAL_CALL:
        if (!xl6IrCgenEmitDispatchCallInstruction(out, module, instruction, diags)) return 0;
        break;
    case XL6_IR_INSTR_PROTOCOL_CALL:
        if ( !xl6IrCgenEmitDispatchCallInstruction(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_BINARY:
        {
            const char* op = xl6IrCgenOperator(instruction->opcode);
            if ( op == NULL || instruction->operandCount != 2 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine binary operation is unresolved");
            xl6OutputPrintf(out, "    v%u = v%u %s v%u;\n",
                (unsigned int)instruction->result,
                (unsigned int)module->operands[instruction->firstOperand], op,
                (unsigned int)module->operands[instruction->firstOperand + 1]);
        }
        break;
    case XL6_IR_INSTR_UNARY:
        {
            const char* op = xl6IrCgenOperator(instruction->opcode);
            if ( op == NULL || instruction->operandCount != 1 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine unary operation is unresolved");
            xl6OutputPrintf(out, "    v%u = %s v%u;\n",
                (unsigned int)instruction->result, op,
                (unsigned int)module->operands[instruction->firstOperand]);
        }
        break;
    case XL6_IR_INSTR_SCALAR_CONVERT:
        {
            const char* resultType = xl6IrCgenType(module, instruction->type);
            if ( resultType == NULL || instruction->result == XL6_IR_VALUE_INVALID ||
                 instruction->operandCount != 1 ||
                 instruction->scalarConversionAbi <=
                     XL6_IR_SCALAR_CONVERSION_INVALID ||
                 instruction->scalarConversionAbi >
                     XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine scalar conversion is unresolved");
            xl6OutputPrintf(out, "    v%u = (%s)v%u;\n",
                (unsigned int)instruction->result, resultType,
                (unsigned int)module->operands[instruction->firstOperand]);
        }
        break;
    case XL6_IR_INSTR_VALUE:
        if ( instruction->operandCount != 1 ||
             ((instruction->result == XL6_IR_VALUE_INVALID) ==
              (instruction->targetValue == XL6_IR_VALUE_INVALID)) )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR async machine value projection is unresolved");
        xl6OutputPrintf(out, "    v%u = v%u;\n",
            (unsigned int)(instruction->result != XL6_IR_VALUE_INVALID
                ? instruction->result : instruction->targetValue),
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_INSTR_DISCARD:
    case XL6_IR_INSTR_TRAP_IF_FALSE:
        if ( !xl6IrCgenEmitSimpleObservation(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_BRANCH:
        if ( block->trueTarget == SIZE_MAX || block->falseTarget == SIZE_MAX )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR async machine branch targets are unresolved");
        xl6OutputPrintf(out, "    if (v%u) goto ap%lu; else goto ap%lu;\n",
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned long)block->trueTarget, (unsigned long)block->falseTarget);
        break;
    case XL6_IR_INSTR_JUMP:
        if ( block->trueTarget == SIZE_MAX )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR async machine jump target is unresolved");
        xl6OutputPrintf(out, "    goto ap%lu;\n", (unsigned long)block->trueTarget);
        break;
    case XL6_IR_INSTR_ERROR_TEST:
        if ( block->trueTarget == SIZE_MAX || block->falseTarget == SIZE_MAX ||
             instruction->operandCount != 1 )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR async machine error test is unresolved");
        {
            XL6_IrValueId tested = module->operands[instruction->firstOperand];
            size_t sourceIndex = SIZE_MAX;
            const XL6_IrInstruction* source =
                xl6IrCgenValueDefinition(module, tested, &sourceIndex);
            if (source != NULL && source->kind == XL6_IR_INSTR_AWAIT) {
                xl6OutputPrintf(out, "    if (await_ok_%u) goto ap%lu; else goto ap%lu;\n",
                    (unsigned int)tested, (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            } else if (source != NULL && source->kind == XL6_IR_INSTR_GENERATOR_NEXT) {
                xl6IrCgenEmitGeneratorNextTest(out, sourceIndex, tested, "ap",
                    block->trueTarget, block->falseTarget);
            } else if (source != NULL && (source->kind == XL6_IR_INSTR_CLEANUP_END ||
                       (source->kind == XL6_IR_INSTR_RUNTIME_CALL && source->runtimeOp == XL6_IR_RUNTIME_STACK_CLEAR))) {
                xl6OutputPrintf(out, "    if (v%u) goto ap%lu; else goto ap%lu;\n",
                    (unsigned int)tested, (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            } else if (source != NULL && xl6IrCallInstructionErrorAbi(module, source) == XL6_IR_ERROR_ABI_MAY_RAISE &&
                       xl6IrCallInstructionErrorPhysicalAbi(module, source) != XL6_IR_CALL_ERROR_PHYSICAL_INVALID) {
                if (!xl6IrCgenEmitCallErrorTest(out, module, source, sourceIndex, "ap",
                        block->trueTarget, block->falseTarget, diags)) return 0;
            } else
                xl6OutputPrintf(out,
                    "    if (xrtGetError() == NULL) goto ap%lu; else goto ap%lu;\n",
                    (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
        }
        break;
    case XL6_IR_INSTR_ERROR_RAISE:
    case XL6_IR_INSTR_CLEANUP_BEGIN:
    case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
    case XL6_IR_INSTR_CLEANUP_CAPTURE:
    case XL6_IR_INSTR_CLEANUP_END:
    case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
    case XL6_IR_INSTR_ERROR_UNWIND_END:
    case XL6_IR_INSTR_ERROR_TAKE:
    case XL6_IR_INSTR_ERROR_SET_TAKE:
    case XL6_IR_INSTR_ERROR_CLEAR:
        if (!xl6IrCgenEmitErrorStateInstruction(out, module, instruction))
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR error-state ownership ABI is unresolved");
        break;
    case XL6_IR_INSTR_RETURN:
        if (!xl6IrCgenEmitAsyncResultCommit(out, module, target, machine, instruction, launch, diags)) return 0;
        break;
    case XL6_IR_INSTR_THROW:
        xl6OutputPrintf(out,
            "    { xl6_error_result retired = {false, xrtTakeError()};\n"
            "      retired = xlir_async_machine_frame_retire_%u(frame, retired);\n",
            (unsigned int)target->symbol);
        xl6IrCgenEmitAsyncCompletionBarrier(out, launch);
        if ( launch )
            xl6OutputPrintf(out,
                "      xfuture* failed = NULL; xpromise* rejected = xrtPromiseCreate(&failed, NULL);\n"
                "      if (rejected == NULL) {\n"
                "        if (failed != NULL) { xrtFutureDestroy(failed); } xlir_async_machine_frame_free_%u(frame, NULL);\n"
                "        xrtSetErrorTake(retired.error); if (xrtGetError() == NULL) xrtSetErrorKind(XERR_INTERNAL); return NULL; }\n"
                "      xrtSetErrorTake(retired.error); xlir_async_result_fail(rejected);\n"
                "      xrtPromiseDestroy(rejected); xlir_async_machine_frame_free_%u(frame, NULL); return failed; }\n",
                (unsigned int)target->symbol, (unsigned int)target->symbol);
        else
            xl6OutputPuts(
                "      xrtSetErrorTake(retired.error); xlir_async_result_fail(destination);\n"
                "      return; }\n", out);
        break;
    case XL6_IR_INSTR_AWAIT:
        {
            const XL6_IrAsyncSuspension* suspension;
            size_t action;
            size_t nextState;
            if ( block->kind != XL6_IR_ASYNC_MACHINE_BLOCK_SUSPEND ||
                 block->suspension >= module->asyncSuspensionCount ||
                 instruction->operandCount != 1 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine suspension ABI is unresolved");
            suspension = &module->asyncSuspensions[block->suspension];
            nextState = block->suspension - machine->firstSuspension;
            for ( action = 0; action < suspension->spillActionCount; ++action ) {
                const XL6_IrAsyncFrameAction* spill =
                    &module->asyncFrameActions[suspension->firstSpillAction + action];
                const XL6_IrAsyncMachineFrameSlot* frame =
                    &module->asyncMachineFrameSlots[spill->frameSlot];
                size_t frameLocal = spill->frameSlot - machine->firstFrameSlot;
                int acquire = frame->initBit != SIZE_MAX && frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED;
                if (acquire) xl6OutputPrintf(out, "    if (!frame->i%lu) {\n", (unsigned long)frame->initBit);
                {
                    char place[64],input[32],initialized[64]; int acquired;
                    const char* type=xl6IrCgenType(module,frame->type);
                    snprintf(place,sizeof(place),"frame->f%lu",(unsigned long)frameLocal);
                    snprintf(input,sizeof(input),"v%u",(unsigned int)frame->value);
                    snprintf(initialized,sizeof(initialized),"frame->i%lu",(unsigned long)frame->initBit);
                    if (!xl6IrCgenEmitFrameAcquisitionBegin(out,type,frame->transferAbi,input,&acquired,
                            xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,frame->type)))) return 0;
                    if (acquired) {
                        if (!xl6IrCgenEmitFrameAcquisitionFailureBegin(out,frame->transferAbi,input)) return 0;
                        xl6IrCgenEmitAsyncSetupFailure(out,module,suspension); xl6OutputPuts("    }\n",out);
                    }
                    if (!xl6IrCgenEmitFrameAcquisitionCommit(out,type,place,frame->initBit==SIZE_MAX?NULL:initialized)) return 0;
                }
                if (acquire) xl6OutputPuts("    }\n", out);
            }
            if ( launch ) {
                xl6OutputPuts("    output = NULL;\n",out);
                if (!xl6IrCgenEmitSlotCommit(out,"xpromise*","frame->outer","xrtPromiseCreate(&output, NULL)",NULL)) return 0;
                xl6OutputPuts(
                    "    if (frame->outer == NULL) {\n"
                    "        if (output != NULL) { xrtFutureDestroy(output); output = NULL; }\n", out);
                xl6IrCgenEmitAsyncSetupFailure(out, module, suspension);
                xl6OutputPrintf(out,
                    "    }\n"
                    "    if (!xlir_async_machine_watch_%u_%lu(frame, v%u, NULL)) {\n"
                    "        xerror* primary = xrtTakeError();\n"
                    "",
                    (unsigned int)target->symbol, (unsigned long)nextState,
                    (unsigned int)module->operands[instruction->firstOperand]);
                if (!xl6IrCgenEmitDetachedRelease(out,"xpromise*","xrtPromiseDestroy","frame->outer",NULL)) return 0;
                xl6OutputPuts("        xrtFutureDestroy(output); output = NULL; xrtSetErrorTake(primary);\n",out);
                xl6IrCgenEmitAsyncSetupFailure(out, module, suspension);
                xl6OutputPuts("    }\n    xl6ContinuationCommit(&frame->ticket);\n    return output;\n", out);
            } else {
                size_t frameLocal;
                xl6OutputPrintf(out,
                    "    { xlir_async_machine_frame_%u* next = (xlir_async_machine_frame_%u*)xlir_async_frame_allocate(sizeof(*next), &xlir_async_machine_storage_%u);\n"
                    "      if (next == NULL) {\n",
                    (unsigned int)target->symbol, (unsigned int)target->symbol, (unsigned int)target->symbol);
                xl6IrCgenEmitAsyncSetupFailure(out, module, suspension);
                xl6OutputPuts(
                    "      }\n"
                    "      next->outer = NULL;\n"
                    "      next->source = NULL;\n", out);
                for ( frameLocal = 0; frameLocal < machine->frameSlotCount; ++frameLocal ) {
                    const XL6_IrAsyncMachineFrameSlot* nextFrame =
                        &module->asyncMachineFrameSlots[machine->firstFrameSlot + frameLocal];
                    if ( nextFrame->initBit != SIZE_MAX )
                        xl6OutputPrintf(out, "      next->f%lu = (%s){0}; next->i%lu = 0;\n",
                            (unsigned long)frameLocal,
                            xl6IrCgenType(module,nextFrame->type),
                            (unsigned long)nextFrame->initBit);
                }
                if (!xl6IrCgenEmitAsyncFrameHandoff(out, module, machine,
                        suspension->firstHandoffAction, suspension->handoffActionCount, 0)) return 0;
                xl6OutputPrintf(out,
                    "      if (!xlir_async_machine_watch_%u_%lu(next, v%u, &frame->ticket)) {\n",
                    (unsigned int)target->symbol, (unsigned long)nextState,
                    (unsigned int)module->operands[instruction->firstOperand]);
                if (!xl6IrCgenEmitAsyncFrameHandoff(out, module, machine,
                        suspension->firstRollbackAction, suspension->rollbackActionCount, 1)) return 0;
                xl6OutputPuts(
                    "        xlir_async_frame_dispose(next);\n", out);
                xl6IrCgenEmitAsyncSetupFailure(out, module, suspension);
                xl6OutputPuts("      }\n      xl6ContinuationCommit(&next->ticket);\n      return; }\n", out);
            }
        }
        break;
    default:
        return xl6IrCgenDiag(diags, module, instruction->origin,
                             "XLIR async machine instruction is not implemented");
    }
    return !xl6OutputHasFailed(out);
}
