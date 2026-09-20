/* Exact enum translation, not a second type/parameter semantics inference. */
static XL6_IrAsyncFrameTransferAbi xl6IrCgenAsyncCaptureTransfer(XL6_IrAsyncCaptureAbi abi)
{
    switch (abi) {
    case XL6_IR_ASYNC_CAPTURE_COPY_VALUE: return XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE;
    case XL6_IR_ASYNC_CAPTURE_COPY_STRING: return XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING;
    case XL6_IR_ASYNC_CAPTURE_RETAIN_XVALUE: return XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE;
    case XL6_IR_ASYNC_CAPTURE_RETAIN_FUTURE: return XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE;
    case XL6_IR_ASYNC_CAPTURE_RETAIN_CLOSURE: return XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CLOSURE;
    case XL6_IR_ASYNC_CAPTURE_RETAIN_ERROR: return XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_ERROR;
    case XL6_IR_ASYNC_CAPTURE_TAKE_PARAMETER: return XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER;
    default: return XL6_IR_ASYNC_FRAME_TRANSFER_INVALID;
    }
}
static int xl6IrCgenEmitPrototype(XL6_Output* out, const XL6_IrModule* module, const XL6_IrFunction* function, XL6_DiagList* diags)
{
    const char* returnType;
    const XL6_IrClosureTarget* closure;
    size_t i;
    if ( function->kind == XL6_IR_FUNCTION_MODULE_INIT ) return 1;
    if ( function->kind == XL6_IR_FUNCTION_MODULE_FINI ) {
        if (function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE)
            xl6OutputPrintf(out, "static %s xlir_module_fini_body(void)",
                xl6IrCgenErrorResultType(module, XL6_IR_TYPE_INVALID));
        else xl6OutputPuts("static void xlir_module_fini(void)", out);
        return 1;
    }
    returnType = function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
        ? xl6IrCgenErrorResultType(module, function->returnType)
        : (function->returnType == XL6_IR_TYPE_INVALID ? "void" : xl6IrCgenType(module, function->returnType));
    if ( returnType == NULL ) return xl6IrCgenDiag(diags, module, function->declaration, "XLIR C emitter does not support this function return ABI");
    xl6OutputPrintf(out, "static %s xlir_fn_%u(", returnType, (unsigned int)function->symbol);
    closure = xl6IrCgenClosureTargetGet(module, function->symbol);
    if ( closure != NULL ) xl6OutputPuts("void* environment", out);
    else if ( function->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( i = 0; i < function->parameterCount; ++i ) {
        XL6_SymbolId symbol = module->functionParameters[function->firstParameter + i];
        const char* type = xl6IrCgenFunctionParameterType(module, function, i);
        if ( type == NULL ) return xl6IrCgenDiag(diags, module, function->declaration, "XLIR C emitter does not support a parameter ABI");
        if ( i > 0 || closure != NULL ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s%s s%u", type,
            xl6IrCgenParameterAbiIsIndirect(
                xl6IrCgenStorageParameterAbi(
                    module, xl6IrStorageGet(module, symbol), NULL))
                ? "*" : "",
            (unsigned int)symbol);
    }
    xl6OutputPuts(")", out);
    return 1;
}

static int xl6IrCgenAsyncResultAbiMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrType* future;
    const XL6_IrType* worker;
    if ( module == NULL || target == NULL ||
         (future = xl6IrTypeGet(&module->types, target->returnType)) == NULL ||
         future->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR || future->paramCount != 1 ) return 0;
    worker = xl6IrTypeGet(&module->types, target->workerReturnType);
    switch ( target->asyncResultAbi ) {
    case XL6_IR_ASYNC_RESULT_VOID:
        return target->workerReturnType == XL6_IR_TYPE_INVALID;
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_BOOL &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_COPY_I64:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_I64 &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_COPY_U64:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_U64 &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_TYPE_ID &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_COPY_F64:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_F64 &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_RAW_POINTER &&
               worker->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
               future->params[0] == target->workerReturnType;
    case XL6_IR_ASYNC_RESULT_COPY_TAGGED:
        return worker != NULL && worker->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE &&
               future->params[0] == target->workerReturnType;
    default: return 0;
    }
}

