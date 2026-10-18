#include "xl_api_internal.h"

#include "include/xl.h"
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_static_module.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_dynamic_fields.h"
#include "src/runtime/xl6_value_object_fields.h"
#include "src/runtime/xl6_value_named_read.h"
#include "src/runtime/xl6_value_named_write.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_closure_capture_v3.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_slot.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_value_iterator_internal.h"
#include "src/runtime/xl6_list_value_iterator.h"
#include "src/runtime/xl6_frame_storage.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_network_service.h"
#include "src/runtime/xl6_log.h"
#include "src/runtime/xl6_text_value.h"
#include "xl_cancellation_retirement.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_stack_erased.h"
#include "src/runtime/xl6_stack_erased_methods.h"
#include "src/runtime/xl6_stack_erased_try.h"
#include "src/runtime/xl6_dynamic_product.h"
#include "src/runtime/xl6_dynamic_collection_vector.h"
#include "src/runtime/xl6_char_checked.h"
#include "src/runtime/xl6_stack_callable.h"
#include "src/runtime/xl6_stack_batch.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_default.h"
#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_call_frame.h"
#include "src/runtime/xl6_dict_call_frame.h"
#include "src/runtime/xl6_array_entry.h"
#include "src/runtime/xl6_array_producer.h"
#include "src/runtime/xl6_tree_projection.h"
#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_semaphore.h"
#include "src/runtime/xl6_semaphore_admission.h"
#include "src/runtime/xl6_sync.h"
#include "src/runtime/xl6_sync_admission.h"
#include "src/runtime/xl6_thread.h"
#include "src/runtime/xl6_thread_admission.h"
#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_channel.h"
#include "src/runtime/xl6_channel_case.h"
#include "src/runtime/xl6_wait_queue.h"
#include "src/runtime/xl6_array_os_argv.h"
#include "src/runtime/xl6_dynamic_collection.h"
#include "src/runtime/xl6_value_contains.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_path_build.h"
#include "src/runtime/xl6_array_shuffle.h"
#include "src/runtime/xl6_collection_count.h"
#include "src/runtime/xl6_array_scalar_read.h"
#include "src/runtime/xl6_array_string_read.h"
#include "src/runtime/xl6_array_value_read.h"
#include "src/runtime/xl6_array_import.h"
#include "src/runtime/xl6_array_import_chain.h"
#include "src/runtime/xl6_restore_class.h"
#include "src/runtime/xl6_restore_open.h"
#include "src/runtime/xl6_serialization_read.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_slot_map_iterator_internal.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_nullable_slot_map_key.h"
#include "src/runtime/xl6_nullable_slot_map_key_capture.h"
#include "src/runtime/xl6_cell_native.h"
#include "src/runtime/xl6_array_native_value_admission.h"
#include "src/runtime/xl6_nullable_slot_map_key_collection.h"
#include "src/runtime/xl6_dict_graph_access.h"
#include "src/runtime/xl6_dict_graph.h"
#include "src/runtime/xl6_dict_value_iterator.h"
#include "src/runtime/xl6_list_methods.h"
#include "src/runtime/xl6_list_value_read.h"
#include "src/runtime/xl6_dict_value_index.h"
#include "src/runtime/xl6_dict_methods.h"
#include "src/runtime/xl6_dict_import_chain.h"
#include "src/runtime/xl6_list_typed_read.h"
#include "src/runtime/xl6_list_value_write.h"
#include "src/runtime/xl6_list_algebra.h"
#include "src/runtime/xl6_value_copy.h"
#include "src/runtime/xl6_list_graph.h"
#include "src/runtime/xl6_list_import_chain.h"
#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_set_key_policy.h"
#include "src/runtime/xl6_native_leaf_value.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_set_schema.h"
#include "src/runtime/xl6_dict_schema.h"
#include "src/runtime/xl6_set_value_iterator.h"
#include "src/runtime/xl6_set_graph_access.h"
#include "src/runtime/xl6_set_graph.h"
#include "src/runtime/xl6_set_methods.h"
#include "src/runtime/xl6_set_import_chain.h"
#include "src/runtime/xl6_collection_count_all.h"
#include "src/runtime/xl6_array_algebra.h"
#include "src/runtime/xl6_array_graph.h"
#include "src/runtime/xl6_persistence_project.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_float_convert.h"
#include "src/runtime/xl6_integer_convert.h"
#include "src/runtime/xl6_sequence_projection_schema.h"
#include "src/runtime/xl6_container_projection.h"
#include "src/runtime/xl6_value_domain_projection.h"
#include "src/runtime/xl6_resource_reference_admission.h"
#include "src/runtime/xl6_native_scalar_value.h"
#include "src/runtime/xl6_value_format.h"
#include "src/runtime/xl6_class_text.h"
#include "src/runtime/xl6_error_text.h"
#include "src/runtime/xl6_nominal_admission.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_array_value_iterator.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "src/runtime/xl6_future_metadata.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_native_future_payload.h"

#if defined(CONFIG_TCC_STATIC) && !defined(XL_STATIC_TCC)
#define XL_STATIC_TCC 1
#endif

#if defined(XL_STATIC_TCC) && !defined(TCC_DISABLE_BUILTIN_VFS)
#define XL_STATIC_TCC_VFS 1
#endif

