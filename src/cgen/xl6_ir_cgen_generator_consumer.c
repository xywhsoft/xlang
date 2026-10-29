/* The same verified generator consumer operations execute in ordinary,
 * async and generator CFGs. Only the destination label namespace differs. */
static int xl6IrCgenEmitGeneratorConsumerInstruction(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    size_t index = (size_t)(instruction - module->instructions);
    switch (instruction->kind) {
    case XL6_IR_INSTR_GENERATOR_NEXT:
        xl6OutputPrintf(out, "    xlir_generator_step g%lu = xlir_generator_next(v%u);\n",
            (unsigned long)index, (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_INSTR_GENERATOR_ITEM_GET:
    case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
        xl6OutputPrintf(out, "    xlir_generator_item_get(v%u, &v%u, sizeof(v%u), %s);\n",
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)instruction->result, (unsigned int)instruction->result,
            instruction->kind == XL6_IR_INSTR_GENERATOR_ITEM_TAKE ? "true" : "false");
        break;
    default: return 0;
    }
    return !xl6OutputHasFailed(out);
}
static void xl6IrCgenEmitGeneratorNextTest(XL6_Output* out, size_t sourceIndex,
    XL6_IrValueId result, const char* prefix, size_t success, size_t failure)
{
    xl6OutputPrintf(out, "    if (g%lu.ok) { v%u = g%lu.value; goto %s%lu; } else goto %s%lu;\n",
        (unsigned long)sourceIndex, (unsigned int)result, (unsigned long)sourceIndex,
        prefix, (unsigned long)success, prefix, (unsigned long)failure);
}
