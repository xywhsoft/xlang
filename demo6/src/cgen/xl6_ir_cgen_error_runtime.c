/* One physical emission contract for borrowed error projections, shared by
 * ordinary functions, async continuations and generator resume programs. */
typedef struct XL6_IrCgenErrorProjection {
    XL6_IrRuntimeOp operation;
    XL6_IrCAbi result;
    const char* prefix;
    const char* suffix;
} XL6_IrCgenErrorProjection;

static const XL6_IrCgenErrorProjection* xl6IrCgenErrorProjection(
    XL6_IrRuntimeOp operation)
{
    static const XL6_IrCgenErrorProjection projections[] = {
        {XL6_IR_RUNTIME_ERROR_MESSAGE_COPY, XL6_IR_C_ABI_MANAGED_STRING, "xlir_string_from_cstr(xrtErrorMessage(", "))"},
        {XL6_IR_RUNTIME_ERROR_FILE_COPY, XL6_IR_C_ABI_MANAGED_STRING, "xlir_string_from_cstr(xrtErrorFile(", "))"},
        {XL6_IR_RUNTIME_ERROR_DOMAIN_COPY, XL6_IR_C_ABI_MANAGED_STRING, "xlir_string_from_cstr(xrtErrorDomain(", "))"},
        {XL6_IR_RUNTIME_ERROR_OPERATION_COPY, XL6_IR_C_ABI_MANAGED_STRING, "xlir_string_from_cstr(xrtErrorOperation(", "))"},
        {XL6_IR_RUNTIME_ERROR_DATA_COPY, XL6_IR_C_ABI_MANAGED_STRING, "xlir_string_from_cstr(xrtErrorData(", "))"},
        {XL6_IR_RUNTIME_ERROR_KIND_COPY, XL6_IR_C_ABI_MANAGED_STRING, "xlir_string_from_cstr(xlir_error_kind_name(", "))"},
        {XL6_IR_RUNTIME_ERROR_CODE, XL6_IR_C_ABI_I64, "(int64_t)xrtErrorCode(", ")"},
        {XL6_IR_RUNTIME_ERROR_LINE, XL6_IR_C_ABI_I64, "(int64_t)xrtErrorLine(", ")"},
        {XL6_IR_RUNTIME_ERROR_COLUMN, XL6_IR_C_ABI_I64, "(int64_t)xrtErrorColumn(", ")"},
        {XL6_IR_RUNTIME_ERROR_SYSTEM_CODE, XL6_IR_C_ABI_I64, "(int64_t)xrtErrorSystemCode(", ")"},
        {XL6_IR_RUNTIME_ERROR_CAUSE_REF, XL6_IR_C_ABI_RUNTIME_ERROR_PTR, "xrtErrorRef(xrtErrorCause(", "))"},
        {XL6_IR_RUNTIME_ERROR_KIND_IS, XL6_IR_C_ABI_BOOL, "xlir_error_kind_is(", ")"}
    };
    size_t i;
    for (i = 0; i < sizeof(projections) / sizeof(projections[0]); ++i)
        if (projections[i].operation == operation) return &projections[i];
    return NULL;
}

static int xl6IrCgenErrorProjectionAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrCgenErrorProjection* projection;
    const XL6_IrInstruction* operand;
    const XL6_IrType* input;
    const XL6_IrType* result;
    if (module == NULL || instruction == NULL ||
        (projection = xl6IrCgenErrorProjection(instruction->runtimeOp)) == NULL ||
        instruction->result == XL6_IR_VALUE_INVALID || instruction->operandCount != 1 ||
        instruction->firstOperand >= module->operandCount ||
        ((instruction->runtimeOp == XL6_IR_RUNTIME_ERROR_KIND_IS) !=
            (instruction->immediateText != NULL))) return 0;
    operand = xl6IrCgenValueDefinition(module,
        module->operands[instruction->firstOperand], NULL);
    input = operand != NULL ? xl6IrTypeGet(&module->types, operand->type) : NULL;
    result = xl6IrTypeGet(&module->types, instruction->type);
    return input != NULL && input->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
           result != NULL && result->cAbi == projection->result;
}

