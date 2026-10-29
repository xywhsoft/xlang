static void xl6IrCgenEmitLanguageFunctionType(XL6_Output* out)
{
        xl6OutputPuts(
            "#ifndef XLIR_XRT_DECLARATIONS\n"
            "typedef struct xvalue xvalue;\n"
            "#endif\n"
            "typedef struct xl6_call_frame xl6_call_frame;\n"
            "typedef struct xl6_call_result xl6_call_result;\n"
            "struct xl6_func_sig;\n"
            "#ifndef XL6_FUNCTION_TYPEDEF\n"
            "#define XL6_FUNCTION_TYPEDEF\n"
            "#define XL6_FUNCTION_DEFINED\n"
            "typedef void (*XL6_NativeFunction)(void);\n"
            "typedef struct XL6_Function {\n"
            "    unsigned long long signature;\n"
            "    const struct xl6_func_sig* sig;\n"
            "    void* env;\n"
            "    void* invoke_env;\n"
            "    XL6_NativeFunction invoke;\n"
            "    XL6_NativeFunction c_entry;\n"
            "    xvalue* (*invoke_var)(void*, int, xvalue**);\n"
            "    bool (*invoke_xcall)(void*, xl6_call_frame*, xl6_call_result*);\n"
            "    void (*retain)(void*);\n"
            "    void (*release)(void*);\n"
            "} XL6_Function;\n"
            "#endif\n",
            out);
}

