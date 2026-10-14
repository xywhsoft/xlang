/* Collection, indexing, field and typed-container RuntimeOp emission. */
static int xl6IrCgenEmitCollectionRuntimeInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_FUTURE_CAST_CHECKED) {
        if (!xl6IrFutureCastInstructionCompatible(module,instruction))
            return xl6IrCgenRuntimeEmitDiag(diags,module,instruction->origin,"Future cast has no exact owned projection ABI");
        const XL6_IrType* identity=xl6IrTypeGet(&module->types,instruction->payloadType);
        xl6OutputPrintf(out,"    v%u = NULL;\n    bool projection_ok_%u = xl6PayloadReadFutureV1(v%u, 0x%016llXULL, %uu, &v%u);\n",
            (unsigned)instruction->result,(unsigned)instruction->result,
            (unsigned)module->operands[instruction->firstOperand],(unsigned long long)identity->typeValueId,
            instruction->castAbi==XL6_IR_CAST_XVALUE_TO_NULLABLE_FUTURE_TRY ? 2u : 0u,(unsigned)instruction->result);
    } else if(instruction->runtimeOp==XL6_IR_RUNTIME_NAMED_READ_LITERAL_COPY ||
        instruction->runtimeOp==XL6_IR_RUNTIME_NAMED_SET_LITERAL_TAKE) {
        if(!xl6IrNamedValueInstructionCompatible(module,instruction))
            return xl6IrCgenRuntimeEmitDiag(diags,module,instruction->origin,"XLIR named record emission requires an exact owned ABI");
        bool write=instruction->runtimeOp==XL6_IR_RUNTIME_NAMED_SET_LITERAL_TAKE;
        xl6OutputPrintf(out,"    v%u = %s(v%u, %s, sizeof(%s)-1, ",
            (unsigned)instruction->result,write ? "xl6ValueNameSetTakeV1" : "xl6ValueNameReadCopyV1",
            (unsigned)module->operands[instruction->firstOperand],instruction->immediateText,instruction->immediateText);
        if(write) xl6OutputPrintf(out,"v%u);\n",(unsigned)module->operands[instruction->firstOperand+1]);
        else xl6OutputPuts("true);\n",out);
    } else if ( xl6IrCgenEmitXValueIteratorInstruction(
                               out, module, instruction) ) {
        } else if ( xl6IrCgenEmitXValueProjectionInstruction(
                               out, module, instruction) ) {
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_array_at_i64_or_null(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW ) {
                    xl6OutputPrintf(out, "    v%u = xl6ArrayValueSetTakeV1(v%u, v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1],
                                    (unsigned int)module->operands[instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueIntMap();\n", (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64 ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueIntMapGet(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL ) {
                    xl6OutputPrintf(out,
                        "    v%u = (v%u == NULL || xrtValueType(v%u) == 0) ? xrtValueNull() : xrtValueIntMapGet(v%u, v%u);\n"
                        "    if (v%u == NULL) v%u = xrtValueNull();\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)instruction->result,
                        (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_int_map_at_i64_integer(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_int_map_at_i64_u64(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_int_map_at_i64_bool(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64 ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_int_map_at_i64_f64(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_int_map_at_i64_string_copy(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueIntMapSetNew(v%u, v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1],
                                    (unsigned int)module->operands[instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_NEW ) {
                    char target[24];
                    if (!xl6IrSetCreateRuntimeInstructionCompatible(module,instruction)) return -1;
                    snprintf(target,sizeof(target),"v%u",(unsigned)instruction->result);
                    return xl6IrCgenEmitSetTargetCreate(out,module,instruction->type,target,
                        instruction->sequenceAdmissionIdentitySet)>0;
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW ) {
                    xl6OutputPrintf(out, "    v%u = xl6SetValueAddTakeV1(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD ) {
                    const char* operation = xl6IrCgenDynamicCollectionOpToken(
                        instruction->dynamicCollectionOp);
                    XL6_IrValueId receiver =
                        module->operands[instruction->firstOperand];
                    if ( instruction->operandCount == 1 )
                        xl6OutputPrintf(out,
                            "    v%u = xl6DynamicCollectionCallTakeV1(v%u, %s, NULL, NULL);\n",
                            (unsigned int)instruction->result,
                            (unsigned int)receiver, operation);
                    else if ( instruction->operandCount == 2 )
                        xl6OutputPrintf(out,
                            "    v%u = xl6DynamicCollectionCallTakeV1(v%u, %s, v%u, NULL);\n",
                            (unsigned int)instruction->result,
                            (unsigned int)receiver, operation,
                            (unsigned int)module->operands[
                                instruction->firstOperand + 1]);
                    else
                        xl6OutputPrintf(out,
                            "    v%u = xl6DynamicCollectionCallTakeV1(v%u, %s, v%u, v%u);\n",
                            (unsigned int)instruction->result,
                            (unsigned int)receiver, operation,
                            (unsigned int)module->operands[
                                instruction->firstOperand + 1],
                            (unsigned int)module->operands[
                                instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_NEW ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueObject();\n", (unsigned int)instruction->result);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_object_get_literal_or_null(v%u, xlir_literal_view(%s));\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    instruction->immediateText);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING ) {
                    xl6OutputPrintf(out, "    v%u = xlir_xvalue_object_get_string_or_null(v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_dynamic_field_get_or_null(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_index_get_xvalue_key_or_null(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW ) {
                    xl6OutputPrintf(out,
                        "    v%u = xl6ValueIndexSetTakeV1(v%u, v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)module->operands[instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW ) {
                    xl6OutputPrintf(out, "    v%u = xrtValueObjectSetNewPostCommit(v%u, v%u, v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand],
                                    (unsigned int)module->operands[instruction->firstOperand + 1],
                                    (unsigned int)module->operands[instruction->firstOperand + 2]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW ) {
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_dynamic_field_set_new(v%u, v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)module->operands[instruction->firstOperand + 2]);
                    } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER ) {
                        xl6OutputPrintf(out,
                            "    v%u = xlir_xvalue_object_get_literal_raw_pointer(v%u, xlir_literal_view(%s));\n",
                            (unsigned int)instruction->result,
                            (unsigned int)module->operands[instruction->firstOperand],
                            instruction->immediateText);
                    } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW ) {
                        xl6OutputPrintf(out,
                            "    v%u = xlir_xvalue_object_get_literal_native_pointer(v%u, xlir_literal_view(%s));\n",
                            (unsigned int)instruction->result,
                            (unsigned int)module->operands[instruction->firstOperand],
                            instruction->immediateText);
                    } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE ) {
                        xl6OutputPrintf(out,
                            "    v%u = xlir_xvalue_object_take_literal_native_handle(v%u, xlir_literal_view(%s));\n",
                            (unsigned int)instruction->result,
                            (unsigned int)module->operands[instruction->firstOperand],
                            instruction->immediateText);
                    } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE ) {
                        const XL6_IrType* tagged = xl6IrTypeGet(
                            &module->types, instruction->type);
                        if ( tagged == NULL ||
                             !xl6IrCgenRuntimeTaggedNullableXValueAbi(module, instruction) )
                            return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                                "XLIR C emitter tagged nullable class field projection ABI is unresolved");
                        xl6OutputPrintf(out,
                            "    v%u = xlir_xvalue_object_get_literal_%s(v%u, xlir_literal_view(%s));\n",
                            (unsigned int)instruction->result, tagged->cName,
                            (unsigned int)module->operands[instruction->firstOperand],
                            instruction->immediateText);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE ) {
                    if ( !xl6IrCgenRuntimeObjectClosureFieldAbi(module, instruction, 1) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter typed closure field store ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_xvalue_object_set_literal_closure(v%u, xlir_literal_view(%s), v%u, xlir_xcall_entry_t%u, 0x%016llXULL);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        instruction->immediateText,
                        (unsigned int)module->operands[instruction->firstOperand + 1],
                        (unsigned int)instruction->calleeType,
                        (unsigned long long)xl6IrTypeGet(
                            &module->types,
                            instruction->runtimeIdentityType)->typeValueId);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE ) {
                    if ( !xl6IrCgenRuntimeObjectClosureFieldAbi(module, instruction, 0) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter typed closure field load ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    bool projection_ok_%u = xlir_xvalue_object_get_literal_closure(v%u, xlir_literal_view(%s), (void (*)(void))xlir_xvalue_closure_entry_t%u, &v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        instruction->immediateText,
                        (unsigned int)instruction->calleeType,
                        (unsigned int)instruction->result);
                } else return 0;
    return 1;
}
