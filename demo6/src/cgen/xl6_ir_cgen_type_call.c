static int xl6IrCgenTypeSelected(
    const XL6_IrModule* module, XL6_IrTypeId type,
    const unsigned char* selectedTypes)
{
    if ( selectedTypes == NULL ) return 1;
    return module != NULL && type != XL6_IR_TYPE_INVALID &&
           (size_t)type <= module->types.count && selectedTypes[type - 1] != 0;
}

static int xl6IrCgenEmitUnionTypes(
    XL6_Output* out, const XL6_IrModule* module,
    const unsigned char* selectedTypes, XL6_DiagList* diags,
    XL6_IrCgenHeaderBindingVisitor visitor, void* context)
{
    unsigned char* emitted;
    size_t emittedCount = 0;
    size_t requiredCount = 0;
    size_t pass;
    size_t layoutIndex;
    for ( layoutIndex = 0; layoutIndex < module->unionLayoutCount; ++layoutIndex )
        if ( xl6IrCgenTypeSelected(
                 module, module->unionLayouts[layoutIndex].type, selectedTypes) )
            requiredCount += 1;
    if ( requiredCount == 0 ) return 1;
    emitted = (unsigned char*)xrtCalloc(
        module->unionLayoutCount, sizeof(*emitted));
    if ( emitted == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                             "XLIR union declaration ordering allocation failed");
    for ( pass = 0; pass < module->unionLayoutCount &&
                    emittedCount < requiredCount; ++pass ) {
        size_t before = emittedCount;
        for ( layoutIndex = 0; layoutIndex < module->unionLayoutCount; ++layoutIndex ) {
            const XL6_IrUnionLayout* layout = &module->unionLayouts[layoutIndex];
            const XL6_IrType* unionType = xl6IrTypeGet(&module->types, layout->type);
            size_t fieldOffset;
            int ready = 1;
            if ( emitted[layoutIndex] ||
                 !xl6IrCgenTypeSelected(module, layout->type, selectedTypes) )
                continue;
            for ( fieldOffset = 0; fieldOffset < layout->fieldCount; ++fieldOffset ) {
                const XL6_IrUnionField* field =
                    &module->unionFields[layout->firstField + fieldOffset];
                const XL6_IrType* fieldType = xl6IrTypeGet(&module->types, field->type);
                size_t dependency;
                if ( fieldType == NULL ) { ready = 0; break; }
                if ( fieldType->cAbi != XL6_IR_C_ABI_UNION ) continue;
                for ( dependency = 0; dependency < module->unionLayoutCount; ++dependency )
                    if ( module->unionLayouts[dependency].type == field->type ) break;
                if ( dependency == module->unionLayoutCount ||
                     !xl6IrCgenTypeSelected(
                         module, module->unionLayouts[dependency].type,
                         selectedTypes) ||
                     !emitted[dependency] ) {
                    ready = 0; break;
                }
            }
            if ( !ready ) continue;
            if ( unionType == NULL || unionType->kind != XL6_IR_TYPE_UNION ||
                 unionType->cAbi != XL6_IR_C_ABI_UNION ||
                 unionType->cName[0] == '\0' ) {
                xrtFree(emitted);
                return xl6IrCgenDiag(diags, module, layout->declaration,
                    "XLIR union type has no deterministic nominal C ABI");
            }
            xl6OutputPrintf(out,
                "#ifndef XL6_ABI_DEFINED_%s\n"
                "#define XL6_ABI_DEFINED_%s 1\n"
                "typedef union ",
                unionType->cName, unionType->cName);
            {
                size_t startByte = out->buffer.Size;
                xl6OutputPuts(unionType->cName, out);
                if (visitor != NULL && !visitor(context, unionType->nominalSymbol,
                        unionType->cName, "type", startByte, out->buffer.Size)) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, layout->declaration,
                                         "cannot collect bounded C union tag binding");
                }
            }
            xl6OutputPuts(" {\n", out);
            for ( fieldOffset = 0; fieldOffset < layout->fieldCount; ++fieldOffset ) {
                size_t fieldIndex = layout->firstField + fieldOffset;
                const XL6_IrUnionField* field = &module->unionFields[fieldIndex];
                const char* cType = xl6IrCgenType(module, field->type);
                if ( cType == NULL || field->cName[0] == '\0' ) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, field->declaration,
                        "XLIR union field has no exact C value ABI");
                }
                xl6OutputPrintf(out, "    %s %s;\n", cType, field->cName);
            }
            xl6OutputPuts("} ", out);
            {
                size_t startByte = out->buffer.Size;
                xl6OutputPuts(unionType->cName, out);
                if (visitor != NULL && !visitor(context, unionType->nominalSymbol,
                        unionType->cName, "type", startByte, out->buffer.Size)) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, layout->declaration,
                                         "cannot collect bounded C union typedef binding");
                }
            }
            xl6OutputPuts(";\n#endif\n", out);
            emitted[layoutIndex] = 1;
            emittedCount += 1;
        }
        if ( emittedCount == before ) break;
    }
    xrtFree(emitted);
    if ( emittedCount != requiredCount )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR union layouts contain an incomplete or recursive by-value dependency");
    xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenCellKind(const XL6_IrType* element)
{
    if (element == NULL) return NULL;
    if(xl6IrNativeValuePlanForType(element))return "XL6_CELL_NATIVE_LEAF_V2";
    switch (element->cAbi) {
    case XL6_IR_C_ABI_BOOL: return "XL6_CELL_BOOL";
    case XL6_IR_C_ABI_CHAR_U32: return "XL6_CELL_CHAR";
    case XL6_IR_C_ABI_I64: return "XL6_CELL_I64";
    case XL6_IR_C_ABI_U64: return "XL6_CELL_U64";
    case XL6_IR_C_ABI_TYPE_ID: return "XL6_CELL_TYPE_ID";
    case XL6_IR_C_ABI_F64: return "XL6_CELL_F64";
    case XL6_IR_C_ABI_MANAGED_STRING: return "XL6_CELL_STRING";
    case XL6_IR_C_ABI_MANAGED_BYTES:
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: return "XL6_CELL_XVALUE";
    case XL6_IR_C_ABI_TAGGED_NULLABLE:
        return element->cName[0] != '\0' ? "XL6_CELL_TRIVIAL_BYTES" : NULL;
    default: return NULL;
    }
}

