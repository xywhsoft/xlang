static const XL6_IrInstruction* xl6IrCgenValueDefinition(
    const XL6_IrModule* module, XL6_IrValueId value, size_t* index);
static int xl6IrCgenRuntimeClassIdentityBindAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
static int xl6IrCgenRuntimeClassConstructionCommitAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);

static int xl6IrCgenDiag(XL6_DiagList* diags, const XL6_IrModule* module, XL6_IrNodeId origin, const char* message)
{
    const XL6_IrNode* node = xl6IrNodeGet(module, origin);
    if ( diags != NULL ) xl6DiagAdd(diags, node != NULL ? node->span.line : 0,
                                    node != NULL ? node->span.column : 0, message);
    return 0;
}

static int xl6IrCgenCallParameterDiag(
    XL6_DiagList* diags, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    size_t parameter, const char* reason)
{
    const XL6_IrNode* node = target != NULL ? xl6IrNodeGet(module, target->declaration) : NULL;
    const XL6_IrType* type = target != NULL && parameter < target->parameterCount
        ? xl6IrTypeGet(&module->types, target->parameterTypes[parameter]) : NULL;
    char message[256];
    snprintf(message, sizeof(message),
             "XLIR C emitter call target %u parameter %lu (t%u/cabi=%d) %s",
             target != NULL ? (unsigned int)target->symbol : 0u,
             (unsigned long)parameter,
             target != NULL && parameter < target->parameterCount
                 ? (unsigned int)target->parameterTypes[parameter] : 0u,
             type != NULL ? (int)type->cAbi : (int)XL6_IR_C_ABI_UNSUPPORTED,
             reason != NULL ? reason : "has no deterministic C ABI");
    if ( diags != NULL ) xl6DiagAdd(diags, node != NULL ? node->span.line : 0,
                                    node != NULL ? node->span.column : 0, message);
    return 0;
}

static const char* xl6IrCgenType(const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* item = xl6IrTypeGet(&module->types, type);
    if ( item == NULL ) return NULL;
    switch ( item->cAbi ) {
    case XL6_IR_C_ABI_VOID: return "void";
    case XL6_IR_C_ABI_BOOL: return "bool";
    case XL6_IR_C_ABI_I64: return "int64_t";
    case XL6_IR_C_ABI_TIME_I64: return "int64_t";
    case XL6_IR_C_ABI_U64: return "uint64_t";
    case XL6_IR_C_ABI_TYPE_ID: return "uint64_t";
    case XL6_IR_C_ABI_F64: return "double";
    case XL6_IR_C_ABI_F32: return "float";
    case XL6_IR_C_ABI_CHAR_U32: return "uint32_t";
    case XL6_IR_C_ABI_C_CHAR: return "char";
    case XL6_IR_C_ABI_C_SCHAR: return "signed char";
    case XL6_IR_C_ABI_C_UCHAR: return "unsigned char";
    case XL6_IR_C_ABI_C_SHORT: return "short";
    case XL6_IR_C_ABI_C_USHORT: return "unsigned short";
    case XL6_IR_C_ABI_C_INT: return "int";
    case XL6_IR_C_ABI_C_UINT: return "unsigned int";
    case XL6_IR_C_ABI_C_LONG: return "long";
    case XL6_IR_C_ABI_C_ULONG: return "unsigned long";
    case XL6_IR_C_ABI_C_LONGLONG: return "long long";
    case XL6_IR_C_ABI_C_ULONGLONG: return "unsigned long long";
    case XL6_IR_C_ABI_C_SIZE: return "size_t";
    case XL6_IR_C_ABI_C_SSIZE: return "intptr_t";
    case XL6_IR_C_ABI_C_PTRDIFF: return "ptrdiff_t";
    case XL6_IR_C_ABI_C_INTPTR: return "intptr_t";
    case XL6_IR_C_ABI_C_UINTPTR: return "uintptr_t";
    case XL6_IR_C_ABI_C_BOOL: return "bool";
    case XL6_IR_C_ABI_C_FLOAT: return "float";
    case XL6_IR_C_ABI_C_DOUBLE: return "double";
    case XL6_IR_C_ABI_C_WCHAR: return "wchar_t";
    case XL6_IR_C_ABI_MANAGED_STRING: return "xlir_string";
    case XL6_IR_C_ABI_MANAGED_BYTES: return "xvalue*";
    case XL6_IR_C_ABI_NATIVE_VALUE: {
        const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(item);
        return plan ? plan->cName : NULL;
    }
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: return "xvalue*";
    case XL6_IR_C_ABI_NATIVE_HANDLE: return "void*";
    case XL6_IR_C_ABI_RAW_POINTER: return "void*";
    case XL6_IR_C_ABI_RUNTIME_ERROR_PTR: return "xerror*";
    case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR: return "xfuture*";
    case XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR: return "xlir_generator*";
    case XL6_IR_C_ABI_CLOSURE_PTR: return "xlir_closure*";
    case XL6_IR_C_ABI_PRODUCT: return item->cName[0] != '\0' ? item->cName : NULL;
    case XL6_IR_C_ABI_RUNTIME_CELL_PTR: return "xlir_cell*";
    case XL6_IR_C_ABI_TAGGED_NULLABLE: return item->cName[0] != '\0' ? item->cName : NULL;
    case XL6_IR_C_ABI_UNION: return item->cName[0] != '\0' ? item->cName : NULL;
    case XL6_IR_C_ABI_STORAGE_REF_PTR: return "void*";
    case XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR:
        return "xlir_xvalue_iterator*";
    default: return NULL;
    }
}

