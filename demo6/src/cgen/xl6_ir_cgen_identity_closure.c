static const XL6_IrFunction* xl6IrCgenInstructionOwner(
    const XL6_IrModule* module, size_t instructionIndex)
{
    size_t i;
    if ( module == NULL ) return NULL;
    for ( i = 0; i < module->functionCount; ++i ) {
        const XL6_IrFunction* function = &module->functions[i];
        if ( instructionIndex >= function->firstInstruction &&
             instructionIndex < function->firstInstruction + function->instructionCount ) return function;
    }
    return NULL;
}

static int xl6IrCgenInstructionIsReachable(
    const XL6_IrModule* module, size_t instructionIndex)
{
    const XL6_IrFunction* owner = xl6IrCgenInstructionOwner(module, instructionIndex);
    const XL6_IrCallTarget* target;
    const XL6_IrAsyncMachine* machine;
    size_t block;
    if ( owner == NULL ) return 0;
    if ( owner->reachability != XL6_IR_REACHABILITY_UNREACHABLE ) return 1;
    if ( owner->kind != XL6_IR_FUNCTION_ASYNC_WORKER ) return 0;
    target = xl6IrCallTargetGet(module, owner->symbol);
    machine = target != NULL &&
        target->reachability != XL6_IR_REACHABILITY_UNREACHABLE
        ? xl6IrCgenAsyncMachine(module, target) : NULL;
    if ( machine == NULL || machine->function >= module->functionCount ||
         &module->functions[machine->function] != owner ) return 0;
    /* Canonical machine programs are intentionally absent from the ordinary
     * function emission closure. Runtime dependency discovery follows the same
     * verified block slice as the machine renderer. */
    for ( block = 0; block < machine->blockCount; ++block ) {
        const XL6_IrAsyncMachineBlock* program =
            &module->asyncMachineBlocks[machine->firstBlock + block];
        if ( instructionIndex >= program->firstInstruction &&
             instructionIndex - program->firstInstruction < program->instructionCount )
            return 1;
    }
    return 0;
}

static int xl6IrCgenNominalIdentitySetUsed(
    const XL6_IrModule* module, const XL6_IrNominalIdentitySet* set)
{
    size_t instructionIndex;
    if ( module == NULL || set == NULL ) return 0;
    for ( instructionIndex = 0;
          instructionIndex < module->instructionCount; ++instructionIndex ) {
        const XL6_IrInstruction* instruction =
            &module->instructions[instructionIndex];
        if ( !xl6IrCgenInstructionIsReachable(module, instructionIndex) ||
             instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
             instruction->payloadType != set->targetType ) continue;
        if ( instruction->runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST ||
             instruction->runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED ||
             instruction->runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE ) return 1;
    }
    return 0;
}

/* Render the already verified identity arena literally.  This is deliberately
 * not a class/protocol algorithm: Cgen selects one stored scalar identity and
 * emits a closed switch over the stored members. */
