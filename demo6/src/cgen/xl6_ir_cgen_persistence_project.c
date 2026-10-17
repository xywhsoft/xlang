/* Data only: inclusion, inheritance, keys and native identities belong to IR. */
static void xl6IrCgenEmitPersistenceByteView(XL6_Output* out, const char* text, int literal)
{
    xl6OutputPuts("{", out);
    if (literal) xl6OutputPuts(text, out);
    else xl6IrCgenEmitCStringLiteral(out, text);
    xl6OutputPuts(", sizeof(", out);
    if (literal) xl6OutputPuts(text, out);
    else xl6IrCgenEmitCStringLiteral(out, text);
    xl6OutputPuts(") - 1}", out);
}

static int xl6IrCgenEmitPersistenceProjectPlans(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t p, s, f;
    int encode = 0, io = 0, writer = 0;
    if (module->persistenceProjectPlanCount == 0) return 1;
    if (!xl6IrVerifyPersistenceProjectPlans(module))
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR persistence projection tables are invalid");
    xl6OutputPuts("#include <__xl_runtime/xl6_persistence_project.h>\n", out);
    unsigned graphProviders=0;
    for (s=0;s<module->instructionCount;++s)
        if (xl6IrCgenInstructionIsReachable(module,s) &&
            (module->instructions[s].runtimeOp == XL6_IR_RUNTIME_PUBLIC_DATA_PROJECT || module->instructions[s].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT ||
             module->instructions[s].runtimeOp==XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT)) {
            graphProviders|=xl6IrValueGraphProvidersForType(module,module->instructions[s].payloadType);
        }
    if (!xl6IrCgenEmitValueGraphHeaders(out,graphProviders,1)) return 0;
    for (s = 0; s < module->instructionCount; ++s)
        if (module->instructions[s].kind == XL6_IR_INSTR_RUNTIME_CALL) {
            const XL6_IrPersistenceInvocationContract* invocation =
                xl6IrPersistenceInvocationRuntime(module->instructions[s].runtimeOp);
            if (invocation != NULL) {
                if (invocation->action == XL6_IR_PERSISTENCE_INVOKE_ENCODE) encode = 1;
                else if (xl6IrPersistenceInvocationIsWriter(invocation)) writer = 1;
                else io = 1;
            }
        }
    if (encode) xl6OutputPuts("#include <__xl_runtime/xl6_serialization_encode.h>\n", out);
    if (io) xl6OutputPuts("#include <__xl_runtime/xl6_serialization_io.h>\n", out);
    if (writer) xl6OutputPuts("#include <__xl_runtime/xl6_serialization_writer.h>\n", out);
    for (p = 0; p < module->persistenceProjectPlanCount; ++p) {
        const XL6_IrPersistenceProjectPlan* plan = &module->persistenceProjectPlans[p];
        int open = 0;
        for (s = 0; s < plan->schemaCount; ++s) {
            size_t index = plan->firstSchema + s;
            const XL6_IrPersistenceProjectSchema* schema = &module->persistenceProjectSchemas[index];
            if (schema->fieldCount != 0) {
                xl6OutputPrintf(out, "static const XL6_PersistenceFieldV1 xlir_persist_fields_%lu[] = {\n", (unsigned long)index);
                for (f = 0; f < schema->fieldCount; ++f) {
                    const XL6_IrPersistenceProjectField* field = &module->persistenceProjectFields[schema->firstField + f];
                    uint32 flags = 0;
                    if (field->omitFlags & XL6_IR_FIELD_PERSIST_OMIT_NULL) flags |= 1;
                    if (field->omitFlags & XL6_IR_FIELD_PERSIST_OMIT_EMPTY) flags |= 2;
                    xl6OutputPuts("    {", out);
                    xl6IrCgenEmitPersistenceByteView(out, module->classFields[field->classField].name, 0);
                    xl6OutputPuts(", ", out);
                    xl6IrCgenEmitPersistenceByteView(out, field->targetKey,
                        field->targetKeyAbi == XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL);
                    xl6OutputPrintf(out, ", %u},\n", (unsigned int)flags);
                }
                xl6OutputPuts("};\n", out);
            }
            if (schema->dynamicDestroySymbol != 0) {
                open = 1;
                const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, schema->dynamicDestroySymbol);
                if (!destroy || destroy->nativeHandlePolicyAbi != XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1 ||
                    !destroy->nativeHandlePolicyCName)
                    return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                        "XLIR Open projection requires an explicit resident handle policy");
                xl6OutputPrintf(out, "static const XL6_PersistenceOpenV1 xlir_persist_open_%lu = {", (unsigned long)index);
                xl6IrCgenEmitPersistenceByteView(out, module->classFields[schema->dynamicField].name, 0);
                xl6OutputPuts(", ", out);
                xl6IrCgenEmitPersistenceByteView(out, module->classFields[schema->dynamicHandleField].name, 0);
                xl6OutputPuts("};\n", out);
            }
        }
        if (plan->schemaCount != 0) {
            xl6OutputPrintf(out, "static const XL6_PersistenceSchemaV1 xlir_persist_schemas_%u[] = {\n", (unsigned int)plan->format);
            for (s = 0; s < plan->schemaCount; ++s) {
                size_t index = plan->firstSchema + s;
                const XL6_IrPersistenceProjectSchema* schema = &module->persistenceProjectSchemas[index];
                xl6OutputPrintf(out, "    {UINT64_C(0x%016llX), %u, %lu, ",
                    (unsigned long long)schema->identity, schema->disabled ? 1u : 0u, (unsigned long)schema->fieldCount);
                if (schema->fieldCount) xl6OutputPrintf(out, "xlir_persist_fields_%lu", (unsigned long)index);
                else xl6OutputPuts("NULL", out);
                xl6OutputPuts(", ", out);
                if (schema->dynamicDestroySymbol) xl6OutputPrintf(out, "&xlir_persist_open_%lu", (unsigned long)index);
                else xl6OutputPuts("NULL", out);
                xl6OutputPuts("},\n", out);
            }
            xl6OutputPuts("};\n", out);
        }
        if (plan->sequenceCount != 0) {
            xl6OutputPrintf(out, "static const xvaluegraphsequencev1 xlir_persist_sequences_%u[] = {\n", (unsigned int)plan->format);
            for (s = 0; s < plan->sequenceCount; ++s) {
                const XL6_IrPersistenceProjectSequence* sequence = &module->persistenceProjectSequences[plan->firstSequence + s];
                xl6OutputPrintf(out, "    {UINT64_C(0x%016llX), %s},\n", (unsigned long long)sequence->identity,
                    sequence->sourceAbi == XL6_IR_COLLECTION_LIST ? "XVALUE_INT_MAP" : "XVALUE_SET");
            }
            xl6OutputPuts("};\n", out);
        }
        xl6OutputPrintf(out, "static const XL6_PersistencePlanV1 xlir_persist_plan_%u = {sizeof(XL6_PersistencePlanV1), %lu, ",
            (unsigned int)plan->format, (unsigned long)plan->schemaCount);
        if (plan->schemaCount) xl6OutputPrintf(out, "xlir_persist_schemas_%u", (unsigned int)plan->format);
        else xl6OutputPuts("NULL", out);
        xl6OutputPuts(open ? ", xl6PersistenceProjectOpenV1, " : ", NULL, ", out);
        if (plan->sequenceCount) xl6OutputPrintf(out, "xlir_persist_sequences_%u", (unsigned int)plan->format);
        else xl6OutputPuts("NULL", out);
        xl6OutputPrintf(out, ", %lu};\n", (unsigned long)plan->sequenceCount);
    }
    return !xl6OutputHasFailed(out);
}