static const char* xl6IrCgenExportValueType(
    const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_IrExportValueAbi abi)
{
    switch ( abi ) {
    case XL6_IR_EXPORT_VALUE_TYPE_REPRESENTATION:
        return xl6IrCgenType(module, type);
    case XL6_IR_EXPORT_VALUE_I8: return "int8_t";
    case XL6_IR_EXPORT_VALUE_U8: return "uint8_t";
    case XL6_IR_EXPORT_VALUE_I16: return "int16_t";
    case XL6_IR_EXPORT_VALUE_U16: return "uint16_t";
    case XL6_IR_EXPORT_VALUE_I32: return "int32_t";
    case XL6_IR_EXPORT_VALUE_U32: return "uint32_t";
    default: return NULL;
    }
}

static int xl6IrCgenInstructionIsReachable(
    const XL6_IrModule* module, size_t instructionIndex);
static void xl6IrCgenEmitCStringLiteral(XL6_Output* out, const char* text);
static int xl6IrCgenEmitPrototype(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrFunction* function, XL6_DiagList* diags);

static int xl6IrCgenLanguageFunctionValueSignatureAbi(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    return xl6IrLanguageFunctionValueSignatureSupported(module, type);
}

static const XL6_IrClassLayout* xl6IrCgenClassLayout(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    size_t i;
    if ( module == NULL || type == XL6_IR_TYPE_INVALID ) return NULL;
    for ( i = 0; i < module->classLayoutCount; ++i )
        if ( module->classLayouts[i].type == type )
            return &module->classLayouts[i];
    return NULL;
}

static int xl6IrCgenPersistenceTypeSupportedDepth(
    const XL6_IrModule* module, XL6_IrTypeId type,
    uint32 format, size_t depth, int needsClassConstruction);

static int xl6IrCgenPersistenceTypeSupportedDepth(
    const XL6_IrModule* module, XL6_IrTypeId typeId,
    uint32 format, size_t depth, int needsClassConstruction)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, typeId);
    if ( type == NULL ) return 0;
    switch ( type->persistenceValueAbi ) {
    case XL6_IR_PERSISTENCE_VALUE_XRT_CLONE:
        return 1;
    case XL6_IR_PERSISTENCE_VALUE_CLASS_SCHEMA: {
        const XL6_IrClassLayout* layout =
            xl6IrCgenClassLayout(module, typeId);
        return layout != NULL &&
               (!needsClassConstruction ||
                layout->persistenceConstructionAbi ==
                    XL6_IR_PERSISTENCE_CONSTRUCTION_DECLARATION_DEFAULT) &&
               xl6IrPersistenceRestoreSchemaGet(module, typeId, format) != NULL;
    }
    case XL6_IR_PERSISTENCE_VALUE_NULLABLE:
    case XL6_IR_PERSISTENCE_VALUE_ARRAY:
    case XL6_IR_PERSISTENCE_VALUE_LIST_SEQUENCE:
    case XL6_IR_PERSISTENCE_VALUE_SET_SEQUENCE:
    case XL6_IR_PERSISTENCE_VALUE_DICT_OBJECT:
        return type->persistencePayloadType != XL6_IR_TYPE_INVALID &&
               xl6IrCgenPersistenceTypeSupportedDepth(
                   module, type->persistencePayloadType, format,
                   depth + 1, needsClassConstruction);
    default:
        return 0;
    }
}