static int xl6IrCgenEmitProductTypes(
    XL6_Output* out, const XL6_IrModule* module,
    const unsigned char* selectedTypes, XL6_DiagList* diags)
{
    size_t typeIndex;
    int emitted = 0;
    int scalarCarriersIncluded = 0;
    /* Native record declarations precede every product which may contain one.
     * The native header owns layout and checked-result ABI; no duplicate body. */
    for (typeIndex=0;typeIndex<module->types.count;++typeIndex) {
        const XL6_IrType* type=&module->types.items[typeIndex];
        if (type->cAbi != XL6_IR_C_ABI_NATIVE_VALUE ||
            !xl6IrCgenTypeSelected(module,(XL6_IrTypeId)(typeIndex+1),selectedTypes)) continue;
        const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(type);
        if (!plan) return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,
            "XLIR native value type has no closed resident ABI");
        xl6OutputPrintf(out,"#include <%s>\n",plan->header);
    }
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
        const XL6_IrType* product = &module->types.items[typeIndex];
        size_t elementIndex;
        if ( !xl6IrCgenTypeSelected(
                 module, (XL6_IrTypeId)(typeIndex + 1), selectedTypes) )
            continue;
        if ( product->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE ) {
            const XL6_IrType* element = product->kind == XL6_IR_TYPE_NULLABLE &&
                product->paramCount == 1 && product->params != NULL
                    ? xl6IrTypeGet(&module->types, product->params[0]) : NULL;
            const char* cType = xl6IrCgenNullableScalarCarrier(module, (XL6_IrTypeId)(typeIndex + 1));
            XL6_IrTypeId unionElement=xl6IrNullableUnionElementType(module,(XL6_IrTypeId)(typeIndex+1));
            if (unionElement) {
                xl6OutputPrintf(out,"typedef struct %s { bool hasValue; %s value; } %s;\n",
                    product->cName,xl6IrCgenType(module,unionElement),product->cName);
                emitted=1; continue;
            }
            if ( product->cName[0] == '\0' || element == NULL || cType == NULL ||
                 element->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
                 element->cAbi == XL6_IR_C_ABI_PRODUCT )
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                     "XLIR nullable type has no deterministic tagged C ABI");
            if ( !scalarCarriersIncluded ) {
                xl6OutputPuts("#include <__xl_runtime/xl6_product_abi.h>\n", out);
                scalarCarriersIncluded = 1;
            }
            xl6OutputPrintf(out,
                "typedef %s %s;\n", cType, product->cName);
            emitted = 1;
            continue;
        }
        if ( product->cAbi != XL6_IR_C_ABI_PRODUCT ) continue;
        if ( product->kind != XL6_IR_TYPE_TUPLE || product->cName[0] == '\0' ||
             product->paramCount < 2 || product->params == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                 "XLIR product type has no deterministic structural C ABI");
        if (product->nativeProductAbi != XL6_IR_NATIVE_PRODUCT_NONE) {
            const char* guard = xl6IrNativeProductAbiGuard(product->nativeProductAbi);
            if (!guard) return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR native product has no closed C ABI profile");
            xl6OutputPrintf(out,"#ifndef %s\n#define %s\n",guard,guard);
        }
        xl6OutputPrintf(out, "typedef struct %s {\n", product->cName);
        for ( elementIndex = 0; elementIndex < product->paramCount; ++elementIndex ) {
            const XL6_IrType* element = xl6IrTypeGet(&module->types, product->params[elementIndex]);
            const char* cType = element != NULL && element->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
                ? "struct xvalue*"
                : element != NULL && element->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR
                ? "struct xerror*"
                : element != NULL && element->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR
                ? "struct xfuture*"
                : element != NULL && element->cAbi == XL6_IR_C_ABI_CLOSURE_PTR
                ? "struct xlir_closure*"
                : xl6IrCgenType(module, product->params[elementIndex]);
            if ( element == NULL || cType == NULL || element->cAbi == XL6_IR_C_ABI_PRODUCT )
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                     "XLIR product element has no structural C value ABI");
            xl6OutputPrintf(out, "    %s e%lu;\n", cType, (unsigned long)elementIndex);
        }
        xl6OutputPrintf(out, "} %s;\n", product->cName);
        if (product->nativeProductAbi != XL6_IR_NATIVE_PRODUCT_NONE) xl6OutputPuts("#endif\n",out);
        emitted = 1;
    }
    if ( emitted ) xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

/* Compiler-owned carriers are emitted from the type-pool spelling rather
 * than selected by C ABI in the backend.  Fixed scalar/runtime carriers stay
 * in the shared prelude below for compatibility; tagged nullable and tuple
 * carriers are exact per-TypeId products and therefore live beside their
 * value type. */
static int xl6IrCgenEmitStructuralErrorResultTypes(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t typeIndex;
    int emitted = 0;
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
        const XL6_IrType* type = &module->types.items[typeIndex];
        const char* valueType;
        if ( type->cAbi != XL6_IR_C_ABI_TAGGED_NULLABLE &&
             type->cAbi != XL6_IR_C_ABI_PRODUCT &&
             type->cAbi != XL6_IR_C_ABI_UNION ) continue;
        valueType = xl6IrCgenType(module, (XL6_IrTypeId)(typeIndex + 1));
        if ( valueType == NULL || type->errorResultCName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                 "XLIR structural error-result type has no canonical ABI");
        xl6OutputPrintf(out,
            "typedef struct %s { bool ok; %s value; } %s;\n",
            type->errorResultCName, valueType, type->errorResultCName);
        emitted = 1;
    }
    if ( emitted ) xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

/* Storage linkage is already fixed by XLIR.  A public global is the only
 * storage whose C spelling is not the private s<symbol> label; instruction
 * emission must use this single mapping rather than rediscover export syntax. */
static void xl6IrCgenPutStorageName(
    XL6_Output* out, const XL6_IrModule* module, XL6_SymbolId symbol)
{
    const XL6_IrGlobalExport* exported = xl6IrGlobalExportGet(module, symbol);
    const XL6_IrExternGlobal* imported = xl6IrExternGlobalGet(module, symbol);
    if ( exported != NULL ) xl6OutputPuts(exported->cName, out);
    else if ( imported != NULL ) xl6OutputPuts(imported->cName, out);
    else xl6OutputPrintf(out, "s%u", (unsigned int)symbol);
}

/* Result carriers are named by the verified XLIR type pool.  The sole void
 * carrier has no TypeId; every concrete carrier is consumed verbatim here. */
static const char* xl6IrCgenErrorResultType(const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* item = xl6IrTypeGet(&module->types, type);
    if ( type == XL6_IR_TYPE_INVALID ) return "xlir_result_void";
    return item != NULL && item->errorResultCName[0] != '\0'
        ? item->errorResultCName : NULL;
}

/* The SSA id of a MAY_RAISE direct void call or any first-class void call is
 * an error-carrier token.  It has no C `vN` object and is consumed solely by
 * ERROR_TEST; direct calls map to `rN`, closure calls to their `cN` flag. */
static int xl6IrCgenIsVoidErrorCarrierCall(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrCallTarget* target;
    const XL6_IrType* signature;
    if ( module == NULL || instruction == NULL ||
         instruction->type != XL6_IR_TYPE_INVALID ||
         instruction->result == XL6_IR_VALUE_INVALID ) return 0;
    if ( instruction->kind == XL6_IR_INSTR_CLOSURE_CALL ) {
        signature = xl6IrTypeGet(&module->types, instruction->calleeType);
        return signature != NULL && signature->kind == XL6_IR_TYPE_FUNCTION &&
               signature->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
               signature->callableResultType == XL6_IR_TYPE_INVALID;
    }
    if ( instruction->kind == XL6_IR_INSTR_VIRTUAL_CALL ||
         instruction->kind == XL6_IR_INSTR_PROTOCOL_CALL )
        return xl6IrCallInstructionErrorAbi(module, instruction) == XL6_IR_ERROR_ABI_MAY_RAISE;
    if ( instruction->kind != XL6_IR_INSTR_CALL ) return 0;
    target = xl6IrCallTargetGet(module, instruction->targetSymbol);
    return target != NULL && target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
           target->returnType == XL6_IR_TYPE_INVALID &&
           target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE;
}