static int xl6IrCgenEmitLauncherPrototype(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallTarget* target, XL6_DiagList* diags)
{
    size_t parameter;
    if ( target == NULL ||
         (target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC && target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) ||
         target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
         target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER )
        return xl6IrCgenDiag(diags, module,
            target != NULL ? target->declaration : XL6_IR_NODE_INVALID,
            "XLIR async launcher prototype ABI is unresolved");
    xl6OutputPrintf(out, "static %s xlir_%s_start_%u(",
        xl6IrCgenErrorResultType(module, target->returnType),
        target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ? "async" : "generator", (unsigned int)target->symbol);
    if ( target->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
        const char* type = target->parameterTypes != NULL
            ? xl6IrCgenType(module, target->parameterTypes[parameter]) : NULL;
        if ( type == NULL )
            return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR async launcher prototype parameter ABI is unresolved");
        if ( parameter != 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu", type, (unsigned long)parameter);
    }
    xl6OutputPuts(");\n", out);
    return !xl6OutputHasFailed(out);
}

/* Activation captures are fixed by the target/frame contract. Acquisition,
 * rollback and physical drop rendering are shared by async and generators. */
static void xl6IrCgenEmitActivationCleanupBegin(XL6_Output* out, int failed)
{
    xl6OutputPrintf(out,
        "    { xl6_error_scope capture_cleanup = {0}; xl6_error_result capture_outcome;\n"
        "      if (!xl6ErrorScopeBeginCleanup(&capture_cleanup, %s)) abort();\n",
        failed ? "true" : "false");
}

static void xl6IrCgenEmitActivationCleanupObserve(XL6_Output* out)
{
    xl6OutputPuts("    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&capture_cleanup);\n", out);
}

static void xl6IrCgenEmitActivationCleanupEnd(XL6_Output* out)
{
    /* A void physical drop still reports its full outcome to the enclosing
     * invocation/cleanup scope. A NULL diagnostic does not mean success. */
    xl6OutputPuts("    capture_outcome = xl6ErrorScopeEnd(&capture_cleanup);\n"
        "    if (!capture_outcome.ok) xl6ErrorScopePropagateFailure(capture_outcome);\n"
        "    else xrtSetErrorTake(capture_outcome.error); }\n", out);
}

/* Raw runtime launchers may capture a cleanup failure into their current
 * error scope. The language-visible launch boundary owns that scope and
 * returns its outcome; it never marks an unrelated caller scope failed. */
static int xl6IrCgenEmitLaunchOutcomeBoundary(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallTarget* target, XL6_DiagList* diags)
{
    const char* name; const char* carrier; size_t p;
    if (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER)
        return xl6IrCgenDiag(diags, module, target->declaration, "XLIR launcher has no explicit outcome carrier");
    if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) name = "async";
    else if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) name = "generator";
    else return 0;
    carrier = xl6IrCgenErrorResultType(module, target->returnType);
    if (carrier == NULL) return 0;
    xl6OutputPrintf(out, "static %s xlir_%s_start_%u(", carrier, name, (unsigned int)target->symbol);
    if (!target->parameterCount) xl6OutputPuts("void", out);
    for (p = 0; p < target->parameterCount; ++p) {
        if (p) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu", xl6IrCgenType(module, target->parameterTypes[p]), (unsigned long)p);
    }
    xl6OutputPrintf(out, ")\n{\n    %s result = {false};\n", carrier);
    xl6OutputPuts("    xl6_error_scope launch = {0}; xl6_error_result outcome;\n"
        /* An active language invocation has already initialized this context;
         * rebinding it is the same invariant as all cleanup/resume scopes. */
        "    if (!xl6ErrorScopeBegin(&launch)) abort();\n", out);
    xl6OutputPrintf(out, "    result.value = xlir_%s_start_raw_%u(", name, (unsigned int)target->symbol);
    for (p = 0; p < target->parameterCount; ++p) {
        if (p) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "a%lu", (unsigned long)p);
    }
    xl6OutputPuts(");\n    if (result.value == NULL) xl6ErrorScopeCaptureFailure(&launch);\n"
        "    outcome = xl6ErrorScopeEnd(&launch); result.ok = outcome.ok;\n"
        "    if (!result.ok && result.value != NULL) {\n"
        "        xl6_error_scope rollback = {0};\n"
        "        if (!xl6ErrorScopeBegin(&rollback)) abort();\n"
        "        xrtSetErrorTake(outcome.error); xl6ErrorScopeCaptureFailure(&rollback);\n", out);
    if (!xl6IrCgenEmitOwnedPlaceRelease(out, module, target->returnType, "result.value", diags)) return 0;
    xl6OutputPuts("        outcome = xl6ErrorScopeEnd(&rollback); result.value = NULL;\n"
        /* Scope End restores the caller's diagnostic. A successful launch
         * must not erase it by publishing an empty local diagnostic. */
        "    }\n    if (!result.ok) xrtSetErrorTake(outcome.error);\n    return result;\n}\n", out);
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitActivationFailure(XL6_Output* out)
{
    xl6OutputPuts("    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"activation allocation failed\"));\n", out);
}