static int xl6IrCgenEmitNominalIdentityRuntime(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t setIndex;
    int needsRuntimeTag = 0;
    if ( out == NULL || module == NULL ) return 0;
    for ( setIndex = 0; setIndex < module->nominalIdentitySetCount;
          ++setIndex ) {
        const XL6_IrNominalIdentitySet* set =
            &module->nominalIdentitySets[setIndex];
        if ( xl6IrCgenNominalIdentitySetUsed(module, set) &&
             (set->kind == XL6_IR_NOMINAL_IDENTITY_CLASS_RUNTIME_TAG ||
              set->kind ==
                  XL6_IR_NOMINAL_IDENTITY_PROTOCOL_RUNTIME_TAG) ) {
            needsRuntimeTag = 1;
            break;
        }
    }
    if ( needsRuntimeTag )
        xl6OutputPuts(
            "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
            "static uint64_t xlir_nominal_runtime_tag(const xvalue* source)\n"
            "{\n"
            "    xvalue* item; int64_t tag = 0;\n"
            "    if (source == NULL || xrtValueType(source) != 12) return 0;\n"
            "    item = xrtValueObjectGet(source, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    return item != NULL && xrtValueGetInt(item, &tag) ? (uint64_t)tag : 0;\n"
            "}\n",
            out);
    for ( setIndex = 0; setIndex < module->nominalIdentitySetCount;
          ++setIndex ) {
        const XL6_IrNominalIdentitySet* set =
            &module->nominalIdentitySets[setIndex];
        size_t identity;
        if ( !xl6IrCgenNominalIdentitySetUsed(module, set) ) continue;
        if ( set->kind != XL6_IR_NOMINAL_IDENTITY_CLASS_RUNTIME_TAG &&
             set->kind != XL6_IR_NOMINAL_IDENTITY_PROTOCOL_RUNTIME_TAG &&
             set->kind != XL6_IR_NOMINAL_IDENTITY_EXACT_TYPE_ID )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter nominal identity kind is unresolved");
        xl6OutputPrintf(out,
            "static bool xlir_nominal_set_%u_matches(const xvalue* source)\n"
            "{\n"
            "    uint64_t identity;\n"
            "    if (source == NULL || xrtValueType(source) == 0) return false;\n",
            (unsigned int)(setIndex + 1));
        /* Object-backed classes use runtime tags so derived classes match.
         * Inline/handle-backed class values have no object tag; their bound
         * exact TypeId is the nominal identity (Xid is one such value). */
        if ( set->kind == XL6_IR_NOMINAL_IDENTITY_CLASS_RUNTIME_TAG &&
             set->nonObjectTypeId != 0 )
            xl6OutputPrintf(out,
                "    if (xrtValueType(source) != XVALUE_OBJECT &&\n"
                "        xlir_xvalue_type_id(source) == 0x%016llXULL) return true;\n",
                (unsigned long long)set->nonObjectTypeId);
        xl6OutputPuts("    identity = ", out);
        if ( set->kind == XL6_IR_NOMINAL_IDENTITY_EXACT_TYPE_ID )
            xl6OutputPuts("xlir_xvalue_type_id(source);\n", out);
        else
            xl6OutputPuts(
                "xlir_nominal_runtime_tag(source);\n",
                out);
        xl6OutputPuts("    switch (identity) {\n", out);
        for ( identity = 0; identity < set->identityCount; ++identity )
            xl6OutputPrintf(out,
                "    case 0x%016llXULL: return true;\n",
                (unsigned long long)module->nominalIdentities[
                    set->firstIdentity + identity]);
        xl6OutputPuts(
            "    default: return false;\n"
            "    }\n"
            "}\n",
            out);
    }
    return 1;
}

