/* The same public Watch transport used by language Future callbacks delivers
 * every source outcome. It cannot bypass a machine's cleanup on cancellation. */
static int xl6IrCgenEmitAsyncMachineWatch(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrAsyncSuspension* suspension,
    XL6_SymbolId symbol, size_t state)
{
    const XL6_IrInstruction* await;
    if (out == NULL || module == NULL || suspension == NULL ||
        suspension->awaitInstruction >= module->instructionCount) return 0;
    await = &module->instructions[suspension->awaitInstruction];
    if (await->kind != XL6_IR_INSTR_AWAIT ||
        await->awaitOutcomeAbi !=
            XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR) return 0;
    xl6OutputPrintf(out,
        "static void xlir_async_machine_notify_%u_%lu(void* data)\n"
        "{\n"
        "    xlir_async_machine_frame_%u* frame = (xlir_async_machine_frame_%u*)data;\n"
        "    xfutureresult input = {0}; xerror* saved = xrtTakeError();\n"
        "    bool valid = xl6ContinuationFrameRuntimeV4Get()->Observe(frame, &input);\n"
        "    xlir_async_machine_resume_%u_%lu(valid ? &input : NULL, frame->outer, frame);\n"
        "    xrtClearError(); xrtSetErrorTake(saved);\n"
        "}\n",
        (unsigned int)symbol, (unsigned long)state, (unsigned int)symbol, (unsigned int)symbol,
        (unsigned int)symbol, (unsigned long)state);
    xl6OutputPrintf(out,
        "static int xlir_async_machine_watch_%u_%lu(xlir_async_machine_frame_%u* frame, xfuture* source, XL6_ContinuationTicket* predecessor)\n"
        "{\n"
        "    XL6_ContinuationAdmission status;\n"
        "    XL6_ContinuationProc run = xlir_async_machine_notify_%u_%lu;\n"
        "    XL6_ContinuationProc release = xl6ContinuationFrameRuntimeV2Get()->Retire;\n",
        (unsigned int)symbol, (unsigned long)state, (unsigned int)symbol,
        (unsigned int)symbol, (unsigned long)state);
    xl6OutputPrintf(out,
        "    if (source == NULL) { xrtSetErrorTake(xlir_error_create_at(\"StateError\", \"await on null future\", %d, %d)); return 0; }\n",
        await->sourceSpan.line, await->sourceSpan.column);
    (void)xl6IrCgenEmitSlotCommit(out,"xfuture*","frame->source","xrtFutureRef(source)",NULL);
    xl6OutputPuts(
        "    if (frame->source == NULL) return 0;\n"
        "    status = predecessor != NULL\n"
        "        ? xl6ContinuationReserveSuccessor(predecessor, &frame->ticket, run, release, frame)\n"
        "        : xl6ContinuationExecutorReserveOwned(xlir_continuation_executor, &frame->ticket, run, release, frame,\n"
        "            xlir_code_owner.size != 0 ? &xlir_code_owner : NULL);\n"
        "    if (status != XL6_CONTINUATION_ADMITTED) goto failed;\n", out);
    xl6OutputPuts("    if (!xl6ContinuationFrameRuntimeV3Get()->Bind(frame)) goto failed;\n",out);
    xl6OutputPuts(
        "    if (!xl6ContinuationWatchRegister(&frame->ticket, &frame->watch, source)) goto failed;\n"
        "    return 1;\n"
        "failed:\n"
        "    { xerror* primary = xrtTakeError();\n"
        "      if (frame->ticket.queue != NULL && !xl6ContinuationAbort(&frame->ticket)) abort();\n",out);
    (void)xl6IrCgenEmitDetachedRelease(out,"xfuture*","xrtFutureDestroy","frame->source",NULL);
    xl6OutputPuts("      xrtSetErrorTake(primary); }\n"
        "    return 0;\n"
        "}\n", out);
    return !xl6OutputHasFailed(out);
}

/* Finish observation before entering the source CFG. The temporary Watch
 * reference must not defer a lexical owner's destructor until after publish.
 * This cleanup transaction also covers native payload destructors: it cannot
 * contaminate the producer's outer scope or discard a diagnostic-free failure. */
