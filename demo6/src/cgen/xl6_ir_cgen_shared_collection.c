/* One physical collection emitter is shared by sync functions and both
 * continuation machines.  No backend path resolves source member spellings. */
static int xl6IrCgenCollectionReceiverIsList(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (!module || !instruction || !instruction->operandCount ||
        instruction->firstOperand>=module->operandCount) return 0;
    const XL6_IrInstruction* source=xl6IrCgenValueDefinition(module,
        module->operands[instruction->firstOperand],NULL);
    const XL6_IrType* type=source ? xl6IrTypeGet(&module->types,source->type) : NULL;
    if (type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params)
        type=xl6IrTypeGet(&module->types,type->params[0]);
    return type && type->collectionAbi==XL6_IR_COLLECTION_LIST;
}
static const char* xl6IrCgenCollectionCountSymbol(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    int list=xl6IrCgenCollectionReceiverIsList(module,instruction);
    switch (instruction->runtimeOp) {
    case XL6_IR_RUNTIME_XVALUE_COUNT: return list ? "xl6ListValueCountV1" : "xl6CollectionCountV1";
    case XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE:
        return list ? "xl6ListValueCountIfPresentV1" : "xl6CollectionCountIfPresentV1";
    case XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL:
        return list ? "xl6ListValueCountOrNullV1" : "xl6CollectionCountOrNullV1";
    default: return NULL;
    }
}
static const char* xl6IrCgenValueCopyProviderSymbol(XL6_IrValueCopyProviderAbi abi,int deep)
{
    switch (abi) {
    case XL6_IR_VALUE_COPY_LIST_V1: return deep ? "xl6ValueCloneGraphV1" : "xl6ListValueCloneV1";
    case XL6_IR_VALUE_COPY_ARRAY_LIST_WIRE_V1: return deep ? "xl6ValueCloneGraphV1" : "xl6ValueCopyV1";
    case XL6_IR_VALUE_COPY_ARRAY_WIRE_V1: return deep ? "xl6ValueDeepCloneV1" : "xl6ValueCloneV1";
    default: return NULL;
    }
}
static const char* xl6IrCgenValueCloneSymbol(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{ return xl6IrCgenValueCopyProviderSymbol(xl6IrValueCopyProviderAbi(module,instruction),0); }
static const char* xl6IrCgenValueDeepCloneSymbol(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{ return xl6IrCgenValueCopyProviderSymbol(xl6IrValueCopyProviderAbi(module,instruction),1); }
static const char* xl6IrCgenRestoreIsolationSymbol(const XL6_IrModule* module,
    XL6_IrTypeId type,XL6_IrPersistenceIsolationAbi abi)
{
    if (abi!=XL6_IR_PERSIST_ISOLATION_RUNTIME_SHALLOW && abi!=XL6_IR_PERSIST_ISOLATION_RUNTIME_DEEP) return NULL;
    int deep=abi==XL6_IR_PERSIST_ISOLATION_RUNTIME_DEEP;
    return xl6IrCgenValueCopyProviderSymbol(xl6IrValueCopyProviderAbiForType(module,type,deep),deep);
}
static const char* xl6IrCgenRestoreObjectProviderSymbol(const XL6_IrModule* module,
    const XL6_IrPersistenceRestoreRoot* root)
{
    XL6_IrValueCopyProviderAbi abi=xl6IrValueCopyProviderAbiForType(module,root->type,1);
    return abi==XL6_IR_VALUE_COPY_LIST_V1 || abi==XL6_IR_VALUE_COPY_ARRAY_LIST_WIRE_V1 ?
        "xl6RestoreClassProviderV1" : "xl6RestoreClassClosedProviderV1";
}
static const char* xl6IrCgenCollectionContainsSymbol(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source=xl6IrCgenValueDefinition(module,
        module->operands[instruction->firstOperand],NULL);
    const XL6_IrType* type=source ? xl6IrTypeGet(&module->types,source->type) : NULL;
    if (type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params)
        type=xl6IrTypeGet(&module->types,type->params[0]);
    return type && type->collectionAbi==XL6_IR_COLLECTION_ARRAY ? "xl6ArrayMethodContainsV1" :
        type && type->collectionAbi==XL6_IR_COLLECTION_LIST ? "xl6ListValueContainsV1" :
        type && (type->collectionAbi==XL6_IR_COLLECTION_DICT || type->collectionAbi==XL6_IR_COLLECTION_SET)
            ? "xl6ValueWireContainsV1" : "xl6ValueContainsV1";
}
static int xl6IrCgenInstructionUsesListMethods(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL) return 0;
    if (xl6IrCollectionCountRuntimeOpIs(instruction->runtimeOp) ||
        instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_CONTAINS)
        return xl6IrCgenCollectionReceiverIsList(module,instruction);
    if (instruction->collectionAbi!=XL6_IR_COLLECTION_LIST) return 0;
    if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE) return 1;
    if (instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD) return 0;
    return instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_CLEAR ||
        instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE ||
        instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TRIM ||
        instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TRIM_TO;
}
static const char* xl6IrCgenArrayMethodSymbol(XL6_IrTypedCollectionOp operation)
{
    switch (operation) {
    case XL6_IR_TYPED_COLLECTION_CLEAR: return "xl6ArrayMethodClearV1";
    case XL6_IR_TYPED_COLLECTION_CAPACITY: return "xl6ArrayMethodCapacityV1";
    case XL6_IR_TYPED_COLLECTION_RESERVE: return "xl6ArrayMethodReserveV1";
    case XL6_IR_TYPED_COLLECTION_TRIM: return "xl6ArrayMethodTrimV1";
    case XL6_IR_TYPED_COLLECTION_SWAP: return "xl6ArrayMethodSwapV1";
    case XL6_IR_TYPED_COLLECTION_INSERT: return "xl6ArrayMethodInsertV1";
    case XL6_IR_TYPED_COLLECTION_INDEX_OF: return "xl6ArrayMethodIndexOfV1";
    case XL6_IR_TYPED_COLLECTION_REVERSE: return "xl6ArrayMethodReverseV1";
    default: return NULL;
    }
}
static const char* xl6IrCgenOwnedIndexReadSymbol(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_ARRAY_READ_VALUE_COPY: return "xl6ArrayValueReadCopyV1";
    case XL6_IR_RUNTIME_ARRAY_READ_VALUE_OR_NULL_COPY: return "xl6ArrayValueReadOrNullCopyV1";
    case XL6_IR_RUNTIME_LIST_READ_VALUE_COPY: return "xl6ListValueReadCopyV1";
    case XL6_IR_RUNTIME_LIST_READ_VALUE_OR_NULL_COPY: return "xl6ListValueReadOrNullCopyV1";
    case XL6_IR_RUNTIME_LIST_READ_VALUE_REQUIRED_COPY: return "xl6ListValueReadRequiredCopyV1";
    case XL6_IR_RUNTIME_INDEX_READ_VALUE_COPY: return "xl6ValueIndexReadCopyV1";
    default: return NULL;
    }
}
static const char* xl6IrCgenListTypedReadSymbol(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_LIST_READ_I64: return "xl6ListReadI64V1";
    case XL6_IR_RUNTIME_LIST_READ_U64: return "xl6ListReadU64V1";
    case XL6_IR_RUNTIME_LIST_READ_BOOL: return "xl6ListReadBoolV1";
    case XL6_IR_RUNTIME_LIST_READ_F64: return "xl6ListReadF64V1";
    case XL6_IR_RUNTIME_LIST_READ_STRING: return "xl6ListReadStringV1";
    default: return NULL;
    }
}
static const char* xl6IrCgenArrayScalarReadSymbol(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER: return "xl6ArrayReadI64V1";
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64: return "xl6ArrayReadU64V1";
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL: return "xl6ArrayReadBoolV1";
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64: return "xl6ArrayReadF64V1";
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY: return "xl6ArrayReadStringV1";
    default: return NULL;
    }
}
static const char* xl6IrCgenArrayExtractSymbol(const XL6_IrType* result)
{
    if (!result) return NULL;
    if (xl6IrArrayTrivialExtractResult(result)) return "xl6ArrayExtractTrivialV1";
    switch (result->cAbi) {
    case XL6_IR_C_ABI_BOOL: return "xl6ArrayExtractBoolV1";
    case XL6_IR_C_ABI_I64: return "xl6ArrayExtractI64V1";
    case XL6_IR_C_ABI_U64: return "xl6ArrayExtractU64V1";
    case XL6_IR_C_ABI_F64: return "xl6ArrayExtractF64V1";
    case XL6_IR_C_ABI_F32: return "xl6ArrayExtractF32V1";
    case XL6_IR_C_ABI_TIME_I64: return "xl6ArrayExtractTimeV1";
    case XL6_IR_C_ABI_CHAR_U32: return "xl6ArrayExtractCharV1";
    case XL6_IR_C_ABI_MANAGED_STRING: return "xl6ArrayExtractStringV1";
    case XL6_IR_C_ABI_MANAGED_BYTES:
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: return "xl6ArrayExtractValueV1";
    default: return NULL;
    }
}
static int xl6IrCgenEmitSharedCollectionRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    int read;
    if ( instruction == NULL ) return 0;
    read = xl6IrCollectionReadRuntimeOpIs(instruction->runtimeOp);
    if ( !read && !xl6IrCollectionMemberRuntimeOpIs(instruction->runtimeOp) ) return 0;
    if ( !(read ? xl6IrCollectionReadRuntimeInstructionCompatible(module, instruction)
                : xl6IrCollectionMemberRuntimeInstructionCompatible(module, instruction)) ) return -1;
    if (xl6IrOwnedIndexReadRuntimeOpIs(instruction->runtimeOp)) {
        const char* symbol=xl6IrCgenOwnedIndexReadSymbol(instruction->runtimeOp);
        xl6OutputPrintf(out,"    v%u = %s(v%u, v%u);\n",(unsigned)instruction->result,symbol,
            (unsigned)module->operands[instruction->firstOperand],
            (unsigned)module->operands[instruction->firstOperand+1]);
    } else if (xl6IrListTypedReadRuntimeOpIs(instruction->runtimeOp)) {
        const XL6_IrType* receiver=xl6IrListIndexReceiverType(module,instruction);
        const XL6_IrType* schema=xl6IrTypeGet(&module->types,receiver->listValueSchemaType);
        const XL6_IrType* element=xl6IrTypeGet(&module->types,instruction->type);
        xl6OutputPrintf(out,"    v%u = %s(v%u, 0x%016llXULL, 0x%016llXULL, 0x%016llXULL, v%u);\n",
            (unsigned)instruction->result,xl6IrCgenListTypedReadSymbol(instruction->runtimeOp),
            (unsigned)module->operands[instruction->firstOperand],
            (unsigned long long)receiver->typeValueId,(unsigned long long)schema->typeValueId,
            (unsigned long long)element->typeValueId,(unsigned)module->operands[instruction->firstOperand+1]);
    } else if (xl6IrArrayScalarReadRuntimeOpIs(instruction->runtimeOp) ||
        instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY) {
        const XL6_IrType* receiver = xl6IrArrayScalarReadReceiverType(module, instruction);
        const XL6_IrType* result = xl6IrTypeGet(&module->types, instruction->type);
        xl6OutputPrintf(out, "    v%u = %s(v%u, 0x%016llXULL, 0x%016llXULL, v%u);\n",
            (unsigned)instruction->result, xl6IrCgenArrayScalarReadSymbol(instruction->runtimeOp),
            (unsigned)module->operands[instruction->firstOperand],
            (unsigned long long)receiver->typeValueId, (unsigned long long)result->typeValueId,
            (unsigned)module->operands[instruction->firstOperand + 1]);
    } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_COUNT ) {
        xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
            (unsigned)instruction->result, xl6IrCgenCollectionCountSymbol(module,instruction),
            (unsigned)module->operands[instruction->firstOperand]);
    } else if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE) {
        xl6OutputPrintf(out,
            "    v%u = (%s){0};\n"
            "    v%u.hasValue = %s(v%u, &v%u.value);\n",
            (unsigned)instruction->result, xl6IrCgenType(module,instruction->type),
            (unsigned)instruction->result, xl6IrCgenCollectionCountSymbol(module,instruction),
            (unsigned)module->operands[instruction->firstOperand],
            (unsigned)instruction->result);
    } else if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL) {
        xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
            (unsigned)instruction->result, xl6IrCgenCollectionCountSymbol(module,instruction),
            (unsigned)module->operands[instruction->firstOperand]);
    } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64 ) {
        xl6OutputPrintf(out, "    v%u = xrtValueArrayAt(v%u, v%u);\n",
            (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand],
            (unsigned)module->operands[instruction->firstOperand + 1]);
    } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_HAS ) {
        xl6OutputPrintf(out, "    v%u = xrtValueSetHas(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
    } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE ) {
        XL6_IrValueId collection =
            module->operands[instruction->firstOperand];
        XL6_IrValueId operand =
            module->operands[instruction->firstOperand + 1];
        if ( instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY )
            xl6OutputPrintf(out,
                "    v%u = xl6ArrayMethodRemoveV1(v%u, v%u, 1);\n",
                (unsigned int)instruction->result,
                (unsigned int)collection,
                (unsigned int)operand);
        else if ( instruction->collectionAbi == XL6_IR_COLLECTION_LIST )
            xl6OutputPrintf(out,
                "    v%u = xl6ListValueRemoveV1(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)collection, (unsigned int)operand);
        else if ( instruction->collectionAbi == XL6_IR_COLLECTION_SET )
            xl6OutputPrintf(out,
                "    v%u = xrtValueSetRemove(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)collection, (unsigned int)operand);
        else
            xl6OutputPrintf(out,
                "    v%u = xrtValueObjectRemove(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)collection, (unsigned int)operand);
    } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CONTAINS ) {
        xl6OutputPrintf(out, "    v%u = %s(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        xl6IrCgenCollectionContainsSymbol(module,instruction),
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
    } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE ) {
        xl6OutputPrintf(out,
            "    v%u = xl6ArrayMethodRemoveValueV1(v%u, v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[
                instruction->firstOperand + 1]);
    } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD ) {
        const XL6_IrType* typedResult =
            xl6IrTypeGet(&module->types, instruction->type);
        XL6_IrValueId receiver =
            module->operands[instruction->firstOperand];
        XL6_IrValueId argument0 = instruction->operandCount > 1
            ? module->operands[instruction->firstOperand + 1]
            : XL6_IR_VALUE_INVALID;
        XL6_IrValueId argument1 = instruction->operandCount > 2
            ? module->operands[instruction->firstOperand + 2]
            : XL6_IR_VALUE_INVALID;
        if (instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP ||
            instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE) {
            bool pop=instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP;
            if (instruction->collectionAbi==XL6_IR_COLLECTION_ARRAY) {
                const XL6_IrInstruction* definition=xl6IrCgenValueDefinition(module,receiver,NULL);
                const XL6_IrType* array=definition ? xl6IrTypeGet(&module->types,definition->type) : NULL;
                const XL6_IrType* element=definition ? xl6IrTypeGet(&module->types,
                    xl6IrCollectionElementType(module,definition->type)) : NULL;
                const char* symbol=xl6IrCgenArrayExtractSymbol(typedResult);
                if (!array || !element || !array->typeValueId || !element->typeValueId || !symbol) return -1;
                if (xl6IrArrayTrivialExtractResult(typedResult)) {
                    XL6_IrTypeId baseId=typedResult->kind==XL6_IR_TYPE_NULLABLE ? typedResult->params[0] : instruction->type;
                    const XL6_IrType* base=xl6IrTypeGet(&module->types,baseId);
                    const char* physical=xl6IrCgenNativeValueTypeExpr(module,instruction->type);
                    const char* basePhysical=xl6IrCgenNativeValueTypeExpr(module,baseId);
                    const char* outputType=xl6IrCgenType(module,instruction->type);
                    if (!base || !base->typeValueId || !physical || !basePhysical || !outputType) return -1;
                    xl6OutputPrintf(out,"    v%u = (%s){0};\n    bool extraction_ok_%u = %s(v%u, 0x%016llXULL, 0x%016llXULL, ",
                        (unsigned)instruction->result,outputType,(unsigned)instruction->result,symbol,(unsigned)receiver,
                        (unsigned long long)array->typeValueId,(unsigned long long)element->typeValueId);
                    if (pop) xl6OutputPuts("0",out); else xl6OutputPrintf(out,"v%u",(unsigned)argument0);
                    xl6OutputPrintf(out,", %s, %s, %s, %s, %u, 0x%016llXULL, &v%u);\n",
                        pop ? "true" : "false",physical,basePhysical,
                        xl6IrCgenNativeValueConverterExpr(module,instruction->type),
                        (unsigned)xl6IrValueAdmissionAbiForType(&module->types,typedResult),
                        (unsigned long long)base->typeValueId,(unsigned)instruction->result);
                    return xl6OutputHasFailed(out) ? -1 : 1;
                }
                xl6OutputPrintf(out,"    v%u = %s(v%u, 0x%016llXULL, 0x%016llXULL, ",
                    (unsigned)instruction->result,symbol,(unsigned)receiver,
                    (unsigned long long)array->typeValueId,(unsigned long long)element->typeValueId);
                if (pop) xl6OutputPuts("0",out);
                else xl6OutputPrintf(out,"v%u",(unsigned)argument0);
                xl6OutputPrintf(out,", %s",pop ? "true" : "false");
                if (typedResult->cAbi==XL6_IR_C_ABI_MANAGED_STRING)
                    xl6OutputPrintf(out,", %s",typedResult->kind==XL6_IR_TYPE_NULLABLE ? "true" : "false");
                xl6OutputPuts(");\n",out);
            } else if (!pop && (instruction->collectionAbi==XL6_IR_COLLECTION_LIST ||
                instruction->collectionAbi==XL6_IR_COLLECTION_DICT)) {
                if (instruction->collectionAbi==XL6_IR_COLLECTION_DICT)
                    xl6OutputPrintf(out,"    v%u = xl6CollectionExtractValueV1(v%u, XL6_COLLECTION_EXTRACT_DICT_V1, 0, v%u);\n",
                        (unsigned)instruction->result,(unsigned)receiver,(unsigned)argument0);
                else xl6OutputPrintf(out,"    v%u = xl6ListValueTakeV1(v%u, v%u);\n",
                    (unsigned)instruction->result,(unsigned)receiver,(unsigned)argument0);
            } else return -1;
            return xl6OutputHasFailed(out) ? -1 : 1;
        }
        switch ( instruction->typedCollectionOp ) {

        case XL6_IR_TYPED_COLLECTION_CLEAR:
            xl6OutputPrintf(out,
                instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY
                    ? "    v%u = xl6ArrayMethodClearV1(v%u);\n"
                    : instruction->collectionAbi == XL6_IR_COLLECTION_LIST
                    ? "    v%u = xl6ListValueClearV1(v%u);\n"
                    : "    v%u = xrtValueClear(v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_CAPACITY:
            xl6OutputPrintf(out,
                instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY
                    ? "    v%u = xl6ArrayMethodCapacityV1(v%u);\n"
                    : "    v%u = (int64_t)xrtValueCapacity(v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_RESERVE:
            if (instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY) {
                xl6OutputPrintf(out,"    v%u = xl6ArrayMethodReserveV1(v%u, v%u);\n",
                    (unsigned)instruction->result,(unsigned)receiver,(unsigned)argument0);
                break;
            }
            xl6OutputPrintf(out,
                "    v%u = v%u >= 0 && xrtValueReserve(v%u, (size_t)v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)argument0,
                (unsigned int)receiver,
                (unsigned int)argument0);
            break;
        case XL6_IR_TYPED_COLLECTION_RESIZE:
            xl6OutputPrintf(out,
                "    v%u = xl6ArrayResizeDefaultV1(v%u, v%u, %u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0,
                (unsigned int)instruction->collectionDefaultAbi);
            break;
        case XL6_IR_TYPED_COLLECTION_TRIM:
            if ( instruction->collectionAbi == XL6_IR_COLLECTION_LIST )
                xl6OutputPrintf(out,
                    "    v%u = xl6ListValueTrimV1(v%u, 0);\n",
                    (unsigned int)instruction->result,
                    (unsigned int)receiver);
            else
                xl6OutputPrintf(out,
                    instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY
                        ? "    v%u = xl6ArrayMethodTrimV1(v%u);\n"
                        : "    v%u = xrtValueTrim(v%u);\n",
                    (unsigned int)instruction->result,
                    (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_TRIM_TO:
            xl6OutputPrintf(out,
                "    v%u = xl6ListValueTrimV1(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0);
            break;
        case XL6_IR_TYPED_COLLECTION_SWAP:
            xl6OutputPrintf(out,
                "    v%u = xl6ArrayMethodSwapV1(v%u, v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0,
                (unsigned int)argument1);
            break;
        case XL6_IR_TYPED_COLLECTION_INSERT:
            xl6OutputPrintf(out,
                "    v%u = xl6ArrayMethodInsertV1(v%u, v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0,
                (unsigned int)argument1);
            break;
        case XL6_IR_TYPED_COLLECTION_INDEX_OF:
            xl6OutputPrintf(out,
                "    v%u = xl6ArrayMethodIndexOfV1(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0);
            break;
        case XL6_IR_TYPED_COLLECTION_REVERSE:
            xl6OutputPrintf(out,
                "    v%u = xl6ArrayMethodReverseV1(v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_IS_DISJOINT:
            xl6OutputPrintf(out,
                "    v%u = xrtValueSetIsDisjoint(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0);
            break;
        default:
            break;
        }
    }
    return xl6OutputHasFailed(out) ? -1 : 1;
}
