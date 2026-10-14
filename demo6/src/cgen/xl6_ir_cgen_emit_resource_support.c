static int xl6IrCgenEmitResourceRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    for(size_t site=0;site<module->dynamicCallSiteCount;++site)
        if(module->dynamicCallSites[site].boundCollectionOp!=XL6_IR_DYNAMIC_COLLECTION_INVALID ||
           (module->dynamicCallSites[site].fallbackCollectionOp>=XL6_IR_DYNAMIC_COLLECTION_TRY_POP &&
            module->dynamicCallSites[site].fallbackCollectionOp<=XL6_IR_DYNAMIC_COLLECTION_TRY_PEEK_AT)) {
            xl6OutputPuts("#include <__xl_runtime/xl6_dynamic_collection_vector.h>\n",out); break;
        }
    for(size_t instruction=0;instruction<module->instructionCount;++instruction)
        if(xl6IrCgenInstructionIsReachable(module,instruction) &&
           ((module->instructions[instruction].runtimeOp==XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL &&
             module->instructions[instruction].dynamicCallResultAbi==XL6_IR_DYNAMIC_CALL_RESULT_OWNED_PRODUCT) ||
            module->instructions[instruction].runtimeOp==XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL)) {
            xl6OutputPuts("#include <__xl_runtime/xl6_dynamic_product.h>\n",out); break;
        }
    int nativeQueue=needs->needsSpscQueueRuntime || needs->needsMpscQueueRuntime || needs->needsMpmcQueueRuntime || needs->needsWaitQueueRuntime;
    if (needs->needsCharValueCheckedRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_char_checked.h>\n",out);
    for(i=0;i<module->instructionCount;++i)
        if(xl6IrCgenInstructionIsReachable(module,i) && xl6IrSlotMapIteratorOpIs(module->instructions[i].runtimeOp)) {
            xl6OutputPuts("#include <__xl_runtime/xl6_slot_map_iterator.h>\n",out); break;
        }
    for(i=0;i<module->instructionCount;++i)
        if(xl6IrCgenInstructionIsReachable(module,i) &&
            xl6IrSlotMapResourceOpIs(xl6IrSlotMapResourceOp(module->instructions[i].runtimeOp))) {
            xl6OutputPuts("#include <__xl_runtime/xl6_slot_map.h>\n",out); break;
        }
    if (nativeQueue) xl6OutputPuts("#include <__xl_runtime/xl6_concurrent_queue_abi.h>\n",out);
    if(needs->needsWaitQueueRuntime)xl6OutputPuts("#include <__xl_runtime/xl6_wait_queue_abi.h>\n",out);
    if (needs->needsNativeChannelRuntime) xl6OutputPuts("#include <__xl_runtime/xl6_channel_abi.h>\n",out);
    if ( needs->needsThreadResourceRuntime )
        xl6OutputPuts(
            "extern xvalue* xl6ThreadCreateV1(xlir_closure*, uint64_t);\n"
            "extern bool xl6ThreadWaitV1(const xvalue*, uint64_t);\n"
            "extern bool xl6ThreadWaitTimeoutV1(const xvalue*, uint64_t, int64_t);\n"
            "extern int64_t xl6ThreadExitCodeV1(const xvalue*, uint64_t);\n"
            "extern int64_t xl6ThreadStateV1(const xvalue*, uint64_t);\n"
            "extern bool xl6ThreadStopV1(const xvalue*, uint64_t);\n"
            "extern xvalue* xl6SequenceThreadCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);\n",
            out);
    if ( needs->needsStackResourceRuntime || needs->needsStackEraseRuntime ||
         needs->needsAvlTreeResourceRuntime )
        xl6OutputPuts(
            "enum { XLIR_CHANNEL_PAYLOAD_TRIVIAL_COPY = 1, XLIR_CHANNEL_PAYLOAD_STRING_COPY = 2, XLIR_CHANNEL_PAYLOAD_XVALUE_RETAIN = 3 };\n\n",
            out);
    if ( needs->needsStackResourceRuntime || needs->needsStackEraseRuntime || needs->needsArrayImportRuntime || needs->needsArrayCreateRuntime || needs->needsAvlTreeResourceRuntime || nativeQueue || needs->needsNativeChannelRuntime )
        xl6OutputPuts(
            "typedef struct xrttype xrttype;\n"
            "extern const xrttype* xl6CharTypeV1(void);\n"
            "extern const xrttype* xl6StringTypeV1(void);\n"
            "extern const xrttype* xl6NullableBoolTypeV1(void);\n"
            "extern const xrttype* xl6NullableI64TypeV1(void);\n"
            "extern const xrttype* xl6NullableU64TypeV1(void);\n"
            "extern const xrttype* xl6NullableF32TypeV1(void);\n"
            "extern const xrttype* xl6NullableF64TypeV1(void);\n"
            "extern const xrttype* xl6NullableTimeTypeV1(void);\n"
            "extern const xrttype* xl6NullableCharTypeV1(void);\n"
            "extern const xrttype* xl6NullableNativeI8TypeV1(void);\n"
            "extern const xrttype* xl6NullableNativeU8TypeV1(void);\n"
            "extern const xrttype* xl6NullableNativeI16TypeV1(void);\n"
            "extern const xrttype* xl6NullableNativeU16TypeV1(void);\n"
            "extern const xrttype* xl6NullableNativeI32TypeV1(void);\n"
            "extern const xrttype* xl6NullableNativeU32TypeV1(void);\n"
            "extern const xrttype* xl6NullableStringTypeV1(void);\n"
            "typedef struct xvalueconverter xvalueconverter;\n"
            "extern const xvalueconverter* xl6ArrayReferenceConverterV1(void);\n"
            "extern const xvalueconverter* xl6CharConverterV1(void);\n"
            "extern const xvalueconverter* xl6StringConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableStringConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableBoolConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableI64ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableU64ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableF32ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableF64ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableTimeConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableCharConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableNativeI8ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableNativeU8ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableNativeI16ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableNativeU16ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableNativeI32ConverterV1(void);\n"
            "extern const xvalueconverter* xl6NullableNativeU32ConverterV1(void);\n", out);
    if ( needs->needsStackResourceRuntime || needs->needsArrayImportRuntime || needs->needsArrayCreateRuntime || nativeQueue || needs->needsAvlTreeResourceRuntime || needs->needsNativeChannelRuntime )
        xl6OutputPuts(
            "/* Sequence factories live in the resident typed runtime. */\n"
            "typedef struct xrttype xrttype;\n"
            "#include <__xl_runtime/xl6_nominal_admission_abi.h>\n"
            "extern xvalue* xl6SequencePrimitiveCreateV1(unsigned int, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, unsigned int, bool, uint64_t);\n"
            "extern xvalue* xl6SequenceReferenceCreateV1(unsigned int, uint64_t, uint64_t, unsigned int, bool, uint64_t);\n"
            "extern xvalue* xl6SequenceSpscQueueCreateTakeV1(unsigned int, uint64_t, uint64_t, uint64_t, xvalue*, bool);\n"
            "extern xvalue* xl6SequenceMpscQueueCreateTakeV1(unsigned int, uint64_t, uint64_t, uint64_t, xvalue*, bool);\n"
            "extern xvalue* xl6SequenceMpmcQueueCreateTakeV1(unsigned int, uint64_t, uint64_t, uint64_t, xvalue*, bool);\n"
            "extern xvalue* xl6SequenceWaitQueueCreateTakeV1(unsigned int, uint64_t, uint64_t, uint64_t, xvalue*, bool);\n"
            "extern xvalue* xl6SequenceNominalCreateV1(unsigned int, uint64_t, uint64_t, const XL6_NominalAdmissionV1*, bool, uint64_t);\n"
            "extern xvalue* xl6SequenceUnionCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, size_t, bool);\n"
            "extern xvalue* xl6SequenceCScalarCreateV1(unsigned int, uint64_t, uint64_t, const xrttype*, uint64_t);\n"
            "extern xvalue* xl6SequenceNullableCScalarCreateV1(unsigned int, uint64_t, uint64_t, const xrttype*, const xrttype*, uint64_t);\n"
            "extern xvalue* xl6SequenceNumericEnumCreateV1(unsigned int, uint64_t, uint64_t, const xrttype*, unsigned int, bool, uint64_t);\n"
            "extern xvalue* xl6SequenceCallableCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);\n"
            "extern xvalue* xl6SequenceFutureCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);\n"
            "extern xvalue* xl6SequenceSemCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);\n"
            "extern xvalue* xl6SequenceSyncCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool, unsigned int);\n"
            "extern xvalue* xl6SequenceNestedCreateTakeV2(unsigned int, uint64_t, uint64_t, xvalue*, bool);\n",
            out);
    if (needs->needsArrayImportRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_array_import_chain.h>\n"
            "extern xvalue* xl6ArrayEraseV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const xvalueconverter*);\n",out);
    if (needs->needsListCreateRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_list.h>\n"
            "#include <__xl_runtime/xl6_list_schema.h>\n",out);
    if (needs->needsListImportRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_list_import_chain.h>\n",out);
    if (needs->needsSetCreateRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_set_schema.h>\n",out);
    if (needs->needsDictCreateRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_dict_schema.h>\n",out);
    if (needs->needsSetImportRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_set_import_chain.h>\n",out);
    if (needs->needsDictImportRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_dict_import_chain.h>\n",out);
    if (needs->needsSetMethodsRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_set_methods.h>\n",out);
    if (needs->needsDictMethodsRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_dict_methods.h>\n",out);
    if (needs->needsCollectionCountAllRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_collection_count_all.h>\n",out);
    if (needs->needsArrayCreateRuntime)
        xl6OutputPuts("extern xvalue* xl6ArrayProjectionTakeV1(xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const xvalueconverter*);\n",out);
    if(needs->needsStackEraseRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_stack_erased.h>\n",out);
    if(needs->needsStackCallableRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_stack_callable.h>\n",out);
    if (needs->needsStackResourceRuntime)
        xl6OutputPuts(
            "extern bool xl6StackPushBatchV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const xvalue*, uint64_t);\n"
            "extern xvalue* xl6StackExtractArrayV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, uint64_t, int64_t, int64_t, bool, const xvalueconverter*);\n"
            "extern bool xl6StackPushV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, const void*);\n"
        "extern bool xl6StackTakeV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t, void*, bool);\n"
        "extern bool xl6StackTryTakeV2(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t, void*, bool, bool*);\n"
            "extern bool xl6StackReserveV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, int64_t);\n"
            "extern xvalue* xl6StackCloneV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);\n"
            "extern bool xl6StackEqualsV1(const xvalue*, const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);\n"
            "extern bool xl6StackTrimV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);\n"
            "extern bool xl6StackClearV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);\n"
            "extern int64_t xl6StackSizeV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, bool);\n",out);
    if (needs->needsStackResourceRuntime || needs->needsArrayImportRuntime || needs->needsArrayCreateRuntime)
        xl6OutputPuts(
            "extern const xrttype* xrtTypeBool(void);\n"
            "extern const xrttype* xrtTypeInt8(void);\n"
            "extern const xrttype* xrtTypeUInt8(void);\n"
            "extern const xrttype* xrtTypeInt16(void);\n"
            "extern const xrttype* xrtTypeUInt16(void);\n"
            "extern const xrttype* xrtTypeInt32(void);\n"
            "extern const xrttype* xrtTypeUInt32(void);\n"
            "extern const xrttype* xrtTypeInt64(void);\n"
            "extern const xrttype* xrtTypeUInt64(void);\n"
            "extern const xrttype* xrtTypeFloat32(void);\n"
            "extern const xrttype* xrtTypeFloat64(void);\n"
            "extern const xrttype* xrtTypeTime(void);\n"
            "extern const xrttype* xrtTypeType(void);\n"
            "extern const xrttype* xrtTypeStringView(void);\n"
            "extern const xrttype* xrtTypeValue(void);\n",
            out);
    if ( needs->needsAvlTreeResourceRuntime )
        xl6OutputPuts("#include <__xl_runtime/xl6_tree.h>\n"
            "extern xvalue* xl6SequenceTreeCreateTakeV1(unsigned int, uint64_t, uint64_t, const XL6_TreeLayoutV1*, xvalue*, xvalue*, bool);\n",out);
    if ( needs->needsSemaphoreResourceRuntime )
        xl6OutputPuts(
            "/* Fixed resident semaphore ABI: no generated owner or callbacks. */\n"
            "extern xvalue* xl6SemCreateV1(uint64_t, int64_t, int64_t);\n"
            "extern xvalue* xl6SemCreateDefaultV1(uint64_t);\n"
            "extern bool xl6SemWaitV1(const xvalue*, uint64_t);\n"
            "extern bool xl6SemTryWaitV1(const xvalue*, uint64_t);\n"
            "extern bool xl6SemWaitTimeoutV1(const xvalue*, uint64_t, int64_t);\n"
            "extern bool xl6SemPostV1(const xvalue*, uint64_t);\n"
            "extern bool xl6SemPostMultipleV1(const xvalue*, uint64_t, int64_t);\n", out);
    if ( needs->needsSyncResourceRuntime )
        xl6OutputPuts(
            "/* Fixed resident sync ABI: no generated owner, Unit or callbacks. */\n"
            "extern xvalue* xl6MutexCreateV1(uint64_t);\n"
            "extern bool xl6MutexLockV1(const xvalue*, uint64_t);\n"
            "extern bool xl6MutexTryLockV1(const xvalue*, uint64_t);\n"
            "extern bool xl6MutexUnlockV1(const xvalue*, uint64_t);\n"
            "extern xvalue* xl6CondCreateV1(uint64_t);\n"
            "extern bool xl6CondWaitV1(const xvalue*, uint64_t, const xvalue*, uint64_t);\n"
            "extern bool xl6CondWaitTimeoutV1(const xvalue*, uint64_t, const xvalue*, uint64_t, int64_t);\n"
            "extern bool xl6CondSignalV1(const xvalue*, uint64_t);\n"
            "extern bool xl6CondBroadcastV1(const xvalue*, uint64_t);\n"
            "extern xvalue* xl6RWLockCreateV1(uint64_t);\n"
            "extern bool xl6RWLockReadV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockTryReadV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockReadUnlockV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockWriteV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockTryWriteV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockWriteUnlockV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockDowngradeV1(const xvalue*, uint64_t);\n"
            "extern bool xl6RWLockUpgradeV1(const xvalue*, uint64_t);\n", out);
    return !xl6OutputHasFailed(out);
}