static int xl6IrCgenPersistenceLayoutSupported(
    const XL6_IrModule* module, XL6_IrTypeId type, uint32 format)
{
    return xl6IrPersistenceRestoreSchemaGet(module, type, format) != NULL;
}


static uint32 xl6IrCgenPersistenceRuntimeFormat(XL6_IrRuntimeOp operation)
{
    const XL6_IrPersistenceInvocationContract* configured = xl6IrPersistenceInvocationRuntime(operation);
    if (configured != NULL) return configured->projectionFormat;
    switch ( operation ) {
    case XL6_IR_RUNTIME_PUBLIC_DATA_PROJECT: return XL6_IR_PERSIST_FORMAT_PUBLIC;
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_READ:
        return XL6_IR_PERSIST_FORMAT_JSON;
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_READ:
        return XL6_IR_PERSIST_FORMAT_XLON;
    default:
        return XL6_IR_PERSIST_FORMAT_NONE;
    }
}

static int xl6IrCgenPersistenceStringifyAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrInstruction* pretty;
    const XL6_IrType* sourceType;
    const XL6_IrType* prettyType;
    const XL6_IrType* resultType;
    uint32 format;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         (instruction->runtimeOp !=
              XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY &&
          instruction->runtimeOp !=
              XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY) ||
         instruction->operandCount != 2 ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
          instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    format = xl6IrCgenPersistenceRuntimeFormat(instruction->runtimeOp);
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    pretty = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    prettyType = pretty != NULL
        ? xl6IrTypeGet(&module->types, pretty->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return source != NULL && pretty != NULL && sourceType != NULL &&
        prettyType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        prettyType->cAbi == XL6_IR_C_ABI_BOOL &&
        resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        xl6IrPersistenceWireMatches(module, source->type, instruction->payloadType, format);
}


static int xl6IrCgenPersistenceParseAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* text;
    const XL6_IrInstruction* target;
    const XL6_IrType* textType;
    const XL6_IrType* targetType;
    const XL6_IrType* resultType;
    uint32 format;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         (instruction->runtimeOp != XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE &&
          instruction->runtimeOp != XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE &&
          instruction->runtimeOp != XL6_IR_RUNTIME_PERSISTENCE_JSON_READ &&
          instruction->runtimeOp != XL6_IR_RUNTIME_PERSISTENCE_XLON_READ) ||
         instruction->operandCount != 2 ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
          instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    format = xl6IrCgenPersistenceRuntimeFormat(instruction->runtimeOp);
    text = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    target = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    textType = text != NULL
        ? xl6IrTypeGet(&module->types, text->type) : NULL;
    targetType = target != NULL
        ? xl6IrTypeGet(&module->types, target->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return text != NULL && target != NULL && textType != NULL &&
        targetType != NULL && resultType != NULL &&
        target->kind == XL6_IR_INSTR_RUNTIME_CALL &&
        target->runtimeOp == XL6_IR_RUNTIME_CLASS_DEFAULT_INVOKE &&
        target->payloadType == instruction->payloadType &&
        textType->kind != XL6_IR_TYPE_NULLABLE &&
        textType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        textType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        target->type == instruction->payloadType &&
        instruction->type == instruction->payloadType &&
        targetType->kind == XL6_IR_TYPE_CLASS &&
        targetType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        targetType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        resultType == targetType &&
        xl6IrCgenPersistenceLayoutSupported(
            module, instruction->payloadType, format);
}

