/* One physical collection emitter is shared by sync functions and both
 * continuation machines.  No backend path resolves source member spellings. */
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
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_COUNT ) {
        xl6OutputPrintf(out, "    v%u = (int64_t)xrtValueCount(v%u);\n",
            (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand]);
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
                "    v%u = v%u >= 0 && xrtValueArrayRemove(v%u, (size_t)v%u, 1u);\n",
                (unsigned int)instruction->result,
                (unsigned int)operand, (unsigned int)collection,
                (unsigned int)operand);
        else if ( instruction->collectionAbi == XL6_IR_COLLECTION_LIST )
            xl6OutputPrintf(out,
                "    v%u = xrtValueIntMapRemove(v%u, v%u);\n",
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
        xl6OutputPrintf(out, "    v%u = xlir_xvalue_contains(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
    } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE ) {
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_array_remove_value(v%u, v%u);\n",
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
        const char* helper = NULL;
        if ( typedResult != NULL &&
             (instruction->typedCollectionOp ==
                  XL6_IR_TYPED_COLLECTION_POP ||
              instruction->typedCollectionOp ==
                  XL6_IR_TYPED_COLLECTION_TAKE) ) {
            switch ( typedResult->cAbi ) {
            case XL6_IR_C_ABI_BOOL:
                helper = instruction->typedCollectionOp ==
                        XL6_IR_TYPED_COLLECTION_POP
                    ? "xlir_xvalue_typed_array_pop_bool"
                    : "xlir_xvalue_typed_collection_take_bool";
                break;
            case XL6_IR_C_ABI_I64:
                helper = instruction->typedCollectionOp ==
                        XL6_IR_TYPED_COLLECTION_POP
                    ? "xlir_xvalue_typed_array_pop_i64"
                    : "xlir_xvalue_typed_collection_take_i64";
                break;
            case XL6_IR_C_ABI_U64:
                helper = instruction->typedCollectionOp ==
                        XL6_IR_TYPED_COLLECTION_POP
                    ? "xlir_xvalue_typed_array_pop_u64"
                    : "xlir_xvalue_typed_collection_take_u64";
                break;
            case XL6_IR_C_ABI_F64:
                helper = instruction->typedCollectionOp ==
                        XL6_IR_TYPED_COLLECTION_POP
                    ? "xlir_xvalue_typed_array_pop_f64"
                    : "xlir_xvalue_typed_collection_take_f64";
                break;
            case XL6_IR_C_ABI_MANAGED_STRING:
                helper = instruction->typedCollectionOp ==
                        XL6_IR_TYPED_COLLECTION_POP
                    ? "xlir_xvalue_typed_array_pop_string"
                    : "xlir_xvalue_typed_collection_take_string";
                break;
            case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR:
                helper = instruction->typedCollectionOp ==
                        XL6_IR_TYPED_COLLECTION_POP
                    ? "xlir_xvalue_typed_array_pop_xvalue"
                    : "xlir_xvalue_typed_collection_take_xvalue";
                break;
            default: break;
            }
        }
        switch ( instruction->typedCollectionOp ) {
        case XL6_IR_TYPED_COLLECTION_POP:
            xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
                (unsigned int)instruction->result, helper,
                (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_TAKE:
            if ( instruction->collectionAbi == XL6_IR_COLLECTION_DICT )
                xl6OutputPrintf(out,
                    "    v%u = %s(v%u, %d, 0, v%u);\n",
                    (unsigned int)instruction->result, helper,
                    (unsigned int)receiver,
                    (int)instruction->collectionAbi,
                    (unsigned int)argument0);
            else
                xl6OutputPrintf(out,
                    "    v%u = %s(v%u, %d, v%u, (xlir_string){0});\n",
                    (unsigned int)instruction->result, helper,
                    (unsigned int)receiver,
                    (int)instruction->collectionAbi,
                    (unsigned int)argument0);
            break;
        case XL6_IR_TYPED_COLLECTION_CLEAR:
            xl6OutputPrintf(out,
                "    v%u = xrtValueClear(v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_CAPACITY:
            xl6OutputPrintf(out,
                "    v%u = (int64_t)xrtValueCapacity(v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_RESERVE:
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
                    "    v%u = (int64_t)xrtValueIntMapTrim(v%u, 0u);\n",
                    (unsigned int)instruction->result,
                    (unsigned int)receiver);
            else
                xl6OutputPrintf(out,
                    "    v%u = xrtValueTrim(v%u);\n",
                    (unsigned int)instruction->result,
                    (unsigned int)receiver);
            break;
        case XL6_IR_TYPED_COLLECTION_TRIM_TO:
            xl6OutputPrintf(out,
                "    v%u = v%u >= 0 ? (int64_t)xrtValueIntMapTrim(v%u, (size_t)v%u) : 0;\n",
                (unsigned int)instruction->result,
                (unsigned int)argument0,
                (unsigned int)receiver,
                (unsigned int)argument0);
            break;
        case XL6_IR_TYPED_COLLECTION_SWAP:
            xl6OutputPrintf(out,
                "    v%u = v%u >= 0 && v%u >= 0 && xrtValueArraySwap(v%u, (size_t)v%u, (size_t)v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)argument0,
                (unsigned int)argument1,
                (unsigned int)receiver,
                (unsigned int)argument0,
                (unsigned int)argument1);
            break;
        case XL6_IR_TYPED_COLLECTION_INSERT:
            xl6OutputPrintf(out,
                "    v%u = v%u >= 0 && xrtValueArrayInsert(v%u, (size_t)v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)argument0,
                (unsigned int)receiver,
                (unsigned int)argument0,
                (unsigned int)argument1);
            break;
        case XL6_IR_TYPED_COLLECTION_INDEX_OF:
            xl6OutputPrintf(out,
                "    v%u = xlir_xvalue_typed_array_index_of(v%u, v%u);\n",
                (unsigned int)instruction->result,
                (unsigned int)receiver,
                (unsigned int)argument0);
            break;
        case XL6_IR_TYPED_COLLECTION_REVERSE:
            xl6OutputPrintf(out,
                "    v%u = xlir_xvalue_typed_array_reverse(v%u);\n",
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