/* These SSA markers have no addressable payload: checked void outcomes,
 * void await outcomes, and descriptor-bound logical empty arguments. */
static int xl6IrCgenIsControlCarrier(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    return xl6IrCgenIsVoidErrorCarrierCall(module, instruction) ||
           xl6IrInstructionIsZeroPayload(module, instruction) ||
           (instruction != NULL && instruction->kind == XL6_IR_INSTR_AWAIT &&
            instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID &&
            instruction->type == XL6_IR_TYPE_INVALID &&
             instruction->result != XL6_IR_VALUE_INVALID);
}

/* SSA results and physical C locals are deliberately separate contracts.  A
 * borrowed LOAD may exist only to make an owner activation explicit to XLIR,
 * while a result-carrier call may expose only its success/failure outcome.
 * Every other explicit IR reference requires a material C value.  Keep this
 * closed and representation-only: it never recovers source intent or decides
 * whether an operation itself is dead. */
static int xl6IrCgenResultHasMaterialUse(
    const XL6_IrModule* module, const XL6_IrInstruction* definition)
{
    XL6_IrValueId value;
    size_t i, operand;
    if ( module == NULL || definition == NULL ||
         (value = definition->result) == XL6_IR_VALUE_INVALID ) return 0;
    for ( i = 0; i < module->instructionCount; ++i ) {
        const XL6_IrInstruction* user = &module->instructions[i];
        if ( user->targetValue == value || user->pendingFailure == value )
            return 1;
        for ( operand = 0; operand < user->operandCount; ++operand ) {
            if ( module->operands[user->firstOperand + operand] != value )
                continue;
            /* RESULT_CARRIER error tests read r<instruction>.ok.  The payload
             * is material only when another explicit consumer names it. */
            if ( user->kind == XL6_IR_INSTR_ERROR_TEST &&
                 xl6IrCallInstructionErrorPhysicalAbi(module, definition) ==
                     XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER )
                continue;
            return 1;
        }
    }
    for ( i = 0; i < module->callbackCaptureCount; ++i )
        if ( module->callbackCaptures[i].value == value ) return 1;
    for ( i = 0; i < module->asyncMachineFrameSlotCount; ++i )
        if ( module->asyncMachineFrameSlots[i].value == value ) return 1;
    for ( i = 0; i < module->asyncLiveSlotCount; ++i )
        if ( module->asyncLiveSlots[i].value == value ) return 1;
    for ( i = 0; i < module->asyncFrameActionCount; ++i )
        if ( module->asyncFrameActions[i].value == value ) return 1;
    for ( i = 0; i < module->asyncSuspensionCount; ++i )
        if ( module->asyncSuspensions[i].inputValue == value ||
             module->asyncSuspensions[i].resultValue == value ) return 1;
    for ( i = 0; i < module->generatorFrameSlotCount; ++i )
        if ( module->generatorFrameSlots[i].value == value ) return 1;
    for ( i = 0; i < module->generatorLiveSlotCount; ++i )
        if ( module->generatorLiveSlots[i].value == value ) return 1;
    for ( i = 0; i < module->generatorFrameActionCount; ++i )
        if ( module->generatorFrameActions[i].value == value ) return 1;
    for ( i = 0; i < module->generatorSuspensionCount; ++i )
        if ( module->generatorSuspensions[i].itemValue == value ) return 1;
    for ( i = 0; i < module->instantiationCount; ++i )
        if ( module->instantiations[i].receiver == value ) return 1;
    return 0;
}

static int xl6IrCgenResultStorageOmitted(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* type;
    if ( module == NULL || instruction == NULL ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         xl6IrCgenResultHasMaterialUse(module, instruction) ) return 0;
    if ( instruction->kind == XL6_IR_INSTR_LOAD )
        return xl6IrValueOwnershipGet(module, instruction->result) ==
            XL6_IR_OWNERSHIP_BORROW;
    type = xl6IrTypeGet(&module->types, instruction->type);
    if (instruction->kind == XL6_IR_INSTR_CONST && type != NULL &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL)
        return 1;
    return type != NULL &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        xl6IrValueOwnershipGet(module, instruction->result) ==
            XL6_IR_OWNERSHIP_COPY &&
        xl6IrCallInstructionErrorPhysicalAbi(module, instruction) ==
            XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER;
}

static int xl6IrCgenIsEntryParameter(
    const XL6_IrModule* module, XL6_SymbolId symbol, size_t parameterIndex)
{
    return module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN &&
           module->abi.entrySymbol == symbol && parameterIndex == 0;
}

static XL6_IrParameterAbi xl6IrCgenStorageParameterAbi(
    const XL6_IrModule* module, const XL6_IrStorage* storage,
    size_t* outParameter)
{
    size_t functionIndex;
    if ( outParameter != NULL ) *outParameter = SIZE_MAX;
    if ( module == NULL || storage == NULL ||
         storage->kind != XL6_IR_STORAGE_PARAMETER )
        return XL6_IR_PARAMETER_ABI_INVALID;
    for ( functionIndex = 0; functionIndex < module->functionCount;
          ++functionIndex ) {
        const XL6_IrFunction* function = &module->functions[functionIndex];
        const XL6_IrCallTarget* target;
        const XL6_IrType* closureSignature = NULL;
        size_t parameter;
        if ( function->declaration != storage->owner ) continue;
        if (function->kind == XL6_IR_FUNCTION_ARGUMENT_DEFAULT) {
            for (parameter = 0; parameter < function->parameterCount; ++parameter)
                if (module->functionParameters[function->firstParameter + parameter] == storage->symbol) {
                    const XL6_IrType* type = xl6IrTypeGet(&module->types, storage->type);
                    if (outParameter) *outParameter = parameter;
                    return !storage->cellType && type && type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL
                        ? XL6_IR_PARAMETER_ABI_VALUE : XL6_IR_PARAMETER_ABI_BORROW;
                }
            continue;
        }
        if (function->kind == XL6_IR_FUNCTION_CLASS_FINI) {
            if (function->parameterCount != 1 ||
                module->functionParameters[function->firstParameter] != storage->symbol) continue;
            if (outParameter != NULL) *outParameter = 0;
            return XL6_IR_PARAMETER_ABI_BORROW;
        }
        target = xl6IrCallTargetGet(module, function->symbol);
        if ( target == NULL && function->kind == XL6_IR_FUNCTION_NAMED_ADAPTER ) {
            size_t closureIndex;
            for ( closureIndex = 0; closureIndex < module->closureTargetCount;
                  ++closureIndex ) {
                const XL6_IrClosureTarget* closure =
                    &module->closureTargets[closureIndex];
                if ( closure->kind == XL6_IR_CLOSURE_TARGET_NAMED_ADAPTER &&
                     closure->targetSymbol == function->symbol ) {
                    closureSignature = xl6IrTypeGet(
                        &module->types, closure->signatureType);
                    break;
                }
            }
        }
        if ( (target == NULL && closureSignature == NULL) ||
             (target != NULL &&
              (target->parameterAbis == NULL ||
               target->parameterCount != function->parameterCount)) ||
             (closureSignature != NULL &&
              (closureSignature->parameterAbis == NULL ||
               closureSignature->paramCount != function->parameterCount)) )
            return XL6_IR_PARAMETER_ABI_INVALID;
        for ( parameter = 0; parameter < function->parameterCount; ++parameter )
            if ( module->functionParameters[function->firstParameter + parameter] ==
                     storage->symbol ) {
                if ( outParameter != NULL ) *outParameter = parameter;
                return target != NULL ? target->parameterAbis[parameter]
                    : closureSignature->parameterAbis[parameter];
            }
        return XL6_IR_PARAMETER_ABI_INVALID;
    }
    return XL6_IR_PARAMETER_ABI_INVALID;
}