static const char* xl6IrCgenDynamicCollectionOpToken(
    XL6_IrDynamicCollectionOp operation)
{
    switch ( operation ) {
    case XL6_IR_DYNAMIC_COLLECTION_PUSH: return "XL6_DYNAMIC_COLLECTION_PUSH";
    case XL6_IR_DYNAMIC_COLLECTION_INSERT: return "XL6_DYNAMIC_COLLECTION_INSERT";
    case XL6_IR_DYNAMIC_COLLECTION_REMOVE: return "XL6_DYNAMIC_COLLECTION_REMOVE";
    case XL6_IR_DYNAMIC_COLLECTION_REMOVE_VALUE:
        return "XL6_DYNAMIC_COLLECTION_REMOVE_VALUE";
    case XL6_IR_DYNAMIC_COLLECTION_POP: return "XL6_DYNAMIC_COLLECTION_POP";
    case XL6_IR_DYNAMIC_COLLECTION_TAKE: return "XL6_DYNAMIC_COLLECTION_TAKE";
    case XL6_IR_DYNAMIC_COLLECTION_CLEAR: return "XL6_DYNAMIC_COLLECTION_CLEAR";
    case XL6_IR_DYNAMIC_COLLECTION_RESERVE: return "XL6_DYNAMIC_COLLECTION_RESERVE";
    case XL6_IR_DYNAMIC_COLLECTION_SWAP: return "XL6_DYNAMIC_COLLECTION_SWAP";
    case XL6_IR_DYNAMIC_COLLECTION_ADD: return "XL6_DYNAMIC_COLLECTION_ADD";
    case XL6_IR_DYNAMIC_COLLECTION_CONTAINS: return "XL6_DYNAMIC_COLLECTION_CONTAINS";
    case XL6_IR_DYNAMIC_COLLECTION_INDEX_OF:
        return "XL6_DYNAMIC_COLLECTION_INDEX_OF";
    case XL6_IR_DYNAMIC_COLLECTION_COUNT: return "XL6_DYNAMIC_COLLECTION_COUNT";
    case XL6_IR_DYNAMIC_COLLECTION_CAPACITY: return "XL6_DYNAMIC_COLLECTION_CAPACITY";
    case XL6_IR_DYNAMIC_COLLECTION_PEEK: return "XL6_DYNAMIC_COLLECTION_PEEK";
    case XL6_IR_DYNAMIC_COLLECTION_PEEK_AT: return "XL6_DYNAMIC_COLLECTION_PEEK_AT";
    case XL6_IR_DYNAMIC_COLLECTION_TRIM: return "XL6_DYNAMIC_COLLECTION_TRIM";
    case XL6_IR_DYNAMIC_COLLECTION_CLONE: return "XL6_DYNAMIC_COLLECTION_CLONE";
    case XL6_IR_DYNAMIC_COLLECTION_PUSH_BATCH: return "XL6_DYNAMIC_COLLECTION_PUSH_BATCH";
    case XL6_IR_DYNAMIC_COLLECTION_POP_BATCH: return "XL6_DYNAMIC_COLLECTION_POP_BATCH";
    case XL6_IR_DYNAMIC_COLLECTION_PEEK_BATCH: return "XL6_DYNAMIC_COLLECTION_PEEK_BATCH";
    case XL6_IR_DYNAMIC_COLLECTION_PEEK_BATCH_AT: return "XL6_DYNAMIC_COLLECTION_PEEK_BATCH_AT";
    case XL6_IR_DYNAMIC_COLLECTION_EQUALS: return "XL6_DYNAMIC_COLLECTION_EQUALS";
    case XL6_IR_DYNAMIC_COLLECTION_TRY_POP: return "XL6_DYNAMIC_COLLECTION_TRY_POP";
    case XL6_IR_DYNAMIC_COLLECTION_TRY_PEEK: return "XL6_DYNAMIC_COLLECTION_TRY_PEEK";
    case XL6_IR_DYNAMIC_COLLECTION_TRY_PEEK_AT: return "XL6_DYNAMIC_COLLECTION_TRY_PEEK_AT";
    default: return NULL;
    }
}

static const char* xl6IrCgenTypedCollectionOpToken(
    XL6_IrTypedCollectionOp operation)
{
    switch ( operation ) {
    case XL6_IR_TYPED_COLLECTION_POP: return "XLIR_TYPED_COLLECTION_POP";
    case XL6_IR_TYPED_COLLECTION_TAKE: return "XLIR_TYPED_COLLECTION_TAKE";
    case XL6_IR_TYPED_COLLECTION_CLEAR: return "XLIR_TYPED_COLLECTION_CLEAR";
    case XL6_IR_TYPED_COLLECTION_CAPACITY: return "XLIR_TYPED_COLLECTION_CAPACITY";
    case XL6_IR_TYPED_COLLECTION_RESERVE: return "XLIR_TYPED_COLLECTION_RESERVE";
    case XL6_IR_TYPED_COLLECTION_RESIZE: return "XLIR_TYPED_COLLECTION_RESIZE";
    case XL6_IR_TYPED_COLLECTION_TRIM: return "XLIR_TYPED_COLLECTION_TRIM";
    case XL6_IR_TYPED_COLLECTION_TRIM_TO: return "XLIR_TYPED_COLLECTION_TRIM_TO";
    case XL6_IR_TYPED_COLLECTION_SWAP: return "XLIR_TYPED_COLLECTION_SWAP";
    case XL6_IR_TYPED_COLLECTION_INSERT: return "XLIR_TYPED_COLLECTION_INSERT";
    case XL6_IR_TYPED_COLLECTION_INDEX_OF: return "XLIR_TYPED_COLLECTION_INDEX_OF";
    case XL6_IR_TYPED_COLLECTION_REVERSE: return "XLIR_TYPED_COLLECTION_REVERSE";
    case XL6_IR_TYPED_COLLECTION_IS_DISJOINT:
        return "XLIR_TYPED_COLLECTION_IS_DISJOINT";
    default: return NULL;
    }
}

