/* Native ownership callbacks share one cleanup boundary. The verified target
 * supplies linkage and effects; network wrappers must not silently replace a
 * possibly raising language destructor with a hard-coded runtime free. */
static int xl6IrCgenNativeDestructorAbi(const XL6_IrCallTarget* target)
{
    return target != NULL && target->kind == XL6_IR_CALL_TARGET_EXTERN_C &&
        target->cName != NULL && target->cName[0] != '\0' &&
        target->returnType == XL6_IR_TYPE_INVALID && target->parameterCount == 1 &&
        target->parameterAbis != NULL && target->parameterAbis[0] == XL6_IR_PARAMETER_ABI_BORROW &&
        ((target->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE && target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_NONE) ||
         (target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE && target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE));
}

static int xl6IrCgenEmitNativeDestructorCallback(XL6_Output* out,
    const XL6_IrCallTarget* target, const char* callback)
{
    if (!xl6IrCgenNativeDestructorAbi(target) || callback == NULL) return 0;
    xl6OutputPrintf(out,
        "void %s(void*);\n"
        "static void %s(void* handle, void* user)\n"
        "{\n"
        "    xl6_error_scope cleanup = {0}; xl6_error_result outcome; (void)user;\n"
        "    if (handle == NULL) return;\n"
        "    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort();\n"
        "    %s(handle);\n"
        "    outcome = xl6ErrorScopeEnd(&cleanup);\n"
        "    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);\n"
        "}\n", target->cName, callback, target->cName);
    return !xl6OutputHasFailed(out);
}

/* MOVE empties a verified physical owner slot without invoking destruction.
 * STORAGE_TAKE additionally produces the independent owner in one operation.
 * Sync, async and generator emission share the same linkage/parameter mapping;
 * a continuation supplies its already resolved frame field and activation bit. */
static int xl6IrCgenEmitMoveInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t frameField, size_t initBit,
    XL6_DiagList* diags)
{
    const XL6_IrStorage* storage = xl6IrStorageGet(module, instruction->targetSymbol);
    const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->type);
    const char* spelling = xl6IrCgenType(module, instruction->type);
    int take = instruction->kind == XL6_IR_INSTR_STORAGE_TAKE;
    if (instruction->operandCount != (take ? 0u : 1u) ||
        (take && instruction->result == XL6_IR_VALUE_INVALID) || type == NULL || spelling == NULL ||
        storage == NULL || storage->type != instruction->type ||
        storage->cellType != XL6_IR_TYPE_INVALID)
        return xl6IrCgenDiag(diags, module, instruction->origin,
            "XLIR move has no exact non-cell storage ABI");
    {
        char* place=xl6IrCgenStoragePlace(module,storage,frameField);
        char output[32],initialized[64]; if (!place) return 0;
        snprintf(output,sizeof(output),"v%u",(unsigned int)instruction->result);
        snprintf(initialized,sizeof(initialized),"frame->i%lu",(unsigned long)initBit);
        xl6OutputPuts("    {\n", out);
        xl6IrCgenSlotEmpty(out,spelling,"xlir_empty");
        xl6IrCgenSlotPlanBegin(out);
        if (take) xl6IrCgenSlotWrite(out,output,place);
        xl6IrCgenSlotReset(out,"xlir_empty",place,initBit==SIZE_MAX?NULL:initialized);
        xl6IrCgenSlotPlanEnd(out); xl6OutputPuts("    }\n",out); xrtFree(place);
    }
    return !xl6OutputHasFailed(out);
}

/* Mechanical rendering of RELEASE's detach-before-destroy contract. These
 * places are projections of verified storage/frame bindings, never AST text. */
static int xl6IrCgenEmitDetachedRelease(
    XL6_Output* out, const char* type, const char* destroy,
    const char* place, const char* initialized)
{
    if (out == NULL || type == NULL || destroy == NULL || place == NULL) return 0;
    if (initialized != NULL) xl6OutputPrintf(out, "    if (%s) ", initialized);
    else xl6OutputPuts("    ", out);
    xl6OutputPrintf(out, "{ %s xlir_released;\n", type);
    xl6IrCgenSlotEmpty(out,type,"xlir_empty");
    xl6IrCgenSlotPlanBegin(out); xl6IrCgenSlotWrite(out,"xlir_released",place);
    xl6IrCgenSlotReset(out,"xlir_empty",place,initialized); xl6IrCgenSlotPlanEnd(out);
    xl6OutputPrintf(out, "    %s(xlir_released); }\n", destroy);
    return !xl6OutputHasFailed(out);
}