static int xl6IrCgenParameterAbiIsIndirect(XL6_IrParameterAbi abi)
{
    return abi == XL6_IR_PARAMETER_ABI_INOUT ||
           abi == XL6_IR_PARAMETER_ABI_OUT;
}

/* The canonical language place can differ from an ordinary extern's fixed C
 * text argument. Both SSA calls and prepared dynamic witnesses use this map. */
static const char* xl6IrCgenDirectPlaceSuffix(
    const XL6_IrModule* module, const XL6_IrCallTarget* target, size_t parameter)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[parameter]);
    return type != NULL && type->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        (target->kind == XL6_IR_CALL_TARGET_EXTERN_C || target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
        target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NONE ? ".Data" : "";
}

static const char* xl6IrCgenNativeSlotTypeExpr(XL6_IrNativeSlotAbi);
static const char* xl6IrCgenSequenceTransferAbiToken(XL6_IrRuntimeResourcePayloadAbi);
static int xl6IrCgenEmitNativeServices(XL6_Output* out,const XL6_IrCallTarget* target)
{
    const unsigned mask=target->nativeServicesRequired;
    if(mask==XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1){
        xl6OutputPuts("(XL6_ExecutionServicesV1){ xlir_execution_source }",out);
        return !xl6OutputHasFailed(out);
    }
    if(mask!=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1 && mask!=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|
        XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1))return 0;
    xl6OutputPuts("(XL6_RuntimeServicesV1){ .executor = xlir_continuation_executor",out);
    if(mask&XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1)xl6OutputPuts(", .deadline = xlir_deadline_source",out);
    xl6OutputPuts(" }",out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitNativeContext(XL6_Output* out,const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_NULLARY_VOID_ARRAY_V1){
        const XL6_IrType* array=xl6IrTypeGet(&module->types,target->nativeContextType);
        const XL6_IrType* callback=array&&array->paramCount==1&&array->params?xl6IrTypeGet(&module->types,array->params[0]):NULL;
        if(!array||!callback)return 0;
        xl6OutputPrintf(out,"(XL6_NullaryVoidArrayContextV1){0x%016llXULL, 0x%016llXULL}",
            (unsigned long long)array->typeValueId,(unsigned long long)callback->typeValueId);
        return !xl6OutputHasFailed(out);
    }
    if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1) {
        const XL6_IrClassField* field=xl6IrNativeClassContextField(module,target);
        if(!field || !field->nativeHandlePolicyCName)return 0;
        xl6OutputPuts("(XL6_ChannelResultFieldContextV1){",out);
        xl6IrCgenEmitCStringLiteral(out,field->name);
        xl6OutputPrintf(out,", %lu, %s()}",(unsigned long)strlen(field->name),field->nativeHandlePolicyCName);
        return !xl6OutputHasFailed(out);
    }
    if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1) {
        const XL6_IrClassField* field=xl6IrNativeClassArrayContextField(module,target);
        const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->nativeContextType);
        const XL6_IrType* element=owner && owner->params ? xl6IrTypeGet(&module->types,owner->params[0]) : NULL;
        const char* slot=owner ? xl6IrCgenNativeSlotTypeExpr(owner->sequenceLayout.slotAbi) : NULL;
        if(!field || !element || !slot || !field->nativeHandlePolicyCName)return 0;
        xl6OutputPrintf(out,"(XL6_ChannelCaseArrayContextV1){{0x%016llXULL, 0x%016llXULL, 3, sizeof(xvalue*), %s}, ",
            (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,slot);
        xl6IrCgenEmitCStringLiteral(out,field->name);
        xl6OutputPrintf(out,", %lu, %s()}",(unsigned long)strlen(field->name),field->nativeHandlePolicyCName);
        return !xl6OutputHasFailed(out);
    }
    if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1) {
        XL6_IrTypeId schemaId=0;
        if(!xl6IrChannelConstructionRecipe(module,target->nativeContextType,&schemaId,NULL))return 0;
        const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->nativeContextType);
        const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
        const XL6_IrType* element=xl6IrTypeGet(&module->types,owner->params[0]);
        const char* type=xl6IrCgenType(module,owner->params[0]);
        const char* slot=xl6IrCgenNativeSlotTypeExpr(schema->sequenceLayout.slotAbi);
        const char* transfer=xl6IrCgenSequenceTransferAbiToken(schema->sequenceLayout.transferAbi);
        if(!type || !slot || !transfer || !element)return 0;
        xl6OutputPrintf(out,"(XL6_SequenceLayoutV1){0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s}",
            (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,transfer,type,slot);
        return !xl6OutputHasFailed(out);
    }
    if(target->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1)return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->nativeContextType);
    const char* name=type?xl6IrCgenType(module,target->nativeContextType):NULL;
    const char* kind=NULL;
    switch(target->nativeContextPayloadAbi) {
    case XL6_IR_ASYNC_RESULT_VOID:kind="XL6_FUTURE_VALUE_VOID";break;
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:case XL6_IR_ASYNC_RESULT_COPY_I64:
    case XL6_IR_ASYNC_RESULT_COPY_U64:case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
    case XL6_IR_ASYNC_RESULT_COPY_F64:case XL6_IR_ASYNC_RESULT_COPY_TAGGED:
    case XL6_IR_ASYNC_RESULT_COPY_F32:case XL6_IR_ASYNC_RESULT_COPY_TIME:
    case XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER:case XL6_IR_ASYNC_RESULT_COPY_CHAR:
    case XL6_IR_ASYNC_RESULT_COPY_C_SCALAR:
        kind="XL6_FUTURE_VALUE_TRIVIAL";break;
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:kind="XL6_FUTURE_VALUE_STRING";break;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:kind="XL6_FUTURE_VALUE_XVALUE";break;
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR:kind="XL6_FUTURE_VALUE_ERROR";break;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:kind="XL6_FUTURE_VALUE_CLOSURE";break;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:kind="XL6_FUTURE_VALUE_FUTURE";break;
    case XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR:kind="XL6_FUTURE_VALUE_GENERATOR";break;
    case XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE:kind="XL6_FUTURE_VALUE_NATIVE";break;
    default:return 0;
    }
    /* A void Future has a logical null TypeId but no material C value slot.
     * Its descriptor still carries identity; no C type name is required. */
    if(!type || !kind || (!name && target->nativeContextPayloadAbi!=XL6_IR_ASYNC_RESULT_VOID))return 0;
    xl6OutputPrintf(out,"(XL6_FutureValueProfileV1){ sizeof(XL6_FutureValueProfileV1), %s, ",kind);
    if(target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_VOID)xl6OutputPuts("0",out);
    else xl6OutputPrintf(out,"sizeof(%s)",name);
    xl6OutputPrintf(out,", 0x%016llXULL, ",(unsigned long long)type->typeValueId);
    if(target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE) {
        const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(type);
        if(!native || !native->captureProfileSymbol)return 0;
        xl6OutputPrintf(out,"%s()",native->captureProfileSymbol);
    } else xl6OutputPuts("NULL",out);
    xl6OutputPuts(" }",out);return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitCallOperand(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallTarget* target, size_t parameter,
    XL6_IrValueId value)
{
    if ( target == NULL || parameter >= target->parameterCount ||
         target->parameterPhysicalAbis == NULL ) return 0;
    if (target->parameterPhysicalAbis[parameter] == XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD) {
        const XL6_IrInstruction* definition = xl6IrCgenValueDefinition(module, value, NULL);
        if (!xl6IrInstructionIsZeroPayload(module, definition) ||
            definition->type != target->parameterTypes[parameter]) return 0;
        xl6OutputPuts("NULL", out); return !xl6OutputHasFailed(out);
    }
    if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_RESULT_CLOSURE_BORROW){
        xl6OutputPrintf(out,"(XL6_CheckedResultClosureBorrowV1){v%u, xlir_checked_result_t%u, xlir_closure_owner}",
            (unsigned)value,(unsigned)target->parameterTypes[parameter]);return !xl6OutputHasFailed(out);
    }
    if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_VALUE_ADDRESS_BORROW) {
        xl6OutputPrintf(out,"&v%u",(unsigned int)value);return !xl6OutputHasFailed(out);
    }
    if ( target->parameterPhysicalAbis[parameter] ==
             XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE ) {
        xl6OutputPrintf(out, "xlir_language_function_value_t%u(v%u)",
            (unsigned int)target->parameterTypes[parameter],
            (unsigned int)value);
        return !xl6OutputHasFailed(out);
    }
    if ( target->parameterPhysicalAbis[parameter] ==
             XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK ) {
        xl6OutputPrintf(out,
            "(xlir_raw_callback_t%u)(v%u != NULL ? v%u->raw_callback : NULL)",
            (unsigned int)target->parameterTypes[parameter],
            (unsigned int)value, (unsigned int)value);
        return !xl6OutputHasFailed(out);
    }
    if ( target->parameterPhysicalAbis[parameter] ==
             XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ) {
        if ( xl6IrCgenNativeClassPhysicalField(
                 module, target, parameter) == NULL ) return 0;
        xl6OutputPrintf(out, "v%u", (unsigned int)value);
        return !xl6OutputHasFailed(out);
    }
    if ( target->parameterPhysicalAbis[parameter] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT &&
         target->parameterPhysicalAbis[parameter] != XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW ) return 0;
    if ( xl6IrCgenParameterAbiIsIndirect(target->parameterAbis[parameter]) ) {
        const char* type = xl6IrCgenType(
            module, target->parameterTypes[parameter]);
        if ( type == NULL ) return 0;
        xl6OutputPrintf(out, "(%s*)v%u", type, (unsigned int)value);
    } else {
        xl6OutputPrintf(out, "v%u%s", (unsigned int)value,
            xl6IrCgenDirectPlaceSuffix(module, target, parameter));
    }
    return !xl6OutputHasFailed(out);
}