static const XL6_IrClassField* xl6IrCgenNativeClassPhysicalField(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    size_t parameter)
{
    const XL6_IrType* owner;
    size_t i;
    if ( module == NULL || target == NULL ||
         parameter >= target->parameterCount ||
         target->parameterTypes == NULL || target->parameterAbis == NULL ||
         target->parameterPhysicalAbis == NULL ||
         target->parameterPhysicalFieldDeclarations == NULL ||
         (target->kind != XL6_IR_CALL_TARGET_EXTERN_C &&
          target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY) ||
         target->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_BORROW ||
         target->parameterPhysicalAbis[parameter] !=
             XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ||
         (owner = xl6IrTypeGet(
              &module->types, target->parameterTypes[parameter])) == NULL ||
         owner->kind != XL6_IR_TYPE_CLASS ||
         owner->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
         owner->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED )
        return NULL;
    for ( i = 0; i < module->classFieldCount; ++i ) {
        const XL6_IrClassField* field = &module->classFields[i];
        const XL6_IrNode* declaration;
        const XL6_IrType* type;
        if ( field->ownerType != target->parameterTypes[parameter] ||
             field->declaration !=
                 target->parameterPhysicalFieldDeclarations[parameter] )
            continue;
        declaration = xl6IrNodeGet(module, field->declaration);
        type = xl6IrTypeGet(&module->types, field->type);
        return declaration != NULL &&
               declaration->kind == XL6_IR_NODE_FIELD_DECL &&
               type != NULL && type->cAbi == XL6_IR_C_ABI_RAW_POINTER
            ? field : NULL;
    }
    return NULL;
}

/* The protocol table may select an extern C class method.  Its CallTarget is
 * already the complete adapter contract: semantic receiver, exact physical
 * field, and direct remaining parameters.  Keep this predicate mechanical so
 * dispatcher emission never searches source declarations or method names. */
static const XL6_IrClassField* xl6IrCgenProtocolExternReceiverField(
    const XL6_IrModule* module, const XL6_IrProtocolDispatchSlot* slot,
    const XL6_IrCallTarget* target)
{
    const XL6_IrClassField* field;
    size_t parameter;
    if ( module == NULL || slot == NULL || target == NULL ||
         target->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
         target->cName == NULL || target->cName[0] == '\0' ||
         (target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
             ? target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
             : target->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE ||
               target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_NONE) ||
         target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
         target->returnType != slot->returnType ||
         target->parameterCount != slot->parameterCount + 1 ||
         (field = xl6IrCgenNativeClassPhysicalField(module, target, 0)) == NULL )
        return NULL;
    for ( parameter = 1; parameter < target->parameterCount; ++parameter )
        if ( target->parameterPhysicalAbis[parameter] !=
                 XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
             target->parameterPhysicalFieldDeclarations[parameter] !=
                 XL6_IR_NODE_INVALID ) return NULL;
    return field;
}

static const XL6_IrClassField* xl6IrCgenNativeClassResultField(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrType* owner;
    size_t i;
    if ( module == NULL || target == NULL ||
         target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY ||
         (target->resultPhysicalAbi !=
              XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_BORROW &&
          target->resultPhysicalAbi !=
              XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE) ||
         (owner = xl6IrTypeGet(&module->types, target->returnType)) == NULL ||
         owner->kind != XL6_IR_TYPE_CLASS ||
         owner->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
         owner->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED )
        return NULL;
    for ( i = 0; i < module->classFieldCount; ++i ) {
        const XL6_IrClassField* field = &module->classFields[i];
        const XL6_IrType* type;
        if ( field->ownerType != target->returnType ||
             field->declaration != target->resultPhysicalFieldDeclaration )
            continue;
        type = xl6IrTypeGet(&module->types, field->type);
        if ( type == NULL || type->cAbi != XL6_IR_C_ABI_RAW_POINTER )
            return NULL;
        if ( target->resultPhysicalAbi ==
                 XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_BORROW )
            return field->nativeDestroySymbol == 0 &&
                   target->resultPhysicalDestroySymbol == 0 ? field : NULL;
        return field->nativeDestroySymbol != 0 &&
               field->nativeDestroySymbol == target->resultPhysicalDestroySymbol
            ? field : NULL;
    }
    return NULL;
}