static int xl6IrCgenEmitErrorProjection(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrCgenErrorProjection* projection;
    if (instruction == NULL ||
        (projection = xl6IrCgenErrorProjection(instruction->runtimeOp)) == NULL) return 0;
    if (!xl6IrCgenErrorProjectionAbi(module, instruction)) return -1;
    xl6OutputPrintf(out, "    v%u = %sv%u",
        (unsigned int)instruction->result, projection->prefix,
        (unsigned int)module->operands[instruction->firstOperand]);
    if (instruction->runtimeOp == XL6_IR_RUNTIME_ERROR_KIND_IS)
        xl6OutputPrintf(out, ", %s", instruction->immediateText);
    xl6OutputPrintf(out, "%s;\n", projection->suffix);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static void xl6IrCgenDeclareUnwindScope(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrFunction* function)
{
    size_t i;
    /* Verified nonoverlapping activation-local regions share one stack slot. */
    for (i = function->firstInstruction; i < function->firstInstruction + function->instructionCount; ++i)
        if (module->instructions[i].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN ||
            module->instructions[i].kind == XL6_IR_INSTR_CLEANUP_BEGIN ||
            module->instructions[i].kind == XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN) {
            xl6OutputPuts("    xl6_error_scope xlir_unwind = {0};\n", out);
            break;
        }
}

static int xl6IrCgenErrorStateInstructionAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* operand = NULL;
    const XL6_IrType* type;
    if (module == NULL || instruction == NULL ||
        instruction->firstOperand > module->operandCount ||
        instruction->operandCount > module->operandCount - instruction->firstOperand) return 0;
    if (instruction->operandCount == 1)
        operand = xl6IrCgenValueDefinition(module,
            module->operands[instruction->firstOperand], NULL);
    switch (instruction->kind) {
    case XL6_IR_INSTR_CLEANUP_FAIL:
    case XL6_IR_INSTR_CLEANUP_CAPTURE:
        return instruction->errorScope != 0 && instruction->result == XL6_IR_VALUE_INVALID &&
            instruction->type == XL6_IR_TYPE_INVALID && instruction->operandCount == 0;
    case XL6_IR_INSTR_CLEANUP_BEGIN:
    case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
    case XL6_IR_INSTR_CLEANUP_END:
        type = xl6IrTypeGet(&module->types, instruction->type);
        return instruction->errorScope != 0 && instruction->errorScope <= module->instructionCount &&
            (module->instructions[instruction->errorScope - 1].kind == XL6_IR_INSTR_CLEANUP_BEGIN ||
             module->instructions[instruction->errorScope - 1].kind == XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN) &&
            instruction->operandCount == 0 &&
            (instruction->kind != XL6_IR_INSTR_CLEANUP_END
                ? instruction->result == XL6_IR_VALUE_INVALID && instruction->type == XL6_IR_TYPE_INVALID
                : instruction->result != XL6_IR_VALUE_INVALID && type != NULL && type->cAbi == XL6_IR_C_ABI_BOOL);
    case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
    case XL6_IR_INSTR_ERROR_UNWIND_END:
        return instruction->errorScope != 0 && instruction->errorScope <= module->instructionCount &&
            module->instructions[instruction->errorScope - 1].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN &&
            instruction->result == XL6_IR_VALUE_INVALID && instruction->type == XL6_IR_TYPE_INVALID &&
            instruction->operandCount == 0;
    case XL6_IR_INSTR_ERROR_TAKE:
        type = xl6IrTypeGet(&module->types, instruction->type);
        return instruction->operandCount == 0 && instruction->result != XL6_IR_VALUE_INVALID &&
               type != NULL && type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR;
    case XL6_IR_INSTR_ERROR_SET_TAKE:
        type = operand != NULL ? xl6IrTypeGet(&module->types, operand->type) : NULL;
        return instruction->operandCount == 1 && instruction->result == XL6_IR_VALUE_INVALID &&
               type != NULL && type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR;
    case XL6_IR_INSTR_ERROR_CLEAR:
        return instruction->operandCount == 0 && instruction->result == XL6_IR_VALUE_INVALID;
    case XL6_IR_INSTR_ERROR_RAISE:
        type = operand != NULL ? xl6IrTypeGet(&module->types, operand->type) : NULL;
        return instruction->immediateText != NULL && instruction->sourceSpan.line > 0 &&
               instruction->sourceSpan.column > 0 && instruction->result == XL6_IR_VALUE_INVALID &&
               (instruction->operandCount == 0 ||
                (instruction->operandCount == 1 && type != NULL && type->cAbi == XL6_IR_C_ABI_MANAGED_STRING));
    default: return 0;
    }
}