/* Ordinary CALL emission is shared by synchronous CFGs and async-machine
 * program blocks.  The verifier has already selected every source/physical
 * ABI; this predicate is the C backend's exact, representation-only boundary
 * and deliberately contains no async-specific whitelist. */
static int xl6IrCgenDirectCallAbiSupported(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrInstruction* instruction)
{
    if ( module == NULL || target == NULL || instruction == NULL ||
         target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
         target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR ||
         (target->variadicAbi == XL6_IR_CALL_VARIADIC_C
            ? instruction->operandCount < target->parameterCount
            : instruction->operandCount != target->parameterCount) ||
         !xl6IrCgenCallResultAbiIsExact(module, target, instruction) ||
         (target->errorAbi == XL6_IR_ERROR_ABI_NO_RAISE
              ? target->errorPhysicalAbi !=
                    XL6_IR_CALL_ERROR_PHYSICAL_NONE
              : target->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
                !((target->kind == XL6_IR_CALL_TARGET_INTERNAL &&
                   target->errorPhysicalAbi ==
                       XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER) ||
                  ((target->kind == XL6_IR_CALL_TARGET_EXTERN_C ||
                    target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
                   (target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE ||
                    target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT ||
                    target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR ||
                    target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER)))) ||
         (target->errorPhysicalAbi ==
              XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER &&
           xl6IrCgenErrorResultType(module, target->returnType) == NULL) )
        return 0;
    if ( target->kind == XL6_IR_CALL_TARGET_INTERNAL ) return 1;
    return (target->kind == XL6_IR_CALL_TARGET_EXTERN_C ||
            target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
           target->cName != NULL;
}

/* A representation-only view of either verified dispatch table. The domain
 * selects a table and a C symbol prefix, never a different calling convention. */
typedef struct XL6_IrCgenDispatchAbi {
    XL6_IrTypeId receiverType, returnType;
    XL6_IrErrorAbi errorAbi;
    XL6_IrCallErrorPhysicalAbi errorPhysicalAbi;
    const XL6_IrTypeId* parameterTypes;
    const XL6_IrParameterAbi* parameterAbis;
    size_t parameterCount;
    const char* prefix;
} XL6_IrCgenDispatchAbi;

static int xl6IrCgenDispatchCallAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction,
    XL6_IrCgenDispatchAbi* abi)
{
    const XL6_IrInstruction* receiver;
    size_t p;
    if (module == NULL || instruction == NULL || instruction->dispatchSlot == 0 ||
        instruction->targetSymbol != 0 || instruction->immediateText != NULL) return 0;
    if (instruction->kind == XL6_IR_INSTR_VIRTUAL_CALL) {
        const XL6_IrClassDispatchSlot* slot;
        if (instruction->dispatchSlot > module->classDispatchSlotCount) return 0;
        slot = &module->classDispatchSlots[instruction->dispatchSlot - 1];
        abi->receiverType = slot->receiverType; abi->returnType = slot->returnType;
        abi->errorAbi = slot->errorAbi; abi->errorPhysicalAbi = slot->errorPhysicalAbi;
        abi->parameterTypes = slot->parameterTypes; abi->parameterAbis = slot->parameterAbis;
        abi->parameterCount = slot->parameterCount; abi->prefix = "v";
    } else if (instruction->kind == XL6_IR_INSTR_PROTOCOL_CALL) {
        const XL6_IrProtocolDispatchSlot* slot;
        if (instruction->dispatchSlot > module->protocolDispatchSlotCount) return 0;
        slot = &module->protocolDispatchSlots[instruction->dispatchSlot - 1];
        abi->receiverType = slot->protocolType; abi->returnType = slot->returnType;
        abi->errorAbi = slot->errorAbi; abi->errorPhysicalAbi = slot->errorPhysicalAbi;
        abi->parameterTypes = slot->parameterTypes; abi->parameterAbis = slot->parameterAbis;
        abi->parameterCount = slot->parameterCount; abi->prefix = "p";
    } else return 0;
    if (instruction->operandCount != abi->parameterCount + 1 ||
        instruction->firstOperand > module->operandCount ||
        instruction->operandCount > module->operandCount - instruction->firstOperand ||
        instruction->type != abi->returnType ||
        ((instruction->result != XL6_IR_VALUE_INVALID) !=
            (abi->returnType != XL6_IR_TYPE_INVALID || abi->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE)) ||
        (abi->parameterCount != 0 && (abi->parameterTypes == NULL || abi->parameterAbis == NULL))) return 0;
    receiver = xl6IrCgenValueDefinition(module, module->operands[instruction->firstOperand], NULL);
    /* The module verifier proves nominal receiver compatibility. Derived
     * virtual receivers and protocol receivers share the xvalue* physical ABI;
     * the backend must not substitute TypeId equality for that proof. */
    if (receiver == NULL || xl6IrTypeGet(&module->types, receiver->type) == NULL ||
        xl6IrTypeGet(&module->types, receiver->type)->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        xl6IrTypeGet(&module->types, abi->receiverType) == NULL ||
        xl6IrTypeGet(&module->types, abi->receiverType)->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) return 0;
    if (abi->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE) {
        if (abi->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER ||
            xl6IrCgenErrorResultType(module, abi->returnType) == NULL) return 0;
    } else if (abi->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE ||
        abi->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_NONE) return 0;
    if (abi->returnType != XL6_IR_TYPE_INVALID && xl6IrCgenType(module, abi->returnType) == NULL) return 0;
    for (p = 0; p < abi->parameterCount; ++p) {
        const XL6_IrInstruction* argument = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + p + 1], NULL);
        if (argument == NULL || xl6IrCgenType(module, abi->parameterTypes[p]) == NULL ||
            abi->parameterAbis[p] == XL6_IR_PARAMETER_ABI_INVALID) return 0;
        if (xl6IrCgenParameterAbiIsIndirect(abi->parameterAbis[p])) {
            const XL6_IrType* reference = xl6IrTypeGet(&module->types, argument->type);
            if (argument->kind != XL6_IR_INSTR_STORAGE_ADDRESS || reference == NULL ||
                reference->kind != XL6_IR_TYPE_STORAGE_REF || reference->paramCount != 1 ||
                reference->params[0] != abi->parameterTypes[p]) return 0;
        } else if (argument->type != abi->parameterTypes[p]) return 0;
    }
    return 1;
}