/* Physical owner disposal shared by result transport and dispatch adapters. */
static int xl6IrCgenEmitOwnedPlaceRelease(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    const char* place, XL6_DiagList* diags)
{
    const XL6_IrType* owner = xl6IrTypeGet(&module->types, type);
    if (owner == NULL) return 0;
    if (owner->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL) return 1;
    if (owner->cAbi == XL6_IR_C_ABI_PRODUCT) {
        if (!xl6IrCgenProductDropNeeded(module, type))
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR owner has no reachable product drop ABI");
        xl6OutputPrintf(out, "    xlir_product_drop_t%u(&%s);\n", (unsigned int)type, place);
        return !xl6OutputHasFailed(out);
    }
    return xl6IrCgenEmitDetachedRelease(out, xl6IrCgenType(module, type),
        xl6IrCgenStorageReleaseFunction(owner), place, NULL);
}

static int xl6IrCgenEmitPreparationRollback(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrValueId* operands, size_t firstParameter, const char* outcome,
    XL6_DiagList* diags)
{
    size_t p;
    if (target->preparationFailureAbi != XL6_IR_CALL_PREPARATION_RETIRE_OWNED_REVERSE ||
        firstParameter > target->parameterCount)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR call preparation rollback has no verified retirement ABI");
    xl6OutputPuts("    { xl6_error_scope preparation_cleanup = {0};\n"
        /* Entry into CALL already consumed these owners. Latch failure even
         * without a diagnostic before any destructor can raise another one.
         * Rebinding an established language context is an invariant. */
        "      if (!xl6ErrorScopeBeginCleanup(&preparation_cleanup, true)) abort();\n", out);
    for (p = target->parameterCount; p > firstParameter;) {
        char place[40]; --p;
        if (target->parameterAbis[p] != XL6_IR_PARAMETER_ABI_OWNED) continue;
        if (operands != NULL) snprintf(place, sizeof(place), "v%u", (unsigned int)operands[p]);
        else snprintf(place, sizeof(place), "a%lu", (unsigned long)(p - firstParameter));
        if (!xl6IrCgenEmitOwnedPlaceRelease(out, module, target->parameterTypes[p], place, diags)) return 0;
        xl6OutputPuts("      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&preparation_cleanup);\n", out);
    }
    xl6OutputPrintf(out, "      %s = xl6ErrorScopeEnd(&preparation_cleanup); xrtClearError(); }\n", outcome);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitValueRelease(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    char place[48];
    if (instruction->operandCount != 1 || !xl6IrTypeHasStorageReleaseAbi(module, instruction->type))
        return xl6IrCgenDiag(diags, module, instruction->origin,
            "XLIR value release has no exact owned value ABI");
    snprintf(place, sizeof(place), "v%u", (unsigned int)module->operands[instruction->firstOperand]);
    return xl6IrCgenEmitOwnedPlaceRelease(out, module, instruction->type, place, diags);
}

/* Product fields carry the same verified types and owner effects in every
 * execution mode. Suspension changes residence, not product semantics. */
static int xl6IrCgenProductInstructionAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* product;
    const XL6_IrType* element;
    const XL6_IrInstruction* source;
    size_t i;
    if (instruction->kind == XL6_IR_INSTR_PRODUCT_MAKE) {
        product = xl6IrTypeGet(&module->types, instruction->type);
        if (product == NULL || product->kind != XL6_IR_TYPE_TUPLE ||
            product->cAbi != XL6_IR_C_ABI_PRODUCT || instruction->result == XL6_IR_VALUE_INVALID ||
            instruction->operandCount != product->paramCount) return 0;
        for (i = 0; i < product->paramCount; ++i) {
            source = xl6IrCgenValueDefinition(module, module->operands[instruction->firstOperand + i], NULL);
            if (source == NULL || source->type != product->params[i]) return 0;
        }
        return 1;
    }
    if (instruction->kind != XL6_IR_INSTR_PRODUCT_GET && instruction->kind != XL6_IR_INSTR_PRODUCT_TAKE &&
        instruction->kind != XL6_IR_INSTR_PRODUCT_DROP) return 0;
    source = instruction->operandCount == 1
        ? xl6IrCgenValueDefinition(module, module->operands[instruction->firstOperand], NULL) : NULL;
    product = source != NULL ? xl6IrTypeGet(&module->types, source->type) : NULL;
    element = xl6IrTypeGet(&module->types, instruction->type);
    if (product == NULL || product->kind != XL6_IR_TYPE_TUPLE || product->cAbi != XL6_IR_C_ABI_PRODUCT ||
        instruction->elementIndex >= product->paramCount || product->params[instruction->elementIndex] != instruction->type ||
        element == NULL || (instruction->kind == XL6_IR_INSTR_PRODUCT_DROP
            ? instruction->result != XL6_IR_VALUE_INVALID : instruction->result == XL6_IR_VALUE_INVALID)) return 0;
    if (instruction->kind == XL6_IR_INSTR_PRODUCT_GET)
        return element->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    return element->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        xl6IrCgenStorageReleaseFunction(element) != NULL;
}

