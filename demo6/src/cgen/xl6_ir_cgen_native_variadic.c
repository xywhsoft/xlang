/* A C variadic function cannot forward its tail through a portable C thunk.
 * Keep its actual entry in the existing leased closure, expand the native
 * call at each typed use, and admit the result there. Only this callable
 * family reads value-specific text ownership; ordinary strings are unchanged. */
static int xl6IrCgenEmitNativeVariadicEntry(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrInstruction* instruction,
    size_t index, const char* resultType)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types,instruction->calleeType);
    size_t fixed = signature->paramCount - 1;
    XL6_IrValueId closure = module->operands[instruction->firstOperand];
    xl6OutputPrintf(out,"((%s (*)(",resultType);
    for (size_t i = 0; i < fixed; ++i) {
        const XL6_IrType* type = xl6IrTypeGet(&module->types,signature->params[i]);
        const char* physical = type->cAbi == XL6_IR_C_ABI_MANAGED_STRING
            ? (signature->parameterAbis[i] == XL6_IR_PARAMETER_ABI_BORROW ? "const char*" : "char*")
            : xl6IrTypeIsCOwned(type) ? (type->cAbi == XL6_IR_C_ABI_C_STRING ? "xstrview" : "xbytesview")
            : xl6IrCgenType(module,signature->params[i]);
        if (!physical) return 0;
        xl6OutputPrintf(out,"%s%s",i ? ", " : "",physical);
    }
    xl6OutputPrintf(out,", ...))v%u->invoke)(",(unsigned)closure);
    for (size_t i = 0; i + 1 < instruction->operandCount; ++i) {
        XL6_IrValueId value = module->operands[instruction->firstOperand + i + 1];
        const XL6_IrInstruction* definition = xl6IrCgenValueDefinition(module,value,NULL);
        const XL6_IrType* type = xl6IrTypeGet(&module->types,definition->type);
        if (i) xl6OutputPuts(", ",out);
        if (i < fixed && xl6IrTypeIsCOwned(type))
            xl6OutputPrintf(out,"%s(v%u)",type->cAbi == XL6_IR_C_ABI_C_STRING ? "xl6CStringViewV1" : "xl6CBytesViewV1",(unsigned)value);
        else if (i >= fixed && xl6IrCVarargPhysicalAbi(module,definition->type) == XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW)
            xl6OutputPrintf(out,"ct%lu_%lu",(unsigned long)index,(unsigned long)i);
        else xl6OutputPrintf(out,"v%u%s",(unsigned)value,
            type->cAbi == XL6_IR_C_ABI_MANAGED_STRING ? ".Data" : "");
    }
    xl6OutputPuts(")",out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitNativeVariadicCall(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrInstruction* instruction,
    size_t index, XL6_DiagList* diags)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types,instruction->calleeType);
    const XL6_IrType* result = xl6IrTypeGet(&module->types,instruction->type);
    const char* physical = result ? xl6IrCgenType(module,instruction->type) : "void";
    size_t fixed = signature->paramCount - 1, texts = 0;
    unsigned closure = (unsigned)module->operands[instruction->firstOperand];
    unsigned value = (unsigned)instruction->result;
    if (!fixed || !physical)
        return xl6IrCgenDiag(diags,module,instruction->origin,"native variadic closure signature is unresolved");
    xl6OutputPrintf(out,
        "    XL6_ClosureScope closure_scope_%lu = {0};\n"
        "    xlir_closure_enter(&closure_scope_%lu, v%u);\n"
        "    bool c%lu = false;\n    {\n"
        "    xl6_foreign_error_scope fs%lu = {0}; xl6_foreign_error_result fr%lu = {false,NULL};\n",
        (unsigned long)index,(unsigned long)index,closure,(unsigned long)index,
        (unsigned long)index,(unsigned long)index);
    if (result) xl6OutputPrintf(out,"    v%u = (%s){0};\n",value,physical);
    /* A saved no-raise scalar/owned-nullable producer needs no foreign
     * scope unless preparing text can fail. Borrowed text still copies and
     * may allocate, independently of the producer's effect annotation. */
    for (size_t i = fixed; i + 1 < instruction->operandCount; ++i) {
        const XL6_IrInstruction* argument = xl6IrCgenValueDefinition(module,
            module->operands[instruction->firstOperand + i + 1],NULL);
        if (xl6IrCVarargPhysicalAbi(module,argument->type) == XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW) ++texts;
    }
    if (texts) xl6OutputPuts("    bool checked=true;\n",out);
    else xl6OutputPrintf(out,"    bool checked=v%u->call_parameter_defaults[%lu].i64_value != 0;\n",closure,(unsigned long)fixed);
    xl6OutputPrintf(out,"    if (!checked || xl6ForeignCallBegin(&fs%lu)) {\n",(unsigned long)index);
    texts=0;
    for (size_t i = fixed; i + 1 < instruction->operandCount; ++i) {
        XL6_IrValueId argument = module->operands[instruction->firstOperand + i + 1];
        const XL6_IrInstruction* definition = xl6IrCgenValueDefinition(module,argument,NULL);
        if (xl6IrCVarargPhysicalAbi(module,definition->type) != XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW) continue;
        xl6OutputPrintf(out,"    const char* ct%lu_%lu = xlir_string_cstr(v%u);\n",
            (unsigned long)index,(unsigned long)i,(unsigned)argument);
        ++texts;
    }
    if (texts) {
        xl6OutputPuts("    if (",out); size_t emitted = 0;
        for (size_t i = fixed; i + 1 < instruction->operandCount; ++i) {
            const XL6_IrInstruction* argument = xl6IrCgenValueDefinition(module,
                module->operands[instruction->firstOperand + i + 1],NULL);
            if (xl6IrCVarargPhysicalAbi(module,argument->type) != XL6_IR_C_VARARG_PHYSICAL_C_TEXT_BORROW) continue;
            xl6OutputPrintf(out,"%sct%lu_%lu != NULL",emitted++ ? " && " : "",(unsigned long)index,(unsigned long)i);
        }
        xl6OutputPuts(") {\n",out);
    }
    if (result && result->cAbi == XL6_IR_C_ABI_MANAGED_STRING) {
        /* Different pointer qualifiers retain their exact C function type.
         * Rebinding owned and borrowed producers of the same language type
         * remains safe; the flag belongs to the callable, never the string. */
        xl6OutputPrintf(out,"    bool owned = v%u->call_parameter_defaults[%lu].boolean_value; const char* raw;\n    if (owned) raw = ",closure,(unsigned long)fixed);
        if (!xl6IrCgenEmitNativeVariadicEntry(out,module,instruction,index,"char*")) return 0;
        xl6OutputPuts("; else raw = ",out);
        if (!xl6IrCgenEmitNativeVariadicEntry(out,module,instruction,index,"const char*")) return 0;
        xl6OutputPrintf(out,";\n    if (checked && (fs%lu.failed || xrtGetError() != NULL)) {\n"
            "        if (owned) { xerror* saved=xrtTakeError(); xrtFree((void*)raw); xrtSetErrorTake(saved); }\n"
            "    } else {\n"
            "        if (owned) v%u=(xlir_string){raw,raw ? strlen(raw) : 0};\n"
            "        else v%u=xlir_string_from_cstr(raw);\n"
            "        if (v%u.Size > INT64_MAX) { xrtFree((void*)v%u.Data); v%u=(xlir_string){0}; xrtSetErrorKind(XERR_RANGE); }\n",
            (unsigned long)index,value,value,value,value,value);
        if (result->kind != XL6_IR_TYPE_NULLABLE)
            xl6OutputPrintf(out,"        else if (!v%u.Data && !xrtGetError()) xrtSetErrorKind(XERR_VALUE);\n",value);
        xl6OutputPuts("    }\n",out);
    } else {
        if (result) xl6OutputPrintf(out,"    v%u = ",value);
        else xl6OutputPuts("    ",out);
        if (!xl6IrCgenEmitNativeVariadicEntry(out,module,instruction,index,physical)) return 0;
        xl6OutputPuts(";\n",out);
    }
    if (texts) xl6OutputPuts("    }\n",out);
    xl6OutputPrintf(out,"    if (checked) fr%lu=xl6ForeignCallEnd(&fs%lu); else fr%lu.ok=true;\n"
        "    } else fr%lu.error=xrtTakeError();\n"
        "    c%lu=fr%lu.ok; if (!c%lu) xrtSetErrorTake(fr%lu.error);\n"
        "    xlir_closure_leave(&closure_scope_%lu);\n    }\n",
        (unsigned long)index,(unsigned long)index,(unsigned long)index,(unsigned long)index,
        (unsigned long)index,(unsigned long)index,(unsigned long)index,(unsigned long)index,(unsigned long)index);
    return !xl6OutputHasFailed(out);
}