#ifndef XL_ENABLE_EXE_COMPILER
#if defined(XL_RUNTIME_ONLY_DLL)
#define XL_ENABLE_EXE_COMPILER 0
#else
#define XL_ENABLE_EXE_COMPILER 1
#endif
#endif

#if defined(XL_STATIC_TCC_VFS)
#include "tcc_builtin_vfs.h"
#endif

static const float xl__mzerosf = -0.0f;
static const double xl__mzerodf = -0.0;

int xlApiAddTccSymbol(TCCState* s, const char* name, const void* value)
{
#ifdef XL_STATIC_TCC
    return tcc_add_symbol(s, name, value) == 0;
#else
    return xl__tcc.tcc_add_symbol(s, name, value) == 0;
#endif
}

#define XL_ADD_TCC_API_SYMBOL(s, member, name) \
    do { \
        if ( xl__tcc.member != NULL && !xlApiAddTccSymbol((s), (name), (const void*)xl__tcc.member) ) { \
            return 0; \
        } \
    } while (0)

static int xlAddDefaultTccApiSymbols(TCCState* s)
{
    if ( s == NULL || !xl__tcc.ready ) {
        return 1;
    }

    XL_ADD_TCC_API_SYMBOL(s, tcc_new, "tcc_new");
    XL_ADD_TCC_API_SYMBOL(s, tcc_delete, "tcc_delete");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_lib_path, "tcc_set_lib_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_error_func, "tcc_set_error_func");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_options, "tcc_set_options");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_include_path, "tcc_add_include_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_sysinclude_path, "tcc_add_sysinclude_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_define_symbol, "tcc_define_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_undefine_symbol, "tcc_undefine_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_library_path, "tcc_add_library_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_library, "tcc_add_library");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_file, "tcc_add_file");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_symbol, "tcc_add_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_output_type, "tcc_set_output_type");
    XL_ADD_TCC_API_SYMBOL(s, tcc_compile_string, "tcc_compile_string");
    XL_ADD_TCC_API_SYMBOL(s, tcc_output_file, "tcc_output_file");
    XL_ADD_TCC_API_SYMBOL(s, tcc_run, "tcc_run");
    XL_ADD_TCC_API_SYMBOL(s, tcc_relocate, "tcc_relocate");
    XL_ADD_TCC_API_SYMBOL(s, tcc_get_symbol, "tcc_get_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_list_symbols, "tcc_list_symbols");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_backtrace_func, "tcc_set_backtrace_func");
    if ( !xlApiAddTccSymbol(s, "xlDefaultContextOptions", (const void*)xlDefaultContextOptions) ||
         !xlApiAddTccSymbol(s, "xlRuntimeInit", (const void*)xlRuntimeInit) ||
         !xlApiAddTccSymbol(s, "xlRuntimeUnit", (const void*)xlRuntimeUnit) ||
         !xlApiAddTccSymbol(s, "xlRuntimeDrainRetiringChecked", (const void*)xlRuntimeDrainRetiringChecked) ||
         !xlApiAddTccSymbol(s, "xlCreateContext", (const void*)xlCreateContext) ||
         !xlApiAddTccSymbol(s, "xlDestroyContext", (const void*)xlDestroyContext) ||
         !xlApiAddTccSymbol(s, "xlSetWorkDir", (const void*)xlSetWorkDir) ||
         !xlApiAddTccSymbol(s, "xlSetTempDir", (const void*)xlSetTempDir) ||
         !xlApiAddTccSymbol(s, "xlSetSaveTemps", (const void*)xlSetSaveTemps) ||
         !xlApiAddTccSymbol(s, "xlAddIncludePath", (const void*)xlAddIncludePath) ||
         !xlApiAddTccSymbol(s, "xlAddLibraryPath", (const void*)xlAddLibraryPath) ||
         !xlApiAddTccSymbol(s, "xlAddLinkLibrary", (const void*)xlAddLinkLibrary) ||
         !xlApiAddTccSymbol(s, "xlAddResourceFile", (const void*)xlAddResourceFile) ||
         !xlApiAddTccSymbol(s, "xlLoadDependencyLibrary", (const void*)xlLoadDependencyLibrary) ||
         !xlApiAddTccSymbol(s, "xlRegisterCFunction", (const void*)xlRegisterCFunction) ||
         !xlApiAddTccSymbol(s, "xlRunScriptString", (const void*)xlRunScriptString) ||
         !xlApiAddTccSymbol(s, "xlRunScriptFile", (const void*)xlRunScriptFile) ||
         !xlApiAddTccSymbol(s, "xlRunScriptStringArg", (const void*)xlRunScriptStringArg) ||
         !xlApiAddTccSymbol(s, "xlRunScriptFileArg", (const void*)xlRunScriptFileArg) ||
         !xlApiAddTccSymbol(s, "xlCompileScriptStringToModule", (const void*)xlCompileScriptStringToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileScriptFileToModule", (const void*)xlCompileScriptFileToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceStringToModule", (const void*)xlCompileCSourceStringToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFileToModule", (const void*)xlCompileCSourceFileToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFilesToModule", (const void*)xlCompileCSourceFilesToModule) ||
         !xlApiAddTccSymbol(s, "xlRunCSourceString", (const void*)xlRunCSourceString) ||
         !xlApiAddTccSymbol(s, "xlRunCSourceFile", (const void*)xlRunCSourceFile) ||
         !xlApiAddTccSymbol(s, "xlRunCSourceFiles", (const void*)xlRunCSourceFiles) ||
         !xlApiAddTccSymbol(s, "xlActivateModule", (const void*)xlActivateModule) ||
         !xlApiAddTccSymbol(s, "xlDeactivateModule", (const void*)xlDeactivateModule) ||
         !xlApiAddTccSymbol(s, "xlRunModuleMain", (const void*)xlRunModuleMain) ||
         !xlApiAddTccSymbol(s, "xlModuleGetSymbol", (const void*)xlModuleGetSymbol) ||
         !xlApiAddTccSymbol(s, "xlDestroyModule", (const void*)xlDestroyModule) ||
         !xlApiAddTccSymbol(s, "xlGetLastError", (const void*)xlGetLastError) ||
         !xlApiAddTccSymbol(s, "xlClearLastError", (const void*)xlClearLastError) ||
         !xlApiAddTccSymbol(s, "xlGetLastExitCode", (const void*)xlGetLastExitCode) ) {
        return 0;
    }
    if ( !xlApiAddTccSymbol(s, "xlTccRegisterXrtEnvironment", (const void*)xlTccRegisterXrtEnvironment) ) {
        return 0;
    }
#define XL6_LOG_EXPORT(Name, Component) \
    if (!xlApiAddTccSymbol(s, #Name, (const void*)Name)) return 0;
#include "src/runtime/xl6_log_symbols.inc"
#undef XL6_LOG_EXPORT
#define XL6_TEXT_EXPORT(Name, Component) \
    if (!xlApiAddTccSymbol(s, #Name, (const void*)Name)) return 0;
#include "src/runtime/xl6_text_symbols.inc"
#undef XL6_TEXT_EXPORT
    if (!xlApiAddTccSymbol(s, "xl6NativeHandleTakeV1", (const void*)xl6NativeHandleTakeV1) ||
        !xlApiAddTccSymbol(s, "xl6NativeHandleTraceLeaf", (const void*)xl6NativeHandleTraceLeaf) ||
        !xlApiAddTccSymbol(s, "xl6ProcessCommandHandlePolicyV1Get", (const void*)xl6ProcessCommandHandlePolicyV1Get) ||
        !xlApiAddTccSymbol(s, "xl6ProcessHandlePolicyV1Get", (const void*)xl6ProcessHandlePolicyV1Get) ||
        !xlApiAddTccSymbol(s, "xl6ProcessStatusHandlePolicyV1Get", (const void*)xl6ProcessStatusHandlePolicyV1Get) ||
        !xlApiAddTccSymbol(s, "xl6BufferIteratorHandlePolicyV1Get", (const void*)xl6BufferIteratorHandlePolicyV1Get)) return 0;
    if ( !xlApiAddTccSymbol(s, "xl6ProtocolWitnessValidate", (const void*)xl6ProtocolWitnessValidate) ||
         !xlApiAddTccSymbol(s, "xl6ProtocolWitnessFind", (const void*)xl6ProtocolWitnessFind) ||
         !xlApiAddTccSymbol(s, "xl6ProtocolRegister", (const void*)xl6ProtocolRegister) ||
         !xlApiAddTccSymbol(s, "xl6ProtocolUnregister", (const void*)xl6ProtocolUnregister) ||
         !xlApiAddTccSymbol(s, "xl6ProtocolLookup", (const void*)xl6ProtocolLookup) ) {
        return 0;
    }
    if (!xlApiAddTccSymbol(s, "xl6ContinuationExecutorCreateGuaranteed", (const void*)xl6ContinuationExecutorCreateGuaranteed) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorReserveOwned", (const void*)xl6ContinuationExecutorReserveOwned) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorClose", (const void*)xl6ContinuationExecutorClose) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorDestroy", (const void*)xl6ContinuationExecutorDestroy) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorDestroyCompleted", (const void*)xl6ContinuationExecutorDestroyCompleted) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationReserveSuccessor", (const void*)xl6ContinuationReserveSuccessor) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationCommit", (const void*)xl6ContinuationCommit) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationComplete", (const void*)xl6ContinuationComplete) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationAbort", (const void*)xl6ContinuationAbort) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationTraceBind", (const void*)xl6ContinuationTraceBind) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationOwnership", (const void*)xl6ContinuationOwnership) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationWatchRegister", (const void*)xl6ContinuationWatchRegister)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6PayloadDropGet", (const void*)xl6PayloadDropGet) ||
        !xlApiAddTccSymbol(s, "xl6NativeFuturePayloadRuntimeV1Get", (const void*)xl6NativeFuturePayloadRuntimeV1Get) ||
        !xlApiAddTccSymbol(s, "xl6TaskSubmitOwnedPolicyV1", (const void*)xl6TaskSubmitOwnedPolicyV1) ||
        !xlApiAddTccSymbol(s, "xl6PayloadBoxFutureV1", (const void*)xl6PayloadBoxFutureV1) ||
        !xlApiAddTccSymbol(s, "xl6PayloadReadFutureV1", (const void*)xl6PayloadReadFutureV1) ||
        !xlApiAddTccSymbol(s, "xl6PayloadTraceGet", (const void*)xl6PayloadTraceGet) ||
        !xlApiAddTccSymbol(s, "xl6PayloadResolveOwned", (const void*)xl6PayloadResolveOwned) ||
        !xlApiAddTccSymbol(s, "xl6PayloadObserveOutcome", (const void*)xl6PayloadObserveOutcome)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClosureRuntimeV1Get", (const void*)xl6ClosureRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClosureRuntimeV2Get", (const void*)xl6ClosureRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClosureCreateEnvironmentV1", (const void*)xl6ClosureCreateEnvironmentV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClosureRuntimeV3Get", (const void*)xl6ClosureRuntimeV3Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClosureBoxErasedV1", (const void*)xl6ClosureBoxErasedV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6CellRuntimeV1Get", (const void*)xl6CellRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6CellCreateLeafV1", (const void*)xl6CellCreateLeafV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6CellCreateNativeV2", (const void*)xl6CellCreateNativeV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6SlotRuntimeV1Get", (const void*)xl6SlotRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6GeneratorRuntimeV1Get", (const void*)xl6GeneratorRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6GeneratorRuntimeV2Get", (const void*)xl6GeneratorRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6GeneratorRuntimeV3Get", (const void*)xl6GeneratorRuntimeV3Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6GeneratorOwnershipAdapterV1", (const void*)xl6GeneratorOwnershipAdapterV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ContinuationFrameRuntimeV2Get", (const void*)xl6ContinuationFrameRuntimeV2Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationFrameRuntimeV5Get", (const void*)xl6ContinuationFrameRuntimeV5Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationFrameOwnershipAdapterV1", (const void*)xl6ContinuationFrameOwnershipAdapterV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ContinuationFrameRuntimeV4Get", (const void*)xl6ContinuationFrameRuntimeV4Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationProducedStorageV2Get", (const void*)xl6ContinuationProducedStorageV2Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationQueueOwnership", (const void*)xl6ContinuationQueueOwnership) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationQueueOwnershipAdapterV1", (const void*)xl6ContinuationQueueOwnershipAdapterV1) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorOwnership", (const void*)xl6ContinuationExecutorOwnership) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorOwnershipAdapterV1", (const void*)xl6ContinuationExecutorOwnershipAdapterV1) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationExecutorQuiescenceV1Get", (const void*)xl6ContinuationExecutorQuiescenceV1Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationQueueQuiescenceV1Get", (const void*)xl6ContinuationQueueQuiescenceV1Get) ||
        !xlApiAddTccSymbol(s, "xl6TaskPoolQuiescenceV1Get", (const void*)xl6TaskPoolQuiescenceV1Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationFrameRuntimeV3Get", (const void*)xl6ContinuationFrameRuntimeV3Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationProducerPolicyV1Get", (const void*)xl6ContinuationProducerPolicyV1Get) ||
        !xlApiAddTccSymbol(s, "xl6ContinuationWatchPolicyV1Get", (const void*)xl6ContinuationWatchPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ValueIteratorRuntimeV2Get", (const void*)xl6ValueIteratorRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ContinuationExecutorIsIdle", (const void*)xl6ContinuationExecutorIsIdle)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FrameStorageRuntimeV1Get", (const void*)xl6FrameStorageRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskRuntimeV1Get", (const void*)xl6TaskRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskResourceInitV1", (const void*)xl6TaskResourceInitV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskResourceRemoveV1", (const void*)xl6TaskResourceRemoveV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskResourceReleaseV1", (const void*)xl6TaskResourceReleaseV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskNativeBeginV1", (const void*)xl6TaskNativeBeginV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskNativeEndV1", (const void*)xl6TaskNativeEndV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileOpenV1", (const void*)xl6AsyncFileOpenV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileAdoptV1", (const void*)xl6AsyncFileAdoptV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileDestroyV1", (const void*)xl6AsyncFileDestroyV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileCloseV1", (const void*)xl6AsyncFileCloseV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileReadV1", (const void*)xl6AsyncFileReadV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileWriteV1", (const void*)xl6AsyncFileWriteV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileFlushV1", (const void*)xl6AsyncFileFlushV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileSizeV1", (const void*)xl6AsyncFileSizeV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileResizeV1", (const void*)xl6AsyncFileResizeV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileFlagsV1", (const void*)xl6AsyncFileFlagsV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6AsyncFileClosedV1", (const void*)xl6AsyncFileClosedV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6NetworkServiceRuntimeV1Get", (const void*)xl6NetworkServiceRuntimeV1Get) ||
        !xlApiAddTccSymbol(s, "xl6NetworkServiceRuntimeV2Get", (const void*)xl6NetworkServiceRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskRuntimeV2Get", (const void*)xl6TaskRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskRuntimeV3Get", (const void*)xl6TaskRuntimeV3Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskProducerPolicyV1Get", (const void*)xl6TaskProducerPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskNativeDataPolicyV1Get", (const void*)xl6TaskNativeDataPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskWatchPolicyV1Get", (const void*)xl6TaskWatchPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskOwnershipAdapterV1", (const void*)xl6TaskOwnershipAdapterV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskPoolOwnership", (const void*)xl6TaskPoolOwnership)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskPoolOwnershipAdapterV1", (const void*)xl6TaskPoolOwnershipAdapterV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskSubmitPayloadV1", (const void*)xl6TaskSubmitPayloadV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6TaskPoolTryQuiesceV1", (const void*)xl6TaskPoolTryQuiesceV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackRuntimeV1Get", (const void*)xl6FutureCallbackRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackRuntimeV2Get", (const void*)xl6FutureCallbackRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackRuntimeV4Get", (const void*)xl6FutureCallbackRuntimeV4Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackRuntimeV3Get", (const void*)xl6FutureCallbackRuntimeV3Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackCancelPolicyV1Get", (const void*)xl6FutureCallbackCancelPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureAggregateAllV2", (const void*)xl6FutureAggregateAllV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureDebugNameSetV2", (const void*)xl6FutureDebugNameSetV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureDebugNameCopyV2", (const void*)xl6FutureDebugNameCopyV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureFirstObservedMsV2", (const void*)xl6FutureFirstObservedMsV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureFirstTerminalObservedMsV2", (const void*)xl6FutureFirstTerminalObservedMsV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FuturePendingWatchesV2", (const void*)xl6FuturePendingWatchesV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureAggregateRaceV2", (const void*)xl6FutureAggregateRaceV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureAggregateWhenAnyIndexV2", (const void*)xl6FutureAggregateWhenAnyIndexV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureAggregateAllArrayV2", (const void*)xl6FutureAggregateAllArrayV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureAggregateRaceArrayV2", (const void*)xl6FutureAggregateRaceArrayV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureAggregateWhenAnyIndexArrayV2", (const void*)xl6FutureAggregateWhenAnyIndexArrayV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackProducerPolicyV1Get", (const void*)xl6FutureCallbackProducerPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackWatchPolicyV1Get", (const void*)xl6FutureCallbackWatchPolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6FutureCallbackOwnershipAdapterV1", (const void*)xl6FutureCallbackOwnershipAdapterV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6CaptureRuntimeV1Get", (const void*)xl6CaptureRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ActivationCaptureRuntimeV1Get", (const void*)xl6ActivationCaptureRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClassRuntimeV1Get", (const void*)xl6ClassRuntimeV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClassRuntimeV2Get", (const void*)xl6ClassRuntimeV2Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6ClassRuntimeV3Get", (const void*)xl6ClassRuntimeV3Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleCreateV1", (const void*)xl6StaticModuleCreateV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleCreateV2", (const void*)xl6StaticModuleCreateV2)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleDrainForV1", (const void*)xl6StaticModuleDrainForV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleOwnerV1", (const void*)xl6StaticModuleOwnerV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleOwnershipV1", (const void*)xl6StaticModuleOwnershipV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleRetireV1", (const void*)xl6StaticModuleRetireV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleDestroyV1", (const void*)xl6StaticModuleDestroyV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleDrainV1", (const void*)xl6StaticModuleDrainV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleReadBeginV1", (const void*)xl6StaticModuleReadBeginV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleReadAdmitsV1", (const void*)xl6StaticModuleReadAdmitsV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6StaticModuleReadEndV1", (const void*)xl6StaticModuleReadEndV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6DynamicFieldsReadAdmitsV1", (const void*)xl6DynamicFieldsReadAdmitsV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6OwnershipCatalogAdmitsReadV1", (const void*)xl6OwnershipCatalogAdmitsReadV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6OwnershipCatalogCreateV1", (const void*)xl6OwnershipCatalogCreateV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6OwnershipCatalogDestroyV1", (const void*)xl6OwnershipCatalogDestroyV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6OwnershipCatalogResolveV1", (const void*)xl6OwnershipCatalogResolveV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6OwnershipCatalogProbeV1", (const void*)xl6OwnershipCatalogProbeV1)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6OwnershipPlanTryCreateV6", (const void*)xl6OwnershipPlanTryCreateV6)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6XlangContextHandlePolicyV1Get", (const void*)xl6XlangContextHandlePolicyV1Get)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6BufferHandlePolicyV1Get", (const void*)xl6BufferHandlePolicyV1Get)) return 0;