static int xl6IrCgenEmitTaggedNullableXValueRuntime(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t typeIndex;
    xl6OutputPuts(
        "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
        "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
        "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
        "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
        "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n",
        out);
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
        XL6_IrTypeId typeId = (XL6_IrTypeId)(typeIndex + 1);
        const XL6_IrType* tagged = &module->types.items[typeIndex];
        const XL6_IrType* element = xl6IrCgenTaggedNullableElement(module, typeId);
        const char* elementType;
        const char* constructor;
        const char* getter;
        const char* zero;
        int constructorNeedsRetain;
        int needsBox = 0;
        int needsGet = 0;
        int needsIterator = 0;
        int needsCast = 0;
        size_t instructionIndex;
        if ( element == NULL ) continue;
        for ( instructionIndex = 0;
              instructionIndex < module->instructionCount; ++instructionIndex ) {
            const XL6_IrInstruction* instruction =
                &module->instructions[instructionIndex];
            const XL6_IrInstruction* operand;
            if ( !xl6IrCgenInstructionIsReachable(module, instructionIndex) ||
                 instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ) continue;
            operand = instruction->operandCount == 1
                ? xl6IrCgenValueDefinition(module,
                    module->operands[instruction->firstOperand], NULL) : NULL;
            if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX &&
                 operand != NULL && operand->type == typeId ) needsBox = 1;
            if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE &&
                 instruction->type == typeId ) needsGet = 1;
            if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY &&
                 instruction->type == typeId ) needsIterator = 1;
            if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED &&
                 instruction->type == typeId ) needsCast = 1;
        }
        if ( !needsBox && !needsGet && !needsIterator && !needsCast ) continue;
        elementType = xl6IrCgenType(module, tagged->params[0]);
        /* XValue's real accessor writes double, never float*. Narrow only
         * when packing the verified float32 payload in its result carrier. */
        if ( element->cAbi == XL6_IR_C_ABI_F32 ) elementType = "double";
        constructor = element->cAbi == XL6_IR_C_ABI_BOOL ? "xrtValueBool"
            : element->cAbi == XL6_IR_C_ABI_I64 ? "xrtValueInt"
            : element->cAbi == XL6_IR_C_ABI_U64 ? "xrtValueUInt"
            : element->cAbi == XL6_IR_C_ABI_TIME_I64 ? "xrtValueTime"
            : element->cAbi == XL6_IR_C_ABI_CHAR_U32 ? "xrtValueChar"
            : "xrtValueFloat";
        getter = element->cAbi == XL6_IR_C_ABI_BOOL ? "xrtValueGetBool"
            : element->cAbi == XL6_IR_C_ABI_I64 ? "xrtValueGetInt"
            : element->cAbi == XL6_IR_C_ABI_U64 ? "xrtValueGetUInt"
            : element->cAbi == XL6_IR_C_ABI_TIME_I64 ? "xrtValueGetTime"
            : element->cAbi == XL6_IR_C_ABI_CHAR_U32 ? "xrtValueGetChar"
            : "xrtValueGetFloat";
        zero = element->cAbi == XL6_IR_C_ABI_BOOL ? "false"
            : element->cAbi == XL6_IR_C_ABI_F64 ? "0.0" : "0";
        constructorNeedsRetain = element->cAbi == XL6_IR_C_ABI_BOOL;
        if ( elementType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR tagged nullable xvalue bridge has no scalar C ABI");
        if (needsGet || needsIterator || needsCast)
            xl6OutputPrintf(out,"extern bool %s(const xvalue*, %s*);\n",getter,elementType);
        if ( needsBox ) {
            xl6OutputPrintf(out,
                "static xvalue* xlir_xvalue_box_%s(%s value)\n"
                "{\n"
                "    if (!value.hasValue) return xrtValueRetain(xrtValueNull());\n"
                "    return %s%s(value.value)%s;\n"
                "}\n",
                tagged->cName, tagged->cName,
                constructorNeedsRetain ? "xrtValueRetain(" : "",
                constructor,
                constructorNeedsRetain ? ")" : "");
        }
        if ( needsGet ) {
            xl6OutputPrintf(out,
                "static %s xlir_xvalue_object_get_literal_%s(const xvalue* object, xlir_string_view key)\n"
                "{\n"
                "    xvalue* item = xrtValueObjectGet(object, key);\n"
                "    %s value = %s;\n"
                "    if (item == NULL) abort();\n"
                "    if (xrtValueType(item) == 0) return (%s){ false };\n"
                "    if (!%s(item, &value)) abort();\n"
                "    return (%s){ true, value };\n"
                "}\n",
                tagged->cName, tagged->cName, elementType, zero,
                tagged->cName, getter, tagged->cName);
        }
        if ( needsIterator ) {
            xl6OutputPrintf(out,
                "static %s xlir_xvalue_project_%s(const xvalue* item)\n"
                "{\n"
                "    %s value = %s;\n"
                "    if (item == NULL) abort();\n"
                "    if (xrtValueType(item) == 0) return (%s){ false };\n"
                "    if (!%s(item, &value)) abort();\n"
                "    return (%s){ true, value };\n"
                "}\n",
                tagged->cName, tagged->cName, elementType, zero,
                tagged->cName, getter, tagged->cName);
        }
        if ( needsCast ) {
            xl6OutputPrintf(out,
                "static %s xlir_xvalue_try_cast_%s(const xvalue* item, uint64_t exactTypeId)\n"
                "{\n"
                "    %s value = %s;\n"
                "    if (item == NULL || xrtValueType(item) == 0 || xlir_xvalue_type_id(item) != exactTypeId) return (%s){ false };\n"
                "    if (!%s(item, &value)) abort();\n"
                "    return (%s){ true, value };\n"
                "}\n",
                tagged->cName, tagged->cName, elementType, zero,
                tagged->cName, getter, tagged->cName);
        }
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenIsEntryStorage(const XL6_IrModule* module, const XL6_IrStorage* storage)
{
    const XL6_IrFunction* entry;
    if ( storage == NULL || storage->kind != XL6_IR_STORAGE_PARAMETER ||
         module->abi.entryKind != XL6_IR_MODULE_ENTRY_XLANG_MAIN ) return 0;
    entry = xl6IrCgenFindFunction(module, module->abi.entrySymbol);
    return entry != NULL && entry->parameterCount == 1 &&
           module->functionParameters[entry->firstParameter] == storage->symbol;
}

static int xl6IrCgenEmitExternGlobal(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrExternGlobal* imported, XL6_DiagList* diags)
{
    const char* type = imported != NULL ? xl6IrCgenType(module, imported->type) : NULL;
    if ( type == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                             "XLIR C emitter does not support extern global storage ABI");
    xl6OutputPrintf(out, "extern %s %s;\n", type, imported->cName);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitExternPrototype(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target, XL6_DiagList* diags)
{
    const char* returnType;
    size_t i;
    if ( target == NULL ||
         (target->kind != XL6_IR_CALL_TARGET_EXTERN_C &&
          target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY) || target->cName == NULL )
        return xl6IrCgenDiag(diags, module, target != NULL ? target->declaration : XL6_IR_NODE_INVALID,
                             "XLIR extern call target has no deterministic C linkage ABI");
    /* A runtime bridge is not an unresolved foreign symbol: the verified
     * component selected by runtimeBridge has already emitted its canonical
     * definition before this prototype phase.  Re-declaring it from the
     * language's normalized storage type (for example uint32 -> int64_t)
     * would invent a second C ABI and can conflict with the component's
     * fixed-width public signature. */
    if ( target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NONE ) return 1;
    {
        XL6_IrTypeId physicalResult =
            xl6IrCgenPhysicalResultType(module, target);
        returnType = target->returnType == XL6_IR_TYPE_INVALID ? "void"
            : target->resultCAbi == XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER ? "char*"
            : target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY
                ? "const char*" : xl6IrCgenType(module, physicalResult);
    }
    if ( returnType == NULL )
        return xl6IrCgenDiag(diags, module, target->declaration, "XLIR C emitter does not support extern return ABI");
    xl6OutputPrintf(out, "extern %s %s(", returnType, target->cName);
    if ( target->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( i = 0; i < target->parameterCount; ++i ) {
        const char* type;
        if ( i > 0 ) xl6OutputPuts(", ", out);
        if ( target->parameterPhysicalAbis[i] ==
                 XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK ) {
            xl6OutputPrintf(out, "xlir_raw_callback_t%u",
                (unsigned int)target->parameterTypes[i]);
            continue;
        }
        type = xl6IrCgenExternParameterType(module, target, i);
        if ( type == NULL )
            return xl6IrCgenDiag(diags, module, target->declaration, "XLIR C emitter does not support extern parameter ABI");
        xl6OutputPuts(type, out);
        if ( xl6IrCgenParameterAbiIsIndirect(target->parameterAbis[i]) )
            xl6OutputPuts("*", out);
    }
    if ( target->variadicAbi == XL6_IR_CALL_VARIADIC_C )
        xl6OutputPuts(", ...", out);
    xl6OutputPuts(");\n", out);
    return 1;
}

static int xl6IrCgenBlockReferenced(const XL6_IrModule* module, const XL6_IrFunction* function, XL6_IrBlockId blockId)
{
    size_t i;
    for ( i = function->firstInstruction; i < function->firstInstruction + function->instructionCount; ++i ) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        if ( instruction->trueTarget == blockId || instruction->falseTarget == blockId ) return 1;
    }
    return 0;
}

static const char* xl6IrCgenOperator(XL6_IrOpcode opcode)
{
    switch ( opcode ) {
    case XL6_IR_OPCODE_ADD: return "+";
    case XL6_IR_OPCODE_SUB: return "-";
    case XL6_IR_OPCODE_MUL: return "*";
    case XL6_IR_OPCODE_DIV: return "/";
    case XL6_IR_OPCODE_MOD: return "%";
    case XL6_IR_OPCODE_EQ: return "==";
    case XL6_IR_OPCODE_NE: return "!=";
    case XL6_IR_OPCODE_LT: return "<";
    case XL6_IR_OPCODE_LE: return "<=";
    case XL6_IR_OPCODE_GT: return ">";
    case XL6_IR_OPCODE_GE: return ">=";
    case XL6_IR_OPCODE_BIT_AND: return "&";
    case XL6_IR_OPCODE_BIT_OR: return "|";
    case XL6_IR_OPCODE_BIT_XOR: return "^";
    case XL6_IR_OPCODE_SHL: return "<<";
    case XL6_IR_OPCODE_SHR: return ">>";
    case XL6_IR_OPCODE_POS: return "+";
    case XL6_IR_OPCODE_NEG: return "-";
    case XL6_IR_OPCODE_NOT: return "!";
    case XL6_IR_OPCODE_BIT_NOT: return "~";
    default: return NULL;
    }
}

/* The language runtime's dynamic binary ABI is numbered independently from
 * C spelling.  Mapping the already-verified XLIR opcode here preserves the
 * boundary: this is an ABI table, never source-operator reconstruction. */
static int xl6IrCgenXValueBinaryMode(XL6_IrOpcode opcode)
{
    switch ( opcode ) {
    case XL6_IR_OPCODE_ADD: return 0;
    case XL6_IR_OPCODE_SUB: return 1;
    case XL6_IR_OPCODE_MUL: return 2;
    default: return -1;
    }
}

static const XL6_IrClosureTarget* xl6IrCgenClosureTargetGet(
    const XL6_IrModule* module, XL6_SymbolId symbol)
{
    size_t i;
    if ( module == NULL || symbol == 0 ) return NULL;
    for ( i = 0; i < module->closureTargetCount; ++i )
        if ( module->closureTargets[i].targetSymbol == symbol ) return &module->closureTargets[i];
    return NULL;
}

static int xl6IrCgenClosureTypesCompatible(
    const XL6_IrModule* module, XL6_IrTypeId left, XL6_IrTypeId right)
{
    const XL6_IrType* a = xl6IrTypeGet(&module->types, left);
    const XL6_IrType* b = xl6IrTypeGet(&module->types, right);
    size_t i;
    if ( a == NULL || b == NULL || a->kind != XL6_IR_TYPE_FUNCTION ||
         b->kind != XL6_IR_TYPE_FUNCTION || a->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         b->cAbi != XL6_IR_C_ABI_CLOSURE_PTR || a->paramCount != b->paramCount ||
         a->returnCount != b->returnCount ||
         a->callableResultType != b->callableResultType ||
         (a->paramCount != 0 &&
          (a->parameterModes == NULL || b->parameterModes == NULL ||
           a->parameterAbis == NULL || b->parameterAbis == NULL)) ) return 0;
    for ( i = 0; i < a->paramCount; ++i )
        if ( a->params[i] != b->params[i] ||
             a->parameterModes[i] != b->parameterModes[i] ||
             a->parameterAbis[i] != b->parameterAbis[i] ) return 0;
    for ( i = 0; i < a->returnCount; ++i ) if ( a->returns[i] != b->returns[i] ) return 0;
    return 1;
}

static const XL6_IrClosureCapture* xl6IrCgenCaptureGet(
    const XL6_IrModule* module, XL6_SymbolId storageSymbol, size_t* outIndex)
{
    size_t target, capture;
    if ( outIndex != NULL ) *outIndex = 0;
    if ( module == NULL || storageSymbol == 0 ) return NULL;
    for ( target = 0; target < module->closureTargetCount; ++target ) {
        const XL6_IrClosureTarget* closure = &module->closureTargets[target];
        for ( capture = 0; capture < closure->captureCount; ++capture ) {
            const XL6_IrClosureCapture* item = &module->closureCaptures[closure->firstCapture + capture];
            if ( item->storageSymbol == storageSymbol ) {
                if ( outIndex != NULL ) *outIndex = capture;
                return item;
            }
        }
    }
    return NULL;
}

/* Cell operations have one payload/ownership spelling in all execution
 * modes. The caller supplies only a verified frame binding; ordinary local
 * and closure-environment bindings come from the storage/capture tables. */
static int xl6IrCgenEmitCellInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, const XL6_IrClosureTarget* closure,
    size_t frameSlot, size_t initBit, XL6_DiagList* diags)
{
    const XL6_IrStorage* storage = xl6IrStorageGet(module, instruction->targetSymbol);
    const XL6_IrType* element = storage != NULL
        ? xl6IrTypeGet(&module->types, storage->type) : NULL;
    const char* kind = xl6IrCgenCellKind(element);
    const char* payloadType = storage != NULL ? xl6IrCgenType(module, storage->type) : NULL;
    char place[128], initialized[64];
    if (storage == NULL || storage->cellType == XL6_IR_TYPE_INVALID || kind == NULL || payloadType == NULL ||
        module->abi.cellStorageAbi != XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1)
        return xl6IrCgenDiag(diags, module, instruction->origin,
            "XLIR C emitter cell element ABI is unresolved");
    if (frameSlot != SIZE_MAX) {
        snprintf(place, sizeof(place), "frame->f%lu", (unsigned long)frameSlot);
    } else if (storage->kind == XL6_IR_STORAGE_CAPTURE) {
        size_t captureIndex;
        const XL6_IrClosureCapture* capture = xl6IrCgenCaptureGet(module, storage->symbol, &captureIndex);
        if (closure == NULL || capture == NULL || capture->abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL ||
            instruction->kind == XL6_IR_INSTR_CELL_NEW || instruction->kind == XL6_IR_INSTR_CELL_INIT ||
            instruction->kind == XL6_IR_INSTR_CELL_RELEASE)
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR C emitter cannot replace or release a borrowed cell capture");
        snprintf(place, sizeof(place), "((xlir_closure_env_%u*)environment)->c%lu",
            (unsigned int)closure->targetSymbol, (unsigned long)captureIndex);
    } else snprintf(place, sizeof(place), "s%u", (unsigned int)storage->symbol);
    if (initBit != SIZE_MAX) snprintf(initialized, sizeof(initialized), "frame->i%lu", (unsigned long)initBit);
    switch (instruction->kind) {
    case XL6_IR_INSTR_CELL_NEW:
        xl6OutputPrintf(out, "    v%u = (xl6CellRuntimeV1Get()->Create)(%s, sizeof(%s));\n",
            (unsigned int)instruction->result, kind, payloadType);
        break;
    case XL6_IR_INSTR_CELL_INIT:
        xl6OutputPrintf(out, "    (xl6CellRuntimeV1Get()->Initialize)(v%u, &v%u, sizeof(%s));\n",
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1], payloadType);
        {
            char input[32]; snprintf(input,sizeof(input),"v%u",(unsigned int)module->operands[instruction->firstOperand]);
            if (!xl6IrCgenEmitSlotCommit(out,"xlir_cell*",place,input,initBit==SIZE_MAX?NULL:initialized)) return 0;
        }
        break;
    case XL6_IR_INSTR_CELL_LOAD:
        xl6OutputPrintf(out, "    (xl6CellRuntimeV1Get()->Load)(%s, &v%u, sizeof(%s));\n",
            place, (unsigned int)instruction->result, payloadType);
        break;
    case XL6_IR_INSTR_CELL_STORE:
    case XL6_IR_INSTR_CELL_EXCHANGE:
        if (instruction->kind == XL6_IR_INSTR_CELL_EXCHANGE)
            xl6OutputPrintf(out, "    (xl6CellRuntimeV1Get()->Exchange)(%s, &v%u, &v%u, sizeof(%s));\n",
                place, (unsigned int)module->operands[instruction->firstOperand], (unsigned int)instruction->result, payloadType);
        else xl6OutputPrintf(out, "    (xl6CellRuntimeV1Get()->Store)(%s, &v%u, sizeof(%s));\n",
            place, (unsigned int)module->operands[instruction->firstOperand], payloadType);
        break;
    case XL6_IR_INSTR_CELL_RETAIN:
        xl6OutputPrintf(out, "    v%u = xlir_cell_retain(%s);\n", (unsigned int)instruction->result, place);
        break;
    case XL6_IR_INSTR_CELL_RELEASE:
        return xl6IrCgenEmitDetachedRelease(out, "xlir_cell*", "xlir_cell_release", place,
            initBit != SIZE_MAX ? initialized : NULL);
    default: return 0;
    }
    return !xl6OutputHasFailed(out);
}

