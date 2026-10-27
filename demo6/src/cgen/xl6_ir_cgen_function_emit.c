/* The lifecycle ABI is void, while the verified finalizer program has a
 * diagnostic-independent result carrier. This fixed adapter owns only that
 * ABI crossing; cleanup ordering and failure control flow remain in XLIR. */
static int xl6IrCgenEmitModuleFiniBoundary(XL6_Output* out, const XL6_IrModule* module)
{
    if (!xl6IrModuleHasFinalizerWork(module)) return 1;
    xl6OutputPrintf(out,
        "static void xlir_module_fini(void)\n{\n"
        "    %s outcome = xlir_module_fini_body();\n"
        "    if (!outcome.ok) xl6ErrorScopePropagateFailure((xl6_error_result){ false, xrtTakeError() });\n"
        "}\n\n"
        "static void xlir_module_fini_after_failure(void)\n{\n"
        "    xl6_error_scope pending = {0}; xl6_error_result outcome;\n"
        "    if (!xl6ErrorScopeBeginCleanup(&pending, true)) abort();\n"
        "    xlir_module_fini();\n"
        "    outcome = xl6ErrorScopeEnd(&pending);\n"
        "    xl6ErrorScopePropagateFailure(outcome);\n"
        "}\n\n", xl6IrCgenErrorResultType(module, XL6_IR_TYPE_INVALID));
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitResourcePrepareStep(XL6_Output* out, const char* expression)
{
    xl6OutputPrintf(out,
        "    {\n"
        "        XL6_ModulePrepareResult step = %s;\n"
        "        if (step == XL6_MODULE_PREPARE_FAILED || xlir_preparation.failed || xrtGetError()) {\n"
        "            xl6ErrorScopeCaptureFailure(&xlir_preparation); prepared = XL6_MODULE_PREPARE_FAILED;\n"
        "        } else if (step == XL6_MODULE_PREPARE_BUSY) {\n"
        "            if (prepared == XL6_MODULE_PREPARE_READY) prepared = step;\n"
        "        } else if (step != XL6_MODULE_PREPARE_READY) {\n"
        "            xrtSetErrorKind(XERR_INTERNAL); xl6ErrorScopeCaptureFailure(&xlir_preparation);\n"
        "            prepared = XL6_MODULE_PREPARE_FAILED;\n"
        "        }\n"
        "    }\n", expression);
}

/* Every service receives a nonblocking close/probe before any service may
 * wait. The IR's closed ownership ABIs select these direct native calls;
 * neither source spelling nor generated-text inference participates. */
static void xl6IrCgenEmitResourcePreparePass(
    XL6_Output* out, const XL6_IrModule* module)
{
    if (module->abi.executionOwnershipAbi == XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1)
        xl6IrCgenEmitResourcePrepareStep(out,
            "teardown ? xl6ExecutionModulePrepareV1(xlir_execution_source) : XL6_MODULE_PREPARE_READY");
    if (module->abi.taskPoolOwnershipAbi == XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1)
        xl6IrCgenEmitResourcePrepareStep(out,
            "xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, stage != 0)");
    if (module->abi.executorOwnershipAbi == XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1)
        xl6IrCgenEmitResourcePrepareStep(out,
            "teardown && !xlir_continuation_quiesce(stage != 0) ? XL6_MODULE_PREPARE_BUSY : XL6_MODULE_PREPARE_READY");
}

static int xl6IrCgenEmitFunction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrFunction* function,
    const XL6_IrCgenOptions* options, XL6_DiagList* diags)
{
    size_t i;
    const XL6_IrClosureTarget* closure = (function->kind == XL6_IR_FUNCTION_MODULE_INIT ||
                                         function->kind == XL6_IR_FUNCTION_MODULE_FINI)
        ? NULL : xl6IrCgenClosureTargetGet(module, function->symbol);
    if ( function->kind == XL6_IR_FUNCTION_MODULE_INIT ) {
        xl6OutputPuts("static int xlir_module_initialized = 0;\n\n", out);
        if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
            xl6OutputPuts("static int xlir_module_initializing;\n"
                "static int xlir_module_init_body(void);\n"
                "static int xlir_module_init(void)\n"
                "{ int ok; if (xlir_module_initialized == 2) return 1;\n"
                "  if (xlir_module_initializing) return 0;\n"
                "  xlir_module_initializing = 1; ok = xlir_module_init_body();\n"
                "  xlir_module_initializing = 0; return ok; }\n", out);
        /* Preflight leaves graph-owned services to their exact adapters.
         * Blocking teardown drains them before the IR global finalizer;
         * retirement steps retain the nonblocking BUSY contract. */
        xl6OutputPuts("static inline XL6_ModulePrepareResult xlir_module_prepare_resources(bool teardown, bool wait)\n{\n"
            "    (void)teardown; (void)wait;\n", out);
        if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
            xl6OutputPuts("    if (xlir_module_initializing) return XL6_MODULE_PREPARE_BUSY;\n", out);
        xl6OutputPuts("    if (xlir_module_initialized == 0) return XL6_MODULE_PREPARE_READY;\n"
            "    if (xlir_module_initialized != 1 && xlir_module_initialized != 2) return XL6_MODULE_PREPARE_BUSY;\n", out);
        if (module->abi.executorOwnershipAbi != XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE ||
            module->abi.executionOwnershipAbi != XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE ||
            module->abi.taskPoolOwnershipAbi != XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE) {
            xl6OutputPuts("    XL6_ModulePrepareResult prepared = XL6_MODULE_PREPARE_READY;\n"
                "    xl6_error_scope xlir_preparation = {0};\n"
                "    if (!xl6ErrorScopeBegin(&xlir_preparation)) return XL6_MODULE_PREPARE_FAILED;\n",out);
            xl6OutputPuts("    for (int stage = 0; stage < (wait ? 2 : 1); ++stage) {\n"
                "        prepared = XL6_MODULE_PREPARE_READY;\n",out);
            xl6IrCgenEmitResourcePreparePass(out,module);
            xl6OutputPuts("        if (prepared != XL6_MODULE_PREPARE_BUSY || xlir_preparation.failed) break;\n",out);
            xl6OutputPuts("    }\n"
                "    xl6_error_result outcome = xl6ErrorScopeEnd(&xlir_preparation);\n"
                "    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return XL6_MODULE_PREPARE_FAILED; }\n"
                "    return prepared;\n",out);
        } else xl6OutputPuts("    return XL6_MODULE_PREPARE_READY;\n",out);
        xl6OutputPuts("}\n"
            "static inline XL6_ModulePrepareResult xlir_module_prepare_retirement(void)\n"
            "{ return xlir_module_prepare_resources(false, false); }\n", out);
        xl6OutputPuts(module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE
            ? "static int xlir_module_init_body(void)\n{\n"
            : "static int xlir_module_init(void)\n{\n", out);
        if (module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT)
            xl6OutputPuts("    xlir_code_owner_sealed = 1;\n", out);
        if ( module->abi.finalizerFunction != 0 )
            xl6OutputPuts("    if (xlir_module_initialized == 2) return 1;\n"
                           "    if (xlir_module_initialized != 0) return 0;\n", out);
        else xl6OutputPuts("    if (xlir_module_initialized) return 1;\n", out);
        if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
            xl6OutputPuts("    xlir_continuation_executor = xl6ContinuationExecutorCreateFixedV1(XLIR_CONTINUATION_CAPACITY, XLIR_CONTINUATION_WORKERS);\n"
                "    if (xlir_continuation_executor == NULL) return 0;\n", out);
        if(module->abi.deadlineOwnershipAbi==XL6_IR_MODULE_DEADLINE_OWNERSHIP_RESIDENT_BINDINGS_JOIN_V1)
            xl6OutputPuts("    xlir_deadline_source = xl6DeadlineServiceCreateV1(XLIR_DEADLINE_CAPACITY);\n"
                "    if (xlir_deadline_source == NULL) { xerror* pending = xrtTakeError();\n"
                "        if (!xlir_continuation_quiesce(true)) xlir_module_initialized = 1;\n"
                "        xrtSetErrorTake(pending); return 0; }\n",out);
        if (module->abi.taskPoolAbi != XL6_IR_MODULE_TASK_POOL_NONE) {
            xl6OutputPuts("    if (!xlir_async_pool_activate()) {\n", out);
            if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
                xl6OutputPuts("        xerror* pending = xrtTakeError(); if (!xlir_continuation_quiesce(true)) abort(); xrtSetErrorTake(pending);\n", out);
            xl6OutputPuts("        return 0; }\n", out);
        }
        if ( module->abi.finalizerFunction != 0 )
            xl6OutputPuts("    xlir_module_initialized = 1;\n", out);
        if(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1)
            xl6OutputPuts("    if (!xl6ExecutionModuleInitializeV1(&xlir_execution_source)) {\n"
                "        xlir_module_initialized = 1; return 0; }\n",out);
    } else {
        if ( !xl6IrCgenEmitPrototype(out, module, function, diags) ) return 0;
        xl6OutputPuts("\n{\n", out);
        if ( closure != NULL ) xl6OutputPuts("    (void)environment;\n", out);
        if ( function->kind == XL6_IR_FUNCTION_MODULE_FINI ) {
            xl6OutputPuts("    if (xlir_module_initialized != 1 && xlir_module_initialized != 2) ", out);
            if (function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE)
                xl6OutputPrintf(out, "return (%s){ true };\n",
                    xl6IrCgenErrorResultType(module, XL6_IR_TYPE_INVALID));
            else xl6OutputPuts("return;\n", out);
            if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE ||
                module->abi.taskPoolAbi != XL6_IR_MODULE_TASK_POOL_NONE ||
                module->abi.executionOwnershipAbi!=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE)
                xl6OutputPrintf(out, "    if (xlir_module_prepare_resources(true, true) != XL6_MODULE_PREPARE_READY) return (%s){ false };\n",
                    xl6IrCgenErrorResultType(module, XL6_IR_TYPE_INVALID));
            xl6OutputPuts("    xlir_module_initialized = 3;\n", out);
        }
    }
    xl6IrCgenDeclareUnwindScope(out, module, function);
    for ( i = 0; i < module->storageCount; ++i ) {
        const XL6_IrStorage* storage = &module->storages[i];
        /* The verified finalizer has no local storage. Its root declaration
         * identity must not duplicate the initializer's temporary arena. */
        if ( function->kind == XL6_IR_FUNCTION_MODULE_FINI ||
             function->kind == XL6_IR_FUNCTION_CLASS_FINI ) break;
        if ( (storage->kind == XL6_IR_STORAGE_LOCAL ||
              storage->kind == XL6_IR_STORAGE_TEMPORARY) &&
             storage->owner == function->declaration ) {
            XL6_IrTypeId physicalType = storage->cellType !=
                    XL6_IR_TYPE_INVALID
                ? storage->cellType : storage->type;
            const char* type = xl6IrCgenType(module, physicalType);
            if ( type == NULL ) return xl6IrCgenDiag(diags, module, storage->declaration, "XLIR C emitter does not support local storage ABI");
            if ( storage->cellType != XL6_IR_TYPE_INVALID ||
                 xl6IrTypeGet(&module->types, storage->type)->ownershipAbi ==
                     XL6_IR_OWNERSHIP_ABI_MANAGED ) {
                /* C zero initialization is valid for both owning aggregates
                 * and pointer carriers; NULL is not an aggregate value. */
                xl6OutputPrintf(out, "    %s s%u = {0};\n", type,
                    (unsigned int)storage->symbol);
            }
            else
                xl6OutputPrintf(out, "    %s s%u;\n", type, (unsigned int)storage->symbol);
        }
    }
    /* This is a mechanical use of the already-declared parameter ABI.  It
     * keeps generated C warning-clean even when a source function does not
     * reference one of its validated parameters. */
    for ( i = 0; i < function->parameterCount; ++i ) {
        XL6_SymbolId symbol = module->functionParameters[function->firstParameter + i];
        xl6OutputPrintf(out, "    (void)s%u;\n", (unsigned int)symbol);
    }
    for ( i = function->firstInstruction; i < function->firstInstruction + function->instructionCount; ++i ) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        if ( instruction->result != XL6_IR_VALUE_INVALID &&
             !xl6IrCgenIsControlCarrier(module, instruction) &&
             !xl6IrCgenResultStorageOmitted(module, instruction) ) {
            const char* type = xl6IrCgenType(module, instruction->type);
            if ( type == NULL ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter does not support a value ABI");
            xl6OutputPrintf(out, "    %s v%u;\n", type, (unsigned int)instruction->result);
        }
    }
    xl6OutputPrintf(out, "    goto b%u;\n", (unsigned int)function->entryBlock);
    for ( i = function->firstBlock; i < function->firstBlock + function->blockCount; ++i ) {
        const XL6_IrBlock* block = &module->blocks[i];
        size_t j;
        int lastDebugLine = 0;
        if ( block->instructionCount == 0 && (XL6_IrBlockId)(i + 1) != function->entryBlock &&
             !xl6IrCgenBlockReferenced(module, function, (XL6_IrBlockId)(i + 1)) ) continue;
        /* A label must precede a statement in C11. The first IR operation
         * may declare a call/error carrier, so use a null statement. */
        xl6OutputPrintf(out, "b%lu:;\n", (unsigned long)(i + 1));
        for ( j = block->firstInstruction; j < block->firstInstruction + block->instructionCount; ++j ) {
            const XL6_IrInstruction* instruction = &module->instructions[j];
            if ( options != NULL && options->debugMode &&
                 instruction->sourceSpan.line > 0 &&
                 instruction->sourceSpan.line != lastDebugLine ) {
                if ( !xl6IrCgenEmitDebugProbe(
                        out, function, instruction, options) ) return 0;
                lastDebugLine = instruction->sourceSpan.line;
            }
            if ( !xl6IrCgenEmitInstruction(
                    out, module, function, instruction, closure, j, diags) ) return 0;
        }
    }
    xl6OutputPuts("}\n\n", out);
    return !xl6OutputHasFailed(out);
}