static XL6_IrTypeId xl6IrCgenPhysicalResultType(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrClassField* field;
    if ( target == NULL ) return XL6_IR_TYPE_INVALID;
    if ( target->resultPhysicalAbi == XL6_IR_CALL_RESULT_PHYSICAL_DIRECT )
        return target->returnType;
    field = xl6IrCgenNativeClassResultField(module, target);
    return field != NULL ? field->type : XL6_IR_TYPE_INVALID;
}

/* CALL result identity is completely described by its verified CallTarget.
 * A MAY_RAISE void call is the one non-value case with an SSA result: that id
 * names the control/error carrier consumed by ERROR_TEST and deliberately has
 * no physical TypeId.  Keep this rule in one Cgen predicate so emittability
 * checks cannot reinterpret the carrier as a missing ordinary result type. */
static int xl6IrCgenCallResultAbiIsExact(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrInstruction* instruction)
{
    XL6_IrTypeId physicalResult;
    if ( module == NULL || target == NULL || instruction == NULL ) return 0;
    if ( target->returnType == XL6_IR_TYPE_INVALID )
        return instruction->type == XL6_IR_TYPE_INVALID &&
            (target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
                ? instruction->result != XL6_IR_VALUE_INVALID
                : instruction->result == XL6_IR_VALUE_INVALID);
    physicalResult = xl6IrCgenPhysicalResultType(module, target);
    return physicalResult != XL6_IR_TYPE_INVALID &&
           instruction->result != XL6_IR_VALUE_INVALID &&
           instruction->type == physicalResult;
}

static const char* xl6IrCgenExternParameterType(
    const XL6_IrModule* module, const XL6_IrCallTarget* target, size_t parameter)
{
    const XL6_IrType* type;
    if ( module == NULL || target == NULL || parameter >= target->parameterCount ||
         target->parameterTypes == NULL || target->parameterAbis == NULL ||
         target->parameterPhysicalAbis == NULL ) return NULL;
    if ( target->parameterPhysicalAbis[parameter] ==
             XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE )
        return "XL6_Function";
    if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_VALUE_ADDRESS_BORROW)
        return "const void*";
    if ( target->parameterPhysicalAbis[parameter] ==
             XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
        return xl6IrCgenNativeClassPhysicalField(
                   module, target, parameter) != NULL
            ? "void*" : NULL;
    if ( target->parameterPhysicalAbis[parameter] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT &&
         target->parameterPhysicalAbis[parameter] != XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW ) return NULL;
    type = xl6IrTypeGet(&module->types, target->parameterTypes[parameter]);
    /* Legacy C entry points are pointer ABI boundaries. Language and runtime
     * bridge calls keep the full length-bearing value. */
    if ( type != NULL && type->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
         target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NONE )
        return target->parameterAbis[parameter] == XL6_IR_PARAMETER_ABI_BORROW
            ? "const char*" : "char*";
    return xl6IrCgenType(module, target->parameterTypes[parameter]);
}

static const char* xl6IrCgenNullableScalarCarrier(const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* item=xl6IrTypeGet(&module->types,type);
    if (!item || item->cAbi!=XL6_IR_C_ABI_TAGGED_NULLABLE) return NULL;
    switch (item->nativeSlotAbi) {
    case XL6_IR_NATIVE_SLOT_NULLABLE_BOOL: return "XL6_NullableBoolV1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_I64: return "XL6_NullableI64V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_U64: case XL6_IR_NATIVE_SLOT_NULLABLE_TYPE_ID: return "XL6_NullableU64V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_F32: return "XL6_NullableF32V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_F64: return "XL6_NullableF64V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_TIME: return "XL6_NullableTimeV1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_CHAR: return "XL6_NullableCharV1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I8: return "XL6_NullableNativeI8V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U8: return "XL6_NullableNativeU8V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I16: return "XL6_NullableNativeI16V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U16: return "XL6_NullableNativeU16V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I32: return "XL6_NullableNativeI32V1";
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U32: return "XL6_NullableNativeU32V1";
    default: return NULL;
    }
}