static int xl6IrCgenEmitActivationInputRollback(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallTarget* target, XL6_DiagList* diags)
{
    size_t parameter;
    xl6IrCgenEmitActivationFailure(out);
    xl6IrCgenEmitActivationCleanupBegin(out, 1);
    for (parameter = target->parameterCount; parameter != 0; --parameter) {
        char place[32];
        size_t index = parameter - 1;
        if (target->parameterAbis[index] != XL6_IR_PARAMETER_ABI_OWNED) continue;
        snprintf(place, sizeof(place), "a%lu", (unsigned long)index);
        if (!xl6IrCgenEmitOwnedPlaceRelease(
                out, module, target->parameterTypes[index], place, diags)) return 0;
        xl6IrCgenEmitActivationCleanupObserve(out);
    }
    xl6IrCgenEmitActivationCleanupEnd(out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenActivationFrameParameterSupported(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrContinuationFrameSlot* frame, size_t initBitCount)
{
    if (frame->parameter >= target->parameterCount ||
        frame->type != target->parameterTypes[frame->parameter] ||
        frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_INVALID ||
        frame->transferAbi != xl6IrParameterFrameTransfer(
            module, frame->type, target->parameterAbis[frame->parameter])) return 0;
    if (frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE)
        return frame->initBit == SIZE_MAX;
    return frame->initBit < initBitCount &&
        xl6IrCgenFrameDestroyFunction(frame->destroyAbi) != NULL;
}

static int xl6IrCgenEmitFrameAcquisitionBegin(XL6_Output* out,const char* type,
    XL6_IrAsyncFrameTransferAbi transfer,const char* input,int* acquired)
{
    const char* acquire=NULL;
    if (!type || !input || !acquired) return 0;
    switch (transfer) {
    case XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE:
    case XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER:
    case XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE:
    case XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED: break;
    case XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING: acquire = "xrtStrDup"; break;
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE: acquire = "xrtValueRetain"; break;
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE: acquire = "xrtFutureRef"; break;
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CLOSURE: acquire = "xlir_closure_retain"; break;
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CELL: acquire = "xlir_cell_retain"; break;
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_ERROR: acquire = "xrtErrorRef"; break;
    default: return 0;
    }
    *acquired=acquire!=NULL;
    if (acquire) xl6OutputPrintf(out,"    { %s xlir_acquired = %s != NULL ? %s(%s) : NULL;\n",type,input,acquire,input);
    else xl6OutputPrintf(out,"    { %s xlir_acquired = %s;\n",type,input);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitFrameAcquisitionCommit(XL6_Output* out,const char* type,
    const char* place,const char* initialized)
{
    if (!xl6IrCgenEmitSlotCommit(out,type,place,"xlir_acquired",initialized)) return 0;
    xl6OutputPuts("    }\n",out); return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitActivationCapture(
    XL6_Output* out,const char* type,XL6_IrAsyncFrameTransferAbi transfer,
    const char* place,const char* input,const char* initialized,const char* rollback)
{
    int acquired;
    if (transfer==XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE || transfer==XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED ||
        !xl6IrCgenEmitFrameAcquisitionBegin(out,type,transfer,input,&acquired)) return 0;
    if (acquired) {
        xl6OutputPrintf(out,"    if (%s != NULL && xlir_acquired == NULL) {\n",input);
        xl6IrCgenEmitActivationFailure(out); xl6OutputPrintf(out,"    %s return NULL; }\n",rollback);
    }
    if (!xl6IrCgenEmitFrameAcquisitionCommit(out,type,place,initialized)) return 0;
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitAsyncReadyLauncher(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const char* resultType, const char* valueDestroy, int workerMayRaise,
    const char* workerResultType, XL6_DiagList* diags)
{
    size_t parameter;
    if ( target == NULL || target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE ||
         resultType == NULL || valueDestroy == NULL ||
         (workerMayRaise && workerResultType == NULL) ||
         xl6IrCgenFindFunction(module, target->symbol) == NULL )
        return xl6IrCgenDiag(diags, module, target != NULL ? target->declaration : XL6_IR_NODE_INVALID,
                             "XLIR C emitter async ready-Future ABI is unresolved");
    xl6OutputPrintf(out, "static xfuture* xlir_async_start_raw_%u(", (unsigned int)target->symbol);
    if ( target->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
        const char* type = xl6IrCgenType(module, target->parameterTypes[parameter]);
        if ( type == NULL )
            return xl6IrCgenDiag(diags, module, target->declaration,
                                 "XLIR C emitter async ready-Future parameter ABI is unresolved");
        if ( parameter > 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu", type, (unsigned long)parameter);
    }
    xl6OutputPrintf(out,
        ")\n{\n"
        "    xfuture* future = NULL;\n"
        "    xpromise* promise = xrtPromiseCreate(&future, NULL);\n"
        "    %s* value;\n"
        "    if (promise == NULL) {\n", resultType);
    if (!xl6IrCgenEmitActivationInputRollback(out, module, target, diags)) return 0;
    xl6OutputPrintf(out,
        "    return NULL; }\n"
        "    value = (%s*)xrtMalloc(sizeof(*value));\n"
        "    if (value == NULL) {\n", resultType);
    if (!xl6IrCgenEmitActivationInputRollback(out, module, target, diags)) return 0;
    xl6OutputPuts("    xrtPromiseDestroy(promise); xrtFutureDestroy(future); return NULL; }\n", out);
    /* All transport allocation precedes the consuming call. No owned worker
     * result can be stranded by allocating its box after the call. */
    if (workerMayRaise)
        xl6OutputPrintf(out, "    %s worker = xlir_fn_%u(",
            workerResultType, (unsigned int)target->symbol);
    else
        xl6OutputPrintf(out, "    *value = xlir_fn_%u(", (unsigned int)target->symbol);
    for (parameter = 0; parameter < target->parameterCount; ++parameter) {
        if (parameter != 0) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "a%lu", (unsigned long)parameter);
    }
    xl6OutputPuts(");\n", out);
    if (workerMayRaise)
        xl6OutputPuts(
            "    if (!worker.ok) { xerror* error = xrtTakeError(); xrtFree(value);\n"
            "        if (!xlir_future_complete_error(promise, error)) { xrtErrorFree(error); xrtPromiseDestroy(promise); xrtFutureDestroy(future); return NULL; }\n"
            "        xrtErrorFree(error); xrtPromiseDestroy(promise); return future; }\n"
            "    *value = worker.value;\n", out);
    xl6OutputPuts("    if (!", out);
    if (!xl6IrCgenEmitAsyncPayloadPublish(out, target->asyncResultAbi, "promise", "value")) return 0;
    xl6OutputPuts(") {\n", out);
    xl6IrCgenEmitActivationFailure(out);
    xl6IrCgenEmitActivationCleanupBegin(out, 1);
    xl6OutputPrintf(out, "    %s(value, NULL);\n", valueDestroy);
    xl6IrCgenEmitActivationCleanupObserve(out);
    xl6OutputPuts("    xrtPromiseDestroy(promise); xrtFutureDestroy(future);\n", out);
    xl6IrCgenEmitActivationCleanupObserve(out);
    xl6IrCgenEmitActivationCleanupEnd(out);
    xl6OutputPuts("    return NULL; }\n"
        "    xrtPromiseDestroy(promise);\n"
        "    return future;\n"
        "}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static size_t xl6IrCgenAsyncParameterIndex(
    const XL6_IrModule* module, const XL6_IrFunction* function,
    XL6_SymbolId storageSymbol)
{
    size_t parameter;
    if ( module == NULL || function == NULL ) return SIZE_MAX;
    for ( parameter = 0; parameter < function->parameterCount; ++parameter )
        if ( module->functionParameters[function->firstParameter + parameter] == storageSymbol )
            return parameter;
    return SIZE_MAX;
}

static size_t xl6IrCgenAsyncFrameStorageSlot(
    const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    XL6_SymbolId storageSymbol)
{
    size_t local;
    if ( module == NULL || machine == NULL ) return SIZE_MAX;
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrAsyncMachineFrameSlot* frame =
            &module->asyncMachineFrameSlots[machine->firstFrameSlot + local];
        if ( frame->source == XL6_IR_ASYNC_SPILL_SOURCE_STORAGE &&
             frame->storageSymbol == storageSymbol ) return machine->firstFrameSlot + local;
    }
    return SIZE_MAX;
}

static int xl6IrCgenAsyncMarkProgram(
    const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    size_t entryProgramBlock, unsigned char* reachable)
{
    size_t local;
    int changed;
    if ( module == NULL || machine == NULL || reachable == NULL ||
         entryProgramBlock < machine->firstBlock ||
         entryProgramBlock >= machine->firstBlock + machine->blockCount ) return 0;
    reachable[entryProgramBlock - machine->firstBlock] = 1;
    do {
        changed = 0;
        for ( local = 0; local < machine->blockCount; ++local ) {
            const XL6_IrAsyncMachineBlock* block;
            size_t targets[2];
            size_t target;
            if ( !reachable[local] ) continue;
            block = &module->asyncMachineBlocks[machine->firstBlock + local];
            targets[0] = block->trueTarget;
            targets[1] = block->falseTarget;
            if (block->kind == XL6_IR_ASYNC_MACHINE_BLOCK_SUSPEND) {
                const XL6_IrAsyncSuspension* suspension = &module->asyncSuspensions[block->suspension];
                targets[0] = module->asyncMachineEntries[suspension->setupFailureEntry].programBlock;
            }
            for ( target = 0; target < 2; ++target ) {
                size_t next = targets[target];
                if ( next == SIZE_MAX ) continue;
                if ( next < machine->firstBlock ||
                     next >= machine->firstBlock + machine->blockCount ) return 0;
                if ( !reachable[next - machine->firstBlock] ) {
                    reachable[next - machine->firstBlock] = 1;
                    changed = 1;
                }
            }
        }
    } while ( changed );
    return 1;
}

static int xl6IrCgenAsyncProgramReferencesStorage(
    const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    const unsigned char* blocks, XL6_SymbolId symbol)
{
    size_t local;
    if ( module == NULL || machine == NULL || blocks == NULL || symbol == 0 ) return 0;
    for ( local = 0; local < machine->blockCount; ++local ) {
        const XL6_IrAsyncMachineBlock* block;
        size_t instructionIndex;
        if ( !blocks[local] ) continue;
        block = &module->asyncMachineBlocks[machine->firstBlock + local];
        for ( instructionIndex = block->firstInstruction;
              instructionIndex < block->firstInstruction + block->instructionCount;
              ++instructionIndex )
            if ( module->instructions[instructionIndex].targetSymbol == symbol ) return 1;
    }
    return 0;
}

static const XL6_IrStorage* xl6IrCgenAsyncLocalStorage(
    const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    const XL6_IrFunction* function, XL6_SymbolId symbol)
{
    const XL6_IrStorage* storage = xl6IrStorageGet(module, symbol);
    if ( storage == NULL || function == NULL ||
         xl6IrCgenAsyncFrameStorageSlot(module, machine, symbol) != SIZE_MAX ||
         storage->owner != function->declaration ||
         (storage->kind != XL6_IR_STORAGE_LOCAL &&
          storage->kind != XL6_IR_STORAGE_TEMPORARY) ) return NULL;
    return storage;
}


/* Execute verified physical retirement commands; no SSA alias discovery is
 * permitted here. Each command identifies its exact consuming instruction and
 * its commit phase, and several residents can be retired by one call. */
static int xl6IrCgenEmitContinuationTransfers(XL6_Output* out, const XL6_IrModule* module,
    int generator, size_t machineIndex, const XL6_IrInstruction* instruction,
    XL6_IrContinuationFrameActionKind phase)
{
    const XL6_IrAsyncMachine* async = generator ? NULL : &module->asyncMachines[machineIndex];
    const XL6_IrGeneratorMachine* gen = generator ? &module->generatorMachines[machineIndex] : NULL;
    const XL6_IrContinuationFrameAction* actions = generator ? module->generatorFrameActions : module->asyncFrameActions;
    const XL6_IrContinuationFrameSlot* frames = generator ? module->generatorFrameSlots : module->asyncMachineFrameSlots;
    size_t first = generator ? gen->firstTransferAction : async->firstTransferAction;
    size_t count = generator ? gen->transferActionCount : async->transferActionCount;
    size_t firstFrame = generator ? gen->firstFrameSlot : async->firstFrameSlot;
    size_t frameCount = generator ? gen->frameSlotCount : async->frameSlotCount;
    size_t index = (size_t)(instruction - module->instructions), i;
    int started=0;
    for (i = first; count && i < first + count; ++i) {
        const XL6_IrContinuationFrameAction* action = &actions[i];
        const XL6_IrContinuationFrameSlot* frame;
        if (action->instruction != index || action->kind != phase) continue;
        if (action->frameSlot < firstFrame || action->frameSlot - firstFrame >= frameCount) return 0;
        frame = &frames[action->frameSlot];
        if (frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED || frame->initBit == SIZE_MAX) return 0;
        {
            char place[64],initialized[64]; const char* type=xl6IrCgenType(module,frame->type);
            if (!type) return 0;
            snprintf(place,sizeof(place),"frame->f%lu",(unsigned long)(action->frameSlot-firstFrame));
            snprintf(initialized,sizeof(initialized),"frame->i%lu",(unsigned long)frame->initBit);
            if (!started) { xl6IrCgenSlotPlanBegin(out); started=1; }
            xl6IrCgenSlotReset(out,type,place,initialized);
        }
    }
    if (started) xl6IrCgenSlotPlanEnd(out);
    return !xl6OutputHasFailed(out);
}




static int xl6IrCgenAsyncDeclareMachineProgramStorages(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    const XL6_IrFunction* function, const unsigned char* blocks,
    XL6_DiagList* diags)
{
    size_t storageIndex;
    for ( storageIndex = 0; storageIndex < module->storageCount; ++storageIndex ) {
        const XL6_IrStorage* storage = &module->storages[storageIndex];
        const XL6_IrType* typeInfo;
        const char* type;
        if ( xl6IrCgenAsyncLocalStorage(
                 module, machine, function, storage->symbol) == NULL ||
             !xl6IrCgenAsyncProgramReferencesStorage(
                 module, machine, blocks, storage->symbol) ) continue;
        typeInfo = xl6IrTypeGet(&module->types,
            storage->cellType != XL6_IR_TYPE_INVALID ? storage->cellType : storage->type);
        type = xl6IrCgenType(module,
            storage->cellType != XL6_IR_TYPE_INVALID ? storage->cellType : storage->type);
        if ( type == NULL || typeInfo == NULL )
            return xl6IrCgenDiag(diags, module, storage->declaration,
                                 "XLIR async machine local storage has no C ABI");
        if ( storage->cellType != XL6_IR_TYPE_INVALID ||
             typeInfo->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED )
            xl6OutputPrintf(out, "    %s s%u = NULL;\n", type,
                            (unsigned int)storage->symbol);
        else
            xl6OutputPrintf(out, "    %s s%u;\n", type,
                            (unsigned int)storage->symbol);
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenAsyncProgramUnsupported(
    const char** reason, const char* message)
{
    if ( reason != NULL ) *reason = message;
    return 0;
}

static int xl6IrCgenAsyncStartAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrCallTarget* callee;
    size_t argument;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_ASYNC_START ||
         (callee = xl6IrCallTargetGet(module, instruction->targetSymbol)) == NULL ||
         callee->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
         (callee->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL &&
          callee->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE) ||
         !xl6IrCgenAsyncResultAbiMatches(module, callee) ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         instruction->type != callee->returnType ||
         instruction->operandCount != callee->parameterCount ||
         (callee->parameterCount != 0 && callee->parameterTypes == NULL) ) return 0;
    for ( argument = 0; argument < instruction->operandCount; ++argument ) {
        const XL6_IrInstruction* value = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + argument], NULL);
        if ( value == NULL || value->type != callee->parameterTypes[argument] ||
             xl6IrCgenType(module, callee->parameterTypes[argument]) == NULL ) return 0;
    }
    return 1;
}

static int xl6IrCgenAsyncMachineProgramSupported(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrAsyncMachine* machine, const XL6_IrFunction* function,
    const unsigned char* launchBlocks, const unsigned char* resumeBlocks,
    const char** unsupportedReason, char* unsupportedDetail,
    size_t unsupportedDetailCapacity)
{
    const XL6_IrType* result = xl6IrTypeGet(&module->types, target->workerReturnType);
    const XL6_IrAsyncSuspension* suspension;
    size_t local;
    size_t action;
    if ( unsupportedReason != NULL ) *unsupportedReason = NULL;
    if ( result == NULL || function == NULL || machine->suspensionCount == 0 ||
         (target->workerErrorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
          target->workerErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) ||
         (result->cAbi != XL6_IR_C_ABI_BOOL && result->cAbi != XL6_IR_C_ABI_I64 &&
           result->cAbi != XL6_IR_C_ABI_U64 && result->cAbi != XL6_IR_C_ABI_TYPE_ID &&
           result->cAbi != XL6_IR_C_ABI_F64 &&
           result->cAbi != XL6_IR_C_ABI_RAW_POINTER &&
           result->cAbi != XL6_IR_C_ABI_MANAGED_STRING &&
           result->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           result->cAbi != XL6_IR_C_ABI_CLOSURE_PTR &&
           result->cAbi != XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
           result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
           result->cAbi != XL6_IR_C_ABI_TAGGED_NULLABLE) ||
         machine->prologueActionCount > machine->frameSlotCount ||
         machine->destroyActionCount > machine->frameSlotCount )
        return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
            "XLIR async machine result, error, suspension or action-count ABI is not admitted");
    for ( local = 0; local < machine->suspensionCount; ++local ) {
        const XL6_IrType* suspensionType;
        suspension = &module->asyncSuspensions[machine->firstSuspension + local];
        suspensionType = xl6IrTypeGet(&module->types, suspension->resultType);
        if ( suspensionType == NULL ||
             !((suspensionType->cAbi == XL6_IR_C_ABI_BOOL &&
                suspension->resultAbi == XL6_IR_ASYNC_RESULT_COPY_BOOL) ||
               (suspensionType->cAbi == XL6_IR_C_ABI_I64 &&
                suspension->resultAbi == XL6_IR_ASYNC_RESULT_COPY_I64) ||
               (suspensionType->cAbi == XL6_IR_C_ABI_U64 &&
                suspension->resultAbi == XL6_IR_ASYNC_RESULT_COPY_U64) ||
               (suspensionType->cAbi == XL6_IR_C_ABI_TYPE_ID &&
                suspension->resultAbi == XL6_IR_ASYNC_RESULT_COPY_TYPE_ID) ||
               (suspensionType->cAbi == XL6_IR_C_ABI_F64 &&
                 suspension->resultAbi == XL6_IR_ASYNC_RESULT_COPY_F64) ||
               (suspensionType->cAbi == XL6_IR_C_ABI_RAW_POINTER &&
                suspensionType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
                suspension->resultAbi ==
                    XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER) ||
               (suspensionType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
                suspension->resultAbi == XL6_IR_ASYNC_RESULT_OWNED_STRING) ||
                (suspensionType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                 suspension->resultAbi == XL6_IR_ASYNC_RESULT_RETAINED_XVALUE) ||
                 (suspensionType->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
                  suspension->resultAbi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE) ||
                 (suspensionType->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
                  suspension->resultAbi == XL6_IR_ASYNC_RESULT_RETAINED_ERROR) ||
                 (suspensionType->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
                  suspension->resultAbi == XL6_IR_ASYNC_RESULT_RETAINED_FUTURE) ||
                 (suspensionType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE &&
                 suspension->resultAbi == XL6_IR_ASYNC_RESULT_COPY_TAGGED)) )
            return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                "XLIR async suspension result ABI is not admitted");
    }
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrAsyncMachineFrameSlot* frame =
            &module->asyncMachineFrameSlots[machine->firstFrameSlot + local];
        const XL6_IrType* frameType = xl6IrTypeGet(&module->types, frame->type);
        int trivial = frameType != NULL &&
            frameType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
            frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE &&
            frame->initBit == SIZE_MAX;
        int managedString = frameType != NULL &&
            frameType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING &&
            frame->initBit < machine->initBitCount;
        int managedXValue = frameType != NULL &&
            frameType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE &&
            frame->initBit < machine->initBitCount;
        int managedClosure = frameType != NULL &&
            frameType->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE &&
            frame->initBit < machine->initBitCount;
        int protectedLoan = frameType != NULL &&
            frameType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE &&
            frame->borrowFrameSlot >= machine->firstFrameSlot &&
            frame->borrowFrameSlot - machine->firstFrameSlot < machine->frameSlotCount &&
            frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE &&
            frame->initBit == SIZE_MAX;
        int residentTransfer = frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED ||
            frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING ||
            frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE ||
            frame->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CLOSURE;
        if ( frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL ) {
            if (!protectedLoan && !trivial && !(residentTransfer && frameType != NULL &&
                frameType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                xl6IrCgenFrameDestroyFunction(frame->destroyAbi) != NULL))
                return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                    "XLIR async SSA spill frame ABI is not admitted");
        } else if ( frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER ) {
            if (!xl6IrCgenActivationFrameParameterSupported(module, target, frame, machine->initBitCount))
                return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                    "XLIR async launch-parameter frame ABI is not admitted");
        } else if ( frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE ) {
            if ( !xl6IrCgenFrameStorageSupported(module, frame, machine->initBitCount) )
                return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                    "XLIR async frame-storage ABI is not admitted");
        } else
            return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                "XLIR async frame residence is not admitted");
    }
    for ( action = 0; action < machine->actionCount; ++action ) {
        const XL6_IrAsyncFrameAction* frameAction =
            &module->asyncFrameActions[machine->firstAction + action];
        if ( frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_INIT_PARAMETER &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_BIND_STORAGE &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_SPILL_VALUE &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_PROJECT_VALUE &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT &&
             frameAction->kind != XL6_IR_ASYNC_FRAME_ACTION_DESTROY_IF_INITIALIZED )
            return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                "XLIR async frame action is not admitted");
    }
    for ( local = 0; local < machine->blockCount; ++local ) {
        const XL6_IrAsyncMachineBlock* block =
            &module->asyncMachineBlocks[machine->firstBlock + local];
        size_t instructionIndex;
        if ( !launchBlocks[local] && !resumeBlocks[local] ) continue;
        for ( instructionIndex = block->firstInstruction;
              instructionIndex < block->firstInstruction + block->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction = &module->instructions[instructionIndex];
            switch ( instruction->kind ) {
            case XL6_IR_INSTR_PRODUCT_MAKE:
            case XL6_IR_INSTR_PRODUCT_GET:
            case XL6_IR_INSTR_PRODUCT_TAKE:
            case XL6_IR_INSTR_PRODUCT_DROP:
                if (!xl6IrCgenProductInstructionAbi(module, instruction))
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async product instruction has no exact structural/ownership ABI");
                break;
            case XL6_IR_INSTR_CONST:
            case XL6_IR_INSTR_BINARY:
            case XL6_IR_INSTR_UNARY:
            case XL6_IR_INSTR_SCALAR_CONVERT:
            case XL6_IR_INSTR_VALUE:
                break;
            case XL6_IR_INSTR_NULL:
                if ( !xl6IrCgenNullInstructionAbi(module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async null ABI is not admitted");
                break;
            case XL6_IR_INSTR_NULL_TEST:
            case XL6_IR_INSTR_NULLABLE_WRAP:
            case XL6_IR_INSTR_NULLABLE_UNWRAP:
                if ( !xl6IrCgenNullableInstructionAbi(module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async nullable ABI is not admitted");
                break;
            case XL6_IR_INSTR_UNION_ZERO:
            case XL6_IR_INSTR_UNION_GET:
            case XL6_IR_INSTR_UNION_SET:
            case XL6_IR_INSTR_BRANCH:
            case XL6_IR_INSTR_JUMP:
                break;
            case XL6_IR_INSTR_DISCARD:
                if ( !xl6IrCgenDiscardAbi(module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async discard ABI is not admitted");
                break;
            case XL6_IR_INSTR_TRAP_IF_FALSE:
                if ( instruction->result != XL6_IR_VALUE_INVALID ||
                     instruction->operandCount != 1 )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async trap ABI is not admitted");
                break;
            case XL6_IR_INSTR_CLOSURE_MAKE:
                {
                    const XL6_IrClosureTarget* closure =
                        xl6IrCgenClosureTargetGet(module, instruction->targetSymbol);
                    size_t capture;
                    if ( closure == NULL ||
                         instruction->operandCount != closure->captureCount ||
                         closure->firstCapture > module->closureCaptureCount ||
                         closure->captureCount >
                            module->closureCaptureCount - closure->firstCapture )
                        return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                            "XLIR async closure target ABI is not admitted");
                    for ( capture = 0; capture < closure->captureCount; ++capture ) {
                        XL6_IrClosureCaptureAbi abi = module->closureCaptures[
                            closure->firstCapture + capture].abi;
                        if ( abi != XL6_IR_CLOSURE_CAPTURE_COPY_VALUE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_COPY_STRING &&
                             abi != XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL )
                            return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                                "XLIR async closure capture ABI is not admitted");
                    }
                }
                break;
            case XL6_IR_INSTR_CLOSURE_CALL:
                {
                    const XL6_IrType* signature =
                        xl6IrTypeGet(&module->types, instruction->calleeType);
                    if ( signature == NULL || signature->kind != XL6_IR_TYPE_FUNCTION ||
                         signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
                         instruction->operandCount != signature->paramCount + 1 )
                        return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                            "XLIR async closure-call ABI is not admitted");
                }
                break;
            case XL6_IR_INSTR_RETAIN:
                if ( !xl6IrCgenRetainInstructionAbi(
                         module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(
                        unsupportedReason,
                        "XLIR async retain ABI is not admitted");
                break;
            case XL6_IR_INSTR_PROTOCOL_CALL:
                if ( !xl6IrCgenProtocolCallAbi(module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async protocol-call ABI is not admitted");
                break;
            case XL6_IR_INSTR_ASYNC_START:
                if ( !xl6IrCgenAsyncStartAbi(module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async nested-launch ABI is not admitted");
                break;
            case XL6_IR_INSTR_GENERATOR_START:
                if (!xl6IrCgenLaunchCallAbi(module, instruction))
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async generator-launch ABI is not admitted");
                break;
            case XL6_IR_INSTR_GENERATOR_NEXT:
            case XL6_IR_INSTR_GENERATOR_ITEM_GET:
            case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
                /* Operand, item and success-edge ABI are verified once for
                 * every execution mode by the module instruction verifier. */
                break;
            case XL6_IR_INSTR_RUNTIME_CALL:
                /* Runtime operands/results have the same verified ABI in
                 * every execution mode. The shared instruction verifier and
                 * emitter own this contract; only frame/control operations
                 * require a machine-specific admission check. */
                break;
            case XL6_IR_INSTR_LOAD:
                if ( xl6IrCgenAsyncFrameStorageSlot(
                        module, machine, instruction->targetSymbol) == SIZE_MAX &&
                     !(launchBlocks[local] &&
                       xl6IrCgenAsyncParameterIndex(module, function,
                         instruction->targetSymbol) != SIZE_MAX) &&
                     xl6IrCgenAsyncLocalStorage(
                         module, machine, function, instruction->targetSymbol) == NULL &&
                     xl6IrCgenSharedStorage(
                         module, instruction->targetSymbol) == NULL )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async load storage ABI is not admitted");
                break;
            case XL6_IR_INSTR_STORAGE_ADDRESS:
                {
                    const XL6_IrStorage* storage = xl6IrStorageGet(
                        module, instruction->targetSymbol);
                    if ( storage == NULL || instruction->operandCount != 0 ||
                         instruction->result == XL6_IR_VALUE_INVALID ||
                         !xl6IrCgenStorageRefTo(module, instruction, storage->type) ||
                         (xl6IrCgenAsyncFrameStorageSlot(
                              module, machine, instruction->targetSymbol) == SIZE_MAX &&
                          xl6IrCgenAsyncLocalStorage(
                              module, machine, function,
                              instruction->targetSymbol) == NULL &&
                          xl6IrCgenSharedStorage(
                              module, instruction->targetSymbol) == NULL) )
                        return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                            "XLIR async storage-address ABI is not admitted");
                }
                break;
            case XL6_IR_INSTR_VALUE_RELEASE:
                break;
            case XL6_IR_INSTR_STORE:
            case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
            case XL6_IR_INSTR_RELEASE:
            case XL6_IR_INSTR_MOVE:
            case XL6_IR_INSTR_STORAGE_TAKE:
            case XL6_IR_INSTR_CELL_NEW:
            case XL6_IR_INSTR_CELL_INIT:
            case XL6_IR_INSTR_CELL_LOAD:
            case XL6_IR_INSTR_CELL_STORE:
            case XL6_IR_INSTR_CELL_EXCHANGE:
            case XL6_IR_INSTR_CELL_RETAIN:
            case XL6_IR_INSTR_CELL_RELEASE:
                if ( xl6IrCgenAsyncFrameStorageSlot(
                        module, machine, instruction->targetSymbol) == SIZE_MAX &&
                     xl6IrCgenAsyncLocalStorage(
                         module, machine, function, instruction->targetSymbol) == NULL &&
                     ((instruction->kind != XL6_IR_INSTR_STORE &&
                       instruction->kind != XL6_IR_INSTR_MOVE &&
                       instruction->kind != XL6_IR_INSTR_STORAGE_TAKE &&
                       instruction->kind != XL6_IR_INSTR_RELEASE) ||
                      xl6IrCgenSharedStorage(module, instruction->targetSymbol) == NULL) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async store or release storage ABI is not admitted");
                break;
            case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
                /* The module verifier owns the failure-edge ownership proof. */
                break;
            case XL6_IR_INSTR_CALL:
                {
                    const XL6_IrCallTarget* call =
                        xl6IrCallTargetGet(module, instruction->targetSymbol);
                    if ( !xl6IrCgenDirectCallAbiSupported(
                             module, call, instruction) )
                        return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                            "XLIR async direct-call ABI is not admitted");
                }
                break;
            case XL6_IR_INSTR_VIRTUAL_CALL:
                if (!xl6IrCgenVirtualCallAbi(module, instruction))
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async virtual-call ABI is not admitted");
                break;
            case XL6_IR_INSTR_AWAIT:
                if ( (!launchBlocks[local] && !resumeBlocks[local]) ||
                     block->kind != XL6_IR_ASYNC_MACHINE_BLOCK_SUSPEND ||
                     instructionIndex + 1 !=
                        block->firstInstruction + block->instructionCount ||
                     block->suspension < machine->firstSuspension ||
                     block->suspension >=
                        machine->firstSuspension + machine->suspensionCount )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async await placement ABI is not admitted");
                break;
            case XL6_IR_INSTR_ERROR_TEST:
            case XL6_IR_INSTR_RETURN:
            case XL6_IR_INSTR_THROW:
                break;
            case XL6_IR_INSTR_CLEANUP_BEGIN:
            case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
            case XL6_IR_INSTR_CLEANUP_CAPTURE:
            case XL6_IR_INSTR_CLEANUP_END:
            case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
            case XL6_IR_INSTR_ERROR_UNWIND_END:
            case XL6_IR_INSTR_ERROR_TAKE:
            case XL6_IR_INSTR_ERROR_SET_TAKE:
            case XL6_IR_INSTR_ERROR_CLEAR:
            case XL6_IR_INSTR_ERROR_RAISE:
                if ( !xl6IrCgenErrorStateInstructionAbi(module, instruction) )
                    return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                        "XLIR async error-state ownership ABI is not admitted");
                break;
            default:
                if ( unsupportedDetail != NULL &&
                     unsupportedDetailCapacity != 0 ) {
                    snprintf(unsupportedDetail, unsupportedDetailCapacity,
                        "XLIR async instruction kind %d at i%lu (runtime %d) is not admitted",
                        (int)instruction->kind,
                        (unsigned long)instructionIndex,
                        (int)instruction->runtimeOp);
                    if ( unsupportedReason != NULL )
                        *unsupportedReason = unsupportedDetail;
                    return 0;
                }
                return xl6IrCgenAsyncProgramUnsupported(unsupportedReason,
                    "XLIR async instruction kind is not admitted");
            }
        }
    }
    /* Program-block overlap is not storage liveness.  A loop-local can be
     * freshly defined in both the launch and resume programs without crossing
     * the suspension at all.  The XLIR async analysis and verifier own the
     * exact live-slot/frame layout; Cgen must consume that layout instead of
     * reconstructing it from reachability. */
    return 1;
}

static int xl6IrCgenAsyncDeclareMachineProgramValues(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrAsyncMachine* machine,
    const XL6_IrFunction* function, const unsigned char* blocks,
    size_t resumeSuspension,
    XL6_DiagList* diags)
{
    unsigned char* declared;
    size_t local;
    int resume = resumeSuspension != SIZE_MAX;
    for (local = 0; local < machine->suspensionCount; ++local)
        xl6OutputPrintf(out, "    bool await_ok_%u = false;\n",
            (unsigned int)module->asyncSuspensions[machine->firstSuspension + local].resultValue);
    declared = function->valueCount != 0
        ? (unsigned char*)xrtCalloc(function->valueCount, 1) : NULL;
    if ( function->valueCount != 0 && declared == NULL )
        return xl6IrCgenDiag(diags, module, function->declaration,
                             "XLIR async machine value-declaration allocation failed");
    for ( local = 0; local < machine->blockCount; ++local ) {
        const XL6_IrAsyncMachineBlock* block;
        size_t instructionIndex;
        if ( !blocks[local] ) continue;
        block = &module->asyncMachineBlocks[machine->firstBlock + local];
        for ( instructionIndex = block->firstInstruction;
              instructionIndex < block->firstInstruction + block->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction = &module->instructions[instructionIndex];
            size_t valueLocal;
            const char* type;
            if ( instruction->result == XL6_IR_VALUE_INVALID ||
                 xl6IrCgenIsControlCarrier(module, instruction) ||
                 instruction->result < function->firstValue ||
                 instruction->result - function->firstValue >= function->valueCount ) continue;
            valueLocal = instruction->result - function->firstValue;
            if ( declared[valueLocal] ) continue;
            type = xl6IrCgenType(module, instruction->type);
            if ( type == NULL ) {
                xrtFree(declared);
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR async machine value has no C ABI");
            }
            declared[valueLocal] = 1;
            xl6OutputPrintf(out, "    %s v%u%s;\n", type, (unsigned int)instruction->result,
                instruction->kind == XL6_IR_INSTR_AWAIT ? " = {0}" : "");
        }
    }
    if ( resume ) {
        const XL6_IrAsyncSuspension* suspension =
            &module->asyncSuspensions[resumeSuspension];
        const char* type = xl6IrCgenType(module, suspension->resultType);
        size_t action;
        size_t valueLocal = suspension->resultValue - function->firstValue;
        if ( type == NULL || valueLocal >= function->valueCount ) {
            xrtFree(declared);
            return xl6IrCgenDiag(diags, module, function->declaration,
                                 "XLIR async machine resume result has no C ABI");
        }
        if ( !declared[valueLocal] ) {
            declared[valueLocal] = 1;
            xl6OutputPrintf(out, "    %s v%u = {0};\n", type,
                            (unsigned int)suspension->resultValue);
        }
        for ( action = 0; action < suspension->resumeActionCount; ++action ) {
            const XL6_IrAsyncFrameAction* project =
                &module->asyncFrameActions[suspension->firstResumeAction + action];
            const XL6_IrInstruction* definition =
                xl6IrCgenValueDefinition(module, project->value, NULL);
            valueLocal = project->value - function->firstValue;
            type = definition != NULL ? xl6IrCgenType(module, definition->type) : NULL;
            if ( type == NULL || valueLocal >= function->valueCount ) {
                xrtFree(declared);
                return xl6IrCgenDiag(diags, module, function->declaration,
                                     "XLIR async machine projection has no value ABI");
            }
            if ( !declared[valueLocal] ) {
                declared[valueLocal] = 1;
                xl6OutputPrintf(out, "    %s v%u;\n", type, (unsigned int)project->value);
            }
        }
    }
    xrtFree(declared);
    return !xl6OutputHasFailed(out);
}