/* Closure construction and invocation are instruction semantics, not a
 * property of the surrounding synchronous or async renderer.  Keep their C
 * spelling in shared helpers so a verified machine program consumes exactly
 * the same capture/call ABI as an ordinary XLIR function. */
static int xl6IrCgenEmitClosureMakeInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    const XL6_IrClosureTarget* target = instruction != NULL
        ? xl6IrCgenClosureTargetGet(module, instruction->targetSymbol) : NULL;
    const XL6_IrType* signature = target != NULL
        ? xl6IrTypeGet(&module->types, target->signatureType) : NULL;
    size_t capture;
    int needsCopies = 0;
    int conditionCount = 0;
    if ( instruction == NULL || xl6IrCgenType(module, instruction->type) == NULL ||
         target == NULL || signature == NULL ||
         signature->kind != XL6_IR_TYPE_FUNCTION ||
         signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         target->callParameterCount != signature->paramCount ||
         (target->callParameterCount != 0 &&
          (target->callParameterNames == NULL ||
           target->callParameterDefaultAbis == NULL ||
           target->callParameterDefaultLiterals == NULL)) ||
         instruction->operandCount != target->captureCount ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         target->firstCapture > module->closureCaptureCount ||
         target->captureCount > module->closureCaptureCount - target->firstCapture )
        return xl6IrCgenDiag(diags, module,
            instruction != NULL ? instruction->origin : XL6_IR_NODE_INVALID,
            "XLIR C emitter closure construction ABI is unresolved");
    if ( target->captureCount == 0 ) {
        xl6OutputPrintf(out,
            "    v%u = xlir_closure_new((void (*)(void))xlir_closure_invoke_%u, ",
            (unsigned int)instruction->result,
            (unsigned int)instruction->targetSymbol);
        if ( xl6IrClosureRawCallbackAbiHasEntry(target->rawCallbackAbi) )
            xl6OutputPrintf(out, "(void (*)(void))xlir_raw_callback_entry_%u, ",
                (unsigned int)instruction->targetSymbol);
        else
            xl6OutputPuts("NULL, ", out);
        xl6OutputPuts("NULL, NULL, ", out);
        if ( target->callParameterCount != 0 )
            xl6OutputPrintf(out, "xlir_closure_call_names_%u",
                            (unsigned int)target->targetSymbol);
        else
            xl6OutputPuts("NULL", out);
        if ( target->callParameterCount != 0 )
            xl6OutputPrintf(out, ", xlir_closure_call_defaults_%u",
                            (unsigned int)target->targetSymbol);
        else
            xl6OutputPuts(", NULL", out);
        xl6OutputPrintf(out, ", %lu);\n",
                        (unsigned long)target->callParameterCount);
        return !xl6OutputHasFailed(out);
    }
    xl6OutputPrintf(out,
        "    xlir_closure_env_%u* e%lu = (xlir_closure_env_%u*)xrtMalloc(sizeof(*e%lu));\n"
        "    if (e%lu != NULL) {\n",
        (unsigned int)target->targetSymbol, (unsigned long)instructionIndex,
        (unsigned int)target->targetSymbol, (unsigned long)instructionIndex,
        (unsigned long)instructionIndex);
    for ( capture = 0; capture < target->captureCount; ++capture ) {
        const XL6_IrClosureCapture* item =
            &module->closureCaptures[target->firstCapture + capture];
        XL6_IrValueId value = module->operands[instruction->firstOperand + capture];
        if ( item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING ) {
            needsCopies = 1;
            xl6OutputPrintf(out, "        e%lu->c%lu = xlir_string_copy(v%u);\n",
                (unsigned long)instructionIndex, (unsigned long)capture,
                (unsigned int)value);
        } else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_STRING ) {
            needsCopies = 1;
            xl6OutputPrintf(out, "        e%lu->c%lu = xlir_string_copy(v%u);\n",
                (unsigned long)instructionIndex, (unsigned long)capture,
                (unsigned int)value);
        } else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE ) {
            needsCopies = 1;
            xl6OutputPrintf(out,
                "        e%lu->c%lu = v%u != NULL ? xrtValueRetain(v%u) : NULL;\n",
                (unsigned long)instructionIndex, (unsigned long)capture,
                (unsigned int)value, (unsigned int)value);
        } else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE ) {
            needsCopies = 1;
            xl6OutputPrintf(out,
                "        e%lu->c%lu = v%u != NULL ? xrtFutureRef(v%u) : NULL;\n",
                (unsigned long)instructionIndex, (unsigned long)capture,
                (unsigned int)value, (unsigned int)value);
        } else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE ) {
            needsCopies = 1;
            xl6OutputPrintf(out, "        e%lu->c%lu = xlir_closure_retain(v%u);\n",
                (unsigned long)instructionIndex, (unsigned long)capture,
                (unsigned int)value);
        } else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL ||
                    item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_VALUE ) {
            xl6OutputPrintf(out, "        e%lu->c%lu = v%u;\n",
                (unsigned long)instructionIndex, (unsigned long)capture,
                (unsigned int)value);
        } else {
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR C emitter closure capture ownership ABI is unresolved");
        }
    }
    if ( needsCopies ) {
        xl6OutputPuts("        if (", out);
        for ( capture = 0; capture < target->captureCount; ++capture ) {
            const XL6_IrClosureCapture* item =
                &module->closureCaptures[target->firstCapture + capture];
            XL6_IrValueId value = module->operands[instruction->firstOperand + capture];
            if ( item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_STRING )
                xl6OutputPrintf(out, "%se%lu->c%lu.Data == NULL",
                    conditionCount++ ? " || " : "", (unsigned long)instructionIndex,
                    (unsigned long)capture);
            else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING )
                xl6OutputPrintf(out, "%s(v%u.Data != NULL && e%lu->c%lu.Data == NULL)",
                    conditionCount++ ? " || " : "", (unsigned int)value,
                    (unsigned long)instructionIndex, (unsigned long)capture);
            else if ( item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE ||
                      item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE ||
                      item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE )
                xl6OutputPrintf(out, "%s(v%u != NULL && e%lu->c%lu == NULL)",
                    conditionCount++ ? " || " : "", (unsigned int)value,
                    (unsigned long)instructionIndex, (unsigned long)capture);
        }
        xl6OutputPrintf(out,
            ") { xlir_closure_env_drop(&xlir_closure_layout_%u, e%lu); v%u = NULL; } else {\n",
            (unsigned int)target->targetSymbol, (unsigned long)instructionIndex,
            (unsigned int)instruction->result);
    }
    xl6OutputPrintf(out,
        "        v%u = xlir_closure_new((void (*)(void))xlir_closure_invoke_%u, NULL, e%lu, &xlir_closure_layout_%u, ",
        (unsigned int)instruction->result, (unsigned int)target->targetSymbol,
        (unsigned long)instructionIndex, (unsigned int)target->targetSymbol);
    if ( target->callParameterCount != 0 )
        xl6OutputPrintf(out, "xlir_closure_call_names_%u",
                        (unsigned int)target->targetSymbol);
    else
        xl6OutputPuts("NULL", out);
    if ( target->callParameterCount != 0 )
        xl6OutputPrintf(out, ", xlir_closure_call_defaults_%u",
                        (unsigned int)target->targetSymbol);
    else
        xl6OutputPuts(", NULL", out);
    xl6OutputPrintf(out,
        ", %lu);\n"
        "        if (v%u == NULL) xlir_closure_env_drop(&xlir_closure_layout_%u, e%lu);\n",
        (unsigned long)target->callParameterCount,
        (unsigned int)instruction->result, (unsigned int)target->targetSymbol,
        (unsigned long)instructionIndex);
    if ( needsCopies ) xl6OutputPuts("        }\n", out);
    xl6OutputPrintf(out, "    } else v%u = NULL;\n",
                    (unsigned int)instruction->result);
    if ( target->captureCount != 0 ) {
        int hasCellTransfer = 0;
        for ( capture = 0; capture < target->captureCount; ++capture )
            if ( module->closureCaptures[target->firstCapture + capture].abi ==
                 XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL ) hasCellTransfer = 1;
        if ( hasCellTransfer ) {
            xl6OutputPrintf(out, "    if (e%lu == NULL) {",
                            (unsigned long)instructionIndex);
            for ( capture = 0; capture < target->captureCount; ++capture )
                if ( module->closureCaptures[target->firstCapture + capture].abi ==
                     XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL )
                    xl6OutputPrintf(out, " xlir_cell_release(v%u);",
                        (unsigned int)module->operands[
                            instruction->firstOperand + capture]);
            xl6OutputPuts(" }\n", out);
        }
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenClosureCallAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* signature = instruction != NULL
        ? xl6IrTypeGet(&module->types, instruction->calleeType) : NULL;
    return instruction != NULL &&
        instruction->kind == XL6_IR_INSTR_CLOSURE_CALL &&
        signature != NULL && signature->kind == XL6_IR_TYPE_FUNCTION &&
        signature->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
        signature->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        signature->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
        (signature->paramCount == 0 ||
         (signature->parameterModes != NULL &&
          signature->parameterAbis != NULL)) &&
        instruction->targetSymbol == 0 &&
        instruction->operandCount == signature->paramCount + 1 &&
        instruction->result != XL6_IR_VALUE_INVALID &&
        ((signature->callableResultType == XL6_IR_TYPE_INVALID &&
          instruction->type == XL6_IR_TYPE_INVALID) ||
         (signature->callableResultType != XL6_IR_TYPE_INVALID &&
          instruction->type == signature->callableResultType &&
          xl6IrCgenType(module, instruction->type) != NULL));
}

static int xl6IrCgenEmitClosureCallInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    const XL6_IrType* signature = instruction != NULL
        ? xl6IrTypeGet(&module->types, instruction->calleeType) : NULL;
    const char* resultType = NULL;
    size_t argument;
    if ( !xl6IrCgenClosureCallAbi(module, instruction) )
        return xl6IrCgenDiag(diags, module,
            instruction != NULL ? instruction->origin : XL6_IR_NODE_INVALID,
            "XLIR C emitter closure invoke ABI is unresolved");
    if ( signature->callableResultType != XL6_IR_TYPE_INVALID ) {
        resultType = xl6IrCgenType(module, signature->callableResultType);
        if ( resultType == NULL )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR C emitter closure result ABI is unresolved");
    }
    xl6OutputPrintf(out, "    XL6_ClosureScope closure_scope_%lu = {0};\n"
        "    xlir_closure_enter(&closure_scope_%lu, v%u);\n",
        (unsigned long)instructionIndex, (unsigned long)instructionIndex,
        (unsigned int)module->operands[instruction->firstOperand]);
    xl6OutputPrintf(out, "    bool c%lu = ((bool (*)(void*",
                    (unsigned long)instructionIndex);
    if ( resultType != NULL ) xl6OutputPrintf(out, ", %s*", resultType);
    for ( argument = 0; argument < signature->paramCount; ++argument ) {
        const char* type = xl6IrCgenType(module, signature->params[argument]);
        if ( type == NULL )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR C emitter closure parameter ABI is unresolved");
        xl6OutputPrintf(out, ", %s%s", type,
            xl6IrCgenParameterAbiIsIndirect(signature->parameterAbis[argument])
                ? "*" : "");
    }
    xl6OutputPrintf(out, "))v%u->invoke)(v%u->environment",
        (unsigned int)module->operands[instruction->firstOperand],
        (unsigned int)module->operands[instruction->firstOperand]);
    if ( resultType != NULL )
        xl6OutputPrintf(out, ", &v%u", (unsigned int)instruction->result);
    for ( argument = 0; argument < signature->paramCount; ++argument ) {
        XL6_IrValueId value = module->operands[
            instruction->firstOperand + argument + 1];
        if ( xl6IrCgenParameterAbiIsIndirect(signature->parameterAbis[argument]) ) {
            const char* type = xl6IrCgenType(module, signature->params[argument]);
            if ( type == NULL ) return xl6IrCgenDiag(
                diags, module, instruction->origin,
                "XLIR C emitter mutable closure parameter ABI is unresolved");
            xl6OutputPrintf(out, ", (%s*)v%u", type, (unsigned int)value);
        } else {
            xl6OutputPrintf(out, ", v%u", (unsigned int)value);
        }
    }
    xl6OutputPrintf(out, ");\n    xlir_closure_leave(&closure_scope_%lu);\n", (unsigned long)instructionIndex);
    return !xl6OutputHasFailed(out);
}