#define XL6_NATIVE_RESOURCE_POLICY(Owner, Component, Getter) \
    if (!xlApiAddTccSymbol(s, #Getter, (const void*)Getter)) return 0;
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
#define XL6_POLICY_ADD_SYMBOL(Name, Type) \
    if (!xlApiAddTccSymbol(s, "xl6OwnershipCatalogAdd" #Name "V1", (const void*)xl6OwnershipCatalogAdd##Name##V1)) return 0;
    XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_ADD_SYMBOL)
#undef XL6_POLICY_ADD_SYMBOL
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) \
    if (!xlApiAddTccSymbol(s, #Getter, (const void*)Getter)) return 0;
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    if (!xlApiAddTccSymbol(s, "xl6ClassDynamicFieldPathV3", (const void*)xl6ClassDynamicFieldPathV3)) return 0;
    if (!xlApiAddTccSymbol(s, "xl6CodeLeaseAcquire", (const void*)xl6CodeLeaseAcquire) ||
        !xlApiAddTccSymbol(s, "xl6CodeActivityEnterV1", (const void*)xl6CodeActivityEnterV1) ||
        !xlApiAddTccSymbol(s, "xl6CodeActivityLeaveV1", (const void*)xl6CodeActivityLeaveV1) ||
        !xlApiAddTccSymbol(s, "xl6CodeLeaseAcquireScoped", (const void*)xl6CodeLeaseAcquireScoped) ||
        !xlApiAddTccSymbol(s, "xl6CodeScopeEnter", (const void*)xl6CodeScopeEnter) ||
        !xlApiAddTccSymbol(s, "xl6CodeScopeLeave", (const void*)xl6CodeScopeLeave) ||
        !xlApiAddTccSymbol(s, "xl6CodeLeaseRetain", (const void*)xl6CodeLeaseRetain) ||
        !xlApiAddTccSymbol(s, "xl6CodeLeaseRelease", (const void*)xl6CodeLeaseRelease) ||
        !xlApiAddTccSymbol(s, "xl6CodeLeaseOwnership", (const void*)xl6CodeLeaseOwnership) ||
        !xlApiAddTccSymbol(s, "xl6CodeDropCreate", (const void*)xl6CodeDropCreate) ||
        !xlApiAddTccSymbol(s, "xl6CodeDropAbort", (const void*)xl6CodeDropAbort) ||
        !xlApiAddTccSymbol(s, "xl6CodeDropInvoke", (const void*)xl6CodeDropInvoke) ||
        !xlApiAddTccSymbol(s, "xl6CodeFinalizerBind", (const void*)xl6CodeFinalizerBind)) return 0;
    if ( !xlApiAddTccSymbol(s, "xl6ErrorScopeBegin", (const void*)xl6ErrorScopeBegin) ||
         !xlApiAddTccSymbol(s, "xl6ErrorScopeBeginCleanup", (const void*)xl6ErrorScopeBeginCleanup) ||
         !xlApiAddTccSymbol(s, "xl6ErrorScopeEnd", (const void*)xl6ErrorScopeEnd) ||
         !xlApiAddTccSymbol(s, "xl6ErrorScopeCaptureFailure", (const void*)xl6ErrorScopeCaptureFailure) ||
         !xlApiAddTccSymbol(s, "xl6ErrorScopePropagateFailure", (const void*)xl6ErrorScopePropagateFailure) ||
         !xlApiAddTccSymbol(s, "xl6ForeignCallBegin", (const void*)xl6ForeignCallBegin) ||
         !xlApiAddTccSymbol(s, "xl6ForeignCallEnd", (const void*)xl6ForeignCallEnd) ||
         !xlApiAddTccSymbol(s, "xl6ForeignCallbackFail", (const void*)xl6ForeignCallbackFail) ) {
        return 0;
    }
    if ( !xlApiAddTccSymbol(s, "xl6FutureRunThread", (const void*)xl6FutureRunThread) ||
         !xlApiAddTccSymbol(s, "xl6FutureRef", (const void*)xl6FutureRef) ||
         !xlApiAddTccSymbol(s, "xl6FutureDestroy", (const void*)xl6FutureDestroy) ||
         !xlApiAddTccSymbol(s, "xl6FutureWait", (const void*)xl6FutureWait) ||
         !xlApiAddTccSymbol(s, "xl6FutureWaitFor", (const void*)xl6FutureWaitFor) ||
         !xlApiAddTccSymbol(s, "xl6FutureWaitUntil", (const void*)xl6FutureWaitUntil) ||
         !xlApiAddTccSymbol(s, "xl6FutureSetName", (const void*)xl6FutureSetName) ||
         !xlApiAddTccSymbol(s, "xl6FutureCreatedMs", (const void*)xl6FutureCreatedMs) ||
         !xlApiAddTccSymbol(s, "xl6FutureCompletedMs", (const void*)xl6FutureCompletedMs) ||
         !xlApiAddTccSymbol(s, "xl6FuturePendingCount", (const void*)xl6FuturePendingCount) ||
         !xlApiAddTccSymbol(s, "xl6FutureState", (const void*)xl6FutureState) ||
         !xlApiAddTccSymbol(s, "xl6FutureStatus", (const void*)xl6FutureStatus) ||
         !xlApiAddTccSymbol(s, "xl6FutureValue", (const void*)xl6FutureValue) ||
         !xlApiAddTccSymbol(s, "xl6FutureError", (const void*)xl6FutureError) ||
         !xlApiAddTccSymbol(s, "xl6FutureCancel", (const void*)xl6FutureCancel) ||
         !xlApiAddTccSymbol(s, "xl6FutureLinkCancel", (const void*)xl6FutureLinkCancel) ||
         !xlApiAddTccSymbol(s, "xl6FutureAny", (const void*)xl6FutureAny) ||
         !xlApiAddTccSymbol(s, "xl6FutureAll", (const void*)xl6FutureAll) ||
         !xlApiAddTccSymbol(s, "xl6FutureRace", (const void*)xl6FutureRace) ||
         !xlApiAddTccSymbol(s, "xl6FutureAllXValues", (const void*)xl6FutureAllXValues) ||
         !xlApiAddTccSymbol(s, "xl6FutureRaceChecked", (const void*)xl6FutureRaceChecked) ||
         !xlApiAddTccSymbol(s, "xl6FutureWhenAnyIndex", (const void*)xl6FutureWhenAnyIndex) ||
         !xlApiAddTccSymbol(s, "xl6FutureAllChecked", (const void*)xl6FutureAllChecked) ||
         !xlApiAddTccSymbol(s, "xl6FuturePickIndex", (const void*)xl6FuturePickIndex) ||
         !xlApiAddTccSymbol(s, "xl6FutureAllCount", (const void*)xl6FutureAllCount) ||
         !xlApiAddTccSymbol(s, "xl6FutureAllValue", (const void*)xl6FutureAllValue) ||
         !xlApiAddTccSymbol(s, "xl6FutureThen", (const void*)xl6FutureThen) ||
         !xlApiAddTccSymbol(s, "xl6FutureThenOwned", (const void*)xl6FutureThenOwned) ||
         !xlApiAddTccSymbol(s, "xl6FutureCatchOwned", (const void*)xl6FutureCatchOwned) ||
         !xlApiAddTccSymbol(s, "xl6FutureContinueOwned", (const void*)xl6FutureContinueOwned) ||
         !xlApiAddTccSymbol(s, "xl6FutureFinally", (const void*)xl6FutureFinally) ) {
        return 0;
    }
    if ( !xlApiAddTccSymbol(s, "xl6TaskGroupCreate", (const void*)xl6TaskGroupCreate) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupOwnershipV1", (const void*)xl6TaskGroupOwnershipV1) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupOwnershipAdapterV1", (const void*)xl6TaskGroupOwnershipAdapterV1) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupOwnershipAdapterV2", (const void*)xl6TaskGroupOwnershipAdapterV2) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupQuiescenceV1Get", (const void*)xl6TaskGroupQuiescenceV1Get) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupParentPolicyV1Get", (const void*)xl6TaskGroupParentPolicyV1Get) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupChild", (const void*)xl6TaskGroupChild) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupDestroy", (const void*)xl6TaskGroupDestroy) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupClose", (const void*)xl6TaskGroupClose) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupCancel", (const void*)xl6TaskGroupCancel) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupBindParent", (const void*)xl6TaskGroupBindParent) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupAdd", (const void*)xl6TaskGroupAdd) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupCount", (const void*)xl6TaskGroupCount) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupReap", (const void*)xl6TaskGroupReap) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupFuture", (const void*)xl6TaskGroupFuture) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupJoin", (const void*)xl6TaskGroupJoin) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupJoinFor", (const void*)xl6TaskGroupJoinFor) ||
         !xlApiAddTccSymbol(s, "xl6TaskGroupJoinUntil", (const void*)xl6TaskGroupJoinUntil) ) {
        return 0;
    }
    if ( !xlApiAddTccSymbol(s, "xl6ChannelCreate", (const void*)xl6ChannelCreate) ||
         !xlApiAddTccSymbol(s, "xl6ChannelDestroy", (const void*)xl6ChannelDestroy) ||
         !xlApiAddTccSymbol(s, "xl6ChannelSend", (const void*)xl6ChannelSend) ||
         !xlApiAddTccSymbol(s, "xl6ChannelRecv", (const void*)xl6ChannelRecv) ||
         !xlApiAddTccSymbol(s, "xl6ChannelRecvFor", (const void*)xl6ChannelRecvFor) ||
         !xlApiAddTccSymbol(s, "xl6ChannelClose", (const void*)xl6ChannelClose) ||
         !xlApiAddTccSymbol(s, "xl6ChannelCount", (const void*)xl6ChannelCount) ||
         !xlApiAddTccSymbol(s, "xl6ChannelIsClosed", (const void*)xl6ChannelIsClosed) ) {
        return 0;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( !xlApiAddTccSymbol(s, "xlCompileScriptStringToExe", (const void*)xlCompileScriptStringToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileScriptFileToExe", (const void*)xlCompileScriptFileToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFileToExe", (const void*)xlCompileCSourceFileToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFilesToExe", (const void*)xlCompileCSourceFilesToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFileToSharedLibrary", (const void*)xlCompileCSourceFileToSharedLibrary) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFilesToSharedLibrary", (const void*)xlCompileCSourceFilesToSharedLibrary) ||
         !xlApiAddTccSymbol(s, "xlcDefaultContextOptions", (const void*)xlcDefaultContextOptions) ||
         !xlApiAddTccSymbol(s, "xlcCreateContext", (const void*)xlcCreateContext) ||
         !xlApiAddTccSymbol(s, "xlcDestroyContext", (const void*)xlcDestroyContext) ||
         !xlApiAddTccSymbol(s, "xlcSetWorkDir", (const void*)xlcSetWorkDir) ||
         !xlApiAddTccSymbol(s, "xlcSetTempDir", (const void*)xlcSetTempDir) ||
         !xlApiAddTccSymbol(s, "xlcSetSaveTemps", (const void*)xlcSetSaveTemps) ||
         !xlApiAddTccSymbol(s, "xlcSetBackend", (const void*)xlcSetBackend) ||
         !xlApiAddTccSymbol(s, "xlcAddIncludePath", (const void*)xlcAddIncludePath) ||
         !xlApiAddTccSymbol(s, "xlcAddLibraryPath", (const void*)xlcAddLibraryPath) ||
         !xlApiAddTccSymbol(s, "xlcAddLinkLibrary", (const void*)xlcAddLinkLibrary) ||
         !xlApiAddTccSymbol(s, "xlcAddResourceFile", (const void*)xlcAddResourceFile) ||
         !xlApiAddTccSymbol(s, "xlcAddCSourceFile", (const void*)xlcAddCSourceFile) ||
         !xlApiAddTccSymbol(s, "xlcLoadDependencyLibrary", (const void*)xlcLoadDependencyLibrary) ||
         !xlApiAddTccSymbol(s, "xlcRegisterCFunction", (const void*)xlcRegisterCFunction) ||
         !xlApiAddTccSymbol(s, "xlcAddSourceFile", (const void*)xlcAddSourceFile) ||
         !xlApiAddTccSymbol(s, "xlcAddSourceString", (const void*)xlcAddSourceString) ||
         !xlApiAddTccSymbol(s, "xlcSetSourceOverlay", (const void*)xlcSetSourceOverlay) ||
         !xlApiAddTccSymbol(s, "xlcRemoveSourceOverlay", (const void*)xlcRemoveSourceOverlay) ||
         !xlApiAddTccSymbol(s, "xlcClearSourceOverlays", (const void*)xlcClearSourceOverlays) ||
         !xlApiAddTccSymbol(s, "xlcCompile", (const void*)xlcCompile) ||
         !xlApiAddTccSymbol(s, "xlcCheck", (const void*)xlcCheck) ||
         !xlApiAddTccSymbol(s, "xlcCheckWithSources", (const void*)xlcCheckWithSources) ||
         !xlApiAddTccSymbol(s, "xlcCheckWithIndex", (const void*)xlcCheckWithIndex) ||
         !xlApiAddTccSymbol(s, "xlcComplete", (const void*)xlcComplete) ||
         !xlApiAddTccSymbol(s, "xlcEmitTokens", (const void*)xlcEmitTokens) ||
         !xlApiAddTccSymbol(s, "xlcEmitAst", (const void*)xlcEmitAst) ||
         !xlApiAddTccSymbol(s, "xlcEmitIr", (const void*)xlcEmitIr) ||
         !xlApiAddTccSymbol(s, "xlcEmitLoweredIr", (const void*)xlcEmitLoweredIr) ||
         !xlApiAddTccSymbol(s, "xlcEmitC", (const void*)xlcEmitC) ||
         !xlApiAddTccSymbol(s, "xlcEmitDebugC", (const void*)xlcEmitDebugC) ||
         !xlApiAddTccSymbol(s, "xlcEmitHeader", (const void*)xlcEmitHeader) ||
         !xlApiAddTccSymbol(s, "xlcEmitAbi", (const void*)xlcEmitAbi) ||
         !xlApiAddTccSymbol(s, "xlcEmitExe", (const void*)xlcEmitExe) ||
         !xlApiAddTccSymbol(s, "xlcEmitDebugExe", (const void*)xlcEmitDebugExe) ||
         !xlApiAddTccSymbol(s, "xlcRuntimeContext", (const void*)xlcRuntimeContext) ||
         !xlApiAddTccSymbol(s, "xlcRelocateModule", (const void*)xlcRelocateModule) ||
         !xlApiAddTccSymbol(s, "xlcRun", (const void*)xlcRun) ||
         !xlApiAddTccSymbol(s, "xlcGetLastError", (const void*)xlcGetLastError) ||
         !xlApiAddTccSymbol(s, "xlcClearLastError", (const void*)xlcClearLastError) ||
         !xlApiAddTccSymbol(s, "xlcGetLastExitCode", (const void*)xlcGetLastExitCode) ||
         !xlApiAddTccSymbol(s, "xlcEmitSharedLibrary", (const void*)xlcEmitSharedLibrary) ) {
        return 0;
    }
#endif

#ifdef XL_STATIC_TCC_VFS
    if ( !xlApiAddTccSymbol(s, "tcc_vfs_open", (const void*)tcc_vfs_open) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_close", (const void*)tcc_vfs_close) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_read", (const void*)tcc_vfs_read) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_lseek", (const void*)tcc_vfs_lseek) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_fopen", (const void*)tcc_vfs_fopen) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_fclose", (const void*)tcc_vfs_fclose) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_mount_memory", (const void*)tcc_vfs_mount_memory) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_clear_dynamic", (const void*)tcc_vfs_clear_dynamic) ) {
        return 0;
    }
#endif
    return 1;
}

typedef struct XL_XrtRuntimeSymbol {
    const char* name;
    const void* value;
} XL_XrtRuntimeSymbol;

#define XL_XRT_SYMBOL(name) { #name, (const void*)(name) },
static const XL_XrtRuntimeSymbol xl__xrtRuntimeSymbols[] = {
#include "xl_xrt_symbols.inc"
};
#undef XL_XRT_SYMBOL

int xlApiAddDefaultRuntimeSymbols(TCCState* s)
{
    size_t i;

    if ( s == NULL ) {
        return 0;
    }
    if ( !xlApiAddTccSymbol(s, "__mzerosf", &xl__mzerosf) ||
         !xlApiAddTccSymbol(s, "__mzerodf", &xl__mzerodf) ) {
        return 0;
    }
    for ( i = 0; i < sizeof(xl__xrtRuntimeSymbols) / sizeof(xl__xrtRuntimeSymbols[0]); ++i ) {
        if ( !xlApiAddTccSymbol(
                s,
                xl__xrtRuntimeSymbols[i].name,
                xl__xrtRuntimeSymbols[i].value) ) {
            return 0;
        }
    }
    return xlAddDefaultTccApiSymbols(s);
}