static int xl6IrCgenEmitAsyncSourceLeaseRelease(
    XL6_Output* out, const XL6_IrAsyncSuspension* suspension)
{
    const char *release = NULL, *destroy = NULL;
    if (suspension->resultAbi != XL6_IR_ASYNC_RESULT_VOID &&
        !xl6IrCgenAsyncResultDrop(suspension->resultAbi, &release, &destroy)) return 0;
    xl6OutputPrintf(out,
        "    { xl6_error_scope source_cleanup = {0}; xl6_error_result observed;\n"
        "      xerror* original = xrtTakeError(); bool acquired = await_ok_%u;\n"
        "      xfuture* source;\n",(unsigned int)suspension->resultValue);
    xl6IrCgenSlotPlanBegin(out); xl6IrCgenSlotWrite(out,"source","frame->source");
    xl6IrCgenSlotReset(out,"xfuture*","frame->source",NULL); xl6IrCgenSlotPlanEnd(out);
    xl6OutputPuts(
        "      if (!xl6ErrorScopeBegin(&source_cleanup)) abort();\n"
        "      xrtSetErrorTake(original);\n"
        "      if (!acquired) xl6ErrorScopeCaptureFailure(&source_cleanup);\n"
        "      xrtFutureDestroy(source);\n"
        "      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&source_cleanup);\n"
        "      observed = xl6ErrorScopeEnd(&source_cleanup);\n",out);
    if (release != NULL) {
        xl6OutputPuts(
            "      if (acquired && !observed.ok) {\n"
            "        if (!xl6ErrorScopeBegin(&source_cleanup)) abort();\n"
            "        xrtSetErrorTake(observed.error); xl6ErrorScopeCaptureFailure(&source_cleanup);\n", out);
        xl6OutputPrintf(out, "        %s(v%u); v%u = NULL;\n", release,
            (unsigned int)suspension->resultValue, (unsigned int)suspension->resultValue);
        xl6OutputPuts(
            "        if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&source_cleanup);\n"
            "        observed = xl6ErrorScopeEnd(&source_cleanup);\n"
            "      }\n", out);
    }
    xl6OutputPrintf(out,
        "      xrtSetErrorTake(observed.error);\n"
        "      await_ok_%u = observed.ok;\n"
        "    }\n", (unsigned int)suspension->resultValue);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitAsyncMachine(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrAsyncMachine* machine, XL6_DiagList* diags, int* emitted)
{
    const XL6_IrFunction* function;
    const XL6_IrAsyncMachineEntry* launchEntry;
    unsigned char* launchBlocks = NULL;
    unsigned char* resumeBlocks = NULL;
    unsigned char* resumePrograms = NULL;
    size_t local;
    size_t parameter;
    size_t state;
    const char* unsupportedReason = NULL;
    char unsupportedReasonDetail[192];
    int ok = 0;
    unsupportedReasonDetail[0] = '\0';
    if ( emitted != NULL ) *emitted = 0;
    if ( module == NULL || target == NULL || machine == NULL || emitted == NULL ||
         machine->function >= module->functionCount ) return 0;
    if (machine->completionAbi != XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR async completion cleanup barrier is unresolved");
    if (machine->transportAbi != XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH ||
        machine->cancellationAbi != XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR async cancellation delivery transport is unresolved");
    if (machine->sourceLeaseAbi != XL6_IR_ASYNC_SOURCE_LEASE_CAPTURE_THEN_RELEASE)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR async source lease lifetime is unresolved");
    if (machine->executionAbi != XL6_IR_ASYNC_EXECUTION_GUARANTEED_RESUME)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR async execution domain/root admission ABI is unresolved");
    if (machine->ownershipAbi != XL6_IR_ASYNC_OWNERSHIP_STABLE_PRODUCER_WATCH_V1)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR async producer/Watch ownership ABI is unresolved");
    function = &module->functions[machine->function];
    launchEntry = &module->asyncMachineEntries[machine->firstEntry];
    launchBlocks = (unsigned char*)xrtCalloc(machine->blockCount, 1);
    resumeBlocks = (unsigned char*)xrtCalloc(machine->blockCount, 1);
    if ( machine->blockCount != 0 &&
         machine->suspensionCount > SIZE_MAX / machine->blockCount ) {
        xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR direct async machine program matrix is too large");
        goto done;
    }
    resumePrograms = (unsigned char*)xrtCalloc(
        machine->suspensionCount * machine->blockCount, 1);
    if ( launchBlocks == NULL || resumeBlocks == NULL || resumePrograms == NULL ) {
        xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR direct async machine program allocation failed");
        goto done;
    }
    if ( machine->entryCount != 1 + XL6_IR_ASYNC_RESUME_ENTRY_COUNT * machine->suspensionCount ||
         !xl6IrCgenAsyncMarkProgram(module, machine, launchEntry->programBlock, launchBlocks) ) {
        xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR verified async machine launch program is malformed");
        goto done;
    }
    for ( state = 0; state < machine->suspensionCount; ++state ) {
        const XL6_IrAsyncMachineEntry* resumeEntry =
            &module->asyncMachineEntries[machine->firstEntry + 1 + XL6_IR_ASYNC_RESUME_ENTRY_COUNT * state];
        unsigned char* program = resumePrograms + state * machine->blockCount;
        if ( !xl6IrCgenAsyncMarkProgram(
                 module, machine, resumeEntry->programBlock, program) ) {
            xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR verified async machine resume program is malformed");
            goto done;
        }
        for ( local = 0; local < machine->blockCount; ++local )
            if ( program[local] ) resumeBlocks[local] = 1;
    }
    if ( !xl6IrCgenAsyncMachineProgramSupported(
             module, target, machine, function, launchBlocks, resumeBlocks,
             &unsupportedReason, unsupportedReasonDetail,
             sizeof(unsupportedReasonDetail)) ) {
        /* An analyzed machine is the canonical executable async IR. Fail closed
         * so unsupported instruction/frame ABI must be added to verified XLIR
         * admission before C can be emitted. */
        xl6IrCgenDiag(diags, module, target->declaration,
            unsupportedReason != NULL ? unsupportedReason :
            "XLIR verified async machine has no admitted direct C program ABI");
        goto done;
    }

    xl6OutputPrintf(out, "typedef struct xlir_async_machine_frame_%u {\n",
                    (unsigned int)target->symbol);
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrAsyncMachineFrameSlot* frame =
            &module->asyncMachineFrameSlots[machine->firstFrameSlot + local];
        const char* type = xl6IrCgenType(module, frame->type);
        if ( type == NULL ) goto done;
        xl6OutputPrintf(out, "    %s f%lu;\n", type, (unsigned long)local);
    }
    for ( local = 0; local < machine->initBitCount; ++local )
        xl6OutputPrintf(out, "    unsigned char i%lu;\n", (unsigned long)local);
    xl6OutputPuts("    xpromise* outer; xfuture* source; xfuturewatch watch;\n"
                  "    XL6_ContinuationTicket ticket;\n", out);
    xl6OutputPrintf(out, "} xlir_async_machine_frame_%u;\n", (unsigned int)target->symbol);
    if (!xl6IrCgenEmitActivationLayout(out, "xlir_async_machine", target->symbol,
            machine->frameSlotCount ? module->asyncMachineFrameSlots + machine->firstFrameSlot : NULL,
            machine->frameSlotCount, machine->firstFrameSlot,
            machine->destroyActionCount ? module->asyncFrameActions + machine->firstDestroyAction : NULL,
            machine->destroyActionCount)) goto done;
    xl6OutputPrintf(out,
        "static const XL6_ContinuationFrameLayoutV2 xlir_async_machine_storage_%u = {\n"
        "  sizeof(XL6_ContinuationFrameLayoutV2), &xlir_async_machine_layout_%u,\n"
        "  offsetof(xlir_async_machine_frame_%u, ticket), offsetof(xlir_async_machine_frame_%u, watch),\n"
        "  offsetof(xlir_async_machine_frame_%u, source), offsetof(xlir_async_machine_frame_%u, outer)};\n",
        (unsigned int)target->symbol, (unsigned int)target->symbol, (unsigned int)target->symbol,
        (unsigned int)target->symbol, (unsigned int)target->symbol, (unsigned int)target->symbol);
    xl6OutputPrintf(out,
        "static xl6_error_result xlir_async_machine_frame_retire_%u(xlir_async_machine_frame_%u* frame, xl6_error_result outcome)\n"
        "{\n"
        "  if (frame == NULL) return outcome;\n"
        "  return xl6ContinuationFrameRuntimeV2Get()->Cleanup(frame, outcome);\n}\n",
        (unsigned int)target->symbol, (unsigned int)target->symbol);
    xl6OutputPrintf(out,
        "static void xlir_async_machine_frame_free_%u(void* data, void* user)\n"
        "{ (void)user; xlir_async_frame_dispose(data); }\n", (unsigned int)target->symbol);

    for ( state = 0; state < machine->suspensionCount; ++state ) {
        xl6OutputPrintf(out,
            "static void xlir_async_machine_resume_%u_%lu(const xfutureresult*, xpromise*, void*);\n",
            (unsigned int)target->symbol, (unsigned long)state);
        xl6OutputPrintf(out,
            "static int xlir_async_machine_watch_%u_%lu(xlir_async_machine_frame_%u*, xfuture*, XL6_ContinuationTicket*);\n",
            (unsigned int)target->symbol, (unsigned long)state, (unsigned int)target->symbol);
    }
    for ( state = 0; state < machine->suspensionCount; ++state ) {
        const XL6_IrAsyncMachineEntry* resumeEntry =
            &module->asyncMachineEntries[machine->firstEntry + 1 + XL6_IR_ASYNC_RESUME_ENTRY_COUNT * state];
        const XL6_IrAsyncSuspension* suspension =
            &module->asyncSuspensions[machine->firstSuspension + state];
        const unsigned char* program = resumePrograms + state * machine->blockCount;
        xl6OutputPrintf(out,
            "static void xlir_async_machine_resume_%u_%lu(const xfutureresult* input, xpromise* output, void* data)\n"
            "{\n"
            "    xlir_async_machine_frame_%u* frame = (xlir_async_machine_frame_%u*)data;\n"
            "    xpromise* destination = output;\n",
            (unsigned int)target->symbol, (unsigned long)state,
            (unsigned int)target->symbol, (unsigned int)target->symbol);
        xl6IrCgenDeclareUnwindScope(out, module, function);
        if ( !xl6IrCgenAsyncDeclareMachineProgramValues(
                out, module, machine, function, program,
                machine->firstSuspension + state, diags) ) goto done;
        if ( !xl6IrCgenAsyncDeclareMachineProgramStorages(
                out, module, machine, function, program, diags) ) goto done;
        if (machine->futureOutcomeAbi != XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND)
            goto done;
        xl6OutputPrintf(out,
            "    if (frame == NULL) { (void)xrtPromiseClose(output); return; }\n"
            "    if (input == NULL) xrtSetErrorKind(XERR_STATE);\n"
            "    else await_ok_%u = xl6PayloadObserveOutcome(input->State, input->Error);\n",
            (unsigned int)suspension->resultValue);
        if (!xl6IrCgenEmitAwaitPayload(out, module,
                &module->instructions[suspension->awaitInstruction], "input->Value")) goto done;
        if (!xl6IrCgenEmitAsyncSourceLeaseRelease(out, suspension)) goto done;
        for ( local = 0; local < suspension->resumeActionCount; ++local ) {
            const XL6_IrAsyncFrameAction* project =
                &module->asyncFrameActions[suspension->firstResumeAction + local];
            const XL6_IrAsyncMachineFrameSlot* frame =
                &module->asyncMachineFrameSlots[project->frameSlot];
            xl6OutputPrintf(out, "    v%u = frame->f%lu;\n",
                (unsigned int)project->value,
                (unsigned long)(project->frameSlot - machine->firstFrameSlot));
        }
        xl6OutputPrintf(out, "    goto ap%lu;\n", (unsigned long)resumeEntry->programBlock);
        for ( local = 0; local < machine->blockCount; ++local ) {
            const XL6_IrAsyncMachineBlock* block;
            size_t instructionIndex;
            if ( !program[local] ) continue;
            block = &module->asyncMachineBlocks[machine->firstBlock + local];
            xl6OutputPrintf(out, "ap%lu:;\n", (unsigned long)(machine->firstBlock + local));
            for ( instructionIndex = block->firstInstruction;
                  instructionIndex < block->firstInstruction + block->instructionCount;
                  ++instructionIndex )
                if ( !xl6IrCgenEmitAsyncMachineInstruction(
                        out, module, target, machine, function, block,
                        &module->instructions[instructionIndex], state, diags) ) goto done;
        }
        xl6OutputPuts("}\n", out);
        if (!xl6IrCgenEmitAsyncMachineWatch(
                out, module, suspension, target->symbol, state)) goto done;
    }

    xl6OutputPrintf(out, "static xfuture* xlir_async_start_raw_%u(",
                    (unsigned int)target->symbol);
    if ( target->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
        const char* type = xl6IrCgenType(module, target->parameterTypes[parameter]);
        if ( type == NULL ) goto done;
        if ( parameter != 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu", type, (unsigned long)parameter);
    }
    xl6OutputPrintf(out,
        ")\n{\n"
        "    xlir_async_machine_frame_%u* frame = (xlir_async_machine_frame_%u*)xlir_async_frame_allocate(sizeof(*frame), &xlir_async_machine_storage_%u);\n"
        "    xfuture* output;\n"
        "    if (frame == NULL) {\n",
        (unsigned int)target->symbol, (unsigned int)target->symbol, (unsigned int)target->symbol);
    if (!xl6IrCgenEmitActivationInputRollback(out, module, target, diags)) goto done;
    xl6OutputPuts("    return NULL; }\n    frame->outer = NULL; frame->source = NULL;\n", out);
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrAsyncMachineFrameSlot* frame =
            &module->asyncMachineFrameSlots[machine->firstFrameSlot + local];
        if ( frame->initBit == SIZE_MAX ) continue;
        xl6OutputPrintf(out, "    frame->f%lu = NULL; frame->i%lu = 0;\n",
            (unsigned long)local, (unsigned long)frame->initBit);
    }
    for ( local = 0; local < machine->prologueActionCount; ++local ) {
        const XL6_IrAsyncFrameAction* prologue =
            &module->asyncFrameActions[machine->firstPrologueAction + local];
        const XL6_IrAsyncMachineFrameSlot* frame =
            &module->asyncMachineFrameSlots[prologue->frameSlot];
        size_t frameLocal = prologue->frameSlot - machine->firstFrameSlot;
        if ( prologue->kind == XL6_IR_ASYNC_FRAME_ACTION_BIND_STORAGE ) {
            if ( frame->residence != XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE ||
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_NONE ) goto done;
        } else if ( prologue->kind == XL6_IR_ASYNC_FRAME_ACTION_INIT_PARAMETER &&
                    frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER &&
                    frame->parameter < target->parameterCount ) {
            char place[64], input[32], initialized[64], rollback[128];
            snprintf(place, sizeof(place), "frame->f%lu", (unsigned long)frameLocal);
            snprintf(input, sizeof(input), "a%lu", (unsigned long)frame->parameter);
            snprintf(initialized, sizeof(initialized), "frame->i%lu", (unsigned long)frame->initBit);
            snprintf(rollback, sizeof(rollback), "xlir_async_machine_frame_free_%u(frame, NULL);",
                (unsigned int)target->symbol);
            if (!xl6IrCgenEmitActivationCapture(out, xl6IrCgenType(module,frame->type), frame->transferAbi, place, input,
                    frame->initBit == SIZE_MAX ? NULL : initialized, rollback)) goto done;
        } else goto done;
    }
    xl6IrCgenDeclareUnwindScope(out, module, function);
    if ( !xl6IrCgenAsyncDeclareMachineProgramValues(
            out, module, machine, function, launchBlocks, SIZE_MAX, diags) ) goto done;
    if ( !xl6IrCgenAsyncDeclareMachineProgramStorages(
            out, module, machine, function, launchBlocks, diags) ) goto done;
    xl6OutputPrintf(out, "    goto ap%lu;\n", (unsigned long)launchEntry->programBlock);
    for ( local = 0; local < machine->blockCount; ++local ) {
        const XL6_IrAsyncMachineBlock* block;
        size_t instructionIndex;
        if ( !launchBlocks[local] ) continue;
        block = &module->asyncMachineBlocks[machine->firstBlock + local];
        xl6OutputPrintf(out, "ap%lu:;\n", (unsigned long)(machine->firstBlock + local));
        for ( instructionIndex = block->firstInstruction;
              instructionIndex < block->firstInstruction + block->instructionCount;
              ++instructionIndex )
            if ( !xl6IrCgenEmitAsyncMachineInstruction(
                    out, module, target, machine, function, block,
                    &module->instructions[instructionIndex], SIZE_MAX, diags) ) goto done;
    }
    xl6OutputPuts("}\n\n", out);
    if ( xl6OutputHasFailed(out) ) goto done;
    *emitted = 1;
    ok = 1;

done:
    xrtFree(launchBlocks);
    xrtFree(resumeBlocks);
    xrtFree(resumePrograms);
    return ok;
}

/* The worker/launcher split below is driven entirely by a verified async
 * target ABI. No source AST, type spelling, or secondary async emitter is
 * consulted here. The shared pool outlives individual calls and their Futures;
 * its module-resource unload action remains a separate lifecycle obligation. */
static int xl6IrCgenEmitAsyncLauncher(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    XL6_DiagList* diags)
{
    const XL6_IrType* worker;
    const XL6_IrAsyncMachine* machine;
    const char* resultType;
    const char* workerResultType;
    const char* valueDestroy;
    int workerMayRaise;
    size_t parameter;
    if ( target == NULL || target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
         target->futureOutcomeAbi != XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND ||
         target->asyncCompletionAbi != XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH ||
         (target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL &&
          target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE) ||
         (target->workerErrorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
          target->workerErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) ||
         !xl6IrCgenAsyncResultAbiMatches(module, target) )
        return xl6IrCgenDiag(diags, module, target != NULL ? target->declaration : XL6_IR_NODE_INVALID,
                             "XLIR C emitter async launch ABI is unresolved");
    worker = xl6IrTypeGet(&module->types, target->workerReturnType);
    resultType = target->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID
        ? NULL : xl6IrCgenType(module, target->workerReturnType);
    valueDestroy = xl6IrCgenAsyncPayloadDestroy(target->asyncResultAbi);
    if ( target->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID &&
         (worker == NULL || resultType == NULL || valueDestroy == NULL) )
        return xl6IrCgenDiag(diags, module, target->declaration,
                             "XLIR C emitter async worker result ABI is not implemented");
    workerMayRaise = target->workerErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE;
    workerResultType = workerMayRaise ? xl6IrCgenErrorResultType(module, target->workerReturnType) : NULL;
    if ( workerMayRaise && workerResultType == NULL )
        return xl6IrCgenDiag(diags, module, target->declaration,
                             "XLIR C emitter async worker error-result ABI is unresolved");
    machine = xl6IrCgenAsyncMachine(module, target);
    if ( target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE )
        return xl6IrCgenEmitAsyncReadyLauncher(
            out, module, target, resultType, valueDestroy, workerMayRaise, workerResultType, diags);
    if ( machine != NULL ) {
        int machineEmitted = 0;
        if ( !xl6IrCgenEmitAsyncMachine(
                out, module, target, machine, diags, &machineEmitted) ) return 0;
        if ( !machineEmitted )
            return xl6IrCgenDiag(diags, module, target->declaration,
                                 "XLIR verified async machine produced no C program");
        return 1;
    }
    if ( xl6IrCgenFindFunction(module, target->symbol) == NULL )
        return xl6IrCgenDiag(diags, module, target->declaration,
                             "XLIR C emitter async worker has no lowered body ABI");
    if ( target->parameterCount != 0 &&
         (target->asyncCaptureAbis == NULL || target->asyncCaptureDestroyAbis == NULL) )
        return xl6IrCgenDiag(diags, module, target->declaration,
                             "XLIR C emitter async task frame ABI is unresolved");
    xl6OutputPrintf(out, "typedef struct xlir_async_frame_%u {\n", (unsigned int)target->symbol);
    for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
        const char* type = xl6IrCgenType(module, target->parameterTypes[parameter]);
        const XL6_IrType* parameterType = xl6IrTypeGet(&module->types, target->parameterTypes[parameter]);
        if (type == NULL || parameterType == NULL ||
            target->asyncCaptureAbis[parameter] == XL6_IR_ASYNC_CAPTURE_INVALID)
            return xl6IrCgenDiag(diags, module, target->declaration,
                                 "XLIR C emitter async task frame capture ABI is unresolved");
        xl6OutputPrintf(out, "    %s p%lu;\n", type, (unsigned long)parameter);
    }
    if ( target->parameterCount == 0 )
        xl6OutputPuts("    unsigned char reserved;\n", out);
    xl6OutputPrintf(out, "} xlir_async_frame_%u;\n", (unsigned int)target->symbol);
    if (target->parameterCount) {
        xl6OutputPrintf(out, "static const XL6_CaptureSlot xlir_async_slots_%u[] = {\n", (unsigned int)target->symbol);
        for (parameter = 0; parameter < target->parameterCount; ++parameter) {
            const char* kind = xl6IrCgenCaptureKind(target->asyncCaptureDestroyAbis[parameter]);
            if (!kind) return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR C emitter async task frame has no closed capture kind");
            xl6OutputPrintf(out, "    {%s, offsetof(xlir_async_frame_%u, p%lu), sizeof(((xlir_async_frame_%u*)0)->p%lu)},\n",
                kind, (unsigned int)target->symbol, (unsigned long)parameter,
                (unsigned int)target->symbol, (unsigned long)parameter);
        }
        xl6OutputPuts("};\n", out);
    }
    xl6OutputPrintf(out, "static const XL6_CaptureLayoutV1 xlir_async_layout_%u = {sizeof(XL6_CaptureLayoutV1), sizeof(xlir_async_frame_%u), %lu, ",
        (unsigned int)target->symbol, (unsigned int)target->symbol, (unsigned long)target->parameterCount);
    if (target->parameterCount) xl6OutputPrintf(out, "xlir_async_slots_%u", (unsigned int)target->symbol);
    else xl6OutputPuts("NULL", out);
    xl6OutputPuts("};\n", out);
    xl6OutputPrintf(out,
                    "static xl6_error_result xlir_async_frame_retire_%u(xlir_async_frame_%u* frame, xl6_error_result outcome)\n"
                    "{\n"
                    "    xl6_error_scope capture_cleanup = {0};\n"
                    "    if (frame == NULL) return outcome;\n"
                    "    if (!xl6ErrorScopeBegin(&capture_cleanup)) abort();\n"
                    "    xrtSetErrorTake(outcome.error);\n"
                    "    if (!outcome.ok) xl6ErrorScopeCaptureFailure(&capture_cleanup);\n",
                    (unsigned int)target->symbol, (unsigned int)target->symbol);
    xl6OutputPuts("    (xl6TaskRuntimeV2Get()->Cleanup)(frame);\n", out);
    xl6IrCgenEmitActivationCleanupObserve(out);
    xl6OutputPuts("    return xl6ErrorScopeEnd(&capture_cleanup);\n", out);
    xl6OutputPrintf(out, "}\nstatic void xlir_async_frame_free_%u(void* data, void* user)\n"
        "{ (void)user; xlir_task_discard(data); }\n", (unsigned int)target->symbol);

    xl6OutputPrintf(out,
                    "static XL6_TaskOutcome xlir_async_task_%u(xcancel* cancel, void* data, XL6_TaskResult* result)\n"
                    "{\n"
                    "    xlir_async_frame_%u* frame = (xlir_async_frame_%u*)data;\n"
                    "    xl6_error_result outcome = {true, NULL};\n"
                    "    bool cancelled = false;\n",
                    (unsigned int)target->symbol, (unsigned int)target->symbol,
                    (unsigned int)target->symbol);
    if ( target->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID )
        xl6OutputPrintf(out, "    %s* value = NULL;\n", resultType);
    xl6OutputPuts("    if (frame == NULL || result == NULL) { xrtSetErrorKind(XERR_INTERNAL); outcome.ok = false; goto complete; }\n", out);
    if ( target->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID )
        xl6OutputPrintf(out,
                        "    value = (%s*)xrtMalloc(sizeof(*value));\n"
                        "    if (value == NULL) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); outcome.ok = false; goto complete; }\n"
                        "    *value = (%s){0};\n", resultType, resultType);
    /* Detach consuming inputs before calling the worker. Borrowed captures
     * remain owned by the frame's verified pre-publication retirement. */
    for (parameter = 0; parameter < target->parameterCount; ++parameter) {
        if (target->asyncCaptureAbis[parameter] != XL6_IR_ASYNC_CAPTURE_TAKE_PARAMETER) continue;
        xl6OutputPrintf(out, "    %s a%lu = frame->p%lu; frame->p%lu = NULL;\n",
            xl6IrCgenType(module, target->parameterTypes[parameter]),
            (unsigned long)parameter, (unsigned long)parameter, (unsigned long)parameter);
    }
    if ( workerMayRaise ) {
        xl6OutputPrintf(out,
                        "    %s worker;\n"
                        "    worker = xlir_fn_%u(",
                        workerResultType, (unsigned int)target->symbol);
    } else if ( target->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID )
        xl6OutputPrintf(out, "    xlir_fn_%u(", (unsigned int)target->symbol);
    else
        xl6OutputPrintf(out, "    *value = xlir_fn_%u(", (unsigned int)target->symbol);
    for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
        if ( parameter > 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, target->asyncCaptureAbis[parameter] == XL6_IR_ASYNC_CAPTURE_TAKE_PARAMETER
            ? "a%lu" : "frame->p%lu", (unsigned long)parameter);
    }
    xl6OutputPuts(");\n", out);
    if ( workerMayRaise ) {
        if ( target->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID )
            xl6OutputPuts("    outcome.ok = worker.ok;\n", out);
        else
            xl6OutputPuts("    outcome.ok = worker.ok;\n"
                          "    if (worker.ok) *value = worker.value;\n", out);
    }
    xl6OutputPrintf(out,
        "complete:\n"
        "    outcome.error = xrtTakeError();\n"
        "    if (outcome.error != NULL) outcome.ok = false;\n"
        "    cancelled = outcome.ok && xrtCancelRequested(cancel);\n"
        "    if (cancelled) outcome.ok = false;\n"
        "    outcome = xlir_async_frame_retire_%u(frame, outcome);\n"
        "    if (!outcome.ok) {\n"
        "        xl6_error_scope capture_cleanup = {0};\n"
        "        if (!xl6ErrorScopeBegin(&capture_cleanup)) abort();\n"
        "        xrtSetErrorTake(outcome.error); xl6ErrorScopeCaptureFailure(&capture_cleanup);\n",
        (unsigned int)target->symbol);
    if (target->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID)
        xl6OutputPrintf(out, "        if (value) %s(value, NULL);\n", valueDestroy);
    xl6IrCgenEmitActivationCleanupObserve(out);
    xl6OutputPuts(
        "        outcome = xl6ErrorScopeEnd(&capture_cleanup);\n"
        "        xrtSetErrorTake(outcome.error);\n"
        "        if (cancelled) { xrtClearError(); return XL6_TASK_CANCELLED; }\n"
        "        if (!xrtGetError()) xrtSetErrorKind(XERR_INTERNAL);\n"
        "        return XL6_TASK_FAILED;\n"
        "    }\n", out);
    if ( target->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID )
        xl6OutputPuts("    result->Value = NULL; result->Destroy = NULL; result->DestroyData = NULL;\n"
                      "    return XL6_TASK_SUCCESS;\n"
                      "}\n", out);
    else
        xl6OutputPrintf(out,
                        "    result->Value = value; result->Destroy = %s; result->DestroyData = NULL;\n"
                        "    return XL6_TASK_SUCCESS;\n"
                        "}\n", valueDestroy);
    xl6OutputPrintf(out, "static xfuture* xlir_async_start_raw_%u(", (unsigned int)target->symbol);
    if ( target->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
        const char* type = xl6IrCgenType(module, target->parameterTypes[parameter]);
        if ( parameter > 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu", type, (unsigned long)parameter);
    }
    xl6OutputPrintf(out,
        ")\n{\n"
        "    xfuture* future;\n"
        "    xlir_async_frame_%u* frame;\n"
        "    frame = (xlir_async_frame_%u*)xlir_task_allocate(sizeof(*frame), xlir_async_task_%u, &xlir_async_layout_%u);\n"
        "    if (frame == NULL) {\n", (unsigned int)target->symbol, (unsigned int)target->symbol,
        (unsigned int)target->symbol, (unsigned int)target->symbol);
    if (!xl6IrCgenEmitActivationInputRollback(out, module, target, diags)) return 0;
    xl6OutputPuts("    return NULL; }\n", out);
    xl6OutputPrintf(out, "    *frame = (xlir_async_frame_%u){0};\n", (unsigned int)target->symbol);
    /* The target capture vector gives a canonical two-phase acquisition:
     * take every incoming owner before the first fallible copy/retain. */
    for (size_t capturePhase = 0; capturePhase < 2; ++capturePhase)
    for (parameter = 0; parameter < target->parameterCount; ++parameter) {
        char place[64], input[32], rollback[128];
        XL6_IrAsyncFrameTransferAbi transfer = xl6IrCgenAsyncCaptureTransfer(target->asyncCaptureAbis[parameter]);
        if ((transfer == XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER ? 0u : 1u) != capturePhase) continue;
        snprintf(place, sizeof(place), "frame->p%lu", (unsigned long)parameter);
        snprintf(input, sizeof(input), "a%lu", (unsigned long)parameter);
        snprintf(rollback, sizeof(rollback), "xlir_async_frame_free_%u(frame, NULL);",
            (unsigned int)target->symbol);
        if (!xl6IrCgenEmitActivationCapture(out, xl6IrCgenType(module,target->parameterTypes[parameter]), transfer, place, input, NULL, rollback))
            return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR C emitter async task frame capture ABI is unresolved");
    }
    xl6OutputPrintf(out,
                    "    future = xlir_async_pool_submit_language(frame, NULL, %s);\n"
                    "    if (future == NULL) {\n"
                    ,
                    xl6IrCgenAsyncPayloadKind(target->asyncResultAbi));
    xl6IrCgenEmitActivationFailure(out);
    xl6OutputPrintf(out, "    xlir_async_frame_free_%u(frame, NULL); }\n"
        "    return future;\n}\n\n", (unsigned int)target->symbol);
    return !xl6OutputHasFailed(out);
}

/* Synchronous generators use the same verified continuation-frame vocabulary
 * as async machines, but their resume edge is an ordinary call.  Keep the
 * backend view entirely in terms of GeneratorMachine records: no AST walk,
 * source-name lookup, or coroutine compatibility path is permitted here. */