static int xl6IrCgenEmitProductInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    size_t i;
    const XL6_IrType* element = xl6IrTypeGet(&module->types, instruction->type);
    if (!xl6IrCgenProductInstructionAbi(module, instruction))
        return xl6IrCgenDiag(diags, module, instruction->origin,
            "XLIR product instruction has no exact structural/ownership ABI");
    if (instruction->kind == XL6_IR_INSTR_PRODUCT_MAKE) {
        xl6OutputPrintf(out, "    v%u = (%s){ ", (unsigned)instruction->result,
            xl6IrCgenType(module, instruction->type));
        for (i = 0; i < instruction->operandCount; ++i)
            xl6OutputPrintf(out, "%sv%u", i ? ", " : "", (unsigned)module->operands[instruction->firstOperand + i]);
        xl6OutputPuts(" };\n", out);
    } else {
        char place[64];
        snprintf(place, sizeof(place), "v%u.e%lu", (unsigned)module->operands[instruction->firstOperand],
            (unsigned long)instruction->elementIndex);
        if (instruction->kind == XL6_IR_INSTR_PRODUCT_DROP)
            return xl6IrCgenEmitDetachedRelease(out, xl6IrCgenType(module, instruction->type),
                xl6IrCgenStorageReleaseFunction(xl6IrTypeGet(&module->types, instruction->type)), place, NULL);
        xl6OutputPrintf(out, "    v%u = %s;\n", (unsigned)instruction->result, place);
        if (instruction->kind == XL6_IR_INSTR_PRODUCT_TAKE)
            xl6OutputPrintf(out, "    %s = (%s){0};\n", place,
                xl6IrCgenType(module,instruction->type));
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitSymbolRelease(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_SymbolId symbol, int indirect)
{
    XL6_Output spelling;
    char* place;
    int ok;
    const char* destroy = xl6IrCgenStorageReleaseFunction(
        xl6IrTypeGet(&module->types, type));
    if (destroy == NULL || !xl6OutputOpenMemory(&spelling)) return 0;
    if (indirect) xl6OutputPuts("*", &spelling);
    xl6IrCgenPutStorageName(&spelling, module, symbol);
    place = xl6OutputTakeMemoryText(&spelling, NULL);
    xl6OutputDiscard(&spelling);
    if (place == NULL) return 0;
    ok = xl6IrCgenEmitDetachedRelease(out, xl6IrCgenType(module, type), destroy, place, NULL);
    xrtFree(place);
    return ok;
}

static const char* xl6IrCgenFrameDestroyFunction(XL6_IrContinuationFrameDestroyAbi abi)
{
    switch (abi) {
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING: return "xlir_string_drop";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE: return "xrtValueRelease";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE: return "xrtFutureDestroy";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE: return "xlir_closure_release";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL: return "xlir_cell_release";
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR: return "xrtErrorFree";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_VALUE_ITERATOR: return "xlir_xvalue_iterator_destroy";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_GENERATOR: return "xlir_generator_destroy";
    default: return NULL;
    }
}

/* The verifier owns type/ownership matching. Both continuation backends use
 * the same physical storage renderer and destructor vocabulary. */
static int xl6IrCgenFrameStorageSupported(
    const XL6_IrModule* module, const XL6_IrContinuationFrameSlot* frame,
    size_t initBitCount)
{
    if (frame == NULL || xl6IrCgenType(module, frame->type) == NULL ||
        frame->residence != XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE ||
        frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_NONE) return 0;
    if (frame->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE)
        return frame->initBit == SIZE_MAX;
    return frame->initBit < initBitCount &&
        xl6IrCgenFrameDestroyFunction(frame->destroyAbi) != NULL;
}

static int xl6IrCgenEmitFrameRelease(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_IrContinuationFrameDestroyAbi abi, size_t field, size_t initBit)
{
    char place[64], initialized[64];
    if (initBit == SIZE_MAX) return 0;
    snprintf(place, sizeof(place), "frame->f%lu", (unsigned long)field);
    snprintf(initialized, sizeof(initialized), "frame->i%lu", (unsigned long)initBit);
    return xl6IrCgenEmitDetachedRelease(out, xl6IrCgenType(module, type),
        xl6IrCgenFrameDestroyFunction(abi), place, initialized);
}
