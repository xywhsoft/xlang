/* Physical dynamic comparison emission is execution-mode independent. Source
 * operators, boxing and lifetimes have already been resolved and verified. */
static int xl6IrCgenEmitDynamicCompare(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const char* helper;
    int relation = -1;
    if (!xl6IrDynamicCompareRuntimeOpIs(instruction->runtimeOp)) return 0;
    if (!xl6IrDynamicCompareRuntimeInstructionCompatible(module, instruction)) return -1;
    switch (instruction->runtimeOp) {
    case XL6_IR_RUNTIME_XVALUE_EQUAL: helper = "xlir_xvalue_equal"; break;
    case XL6_IR_RUNTIME_XVALUE_APPROX: helper = "xlir_xvalue_approx"; break;
    case XL6_IR_RUNTIME_XVALUE_ORDER_LT: relation = 0; helper = "xlir_xvalue_order"; break;
    case XL6_IR_RUNTIME_XVALUE_ORDER_LE: relation = 1; helper = "xlir_xvalue_order"; break;
    case XL6_IR_RUNTIME_XVALUE_ORDER_GT: relation = 2; helper = "xlir_xvalue_order"; break;
    case XL6_IR_RUNTIME_XVALUE_ORDER_GE: relation = 3; helper = "xlir_xvalue_order"; break;
    default: return -1;
    }
    xl6OutputPrintf(out, "    v%u = %s(v%u, v%u",
        (unsigned int)instruction->result, helper,
        (unsigned int)module->operands[instruction->firstOperand],
        (unsigned int)module->operands[instruction->firstOperand + 1]);
    if (relation >= 0) xl6OutputPrintf(out, ", %d", relation);
    xl6OutputPuts(");\n", out);
    return xl6OutputHasFailed(out) ? -1 : 1;
}
