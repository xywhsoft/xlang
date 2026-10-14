/* Checked and wrapping conversions consume canonical source/target ABI
 * enums.  Effects and error edges remain in XLIR; all execution modes use
 * these same physical helper calls. */
static int xl6IrCgenSpecialF64RuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_F64_NAN ||
           operation == XL6_IR_RUNTIME_F64_POSITIVE_INFINITY;
}

static int xl6IrCgenEmitSpecialF64(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const char* bits;
    if (!xl6IrCgenSpecialF64RuntimeOpIs(instruction->runtimeOp)) return 0;
    if (!xl6IrCgenRuntimeSpecialF64Abi(module, instruction)) return -1;
    /* The language's f64 constants are binary64 values, not locale/CRT parsing
     * requests. Bit construction also works with the Windows TCC host CRT. */
    bits = instruction->runtimeOp == XL6_IR_RUNTIME_F64_NAN
        ? "0x7ff8000000000000" : "0x7ff0000000000000";
    xl6OutputPrintf(out, "    v%u = xlir_f64_from_bits(UINT64_C(%s));\n",
        (unsigned int)instruction->result, bits);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenScalarConversionRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_STRING_TO_F64_CHECKED ||
           operation == XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT ||
           operation == XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT ||
           operation == XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT ||
           operation == XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED ||
           operation == XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED ||
           operation == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED ||
           operation == XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING;
}

static int xl6IrCgenScalarConversionRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    if ( instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ) return 0;
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_STRING_TO_F64_CHECKED:
    case XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT:
    case XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT:
    case XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT:
        return xl6IrCgenRuntimeCheckedFloatConvertAbi(module, instruction);
    case XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED:
        return xl6IrCgenRuntimeCheckedTimeConvertAbi(module, instruction);
    case XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED:
        return xl6IrCgenRuntimeIntegerConvertAbi(module, instruction);
    case XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING:
        return xl6IrCgenRuntimeIntegerWrappingAbi(module, instruction);
    default:
        return 0;
    }
}

static int xl6IrCgenEmitScalarConversionRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if ( instruction == NULL ||
         !xl6IrCgenScalarConversionRuntimeOpIs(instruction->runtimeOp) ) return 0;
    if ( !xl6IrCgenScalarConversionRuntimeAbi(module, instruction) ) return -1;
    if (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT ||
        instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT ||
        instruction->runtimeOp == XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT) {
        xl6OutputPrintf(out, "    bool projection_ok_%u = %s(v%u, &v%u);\n",
            (unsigned)instruction->result,
            instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT
                ? "xl6Float32TextConvertV1"
                : (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT
                    ? "xl6Float32ValueConvertV1" : "xl6Float32ConvertV1"),
            (unsigned)module->operands[instruction->firstOperand], (unsigned)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_STRING_TO_F64_CHECKED ) {
        if ( !xl6IrCgenRuntimeCheckedFloatConvertAbi(module, instruction) )
            return -1;
        xl6OutputPrintf(out,
            "    v%u = xlir_string_to_f64_checked(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
    } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED ||
                instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED ) {
        const char* helper = instruction->runtimeOp ==
                XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED
            ? "xlir_string_to_time_checked"
            : "xlir_xvalue_to_time_checked";
        if ( !xl6IrCgenRuntimeCheckedTimeConvertAbi(module, instruction) )
            return -1;
        xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
            (unsigned int)instruction->result, helper,
            (unsigned int)module->operands[instruction->firstOperand]);
    } else if ( instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED ) {
        XL6_IrValueId sourceValue;
        if ( !xl6IrCgenRuntimeIntegerConvertAbi(module, instruction) )
            return -1;
        sourceValue = module->operands[instruction->firstOperand];
        if ( instruction->integerTarget ==
                 XL6_IR_INTEGER_TARGET_UNICODE_SCALAR ) {
            if (instruction->integerSourceAbi==XL6_IR_INTEGER_SOURCE_XVALUE) {
                xl6OutputPrintf(out,"    bool projection_ok_%u = xl6CharValueCheckedV1(v%u, 0x%016llXULL, false, &v%u);\n",
                    (unsigned)instruction->result,(unsigned)sourceValue,
                    (unsigned long long)xl6IrTypeGet(&module->types,instruction->type)->typeValueId,
                    (unsigned)instruction->result);
                return xl6OutputHasFailed(out) ? -1 : 1;
            }
            const char* helper = instruction->integerSourceAbi ==
                    XL6_IR_INTEGER_SOURCE_I64
                ? "xlir_i64_to_char_checked"
                : "xlir_u64_to_char_checked";
            xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
                (unsigned int)instruction->result, helper,
                (unsigned int)sourceValue);
        } else if ( instruction->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_64 ) {
            const char* format =
                instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_I64
                    ? "    v%u = xlir_i64_to_u64_checked(v%u);\n"
                : instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_U64
                    ? "    v%u = v%u;\n"
                : instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_F64
                    ? "    v%u = xlir_f64_to_u64_checked(v%u);\n"
                : instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_MANAGED_STRING
                    ? "    v%u = xlir_string_to_u64_checked(v%u);\n"
                    : "    v%u = xlir_xvalue_to_u64_checked(v%u);\n";
            xl6OutputPrintf(out, format,
                (unsigned int)instruction->result,
                (unsigned int)sourceValue);
        } else {
            const char* format =
                instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_F64
                    ? "    v%u = xlir_f64_to_i64_checked(v%u, %d);\n"
                : instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_MANAGED_STRING
                    ? "    v%u = xlir_string_to_i64_checked(v%u, %d);\n"
                    : "    v%u = xlir_xvalue_to_i64_checked(v%u, %d);\n";
            xl6OutputPrintf(out, format,
                            (unsigned int)instruction->result,
                            (unsigned int)sourceValue,
                            (int)instruction->integerTarget);
        }
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING ) {
        XL6_IrValueId sourceValue;
        if ( !xl6IrCgenRuntimeIntegerWrappingAbi(module, instruction) )
            return -1;
        sourceValue = module->operands[instruction->firstOperand];
        if ( instruction->integerTarget ==
             XL6_IR_INTEGER_TARGET_UNSIGNED_64 )
            xl6OutputPrintf(out, "    v%u = (uint64_t)v%u;\n",
                (unsigned int)instruction->result,
                (unsigned int)sourceValue);
        else
            xl6OutputPrintf(out,
                "    v%u = xlir_integer_wrap_to_i64((uint64_t)v%u, %d);\n",
                (unsigned int)instruction->result,
                (unsigned int)sourceValue,
                (int)instruction->integerTarget);
    }
    return xl6OutputHasFailed(out) ? -1 : 1;
}