static int xl6IrCgenVirtualCallAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    XL6_IrCgenDispatchAbi abi;
    return instruction != NULL && instruction->kind == XL6_IR_INSTR_VIRTUAL_CALL &&
        xl6IrCgenDispatchCallAbi(module, instruction, &abi);
}

static int xl6IrCgenProtocolCallAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    XL6_IrCgenDispatchAbi abi;
    return instruction != NULL && instruction->kind == XL6_IR_INSTR_PROTOCOL_CALL &&
        xl6IrCgenDispatchCallAbi(module, instruction, &abi);
}

static int xl6IrCgenEmitDispatchCallInstruction(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    XL6_IrCgenDispatchAbi abi;
    size_t p;
    if (!xl6IrCgenDispatchCallAbi(module, instruction, &abi))
        return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter dispatch ABI is unresolved");
    if (abi.errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER)
        xl6OutputPrintf(out, "    %s r%lu = ", xl6IrCgenErrorResultType(module, abi.returnType),
            (unsigned long)(instruction - module->instructions));
    else if (abi.returnType != XL6_IR_TYPE_INVALID)
        xl6OutputPrintf(out, "    v%u = ", (unsigned int)instruction->result);
    else xl6OutputPuts("    ", out);
    xl6OutputPrintf(out, "xlir_%sdispatch_%u(v%u", abi.prefix, (unsigned int)instruction->dispatchSlot,
        (unsigned int)module->operands[instruction->firstOperand]);
    for (p = 0; p < abi.parameterCount; ++p) {
        xl6OutputPuts(", ", out);
        if (xl6IrCgenParameterAbiIsIndirect(abi.parameterAbis[p]))
            xl6OutputPrintf(out, "(%s*)", xl6IrCgenType(module, abi.parameterTypes[p]));
        xl6OutputPrintf(out, "v%u", (unsigned int)module->operands[instruction->firstOperand + p + 1]);
    }
    xl6OutputPuts(");\n", out);
    return !xl6OutputHasFailed(out);
}

/* Launchers and ordinary calls share the explicit result carrier. This one
 * renderer is used by every execution mode; ERROR_TEST alone projects value. */
static int xl6IrCgenLaunchCallAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrCallTarget* target = xl6IrCallTargetGet(module, instruction->targetSymbol);
    return target != NULL && target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER &&
        instruction->result != XL6_IR_VALUE_INVALID && instruction->type == target->returnType &&
        instruction->operandCount == target->parameterCount &&
        ((instruction->kind == XL6_IR_INSTR_ASYNC_START && target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) ||
         (instruction->kind == XL6_IR_INSTR_GENERATOR_START && target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR));
}

static int xl6IrCgenEmitLaunchCall(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    const XL6_IrCallTarget* target = xl6IrCallTargetGet(module, instruction->targetSymbol);
    const char* name; size_t argument;
    if (!xl6IrCgenLaunchCallAbi(module, instruction))
        return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR launch result carrier is unresolved");
    if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC) name = "async";
    else if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) name = "generator";
    else return 0;
    xl6OutputPrintf(out, "    %s r%lu = xlir_%s_start_%u(",
        xl6IrCgenErrorResultType(module, target->returnType),
        (unsigned long)(instruction - module->instructions), name, (unsigned int)target->symbol);
    for (argument = 0; argument < instruction->operandCount; ++argument) {
        if (argument) xl6OutputPuts(", ", out);
        if (!xl6IrCgenEmitCallOperand(out, module, target, argument,
                module->operands[instruction->firstOperand + argument])) return 0;
    }
    xl6OutputPuts(");\n", out); return !xl6OutputHasFailed(out);
}

/* Defined with the other owner operations; both direct and witness calls
 * consume this verified boundary contract, with only physical place mapping
 * differing between SSA operands and adapter parameters. */
static int xl6IrCgenEmitPreparationRollback(
    XL6_Output*, const XL6_IrModule*, const XL6_IrCallTarget*,
    const XL6_IrValueId*, size_t, const char*, XL6_DiagList*);

