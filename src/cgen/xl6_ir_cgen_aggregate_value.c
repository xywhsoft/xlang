/* Exact trivial aggregate operations share one renderer in synchronous,
 * asynchronous and generator bodies. Representation, field identity and the
 * bounds proof have already been checked by XLIR. */
static int xl6IrCgenEmitAggregateValueInstruction(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    switch (instruction->kind) {
    case XL6_IR_INSTR_C_ARRAY_BOUNDS: {
        const XL6_IrInstruction* array = xl6IrCgenValueDefinition(module,
            module->operands[instruction->firstOperand], NULL);
        const XL6_IrType* type = xl6IrTypeGet(&module->types, array->type);
        unsigned index = (unsigned)module->operands[instruction->firstOperand + 1];
        xl6OutputPrintf(out, "    v%u = v%u >= 0 && (uint64_t)v%u < UINT64_C(%llu);\n",
            (unsigned)instruction->result, index, index, (unsigned long long)type->nativeArrayExtent);
        break;
    }
    case XL6_IR_INSTR_C_ARRAY_GET:
        xl6OutputPrintf(out, "    v%u = v%u.Elements[(size_t)v%u];\n",
            (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand],
            (unsigned)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_INSTR_C_ARRAY_SET:
        xl6OutputPrintf(out, "    v%u = v%u;\n    v%u.Elements[(size_t)v%u] = v%u;\n",
            (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand],
            (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand + 1],
            (unsigned)module->operands[instruction->firstOperand + 2]);
        break;
    case XL6_IR_INSTR_AGGREGATE_ZERO:
        xl6OutputPrintf(out, "    v%u = (%s){0};\n",
            (unsigned int)instruction->result,
            xl6IrCgenType(module, instruction->type));
        break;
    case XL6_IR_INSTR_AGGREGATE_GET: {
        const XL6_IrAggregateField* field =
            instruction->elementIndex < module->aggregateFieldCount
                ? &module->aggregateFields[instruction->elementIndex] : NULL;
        if ( field == NULL || field->cName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR aggregate read has no exact C member ABI");
        if (xl6IrTypeGet(&module->types, field->type)->cAbi == XL6_IR_C_ABI_C_ARRAY) {
            xl6OutputPrintf(out, "    memcpy(&v%u, &v%u.%s, sizeof(v%u));\n",
                (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand],
                field->cName, (unsigned)instruction->result);
            break;
        }
        xl6OutputPrintf(out, "    v%u = v%u.%s;\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            field->cName);
        break;
    }
    case XL6_IR_INSTR_AGGREGATE_SET: {
        const XL6_IrAggregateField* field =
            instruction->elementIndex < module->aggregateFieldCount
                ? &module->aggregateFields[instruction->elementIndex] : NULL;
        if ( field == NULL || field->cName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR aggregate write has no exact C member ABI");
        if (xl6IrTypeGet(&module->types, field->type)->cAbi == XL6_IR_C_ABI_C_ARRAY) {
            xl6OutputPrintf(out, "    v%u = v%u;\n    memcpy(&v%u.%s, &v%u, sizeof(v%u));\n",
                (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand],
                (unsigned)instruction->result, field->cName,
                (unsigned)module->operands[instruction->firstOperand + 1],
                (unsigned)module->operands[instruction->firstOperand + 1]);
            break;
        }
        xl6OutputPrintf(out, "    v%u = v%u;\n    v%u.%s = v%u;\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)instruction->result,
            field->cName,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    }
    default:
        return xl6IrCgenDiag(diags, module, instruction->origin, "unresolved aggregate value instruction");
    }
    return !xl6OutputHasFailed(out);
}
