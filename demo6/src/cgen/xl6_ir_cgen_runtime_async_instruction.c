/* Callable, task, Future aggregate and dynamic-call RuntimeOp emission. */
static int xl6IrCgenEmitAsyncRuntimeInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE ) {
                    if ( !xl6IrCgenRuntimeCallableFromClosureAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime callable construction ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_closure_to_xvalue_scalar(v%u, xlir_xcall_entry_t%u, 0x%016llXULL);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)instruction->calleeType,
                        (unsigned long long)xl6IrTypeGet(
                            &module->types,
                            instruction->runtimeIdentityType)->typeValueId);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX ) {
                    if ( !xl6IrCgenRuntimeNullableClosureBoxAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter nullable closure dynamic box ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = v%u != NULL ? xlir_closure_to_xvalue_scalar(v%u, xlir_xcall_entry_t%u, 0x%016llXULL) : xrtValueRetain(xrtValueNull());\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)module->operands[instruction->firstOperand],
                        (unsigned int)instruction->calleeType,
                        (unsigned long long)xl6IrTypeGet(
                            &module->types,
                            instruction->runtimeIdentityType)->typeValueId);
                } else if ( instruction->runtimeOp ==
                             XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE ) {
                    size_t argument;
                    if ( !xl6IrCgenRuntimeTaskSpawnAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter task spawn closure ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_task_spawn_start_%lu(v%u",
                        (unsigned int)instruction->result,
                        (unsigned long)instructionIndex,
                        (unsigned int)module->operands[
                            instruction->firstOperand]);
                    for ( argument = 1;
                          argument < instruction->operandCount; ++argument )
                        xl6OutputPrintf(out, ", v%u",
                            (unsigned int)module->operands[
                                instruction->firstOperand + argument]);
                    xl6OutputPuts(");\n", out);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE ) {
                    if ( !xl6IrCgenRuntimeFutureContinuationAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module,
                            instruction->origin,
                            "XLIR C emitter Future continuation ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xlir_future_continue_start_%lu(v%u, v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned long)instructionIndex,
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned int)module->operands[
                            instruction->firstOperand + 1]);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_CANCEL ) {
                    if ( !xl6IrCgenRuntimeFutureCancelAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime future cancel ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = xlir_future_cancel(v%u);\n",
                                    (unsigned int)instruction->result,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                                XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_FUTURE_RACE_ARRAY ) {
                    if ( !xl6IrCgenRuntimeFutureAggregateArrayAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime Future all/race array ABI is unresolved");
                    if ( instruction->runtimeOp ==
                         XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY ) {
                        if ( instruction->asyncResultAbi ==
                             XL6_IR_ASYNC_RESULT_VOID )
                            xl6OutputPrintf(out,
                                "    v%u = xl6FutureAggregateAllArrayV2(v%u, NULL, NULL);\n",
                                (unsigned int)instruction->result,
                                (unsigned int)module->operands[
                                    instruction->firstOperand]);
                        else
                            xl6OutputPrintf(out,
                                "    v%u = xl6FutureAggregateAllArrayV2(v%u, xlir_future_payload_box_t%u, xlir_future_aggregate_owner);\n",
                                (unsigned int)instruction->result,
                                (unsigned int)module->operands[
                                    instruction->firstOperand],
                                (unsigned int)instruction->payloadType);
                    }
                    else
                        xl6OutputPrintf(out,
                            "    v%u = xl6FutureAggregateRaceArrayV2(v%u);\n",
                            (unsigned int)instruction->result,
                            (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY ) {
                    if ( !xl6IrCgenRuntimeFutureWhenAnyIndexArrayAbi(
                             module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime Future array aggregate ABI is unresolved");
                    xl6OutputPrintf(out,
                        "    v%u = xl6FutureAggregateWhenAnyIndexArrayV2(v%u);\n",
                        (unsigned int)instruction->result,
                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( instruction->runtimeOp ==
                            XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
                            instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_RACE ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX ) {
                    size_t futureIndex;
                    if ( instruction->runtimeOp ==
                             XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX
                            ? !xl6IrCgenRuntimeFutureWhenAnyIndexAbi(
                                  module, instruction)
                            : !xl6IrCgenRuntimeFutureAggregateAbi(
                                  module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime Future aggregate ABI is unresolved");
                    xl6OutputPrintf(out, "    xfuture* fa%lu[%lu] = {",
                                    (unsigned long)instructionIndex,
                                    (unsigned long)instruction->operandCount);
                    for ( futureIndex = 0;
                          futureIndex < instruction->operandCount; ++futureIndex )
                        xl6OutputPrintf(out, "%sv%u", futureIndex != 0 ? ", " : "",
                            (unsigned int)module->operands[
                                instruction->firstOperand + futureIndex]);
                    xl6OutputPuts("};\n", out);
                    if ( instruction->runtimeOp ==
                         XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ) {
                        if ( instruction->asyncResultAbi ==
                             XL6_IR_ASYNC_RESULT_VOID )
                            xl6OutputPrintf(out,
                                "    v%u = xl6FutureAggregateAllV2(fa%lu, %lu, NULL, NULL);\n",
                                (unsigned int)instruction->result,
                                (unsigned long)instructionIndex,
                                (unsigned long)instruction->operandCount);
                        else
                            xl6OutputPrintf(out,
                                "    v%u = xl6FutureAggregateAllV2(fa%lu, %lu, xlir_future_payload_box_t%u, xlir_future_aggregate_owner);\n",
                                (unsigned int)instruction->result,
                                (unsigned long)instructionIndex,
                                (unsigned long)instruction->operandCount,
                                (unsigned int)instruction->payloadType);
                    }
                    else if ( instruction->runtimeOp ==
                              XL6_IR_RUNTIME_FUTURE_RACE )
                        xl6OutputPrintf(out,
                            "    v%u = xl6FutureAggregateRaceV2(fa%lu, %lu);\n",
                            (unsigned int)instruction->result, (unsigned long)instructionIndex,
                            (unsigned long)instruction->operandCount);
                    else
                        xl6OutputPrintf(out,
                            "    v%u = xl6FutureAggregateWhenAnyIndexV2(fa%lu, %lu);\n",
                            (unsigned int)instruction->result, (unsigned long)instructionIndex,
                            (unsigned long)instruction->operandCount);
                } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL ||
                            instruction->runtimeOp ==
                                XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL ) {
                    const XL6_IrDynamicCallSite* site;
                    size_t argument;
                    if ( !xl6IrCgenRuntimeDynamicCallAbi(module, instruction) )
                        return xl6IrCgenRuntimeEmitDiag(diags, module, instruction->origin,
                            "XLIR C emitter runtime dynamic call ABI is unresolved");
                    site = &module->dynamicCallSites[
                        instruction->dynamicCallSite];
                    if ( site->positionalCount != 0 ) {
                        xl6OutputPrintf(out, "    xvalue* da%lu[%lu] = {",
                                        (unsigned long)instructionIndex,
                                        (unsigned long)site->positionalCount);
                        for ( argument = 0; argument < site->positionalCount;
                              ++argument )
                            xl6OutputPrintf(out, "%sv%u", argument != 0 ? ", " : "",
                                (unsigned int)module->operands[
                                    instruction->firstOperand + 1u + argument]);
                        xl6OutputPuts("};\n", out);
                    }
                    if ( site->keywordCount != 0 ) {
                        xl6OutputPrintf(out,
                            "    xlir_string_view dkn%lu[%lu] = {",
                            (unsigned long)instructionIndex,
                            (unsigned long)site->keywordCount);
                        for ( argument = 0; argument < site->keywordCount;
                              ++argument ) {
                            const XL6_IrDynamicCallArgument* keyword =
                                &module->dynamicCallArguments[
                                    site->firstArgument +
                                    site->positionalCount + argument];
                            xl6OutputPrintf(out, "%sxrtStrView(\"%s\")",
                                argument != 0 ? ", " : "", keyword->name);
                        }
                        xl6OutputPuts("};\n", out);
                        xl6OutputPrintf(out,
                            "    xvalue* dkv%lu[%lu] = {",
                            (unsigned long)instructionIndex,
                            (unsigned long)site->keywordCount);
                        for ( argument = 0; argument < site->keywordCount;
                              ++argument )
                            xl6OutputPrintf(out, "%sv%u",
                                argument != 0 ? ", " : "",
                                (unsigned int)module->operands[
                                    instruction->firstOperand + 1u +
                                    site->positionalCount + argument]);
                        xl6OutputPuts("};\n", out);
                    }
                    int product=site->resultAbi==XL6_IR_DYNAMIC_CALL_RESULT_OWNED_PRODUCT;
                    if(product) {
                        const XL6_IrType* resultType=xl6IrTypeGet(&module->types,site->resultType);
                        xl6OutputPrintf(out,"    v%u = (%s){0};\n",(unsigned)instruction->result,xl6IrCgenType(module,site->resultType));
                        xl6OutputPrintf(out,"    xvalue** dpo%lu[%lu] = {",(unsigned long)instructionIndex,(unsigned long)resultType->paramCount);
                        for(size_t field=0;field<resultType->paramCount;++field)
                            xl6OutputPrintf(out,"%s&v%u.e%lu",field?", ":"",(unsigned)instruction->result,(unsigned long)field);
                        xl6OutputPuts("};\n",out);
                        xl6OutputPrintf(out,"    bool projection_ok_%u = ",(unsigned)instruction->result);
                    } else xl6OutputPrintf(out, "    v%u = ",(unsigned int)instruction->result);
                    if ( instruction->runtimeOp ==
                             XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL )
                        xl6OutputPrintf(out,
                            "xlir_dynamic_method_call_d%lu(",
                            (unsigned long)instruction->dynamicCallSite);
                    else if(product) xl6OutputPuts("xl6DynamicFunctionProductTakeV1(",out);
                    else xl6OutputPuts("xlir_xvalue_dynamic_call_owned(", out);
                    xl6OutputPrintf(out, "v%u, %lu, ",
                        (unsigned int)module->operands[
                            instruction->firstOperand],
                        (unsigned long)site->positionalCount);
                    if ( site->positionalCount != 0 )
                        xl6OutputPrintf(out, "da%lu", (unsigned long)instructionIndex);
                    else xl6OutputPuts("NULL", out);
                    xl6OutputPrintf(out, ", %lu, ",
                        (unsigned long)site->keywordCount);
                    if ( site->keywordCount != 0 )
                        xl6OutputPrintf(out, "dkn%lu, dkv%lu",
                            (unsigned long)instructionIndex, (unsigned long)instructionIndex);
                    else xl6OutputPuts("NULL, NULL", out);
                    if(product)
                        xl6OutputPrintf(out,", %lu, dpo%lu",(unsigned long)xl6IrTypeGet(&module->types,site->resultType)->paramCount,(unsigned long)instructionIndex);
                    else if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL)
                        xl6OutputPuts(site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT
                            ? ", true" : ", false", out);
                    xl6OutputPuts(");\n", out);
                } else return 0;
    return 1;
}

static int xl6IrCgenEmitRawRuntimeCallInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    int emitted = xl6IrCgenEmitScalarRuntimeInstruction(
        out, module, instruction, diags);
    if ( emitted < 0 ) return 0;
    if ( emitted > 0 ) return 1;
    emitted = xl6IrCgenEmitCollectionRuntimeInstruction(
        out, module, instruction, diags);
    if ( emitted < 0 ) return 0;
    if ( emitted > 0 ) return 1;
    emitted = xl6IrCgenEmitAsyncRuntimeInstruction(
        out, module, instruction, instructionIndex, diags);
    if ( emitted < 0 ) return 0;
    if ( emitted > 0 ) return 1;
    return xl6IrCgenDiag(diags, module, instruction->origin,
        "XLIR C emitter runtime operation ABI is unresolved");
}

/* The IR fixes which RuntimeOps can invoke language callbacks. Their native
 * result (including true/false) never substitutes for that callback outcome. */
static int xl6IrCgenEmitRuntimeCallInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    int scoped = xl6IrCallErrorPhysicalUsesForeignScope(xl6IrCallInstructionErrorPhysicalAbi(module, instruction));
    if (scoped) {
        xl6OutputPrintf(out,
            "    xl6_foreign_error_scope fs%lu = {0}; xl6_foreign_error_result fr%lu;\n"
            /* As for owned cleanup, the context must bind before invoking an
             * operation that consumes operands. Never skip that consumption
             * and then pretend a recoverable runtime call was performed. */
            "    if (!xl6ForeignCallBegin(&fs%lu)) abort();\n",
            (unsigned long)instructionIndex, (unsigned long)instructionIndex, (unsigned long)instructionIndex);
    }
    if (!xl6IrCgenEmitRawRuntimeCallInstruction(out, module, instruction, instructionIndex, diags)) return 0;
    if (scoped) xl6OutputPrintf(out, "    fr%lu = xl6ForeignCallEnd(&fs%lu);\n",
        (unsigned long)instructionIndex, (unsigned long)instructionIndex);
    return !xl6OutputHasFailed(out);
}