static int xl6IrCgenEmitDirectCall(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags, const char* unresolvedDiagnostic)
{
    const XL6_IrCallTarget* target = instruction != NULL
        ? xl6IrCallTargetGet(module, instruction->targetSymbol) : NULL;
    size_t argument;
    size_t textCount = 0;
    if ( !xl6IrCgenDirectCallAbiSupported(module, target, instruction) )
        return xl6IrCgenDiag(diags, module,
            instruction != NULL ? instruction->origin : XL6_IR_NODE_INVALID,
            unresolvedDiagnostic);
    /* A failed scope entry skips the native call, so its owned physical
     * result must be empty on every invocation, not merely at function entry. */
    if ((target->preparationFailureAbi == XL6_IR_CALL_PREPARATION_RETIRE_OWNED_REVERSE ||
         target->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1) &&
        instruction->result != XL6_IR_VALUE_INVALID &&
        !xl6IrCgenIsControlCarrier(module, instruction) &&
        !xl6IrCgenResultStorageOmitted(module, instruction)) {
        const char* type = xl6IrCgenType(module, instruction->type);
        if (type == NULL) return xl6IrCgenDiag(diags, module, instruction->origin,
            "XLIR preparation failure result has no empty physical representation");
        xl6OutputPrintf(out, "    v%u = (%s){0};\n", (unsigned int)instruction->result, type);
    }
    if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE )
        xl6OutputPrintf(out,
            "    xl6_foreign_error_scope fs%lu = {0}; xl6_foreign_error_result fr%lu = {false, NULL};\n"
            "    if (xl6ForeignCallBegin(&fs%lu)) {\n",
            (unsigned long)instructionIndex, (unsigned long)instructionIndex,
            (unsigned long)instructionIndex);
    for (argument = target->parameterCount; argument < instruction->operandCount; ++argument) {
        const XL6_IrInstruction* value = xl6IrCgenValueDefinition(module,
            module->operands[instruction->firstOperand + argument], NULL);
        if (value == NULL) return 0;
        if (xl6IrCVarargPhysicalAbi(module, value->type) != XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW) continue;
        if (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE)
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR C text variadic preparation requires a checked foreign outcome");
        xl6OutputPrintf(out, "    const char* ct%lu_%lu = xlir_string_cstr(v%u);\n",
            (unsigned long)instructionIndex, (unsigned long)argument, (unsigned int)value->result);
        ++textCount;
    }
    if (textCount != 0) {
        xl6OutputPuts("    if (", out);
        textCount = 0;
        for (argument = target->parameterCount; argument < instruction->operandCount; ++argument) {
            const XL6_IrInstruction* value = xl6IrCgenValueDefinition(module,
                module->operands[instruction->firstOperand + argument], NULL);
            if (xl6IrCVarargPhysicalAbi(module, value->type) != XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW) continue;
            if (textCount++) xl6OutputPuts(" && ", out);
            xl6OutputPrintf(out, "ct%lu_%lu != NULL", (unsigned long)instructionIndex, (unsigned long)argument);
        }
        xl6OutputPuts(") {\n", out);
    }
    if ( target->errorPhysicalAbi ==
             XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER )
        xl6OutputPrintf(out, "    %s r%lu = ",
            xl6IrCgenErrorResultType(module, target->returnType),
            (unsigned long)instructionIndex);
    else if ( target->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_NONE &&
              instruction->result != XL6_IR_VALUE_INVALID &&
              !xl6IrCgenIsControlCarrier(module, instruction) )
        xl6OutputPrintf(out, "    v%u = ", (unsigned int)instruction->result);
    else
        xl6OutputPuts("    ", out);
    if ( target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY )
        xl6OutputPuts("xlir_string_from_cstr(", out);
    if ( target->kind == XL6_IR_CALL_TARGET_INTERNAL )
        xl6OutputPrintf(out, "xlir_fn_%u(",
                        (unsigned int)instruction->targetSymbol);
    else
        xl6OutputPrintf(out, "%s(", target->cName);
    if(target->nativeServicesRequired) {
        if(!xl6IrCgenEmitNativeServices(out,target))return 0;
        if(target->nativeContextAbi || instruction->operandCount)xl6OutputPuts(", ",out);
    }
    if(target->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_NONE) {
        if(!xl6IrCgenEmitNativeContext(out,module,target))return 0;
        if(instruction->operandCount)xl6OutputPuts(", ",out);
    } else if (target->nativeArrayResultAbi == XL6_IR_CALL_NATIVE_ARRAY_RESULT_PRIMITIVE_IDS_V1) {
        const XL6_IrType* owner = xl6IrTypeGet(&module->types, target->returnType);
        const XL6_IrType* item = owner && owner->params && owner->paramCount == 1
            ? xl6IrTypeGet(&module->types, owner->params[0]) : NULL;
        if (!owner || !item) return 0;
        xl6OutputPrintf(out, "0x%016llXULL, 0x%016llXULL%s", (unsigned long long)owner->typeValueId,
            (unsigned long long)item->typeValueId, instruction->operandCount ? ", " : "");
    }
    for ( argument = 0; argument < instruction->operandCount; ++argument ) {
        if ( argument != 0 ) xl6OutputPuts(", ", out);
        if ( argument < target->parameterCount ) {
            if ( !xl6IrCgenEmitCallOperand(
                     out, module, target, argument,
                     module->operands[instruction->firstOperand + argument]) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter call operand ABI is unresolved");
        } else {
            const XL6_IrInstruction* value = xl6IrCgenValueDefinition(module,
                module->operands[instruction->firstOperand + argument], NULL);
            if (xl6IrCVarargPhysicalAbi(module, value->type) == XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW)
                xl6OutputPrintf(out, "ct%lu_%lu", (unsigned long)instructionIndex, (unsigned long)argument);
            else xl6OutputPrintf(out, "v%u", (unsigned int)value->result);
        }
    }
    if(target->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1) {
        if(instruction->operandCount || target->nativeContextAbi || target->nativeServicesRequired)xl6OutputPuts(", ",out);
        xl6OutputPrintf(out,"&v%u",(unsigned)instruction->result);
    }
    if ( target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY )
        xl6OutputPuts(")", out);
    xl6OutputPuts(");\n", out);
    if (textCount != 0) xl6OutputPuts("    }\n", out);
    if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE ) {
        int owned = 0;
        for (argument = 0; argument < target->parameterCount; ++argument)
            if (target->parameterAbis[argument] == XL6_IR_PARAMETER_ABI_OWNED) owned = 1;
        xl6OutputPrintf(out,
            "    fr%lu = xl6ForeignCallEnd(&fs%lu);\n"
            "    } else {",
            (unsigned long)instructionIndex, (unsigned long)instructionIndex);
        if (owned) {
            char outcome[40];
            snprintf(outcome, sizeof(outcome), "fr%lu", (unsigned long)instructionIndex);
            xl6OutputPuts("\n", out);
            if (!xl6IrCgenEmitPreparationRollback(out, module, target,
                    module->operands + instruction->firstOperand, 0, outcome, diags)) return 0;
            xl6OutputPuts("    }\n", out);
        } else xl6OutputPrintf(out, " fr%lu.error = xrtTakeError(); }\n", (unsigned long)instructionIndex);
    }
    return !xl6OutputHasFailed(out);
}

/* FOREIGN_RESULT_ROLLBACK is a closed ownership operation. The IR verifier
 * already proved its call-result source, failure edge and exact destructor;
 * all backends emit the same detach / isolated cleanup / error-restore ABI. */