/* Ownership transfers are identical in all execution modes. Control-flow
 * branches and frame placement remain explicit machine-program facts. */
static int xl6IrCgenEmitErrorStateInstruction(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    if (!xl6IrCgenErrorStateInstructionAbi(module, instruction)) return 0;
    switch (instruction->kind) {
    case XL6_IR_INSTR_CLEANUP_FAIL:
        xl6OutputPuts("    xl6ErrorScopeCaptureFailure(&xlir_unwind);\n", out);
        break;
    case XL6_IR_INSTR_CLEANUP_CAPTURE:
        xl6OutputPuts("    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&xlir_unwind);\n", out);
        break;
    case XL6_IR_INSTR_CLEANUP_BEGIN:
        xl6OutputPuts("    if (!xl6ErrorScopeBegin(&xlir_unwind)) abort();\n", out);
        break;
    case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
        xl6OutputPuts("    if (!xl6ErrorScopeBeginCleanup(&xlir_unwind, false)) abort();\n", out);
        break;
    case XL6_IR_INSTR_CLEANUP_END:
        /* ErrorScopeEnd restores the diagnostic that existed before normal
         * cleanup began. Publish the cleanup-owned error only on failure;
         * writing NULL on success would erase that restored diagnostic. */
        xl6OutputPrintf(out, "    { xl6_error_result cleanup = xl6ErrorScopeEnd(&xlir_unwind);\n"
            "      v%u = cleanup.ok; if (!cleanup.ok) xrtSetErrorTake(cleanup.error); }\n",
            (unsigned int)instruction->result);
        break;
    case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
        /* Failure to establish OS error-context isolation is fatal, not a
         * policy for language exceptions or diagnostic allocation failure. */
        xl6OutputPuts("    { xerror* original = xrtTakeError();\n"
            "      if (!xl6ErrorScopeBegin(&xlir_unwind)) abort();\n"
            "      xrtSetErrorTake(original); xl6ErrorScopeCaptureFailure(&xlir_unwind); }\n", out);
        break;
    case XL6_IR_INSTR_ERROR_UNWIND_END:
        xl6OutputPuts("    { xl6_error_result original = xl6ErrorScopeEnd(&xlir_unwind);\n"
            "      xrtSetErrorTake(original.error); }\n", out);
        break;
    case XL6_IR_INSTR_ERROR_TAKE:
        xl6OutputPrintf(out, "    v%u = xrtTakeError();\n", (unsigned int)instruction->result);
        break;
    case XL6_IR_INSTR_ERROR_SET_TAKE:
        xl6OutputPrintf(out, "    xrtSetErrorTake(v%u);\n",
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_INSTR_ERROR_CLEAR:
        xl6OutputPuts("    xrtClearError();\n", out);
        break;
    case XL6_IR_INSTR_ERROR_RAISE:
        if (instruction->operandCount == 0)
            xl6OutputPrintf(out, "    xrtSetErrorTake(xlir_error_create_at(%s, %s, %d, %d));\n",
                instruction->immediateText, instruction->immediateText,
                instruction->sourceSpan.line, instruction->sourceSpan.column);
        else
            xl6OutputPrintf(out, "    xrtSetErrorTake(xlir_error_create_at(%s, v%u.Data, %d, %d));\n    xlir_string_drop(v%u);\n",
                instruction->immediateText, (unsigned int)module->operands[instruction->firstOperand],
                instruction->sourceSpan.line, instruction->sourceSpan.column,
                (unsigned int)module->operands[instruction->firstOperand]);
        break;
    default: return 0;
    }
    return !xl6OutputHasFailed(out);
}