static int xl6IrCgenEmitRuntimePrelude(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if (module->abi.slotTransferAbi != XL6_IR_SLOT_TRANSFER_RESIDENT_ORDERED_WRITES_V1)
        return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR physical slot transfer ABI is unresolved");
    xl6OutputPuts("#include <__xl_runtime/xl6_slot.h>\n",out);
    if (module->exportCount != 0 ||
        module->abi.hostExecutionAbi == XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1)
        xl6OutputPuts("#include <__xl_runtime/xl6_code_lease.h>\n",out);
    if (needs->needsAsyncMachineRuntime || needs->needsGeneratorRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_capture.h>\n",out);
    if (needs->needsAsyncMachineRuntime || needs->needsAsyncTaskRuntime) {
        if (module->abi.asyncStorageAbi != XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V3)
            return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR async physical storage ABI is unresolved");
        xl6OutputPuts("#include <__xl_runtime/xl6_continuation_future.h>\n"
            "#define xlir_async_frame_allocate (xl6ContinuationFrameRuntimeV5Get()->Allocate)\n"
            "#define xlir_async_frame_dispose (xl6ContinuationFrameRuntimeV2Get()->Discard)\n",out);
    }
    /* One closed declaration surface for shared string support. Prelude
     * adapters may use it before the definitions emitted in runtime support. */
    if (needs->needsStringRuntime)
        xl6OutputPuts(
            "static xlir_string xlir_string_from_utf8(const char*, size_t);\n"
            "static xlir_string xlir_string_copy(xlir_string);\n"
            "static xlir_string xlir_string_from_cstr(const char*);\n"
            "static xlir_string xlir_string_take_cstr(char*);\n"
            "static xlir_string xlir_string_take_known(char*, size_t);\n"
            "static const char* xlir_string_cstr(xlir_string);\n"
            "static void xlir_string_drop(xlir_string);\n", out);
    if (needs->needsStringRuntime)
        xl6OutputPuts("extern bool xl6NullableStringEqualV1(xlir_string, xlir_string);\n",out);
    if ( needs->needsRuntimeTrap ) xl6OutputPuts("#include <stdlib.h>\n", out);
    if ( needs->needsCheckedI64Runtime || needs->needsWrappingIntegerRuntime ||
         needs->needsCheckedIntegerArithmeticRuntime ||
         needs->needsStackResourceRuntime ||
         needs->needsFutureAggregateAllArray || needs->needsFutureAggregateRaceArray )
        xl6OutputPuts("#include <limits.h>\n", out);
    if ( needs->needsErrorRuntime || needs->needsXCallRuntime || needs->needsDynamicMethodRuntime )
        xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("\n", out);
    if (needs->needsXValueIteratorRuntime) {
        if (needs->needsListValueIteratorRuntime)
            xl6OutputPuts("#include <__xl_runtime/xl6_list_value_iterator.h>\n",out);
        if (module->abi.valueIteratorStorageAbi != XL6_IR_VALUE_ITERATOR_STORAGE_RESIDENT_CHECKED_SNAPSHOT_V2)
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR value iterator storage ABI is unresolved");
        xl6OutputPuts(
            "#include <__xl_runtime/xl6_value_iterator.h>\n"
            "typedef XL6_ValueIterator xlir_xvalue_iterator;\n"
            "#define xlir_xvalue_iterator_create (xl6ValueIteratorRuntimeV2Get()->Create)\n"
            "#define xlir_xvalue_iterator_advance (xl6ValueIteratorRuntimeV2Get()->Advance)\n"
            "#define xlir_xvalue_iterator_destroy (xl6ValueIteratorRuntimeV2Get()->Destroy)\n"
            "#define xlir_xvalue_iterator_ownership (xl6ValueIteratorRuntimeV2Get()->Ownership)\n", out);
    }
    if (needs->needsAsyncErrorResult || needs->needsAsyncFutureResult ||
        needs->needsAsyncStringResult || needs->needsAsyncXValueResult ||
        needs->needsAsyncTaskRuntime || needs->needsAsyncReadyRuntime ||
        needs->needsAsyncMachineRuntime || needs->needsFutureContinuationRuntime ||
        needs->needsFutureAwaitRuntime || needs->needsXValueFutureRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_future_payload.h>\n", out);
    if ( needs->needsForeignErrorRuntime || needs->needsXCallRuntime ||
         needs->needsGeneratorRuntime )
        xl6IrCgenEmitForeignErrorDeclarations(out, module->exportCount != 0);
    if ( needs->needsClosureRuntime ) {
        if (module->abi.closureStorageAbi != xl6IrModuleClosureStorageRequirement(module))
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR closure storage ABI is unresolved");
        xl6OutputPuts("#include <__xl_runtime/xl6_closure.h>\n", out);
        if(module->abi.closureStorageAbi==XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3)
            xl6OutputPuts("#include <__xl_runtime/xl6_closure_capture_v3.h>\nextern void* xrtCalloc(size_t,size_t);\n",out);
        xl6OutputPuts("extern void* xrtMalloc(size_t);\nextern void xrtFree(void*);\n", out);
        xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
            ? "#define xlir_closure_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
            : "#define xlir_closure_owner NULL\n", out);
        xl6OutputPuts(
            "#define xlir_closure_new(...) (xl6ClosureRuntimeV2Get()->Create)(xlir_closure_owner, __VA_ARGS__)\n"
            "#define xlir_closure_env_drop (xl6ClosureRuntimeV2Get()->DropEnvironment)\n"
            "#define xlir_closure_retain (xl6ClosureRuntimeV1Get()->Retain)\n"
            "#define xlir_closure_release (xl6ClosureRuntimeV1Get()->Release)\n"
            "#define xlir_closure_ownership (xl6ClosureRuntimeV1Get()->Ownership)\n"
            "#define xlir_closure_enter (xl6ClosureRuntimeV1Get()->Enter)\n"
            "#define xlir_closure_leave (xl6ClosureRuntimeV1Get()->Leave)\n", out);
        if(module->abi.closureStorageAbi==XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3)
            xl6OutputPuts("#define xlir_closure_new_v3(...) (xl6ClosureRuntimeV3Get()->Create)(xlir_closure_owner, __VA_ARGS__)\n"
                "#define xlir_closure_env_drop_v3 (xl6ClosureRuntimeV3Get()->DropEnvironment)\n",out);
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsRawCallbackRuntime ) {
        if ( !xl6IrCgenEmitCallbackTypes(out, module, 0, diags) ) return 0;
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsLanguageFunctionRuntime )
        xl6IrCgenEmitLanguageFunctionType(out);
    if ( needs->needsLanguageFunctionRuntime ) {
        xl6OutputPuts(
            "#define xlir_language_function_retain (xl6ClosureRuntimeV1Get()->RetainEnvironment)\n"
            "#define xlir_language_function_release (xl6ClosureRuntimeV1Get()->ReleaseEnvironment)\n\n",
            out);
    }
    if ( needs->needsCellRuntime ) {
        if (module->abi.cellStorageAbi != xl6IrModuleCellStorageRequirement(module))
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR cell storage ABI is unresolved");
        xl6OutputPuts("#include <__xl_runtime/xl6_cell.h>\n"
            "typedef XL6_Cell xlir_cell;\n"
            "#define xlir_cell_retain (xl6CellRuntimeV1Get()->Retain)\n"
            "#define xlir_cell_release (xl6CellRuntimeV1Get()->Release)\n"
            "#define xlir_cell_ownership (xl6CellRuntimeV1Get()->Ownership)\n\n", out);
        if(module->abi.cellStorageAbi==XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_LEAF_V2){
            xl6OutputPuts("#include <__xl_runtime/xl6_cell_native.h>\n",out);
            for(size_t i=0;i<module->types.count;++i){
                const XL6_IrType* cell=&module->types.items[i];
                if(cell->kind!=XL6_IR_TYPE_CELL || cell->paramCount!=1 || !cell->params)continue;
                const XL6_IrNativeValuePlan* native=xl6IrOwnedTransferPlanForType(xl6IrTypeGet(&module->types,cell->params[0]));
                if(native && native->captureHeader)xl6OutputPrintf(out,"#include <%s>\n",native->captureHeader);
            }
        }
    }
    if ( needs->needsErrorRuntime )
        xl6OutputPuts("#include <__xl_runtime/xl6_error_text.h>\n"
            "#define xlir_error_kind_name xl6ErrorKindNameV1\n"
            "#define xlir_error_kind_is xl6ErrorKindIsV1\n"
            /* Only compiler-owned terminated diagnostic literals use this
             * explicit adapter. Language throw carries the full string value. */
            "#define xlir_error_create_at(kind,message,line,column) xl6ErrorCreateV1((kind),xrtStrView(message),(line),(column))\n"
            "#define xlir_error_create(kind,message) xlir_error_create_at(kind,message,0,0)\n\n", out);
    if ( needs->needsTaskSpawnRuntime )
        xl6OutputPuts("#define xlir_task_spawn_wrap_current_error xl6ErrorWrapAsyncV1\n\n", out);
    if ( needs->needsCheckedIntegerArithmeticRuntime )
        xl6OutputPuts(
            "static unsigned int xlir_integer_target_width(int target)\n"
            "{ switch (target) { case 1: case 2: return 8u; case 3: case 4: return 16u; case 5: case 6: return 32u; case 7: case 8: return 64u; default: return 0u; } }\n"
            "static int xlir_integer_target_is_unsigned(int target)\n"
            "{ return target == 2 || target == 4 || target == 6 || target == 8; }\n"
            "static int64_t xlir_integer_normalize_i64(uint64_t value, int target)\n"
            "{ switch (target) {\n"
            "  case 1: return (int64_t)(int8_t)(uint8_t)value; case 2: return (int64_t)(uint8_t)value;\n"
            "  case 3: return (int64_t)(int16_t)(uint16_t)value; case 4: return (int64_t)(uint16_t)value;\n"
            "  case 5: return (int64_t)(int32_t)(uint32_t)value; case 6: return (int64_t)(uint32_t)value;\n"
            "  case 7: return (int64_t)value; default: return 0; } }\n"
            "static int64_t xlir_i64_binary_checked(int64_t left, int64_t right, int operation, int target)\n"
            "{ unsigned int width = xlir_integer_target_width(target); uint64_t value;\n"
            "  if (width == 0u || target == 8) { xrtSetErrorTake(xlir_error_create(\"RangeError\", \"invalid checked integer target\")); return 0; }\n"
            "  if (operation == 0 || operation == 1) {\n"
            "    if (right == 0) { xrtSetErrorTake(xlir_error_create(\"ZeroDivisionError\", operation == 0 ? \"integer division by zero\" : \"integer modulo by zero\")); return 0; }\n"
            "    if (target == 7 && left == INT64_MIN && right == -1) { xrtSetErrorTake(xlir_error_create(\"OverflowError\", operation == 0 ? \"integer division overflow\" : \"integer modulo overflow\")); return 0; }\n"
            "    if (xlir_integer_target_is_unsigned(target)) value = operation == 0 ? (uint64_t)left / (uint64_t)right : (uint64_t)left % (uint64_t)right;\n"
            "    else value = (uint64_t)(operation == 0 ? left / right : left % right);\n"
            "    return xlir_integer_normalize_i64(value, target);\n"
            "  }\n"
            "  if (right < 0 || (uint64_t)right >= (uint64_t)width) { xrtSetErrorTake(xlir_error_create(\"RangeError\", \"shift count is outside the left operand width\")); return 0; }\n"
            "  value = operation == 2 ? (uint64_t)left << (unsigned int)right : (uint64_t)left >> (unsigned int)right;\n"
            "  return xlir_integer_normalize_i64(value, target);\n"
            "}\n"
            "static uint64_t xlir_u64_binary_checked(uint64_t left, uint64_t right, int operation, int target)\n"
            "{ if (target != 8) { xrtSetErrorTake(xlir_error_create(\"RangeError\", \"invalid checked integer target\")); return 0; }\n"
            "  if (operation == 0 || operation == 1) { if (right == 0) { xrtSetErrorTake(xlir_error_create(\"ZeroDivisionError\", operation == 0 ? \"integer division by zero\" : \"integer modulo by zero\")); return 0; } return operation == 0 ? left / right : left % right; }\n"
            "  if (right >= 64u) { xrtSetErrorTake(xlir_error_create(\"RangeError\", \"shift count is outside the left operand width\")); return 0; }\n"
            "  return operation == 2 ? left << (unsigned int)right : left >> (unsigned int)right;\n"
            "}\n\n",
            out);
    if ( needs->needsGeneratorRuntime ) {
        if (module->abi.generatorStorageAbi != XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V4)
            return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR generator storage ABI is unresolved");
        xl6OutputPuts("#include <__xl_runtime/xl6_generator.h>\n",out);
        xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
            ? "#define xlir_generator_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
            : "#define xlir_generator_owner NULL\n",out);
        xl6OutputPuts(
            "typedef XL6_GeneratorStep xlir_generator_step;\n"
            "#define xlir_generator_create(...) (xl6GeneratorRuntimeV3Get()->Create)(xlir_generator_owner, __VA_ARGS__)\n"
            "#define xlir_generator_ownership (xl6GeneratorRuntimeV1Get()->Ownership)\n"
            "#define xlir_generator_next (xl6GeneratorRuntimeV1Get()->Next)\n"
            "#define xlir_generator_item_get (xl6GeneratorRuntimeV1Get()->ItemGet)\n"
            "#define xlir_generator_close (xl6GeneratorRuntimeV1Get()->Close)\n"
            "#define xlir_generator_destroy (xl6GeneratorRuntimeV1Get()->Destroy)\n"
            "#define xlir_generator_retain (xl6GeneratorRuntimeV2Get()->Retain)\n"
            "#define xlir_generator_put (xl6GeneratorRuntimeV1Get()->Put)\n"
            "#define xlir_generator_set_state (xl6GeneratorRuntimeV1Get()->SetState)\n"
            "#define xlir_generator_finish (xl6GeneratorRuntimeV1Get()->Finish)\n"
            "#define xlir_generator_frame_allocate (xl6GeneratorRuntimeV3Get()->FrameAllocate)\n"
            "#define xlir_generator_frame_discard (xl6GeneratorRuntimeV2Get()->FrameDiscard)\n"
            "\n",out);
    }
    if ( needs->needsCheckedI64Runtime )
        xl6OutputPuts("/* Checked integer conversion is an XLIR runtime-op ABI: its failure is\n"
                      " * observed by the immediately following ERROR_TEST, never by a C cast. */\n"
                      "static int xlir_integer_target_bits(int target)\n"
                      "{ return target <= 2 ? 8 : target <= 4 ? 16 : target <= 6 ? 32 : 64; }\n"
                      "static bool xlir_integer_target_unsigned(int target)\n"
                      "{ return target == 2 || target == 4 || target == 6; }\n"
                      "static int64_t xlir_i64_range_checked(int64_t value, int target)\n"
                      "{\n"
                      "    int bits = xlir_integer_target_bits(target);\n"
                      "    if (xlir_integer_target_unsigned(target)) {\n"
                      "        int64_t limit = bits == 64 ? INT64_MAX : (int64_t)(1ULL << bits);\n"
                      "        if (value < 0 || value >= limit) goto failed;\n"
                      "    } else if (bits < 64) {\n"
                      "        int64_t limit = (int64_t)(1ULL << (bits - 1));\n"
                      "        if (value < -limit || value >= limit) goto failed;\n"
                      "    }\n"
                      "    return value;\n"
                      "failed:\n"
                      "    xrtSetErrorTake(xlir_error_create(\"RangeError\", \"integer value is outside target range\")); return 0;\n"
                      "}\n"
                      "static int64_t xlir_f64_to_i64_checked(double value, int target)\n"
                      "{\n"
                      "    int bits = xlir_integer_target_bits(target); bool isUnsigned = xlir_integer_target_unsigned(target);\n"
                      "    double limit = bits == 64 ? 9223372036854775808.0 : (double)(1ULL << (isUnsigned ? bits : bits - 1));\n"
                      "    if (value != value || value < (isUnsigned ? 0.0 : -limit) || value >= limit) {\n"
                      "        xrtSetErrorTake(xlir_error_create(\"RangeError\", isUnsigned ? \"float value is outside unsigned integer range\" : \"float value is outside integer range\")); return 0;\n"
                      "    }\n"
                       "    return (int64_t)value;\n"
                       "}\n\n", out);
    if ( needs->needsWrappingIntegerRuntime )
        xl6OutputPuts("/* Integer wrapping is a non-raising XLIR ABI.  Convert through\n"
                      " * uint64_t, mask to the target width, then reconstruct signed\n"
                      " * values without relying on implementation-defined narrowing. */\n"
                      "static int64_t xlir_integer_wrap_to_i64(uint64_t value, int target)\n"
                      "{\n"
                      "    int bits = target <= 2 ? 8 : target <= 4 ? 16 : target <= 6 ? 32 : 64;\n"
                      "    bool isUnsigned = target == 2 || target == 4 || target == 6;\n"
                      "    uint64_t mask = bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - UINT64_C(1);\n"
                      "    uint64_t normalized = value & mask;\n"
                      "    if (isUnsigned || (normalized & (UINT64_C(1) << (bits - 1))) == 0) return (int64_t)normalized;\n"
                      "    {\n"
                      "        uint64_t magnitude = ((~normalized) & mask) + UINT64_C(1);\n"
                      "        if (magnitude == (UINT64_C(1) << 63)) return INT64_MIN;\n"
                      "        return -(int64_t)magnitude;\n"
                      "    }\n"
                      "}\n\n", out);
    if ( needs->needsCheckedCharRuntime )
        xl6OutputPuts(
            "/* char is a Unicode scalar, not an arbitrary uint32_t.  This\n"
            " * checked XLIR operation rejects surrogate code points and values\n"
            " * above U+10FFFF before they enter char-typed storage. */\n"
            "static uint32_t xlir_u64_to_char_checked(uint64_t value)\n"
            "{\n"
            "  if (value <= UINT64_C(0x10ffff) &&\n"
            "      !(value >= UINT64_C(0xd800) && value <= UINT64_C(0xdfff)))\n"
            "    return (uint32_t)value;\n"
            "  xrtSetErrorTake(xlir_error_create(\"RangeError\", \"integer value is not a Unicode scalar\")); return 0;\n"
            "}\n"
            "static uint32_t xlir_i64_to_char_checked(int64_t value)\n"
            "{\n"
            "  if (value >= 0) return xlir_u64_to_char_checked((uint64_t)value);\n"
            "  xrtSetErrorTake(xlir_error_create(\"RangeError\", \"integer value is not a Unicode scalar\")); return 0;\n"
            "}\n\n",
            out);
    if ( needs->needsCheckedU64Runtime )
        xl6OutputPuts("/* U64 checked conversion has a distinct result ABI and never passes\n"
                      " * through signed storage. */\n"
                      "static uint64_t xlir_i64_to_u64_checked(int64_t value)\n"
                      "{ if (value >= 0) return (uint64_t)value;\n"
                      "  xrtSetErrorTake(xlir_error_create(\"RangeError\", \"integer value is outside uint64 range\")); return 0; }\n"
                      "static uint64_t xlir_f64_to_u64_checked(double value)\n"
                      "{ if (value == value && value >= 0.0 && value < 18446744073709551616.0) return (uint64_t)value;\n"
                      "  xrtSetErrorTake(xlir_error_create(\"RangeError\", \"float value is outside uint64 range\")); return 0; }\n\n",
                      out);
    if (needs->needsStringI64ConvertRuntime || needs->needsStringU64ConvertRuntime)
        xl6OutputPuts("extern bool xl6IntegerTextConvertV1(xstrview, unsigned int, void*);\n", out);
    if (needs->needsStringF64ConvertRuntime)
        xl6OutputPuts("extern bool xl6Float64TextConvertV1(xstrview, double*);\n", out);
    if ( needs->needsFutureRuntime ) {
        if ( !xl6IrCgenNeedsXrtDeclarations(needs) )
            xl6OutputPuts("typedef enum xfuturestate { XFUTURE_PENDING = 0, XFUTURE_RESOLVED = 1, XFUTURE_FAILED = 2, XFUTURE_CANCELLED = 3, XFUTURE_CLOSED = 4 } xfuturestate;\n"
                          "typedef struct xfuture xfuture;\nextern xfuture* xrtFutureRef(xfuture*);\nextern void xrtFutureDestroy(xfuture*);\n", out);
        if ( needs->needsXValueRuntime )
            xl6OutputPuts(
                "#ifndef XLIR_XRT_DECLARATIONS\n"
                "typedef struct xvalue xvalue;\n"
                "#endif\n"
                "extern xfuture* xrtValueGetFuture(const xvalue*);\n", out);
        if ( needs->needsFutureCancelRuntime )
            xl6OutputPuts("extern bool xrtFutureCancel(xfuture*);\n"
                          "static bool xlir_future_cancel(xfuture* future)\n"
                          "{ return future != NULL && xrtFutureCancel(future); }\n", out);
        if ( needs->needsFutureMemberRuntime != 0 ) {
            xl6OutputPuts("extern xfuturestate xrtFutureState(const xfuture*);\n", out);
            if ( (needs->needsFutureMemberRuntime & 1u) != 0 )
                xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtFutureWait(xfuture*);\n#endif\n"
                              "static bool xlir_future_wait(xfuture* future)\n"
                              "{ return future != NULL && xrtFutureWait(future) == 0 && xrtFutureState(future) == XFUTURE_RESOLVED; }\n", out);
            if ( (needs->needsFutureMemberRuntime & ((1u << 1) | (1u << 2))) != 0 )
                xl6OutputPuts("static uint64_t xlir_future_milliseconds(int64_t milliseconds)\n"
                              "{\n"
                              "    if (milliseconds <= 0) return UINT64_C(0);\n"
                              "    return (uint64_t)milliseconds;\n"
                              "}\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 1)) != 0 )
                xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtFutureWaitFor(xfuture*, int64_t);\n#endif\n"
                              "static bool xlir_future_wait_timeout(xfuture* future, int64_t milliseconds)\n"
                              "{ return future != NULL && xrtFutureWaitFor(future, milliseconds) == 0 && xrtFutureState(future) == XFUTURE_RESOLVED; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 2)) != 0 )
                xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtFutureWaitFor(xfuture*, int64_t);\n#endif\n"
                              "static bool xlir_future_wait_until(xfuture* future, int64_t milliseconds)\n"
                              "{ return future != NULL && xrtFutureWaitFor(future, xl6DeadlineRemainingMillisecondsV1(xlir_future_milliseconds(milliseconds))) == 0 && xrtFutureState(future) == XFUTURE_RESOLVED; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 3)) != 0 )
                xl6OutputPuts("static bool xlir_future_is_done(xfuture* future)\n"
                              "{ return future != NULL && xrtFutureState(future) != XFUTURE_PENDING; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 4)) != 0 )
                xl6OutputPuts("static bool xlir_future_is_pending(xfuture* future)\n"
                              "{ return future != NULL && xrtFutureState(future) == XFUTURE_PENDING; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 5)) != 0 )
                xl6OutputPuts("static bool xlir_future_is_cancelled(xfuture* future)\n"
                              "{ return future != NULL && xrtFutureState(future) == XFUTURE_CANCELLED; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 6)) != 0 )
                xl6OutputPuts("static bool xlir_future_is_failed(xfuture* future)\n"
                              "{ return future != NULL && xrtFutureState(future) == XFUTURE_FAILED; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 7)) != 0 )
                xl6OutputPuts("static int64_t xlir_future_state(xfuture* future)\n"
                              "{ return future != NULL ? (int64_t)xrtFutureState(future) : (int64_t)XFUTURE_CLOSED; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 8)) != 0 )
                xl6OutputPuts("static int64_t xlir_future_status(xfuture* future)\n"
                              "{\n"
                              "    xfuturestate state = future != NULL ? xrtFutureState(future) : XFUTURE_CLOSED;\n"
                              "    if (state == XFUTURE_PENDING || state == XFUTURE_RESOLVED) return INT64_C(0);\n"
                              "    return state == XFUTURE_CANCELLED ? -INT64_C(2) : -INT64_C(1);\n"
                              "}\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 9)) != 0 )
                xl6OutputPuts("extern const xerror* xrtFutureError(const xfuture*);\n"
                              "extern xerror* xrtErrorRef(const xerror*);\n"
                              "static xerror* xlir_future_error_ref(xfuture* future)\n"
                              "{ const xerror* error = future != NULL ? xrtFutureError(future) : NULL; return error != NULL ? xrtErrorRef(error) : NULL; }\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 10)) != 0 )
                xl6OutputPuts("extern bool xl6FutureDebugNameSetV2(xfuture*, xlir_string);\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 11)) != 0 )
                xl6OutputPuts("extern xlir_string xl6FutureDebugNameCopyV2(xfuture*);\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 12)) != 0 )
                xl6OutputPuts("extern int64_t xl6FutureFirstObservedMsV2(xfuture*);\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 13)) != 0 )
                xl6OutputPuts("extern int64_t xl6FutureFirstTerminalObservedMsV2(xfuture*);\n", out);
            if ( (needs->needsFutureMemberRuntime & (1u << 14)) != 0 )
                xl6OutputPuts("extern int64_t xl6FuturePendingWatchesV2(xfuture*);\n", out);
        }
        if (needs->needsFutureAggregateAll || needs->needsFutureAggregateRace || needs->needsFutureAggregateWhenAny ||
            needs->needsFutureAggregateAllArray || needs->needsFutureAggregateRaceArray || needs->needsFutureAggregateWhenAnyArray) {
            if (module->abi.futureAggregateAbi != XL6_IR_FUTURE_AGGREGATE_RESIDENT_SPLIT_V2)
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR Future aggregate mapper ABI is unresolved");
            xl6OutputPuts("#include <__xl_runtime/xl6_future_aggregate.h>\n", out);
            xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
                ? "#define xlir_future_aggregate_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
                : "#define xlir_future_aggregate_owner NULL\n", out);
        }
        if ( needs->needsFutureAwaitRuntime )
            xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtFutureWait(xfuture*);\n#endif\n"
                          "extern xfuturestate xrtFutureState(const xfuture*);\n"
                          "extern const xerror* xrtFutureError(const xfuture*);\n"
                          "extern void* xrtFutureValue(const xfuture*);\n"
                          "static bool xlir_future_wait_checked(xfuture* future, int32_t line, int32_t column)\n"
                          "{\n"
                          "    if (future == NULL) {\n"
                          "        xrtSetErrorTake(xlir_error_create_at(\"StateError\", \"await on null future\", line, column));\n"
                          "        return false;\n"
                          "    }\n"
                          "    (void)xrtFutureWait(future);\n"
                          "    return xl6PayloadObserveOutcome(xrtFutureState(future), xrtFutureError(future));\n"
                          "}\n", out);
        /* AWAIT payload acquisition is emitted once from its verified ABI. */
        if (needs->needsFutureAwaitString) xl6OutputPuts("extern char* xrtStrDup(const char*);\n", out);
        if (needs->needsFutureAwaitXValue)
            xl6OutputPuts("struct xvalue;\nextern struct xvalue* xrtValueRetain(const struct xvalue*);\n", out);
        xl6OutputPuts("\n", out);
    }
    if(needs->needsNativeFuturePayloadRuntime) {
        xl6OutputPuts("#include <__xl_runtime/xl6_native_future_payload.h>\n",out);
    }
    if(needs->needsNativeFuturePayloadRuntime || needs->needsNativeCaptureProfileHeaders) {
        for(size_t i=0;i<module->types.count;++i) {
            const XL6_IrNativeValuePlan* native=xl6IrOwnedTransferPlanForType(&module->types.items[i]);
            if(native)xl6OutputPrintf(out,"#include <%s>\n",native->captureHeader);
        }
    }
    if ( needs->needsAsyncTaskRuntime || needs->needsAsyncReadyRuntime || needs->needsAsyncMachineRuntime ||
         needs->needsFutureContinuationRuntime ) {
        if ( !xl6IrCgenNeedsXrtDeclarations(needs) )
            xl6OutputPuts("typedef void (*xfuturefreeproc)(void*, void*);\n", out);
        xl6OutputPuts("extern void* xrtMalloc(size_t);\nextern void xrtFree(void*);\n", out);
    }
    if ( needs->needsAsyncTaskRuntime || needs->needsAsyncReadyRuntime || needs->needsAsyncMachineRuntime ||
         needs->needsFutureContinuationRuntime ) {
        if ( !xl6IrCgenNeedsXrtDeclarations(needs) )
            xl6OutputPuts("typedef struct xpromise xpromise;\n"
                      "extern xpromise* xrtPromiseCreate(xfuture**, void*);\n"
                      "extern void xrtPromiseDestroy(xpromise*);\n"
                      "extern bool xrtPromiseResolveOwned(xpromise*, void*, xfuturefreeproc, void*);\n", out);
    }
    if ( needs->needsAsyncReadyErrorRuntime && !needs->needsAsyncMachineRuntime &&
         !xl6IrCgenNeedsXrtDeclarations(needs) )
        xl6OutputPuts("extern bool xrtPromiseReject(xpromise*, const struct xerror*);\n", out);
    if ( needs->needsAsyncTaskRuntime || needs->needsAsyncReadyRuntime || needs->needsAsyncMachineRuntime || needs->needsFutureContinuationRuntime ) {
        if ( !xl6IrCgenNeedsXrtDeclarations(needs) ) xl6OutputPuts(
                      "typedef struct xfutureresult { xfuturestate State; void* Value; const struct xerror* Error; } xfutureresult;\n"
                      "typedef void (*xfuturecontinueproc)(const xfutureresult*, xpromise*, void*);\n"
                      "extern xfuture* xrtFutureThenOwned(xfuture*, xfuturecontinueproc, void*, xfuturefreeproc, void*);\n"
                      "extern xfuture* xrtFutureContinueOwned(xfuture*, xfuturecontinueproc, void*, xfuturefreeproc, void*);\n"
                      "extern xpromise* xrtPromiseRef(xpromise*);\n"
                      "extern bool xrtPromiseReject(xpromise*, const struct xerror*);\nextern bool xrtPromiseCancel(xpromise*);\n"
                      "extern bool xrtPromiseClose(xpromise*);\n\n", out);
    }
    if (needs->needsAsyncTaskRuntime || needs->needsAsyncReadyRuntime || needs->needsAsyncMachineRuntime) {
        xl6OutputPuts(
            "static bool xlir_future_complete_error(xpromise* promise, const xerror* error)\n"
            "{\n"
            "    if (error == NULL) return xrtPromiseClose(promise);\n"
            "    switch (xrtErrorKind(error)) {\n"
            "    case XERR_CANCELLED: return xrtPromiseCancel(promise);\n"
            "    case XERR_CLOSED: return xrtPromiseClose(promise);\n"
            "    default: return xrtPromiseReject(promise, error);\n"
            "    }\n"
            "}\n", out);
    }
    if (needs->needsAsyncMachineRuntime) {
        xl6OutputPuts(
            "/* XRT rejects NULL diagnostics. Preserve diagnostic-free failure as\n"
            " * a static INTERNAL error only at this Future transport boundary. */\n"
            "static void xlir_async_result_fail(xpromise* promise)\n"
            "{\n"
            "    xerror* error = xrtTakeError();\n"
            "    if (error == NULL) { xrtSetErrorKind(XERR_INTERNAL); error = xrtTakeError(); }\n"
            "    (void)xlir_future_complete_error(promise, error);\n"
            "    xrtErrorFree(error); xrtClearError();\n"
            "}\n", out);
    }
    if (needs->needsFutureContinuationRuntime) {
        xl6OutputPuts("#include <__xl_runtime/xl6_future_callback.h>\n",out);
        xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
            ? "#define xlir_future_callback_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
            : "#define xlir_future_callback_owner NULL\n",out);
        xl6OutputPuts(
            "#define xlir_future_callback_allocate(...) (xl6FutureCallbackRuntimeV5Get()->Allocate)(xlir_future_callback_owner, __VA_ARGS__)\n"
            "#define xlir_future_callback_discard (xl6FutureCallbackRuntimeV1Get()->Discard)\n",out);
    }
    if (needs->needsAsyncTaskRuntime) {
        if (module->abi.taskStorageAbi != XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V3)
            return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR task frame/pool storage ABI is unresolved");
        xl6OutputPuts("#include <__xl_runtime/xl6_task.h>\n",out);
        xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
            ? "#define xlir_task_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
            : "#define xlir_task_owner NULL\n",out);
        xl6OutputPuts(
            "/* Verified resident frame/pool ABI. The embedding boundary serializes\n"
            " * module activation/finalization; admission and callback tails are resident. */\n"
            "#define xlir_task_allocate(...) (xl6TaskRuntimeV3Get()->Allocate)(xlir_task_owner, __VA_ARGS__)\n"
            "#define xlir_task_discard (xl6TaskRuntimeV1Get()->Discard)\n"
            "static XL6_TaskPool* xlir_async_pool = NULL;\n"
            "static bool xlir_async_pool_activate(void)\n"
            "{ return (xlir_async_pool = xl6TaskRuntimeV1Get()->PoolCreate(NULL)) != NULL; }\n"
            "#define xlir_async_pool_submit(frame, cancel, kind) xl6TaskSubmitPayloadV1(xlir_async_pool, frame, cancel, kind, false)\n"
            "static bool xlir_async_pool_quiesce(void)\n"
            "{ return xl6TaskRuntimeV1Get()->PoolQuiesce(xlir_async_pool); }\n"
            "static void xlir_async_pool_release(void)\n"
            "{ if (!xl6TaskRuntimeV1Get()->PoolDestroy(xlir_async_pool)) abort(); xlir_async_pool = NULL; }\n"
            "\n",out);
    }
    if (needs->needsAsyncTaskRuntime || needs->needsFutureContinuationRuntime || needs->needsAsyncMachineRuntime) xl6OutputPuts(
        "#ifndef XLIR_XRT_DECLARATIONS\n"
        "typedef struct xcancel xcancel;\n"
        "extern bool xrtCancelRequested(const xcancel*);\n"
        "typedef union xfuturewatch { uint64_t Alignment; uint8_t Storage[64]; } xfuturewatch;\n"
        "typedef enum xfuturewatchresult { XFUTURE_WATCH_ERROR = -1, XFUTURE_WATCH_READY = 0, XFUTURE_WATCH_PENDING = 1 } xfuturewatchresult;\n"
        "extern bool xrtFutureWatchInit(xfuturewatch*, void (*)(void*), void (*)(void*), void*);\n"
        "extern xfuturewatchresult xrtFutureWatchAdd(xfuture*, xfuturewatch*);\n"
        "extern xcancel* xrtPromiseCancelToken(const xpromise*);\n"
        "extern void xrtCancelDestroy(xcancel*);\n"
        "extern bool xrtFutureCancel(xfuture*);\n"
        "extern bool xrtFutureResult(const xfuture*, xfutureresult*);\n"
        "extern bool xrtPromiseForward(xpromise*, xfuture*);\n"
        "extern bool xrtPromiseDone(const xpromise*);\n"
        "#endif\n"
        , out);
    if ( needs->needsResultVoid ) xl6OutputPuts("typedef struct xlir_result_void { bool ok; } xlir_result_void;\n", out);
    if ( needs->needsResultBool ) xl6OutputPuts("typedef struct xlir_result_bool { bool ok; bool value; } xlir_result_bool;\n", out);
    if ( needs->needsResultI64 ) xl6OutputPuts("typedef struct xlir_result_i64 { bool ok; int64_t value; } xlir_result_i64;\n", out);
    if ( needs->needsResultU64 ) xl6OutputPuts("typedef struct xlir_result_u64 { bool ok; uint64_t value; } xlir_result_u64;\n", out);
    if ( needs->needsResultTypeId ) xl6OutputPuts("typedef struct xlir_result_type_id { bool ok; uint64_t value; } xlir_result_type_id;\n", out);
    if ( needs->needsResultF64 ) xl6OutputPuts("typedef struct xlir_result_f64 { bool ok; double value; } xlir_result_f64;\n", out);
if ( needs->needsResultF32 ) xl6OutputPuts("typedef struct xlir_result_f32 { bool ok; float value; } xlir_result_f32;\n", out);
if ( needs->needsResultChar ) xl6OutputPuts("typedef struct xlir_result_char { bool ok; uint32_t value; } xlir_result_char;\n", out);
if ( needs->needsResultString ) xl6OutputPuts("typedef struct xlir_result_string { bool ok; xlir_string value; } xlir_result_string;\n", out);
    if ( needs->needsResultXValue ) xl6OutputPuts("struct xvalue;\ntypedef struct xlir_result_xvalue { bool ok; struct xvalue* value; } xlir_result_xvalue;\n", out);
    if ( needs->needsResultCBool && !needs->needsExtendedResults )
        xl6OutputPuts("typedef struct xlir_result_c_bool { bool ok; bool value; } xlir_result_c_bool;\n", out);
    if ( needs->needsResultFuture ) xl6OutputPuts("struct xfuture;\ntypedef struct xlir_result_future { bool ok; struct xfuture* value; } xlir_result_future;\n", out);
    if ( needs->needsResultGenerator ) xl6OutputPuts("typedef struct xlir_result_generator { bool ok; xlir_generator* value; } xlir_result_generator;\n", out);
    if ( needs->needsExtendedResults )
        xl6OutputPuts(
            "typedef struct xlir_result_c_char { bool ok; char value; } xlir_result_c_char;\n"
            "typedef struct xlir_result_c_schar { bool ok; signed char value; } xlir_result_c_schar;\n"
            "typedef struct xlir_result_c_uchar { bool ok; unsigned char value; } xlir_result_c_uchar;\n"
            "typedef struct xlir_result_c_short { bool ok; short value; } xlir_result_c_short;\n"
            "typedef struct xlir_result_c_ushort { bool ok; unsigned short value; } xlir_result_c_ushort;\n"
            "typedef struct xlir_result_c_int { bool ok; int value; } xlir_result_c_int;\n"
            "typedef struct xlir_result_c_uint { bool ok; unsigned int value; } xlir_result_c_uint;\n"
            "typedef struct xlir_result_c_long { bool ok; long value; } xlir_result_c_long;\n"
            "typedef struct xlir_result_c_ulong { bool ok; unsigned long value; } xlir_result_c_ulong;\n"
            "typedef struct xlir_result_c_llong { bool ok; long long value; } xlir_result_c_llong;\n"
            "typedef struct xlir_result_c_ullong { bool ok; unsigned long long value; } xlir_result_c_ullong;\n"
            "typedef struct xlir_result_c_size { bool ok; size_t value; } xlir_result_c_size;\n"
            "typedef struct xlir_result_c_ssize { bool ok; intptr_t value; } xlir_result_c_ssize;\n"
            "typedef struct xlir_result_c_ptrdiff { bool ok; ptrdiff_t value; } xlir_result_c_ptrdiff;\n"
            "typedef struct xlir_result_c_intptr { bool ok; intptr_t value; } xlir_result_c_intptr;\n"
            "typedef struct xlir_result_c_uintptr { bool ok; uintptr_t value; } xlir_result_c_uintptr;\n"
            "typedef struct xlir_result_c_bool { bool ok; bool value; } xlir_result_c_bool;\n"
            "typedef struct xlir_result_c_float { bool ok; float value; } xlir_result_c_float;\n"
            "typedef struct xlir_result_c_double { bool ok; double value; } xlir_result_c_double;\n"
            "typedef struct xlir_result_c_wchar { bool ok; wchar_t value; } xlir_result_c_wchar;\n"
            "typedef struct xlir_result_pointer { bool ok; void* value; } xlir_result_pointer;\n"
            "struct xerror; typedef struct xlir_result_error { bool ok; struct xerror* value; } xlir_result_error;\n"
            "struct xlir_closure; typedef struct xlir_result_closure { bool ok; struct xlir_closure* value; } xlir_result_closure;\n",
            out);
    if ( needs->needsResultVoid || needs->needsResultBool || needs->needsResultI64 || needs->needsResultU64 ||
needs->needsResultTypeId || needs->needsResultF64 || needs->needsResultF32 || needs->needsResultChar || needs->needsResultString ||
         needs->needsResultXValue || needs->needsResultCBool || needs->needsResultFuture || needs->needsResultGenerator ||
         needs->needsExtendedResults )
        xl6OutputPuts("\n", out);
    if (module->abi.nativeFutureMapAbi == XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1) {
        xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
            ? "#define xlir_native_future_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
            : "#define xlir_native_future_owner NULL\n",out);
        xl6IrCgenEmitNativeFutureMap(out);
    }
    return !xl6OutputHasFailed(out);
}