static int xl6IrCgenEmitForeignResultRollback(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, instruction->targetSymbol);
    XL6_IrValueId raw;
    if (destroy == NULL || destroy->cName == NULL || instruction->operandCount != 1)
        return 0;
    raw = module->operands[instruction->firstOperand];
    xl6OutputPrintf(out,
        "    { void* xlir_owned = v%u; xerror* xlir_original = xrtTakeError();\n"
        "      v%u = NULL;\n"
        "      if (xlir_owned != NULL) {\n"
        "        xl6_foreign_error_scope xlir_cleanup = {0}; xl6_foreign_error_result xlir_discard;\n"
        /* Non-NULL requires a completed factory CALL and therefore an already
         * initialized/bound context. Rebinding that same context cannot need
         * new storage. Like ErrorScopeEnd, OS rebinding failure is an invariant
         * violation, not permission to skip disposal or poison an outer scope. */
        "        if (!xl6ForeignCallBegin(&xlir_cleanup)) abort();\n"
        "        %s(xlir_owned);\n"
        "        xlir_discard = xl6ForeignCallEnd(&xlir_cleanup);\n"
        "        xrtErrorFree(xlir_discard.error);\n"
        "      }\n"
        "      xrtSetErrorTake(xlir_original); }\n",
        (unsigned int)raw, (unsigned int)raw, destroy->cName);
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitForeignErrorTest(
    XL6_Output* out, size_t sourceIndex, const char* blockPrefix,
    size_t success, size_t failure)
{
    xl6OutputPrintf(out,
        "    if (fr%lu.ok) goto %s%lu; else { xrtSetErrorTake(fr%lu.error); fr%lu.error = NULL; goto %s%lu; }\n",
        (unsigned long)sourceIndex, blockPrefix, (unsigned long)success,
        (unsigned long)sourceIndex, (unsigned long)sourceIndex,
        blockPrefix, (unsigned long)failure);
}

/* Observe a direct or virtual operation before any caller cleanup. Block
 * names differ between execution modes; the physical outcome ABI does not. */
static int xl6IrCgenEmitCallErrorTest(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrInstruction* source,
    size_t sourceIndex, const char* prefix, size_t success, size_t failure,
    XL6_DiagList* diags)
{
    XL6_IrCallErrorPhysicalAbi physical = xl6IrCallInstructionErrorPhysicalAbi(module, source);
    if (xl6IrCallInstructionErrorAbi(module, source) != XL6_IR_ERROR_ABI_MAY_RAISE)
        return xl6IrCgenDiag(diags, module, source->origin, "XLIR call error test has no canonical outcome ABI");
    if (physical == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE)
        xl6IrCgenEmitForeignErrorTest(out, sourceIndex, prefix, success, failure);
    else if (physical == XL6_IR_CALL_ERROR_PHYSICAL_SCOPED_BOOL_RESULT)
        xl6OutputPrintf(out,
            "    if (!fr%lu.ok) { xrtSetErrorTake(fr%lu.error); fr%lu.error = NULL; goto %s%lu; }\n"
            "    if (v%u) goto %s%lu; else goto %s%lu;\n",
            (unsigned long)sourceIndex, (unsigned long)sourceIndex, (unsigned long)sourceIndex,
            prefix, (unsigned long)failure, (unsigned int)source->result,
            prefix, (unsigned long)success, prefix, (unsigned long)failure);
    else if (physical == XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT) {
        const XL6_IrType* valueType = xl6IrTypeGet(&module->types, source->type);
        xl6OutputPrintf(out, "    if (v%u%s != NULL) goto %s%lu; else goto %s%lu;\n",
            (unsigned int)source->result,
            valueType != NULL && valueType->cAbi == XL6_IR_C_ABI_MANAGED_STRING ? ".Data" : "",
            prefix, (unsigned long)success, prefix, (unsigned long)failure);
    }
    else if (physical == XL6_IR_CALL_ERROR_PHYSICAL_FALSE_RESULT)
        xl6OutputPrintf(out, "    if (v%u) goto %s%lu; else goto %s%lu;\n",
            (unsigned int)source->result, prefix, (unsigned long)success, prefix, (unsigned long)failure);
    else if (physical == XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT)
        xl6OutputPrintf(out, "    if (projection_ok_%u) goto %s%lu; else goto %s%lu;\n",
            (unsigned int)source->result, prefix, (unsigned long)success, prefix, (unsigned long)failure);
    else if (physical == XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR)
        xl6OutputPrintf(out, "    if (xrtGetError() == NULL) goto %s%lu; else goto %s%lu;\n",
            prefix, (unsigned long)success, prefix, (unsigned long)failure);
    else if (physical == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER) {
        xl6OutputPrintf(out, "    if (r%lu.ok) { ", (unsigned long)sourceIndex);
        if (source->type != XL6_IR_TYPE_INVALID &&
            !xl6IrCgenResultStorageOmitted(module, source))
            xl6OutputPrintf(out, "v%u = r%lu.value; ", (unsigned int)source->result, (unsigned long)sourceIndex);
        xl6OutputPrintf(out, "goto %s%lu; } else goto %s%lu;\n",
            prefix, (unsigned long)success, prefix, (unsigned long)failure);
    } else return xl6IrCgenDiag(diags, module, source->origin,
        "XLIR call error test has no physical observation ABI");
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenRetainInstructionAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* type;
    if ( module == NULL || instruction == NULL ) return 0;
    type = xl6IrTypeGet(&module->types, instruction->type);
    return
           instruction->kind == XL6_IR_INSTR_RETAIN &&
           instruction->operandCount == 1 &&
           instruction->firstOperand < module->operandCount &&
           instruction->result != XL6_IR_VALUE_INVALID && type != NULL &&
           (type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR ||
            type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
            type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES ||
            type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
            type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR);
}

static int xl6IrCgenEmitRetainInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const XL6_IrType* type;
    XL6_IrValueId source;
    if ( out == NULL ||
         !xl6IrCgenRetainInstructionAbi(module, instruction) ) return 0;
    type = xl6IrTypeGet(&module->types, instruction->type);
    source = module->operands[instruction->firstOperand];
    if ( type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
        xl6OutputPrintf(out,
            "    v%u = xlir_closure_retain(v%u);\n",
            (unsigned int)instruction->result, (unsigned int)source);
    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR )
        xl6OutputPrintf(out,
            "    v%u = v%u != NULL ? xrtFutureRef(v%u) : NULL;\n",
            (unsigned int)instruction->result,
            (unsigned int)source, (unsigned int)source);
    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
              type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES )
        /* RETAIN copies the verified language slot, including absence. XRT's
         * retain API requires a real value shell; NULL is not such a shell.
         * This same mechanical instruction handles normal/async/generator
         * execution and is independent of the slot's producer. */
        xl6OutputPrintf(out,
            "    v%u = v%u != NULL ? xrtValueRetain(v%u) : NULL;\n",
            (unsigned int)instruction->result, (unsigned int)source,
            (unsigned int)source);
    else
        xl6OutputPrintf(out,
            "    v%u = xrtErrorRef(v%u);\n",
            (unsigned int)instruction->result, (unsigned int)source);
    return !xl6OutputHasFailed(out);
}

/* These common xvalue operations have identical semantics in a normal
 * CFG and in an async continuation block.  Keep their ABI test and rendering
 * together so adding an async suspension never creates a second backend
 * interpretation of the same verified instruction. */
