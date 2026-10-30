#include "xl_api_internal.h"

#include "include/xl.h"
#include "include/xl_package.h"
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
#include "src/runtime/xl6_c_owned_capture.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_slot.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_value_iterator_internal.h"
#include "src/runtime/xl6_list_value_iterator.h"
#include "src/runtime/xl6_frame_storage.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_task_pool_exit.h"
#include "src/runtime/xl6_execution_worker_exit.h"
#include "src/runtime/xl6_executor_jobs.h"
#include "src/runtime/xl6_execution_resource.h"
#include "src/runtime/xl6_execution_net_file.h"
#include "src/runtime/xl6_execution_tls.h"
#include "src/runtime/xl6_execution_tcp_dial.h"
#include "src/runtime/xl6_execution_module_abi.h"
#include "src/runtime/xl6_network_service.h"
#include "src/runtime/xl6_log.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_foreign_resource.h"
#include "src/runtime/xl6_value_copy.h"
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
#include "src/runtime/xl6_value_numeric.h"
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
#include "src/runtime/xl6_channel_future.h"
#include "src/runtime/xl6_channel_direct_future.h"
#include "src/runtime/xl6_deadline_service.h"
#include "src/runtime/xl6_channel_future_deadline.h"
#include "src/runtime/xl6_channel_direct_deadline.h"
#include "src/runtime/xl6_channel_select_deadline.h"
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
        if ( !visit((s), (name), (const void*)xl__tcc.member) ) { \
            return 0; \
        } \
    } while (0)

static int xlVisitDefaultTccApiSymbols(XL_ApiSymbolVisit visit, void* user)
{
    if ( visit == NULL ) {
        return 0;
    }

    XL_ADD_TCC_API_SYMBOL(user, tcc_new, "tcc_new");
    XL_ADD_TCC_API_SYMBOL(user, tcc_delete, "tcc_delete");
    XL_ADD_TCC_API_SYMBOL(user, tcc_set_lib_path, "tcc_set_lib_path");
    XL_ADD_TCC_API_SYMBOL(user, tcc_set_error_func, "tcc_set_error_func");
    XL_ADD_TCC_API_SYMBOL(user, tcc_set_options, "tcc_set_options");
    XL_ADD_TCC_API_SYMBOL(user, tcc_add_include_path, "tcc_add_include_path");
    XL_ADD_TCC_API_SYMBOL(user, tcc_add_sysinclude_path, "tcc_add_sysinclude_path");
    XL_ADD_TCC_API_SYMBOL(user, tcc_define_symbol, "tcc_define_symbol");
    XL_ADD_TCC_API_SYMBOL(user, tcc_undefine_symbol, "tcc_undefine_symbol");
    XL_ADD_TCC_API_SYMBOL(user, tcc_add_library_path, "tcc_add_library_path");
    XL_ADD_TCC_API_SYMBOL(user, tcc_add_library, "tcc_add_library");
    XL_ADD_TCC_API_SYMBOL(user, tcc_add_file, "tcc_add_file");
    XL_ADD_TCC_API_SYMBOL(user, tcc_add_symbol, "tcc_add_symbol");
    XL_ADD_TCC_API_SYMBOL(user, tcc_set_output_type, "tcc_set_output_type");
    XL_ADD_TCC_API_SYMBOL(user, tcc_compile_string, "tcc_compile_string");
    XL_ADD_TCC_API_SYMBOL(user, tcc_output_file, "tcc_output_file");
    XL_ADD_TCC_API_SYMBOL(user, tcc_run, "tcc_run");
    XL_ADD_TCC_API_SYMBOL(user, tcc_relocate, "tcc_relocate");
    XL_ADD_TCC_API_SYMBOL(user, tcc_get_symbol, "tcc_get_symbol");
    XL_ADD_TCC_API_SYMBOL(user, tcc_list_symbols, "tcc_list_symbols");
    XL_ADD_TCC_API_SYMBOL(user, tcc_set_backtrace_func, "tcc_set_backtrace_func");
    if ( !visit(user, "xlGetVersionString", (const void*)xlGetVersionString) ||
         !visit(user, "xlGetVersionCode", (const void*)xlGetVersionCode) ||
         !visit(user, "xlDefaultContextOptions", (const void*)xlDefaultContextOptions) ||
         !visit(user, "xlRuntimeInit", (const void*)xlRuntimeInit) ||
         !visit(user, "xlRuntimeUnit", (const void*)xlRuntimeUnit) ||
         !visit(user, "xlRuntimeDrainRetiringChecked", (const void*)xlRuntimeDrainRetiringChecked) ||
         !visit(user, "xlCreateContext", (const void*)xlCreateContext) ||
         !visit(user, "xlImportCompiledPackage", (const void*)xlImportCompiledPackage) ||
         !visit(user, "xlFindCompiledPackage", (const void*)xlFindCompiledPackage) ||
         !visit(user, "xlCompiledPackageRuntimeIdentity", (const void*)xlCompiledPackageRuntimeIdentity) ||
         !visit(user, "xlCompiledPackageInterfaceHash", (const void*)xlCompiledPackageInterfaceHash) ||
         !visit(user, "xlDestroyContext", (const void*)xlDestroyContext) ||
         !visit(user, "xlSetWorkDir", (const void*)xlSetWorkDir) ||
         !visit(user, "xlSetTempDir", (const void*)xlSetTempDir) ||
         !visit(user, "xlSetSaveTemps", (const void*)xlSetSaveTemps) ||
         !visit(user, "xlAddIncludePath", (const void*)xlAddIncludePath) ||
         !visit(user, "xlAddLibraryPath", (const void*)xlAddLibraryPath) ||
         !visit(user, "xlAddLinkLibrary", (const void*)xlAddLinkLibrary) ||
         !visit(user, "xlAddResourceFile", (const void*)xlAddResourceFile) ||
         !visit(user, "xlLoadDependencyLibrary", (const void*)xlLoadDependencyLibrary) ||
         !visit(user, "xlRegisterCFunction", (const void*)xlRegisterCFunction) ||
         !visit(user, "xlRunScriptString", (const void*)xlRunScriptString) ||
         !visit(user, "xlRunScriptFile", (const void*)xlRunScriptFile) ||
         !visit(user, "xlRunScriptStringArg", (const void*)xlRunScriptStringArg) ||
         !visit(user, "xlRunScriptFileArg", (const void*)xlRunScriptFileArg) ||
         !visit(user, "xlCompileScriptStringToModule", (const void*)xlCompileScriptStringToModule) ||
         !visit(user, "xlCompileScriptFileToModule", (const void*)xlCompileScriptFileToModule) ||
         !visit(user, "xlCompileCSourceStringToModule", (const void*)xlCompileCSourceStringToModule) ||
         !visit(user, "xlCompileCSourceFileToModule", (const void*)xlCompileCSourceFileToModule) ||
         !visit(user, "xlCompileCSourceFilesToModule", (const void*)xlCompileCSourceFilesToModule) ||
         !visit(user, "xlRunCSourceString", (const void*)xlRunCSourceString) ||
         !visit(user, "xlRunCSourceFile", (const void*)xlRunCSourceFile) ||
         !visit(user, "xlRunCSourceFiles", (const void*)xlRunCSourceFiles) ||
         !visit(user, "xlActivateModule", (const void*)xlActivateModule) ||
         !visit(user, "xlDeactivateModule", (const void*)xlDeactivateModule) ||
         !visit(user, "xlRunModuleMain", (const void*)xlRunModuleMain) ||
         !visit(user, "xlModuleGetSymbol", (const void*)xlModuleGetSymbol) ||
         !visit(user, "xlDestroyModule", (const void*)xlDestroyModule) ||
         !visit(user, "xlGetLastError", (const void*)xlGetLastError) ||
         !visit(user, "xlClearLastError", (const void*)xlClearLastError) ||
         !visit(user, "xlGetLastExitCode", (const void*)xlGetLastExitCode) ) {
        return 0;
    }
    if ( !visit(user, "xlTccRegisterXrtEnvironment", (const void*)xlTccRegisterXrtEnvironment) ) {
        return 0;
    }
#define XL6_LOG_EXPORT(Name, Component) \
    if (!visit(user, #Name, (const void*)Name)) return 0;
#include "src/runtime/xl6_log_symbols.inc"
#undef XL6_LOG_EXPORT
#define XL6_TEXT_EXPORT(Name, Component) \
    if (!visit(user, #Name, (const void*)Name)) return 0;
#include "src/runtime/xl6_text_symbols.inc"
#undef XL6_TEXT_EXPORT
    if (!visit(user, "xl6NativeHandleTakeV1", (const void*)xl6NativeHandleTakeV1) ||
        !visit(user, "xl6NativeHandleTraceLeaf", (const void*)xl6NativeHandleTraceLeaf) ||
        !visit(user, "xl6ProcessCommandHandlePolicyV1Get", (const void*)xl6ProcessCommandHandlePolicyV1Get) ||
        !visit(user, "xl6ProcessHandlePolicyV1Get", (const void*)xl6ProcessHandlePolicyV1Get) ||
        !visit(user, "xl6ProcessStatusHandlePolicyV1Get", (const void*)xl6ProcessStatusHandlePolicyV1Get) ||
        !visit(user, "xl6BufferIteratorHandlePolicyV1Get", (const void*)xl6BufferIteratorHandlePolicyV1Get)) return 0;
    if ( !visit(user, "xl6ProtocolWitnessValidate", (const void*)xl6ProtocolWitnessValidate) ||
         !visit(user, "xl6ProtocolWitnessFind", (const void*)xl6ProtocolWitnessFind) ||
         !visit(user, "xl6ProtocolRegister", (const void*)xl6ProtocolRegister) ||
         !visit(user, "xl6ProtocolUnregister", (const void*)xl6ProtocolUnregister) ||
         !visit(user, "xl6ProtocolLookup", (const void*)xl6ProtocolLookup) ) {
        return 0;
    }
    if (!visit(user, "xl6ContinuationExecutorCreateFixedV1", (const void*)xl6ContinuationExecutorCreateFixedV1) ||
        !visit(user, "xl6ContinuationExecutorReserveOwned", (const void*)xl6ContinuationExecutorReserveOwned) ||
        !visit(user, "xl6ContinuationExecutorClose", (const void*)xl6ContinuationExecutorClose) ||
        !visit(user, "xl6ContinuationExecutorDestroy", (const void*)xl6ContinuationExecutorDestroy) ||
        !visit(user, "xl6ContinuationExecutorDestroyCompleted", (const void*)xl6ContinuationExecutorDestroyCompleted) ||
        !visit(user, "xl6ContinuationReserveSuccessor", (const void*)xl6ContinuationReserveSuccessor) ||
        !visit(user, "xl6ContinuationCommit", (const void*)xl6ContinuationCommit) ||
        !visit(user, "xl6ContinuationComplete", (const void*)xl6ContinuationComplete) ||
        !visit(user, "xl6ContinuationAbort", (const void*)xl6ContinuationAbort) ||
        !visit(user, "xl6ContinuationTraceBind", (const void*)xl6ContinuationTraceBind) ||
        !visit(user, "xl6ContinuationOwnership", (const void*)xl6ContinuationOwnership) ||
        !visit(user, "xl6ContinuationWatchRegister", (const void*)xl6ContinuationWatchRegister)) return 0;
    if (!visit(user, "xl6PayloadDropGet", (const void*)xl6PayloadDropGet) ||
        !visit(user, "xl6NativeFuturePayloadRuntimeV1Get", (const void*)xl6NativeFuturePayloadRuntimeV1Get) ||
        !visit(user, "xl6TaskSubmitOwnedPolicyV1", (const void*)xl6TaskSubmitOwnedPolicyV1) ||
        !visit(user, "xl6PayloadBoxFutureV1", (const void*)xl6PayloadBoxFutureV1) ||
        !visit(user, "xl6PayloadReadFutureV1", (const void*)xl6PayloadReadFutureV1) ||
        !visit(user, "xl6PayloadTraceGet", (const void*)xl6PayloadTraceGet) ||
        !visit(user, "xl6PayloadResolveOwned", (const void*)xl6PayloadResolveOwned) ||
        !visit(user, "xl6PayloadObserveOutcome", (const void*)xl6PayloadObserveOutcome)) return 0;
    if (!visit(user, "xl6ClosureRuntimeV1Get", (const void*)xl6ClosureRuntimeV1Get)) return 0;
    if (!visit(user, "xl6ClosureBoxAdmittedV1", (const void*)xl6ClosureBoxAdmittedV1)) return 0;
    if (!visit(user, "xl6ClosureBoxPointActionV1", (const void*)xl6ClosureBoxPointActionV1)) return 0;
    if (!visit(user, "xl6ClosureBoxIsPointActionV1", (const void*)xl6ClosureBoxIsPointActionV1)) return 0;
    if (!visit(user, "xl6ClosureRuntimeV2Get", (const void*)xl6ClosureRuntimeV2Get)) return 0;
    if (!visit(user, "xl6ClosureCreateEnvironmentV1", (const void*)xl6ClosureCreateEnvironmentV1)) return 0;
    if (!visit(user, "xl6ClosureRuntimeV3Get", (const void*)xl6ClosureRuntimeV3Get)) return 0;
    if (!visit(user, "xl6ClosureBoxErasedV1", (const void*)xl6ClosureBoxErasedV1)) return 0;
    if (!visit(user, "xl6CellRuntimeV1Get", (const void*)xl6CellRuntimeV1Get)) return 0;
    if (!visit(user, "xl6CellCreateLeafV1", (const void*)xl6CellCreateLeafV1)) return 0;
    if (!visit(user, "xl6CellCreateNativeV2", (const void*)xl6CellCreateNativeV2)) return 0;
    if (!visit(user, "xl6SlotRuntimeV1Get", (const void*)xl6SlotRuntimeV1Get)) return 0;
    if (!visit(user, "xl6GeneratorRuntimeV1Get", (const void*)xl6GeneratorRuntimeV1Get)) return 0;
    if (!visit(user, "xl6GeneratorRuntimeV2Get", (const void*)xl6GeneratorRuntimeV2Get)) return 0;
    if (!visit(user, "xl6GeneratorRuntimeV3Get", (const void*)xl6GeneratorRuntimeV3Get)) return 0;
    if (!visit(user, "xl6GeneratorOwnershipAdapterV1", (const void*)xl6GeneratorOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6ContinuationFrameFinishV1", (const void*)xl6ContinuationFrameFinishV1) ||
        !visit(user, "xl6ContinuationFrameLaunchV1", (const void*)xl6ContinuationFrameLaunchV1) ||
        !visit(user, "xl6ContinuationFrameRuntimeV2Get", (const void*)xl6ContinuationFrameRuntimeV2Get) ||
        !visit(user, "xl6ContinuationFrameRuntimeV5Get", (const void*)xl6ContinuationFrameRuntimeV5Get) ||
        !visit(user, "xl6ContinuationFrameOwnershipAdapterV1", (const void*)xl6ContinuationFrameOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6ContinuationFrameRuntimeV4Get", (const void*)xl6ContinuationFrameRuntimeV4Get) ||
        !visit(user, "xl6ContinuationProducedStorageV2Get", (const void*)xl6ContinuationProducedStorageV2Get) ||
        !visit(user, "xl6ContinuationQueueOwnership", (const void*)xl6ContinuationQueueOwnership) ||
        !visit(user, "xl6ContinuationQueueOwnershipAdapterV1", (const void*)xl6ContinuationQueueOwnershipAdapterV1) ||
        !visit(user, "xl6ContinuationExecutorOwnership", (const void*)xl6ContinuationExecutorOwnership) ||
        !visit(user, "xl6ContinuationExecutorOwnershipAdapterV1", (const void*)xl6ContinuationExecutorOwnershipAdapterV1) ||
        !visit(user, "xl6ContinuationExecutorQuiescenceV1Get", (const void*)xl6ContinuationExecutorQuiescenceV1Get) ||
        !visit(user, "xl6ContinuationQueueQuiescenceV1Get", (const void*)xl6ContinuationQueueQuiescenceV1Get) ||
        !visit(user, "xl6TaskPoolQuiescenceV1Get", (const void*)xl6TaskPoolQuiescenceV1Get) ||
        !visit(user, "xl6ContinuationFrameRuntimeV3Get", (const void*)xl6ContinuationFrameRuntimeV3Get) ||
        !visit(user, "xl6ContinuationProducerPolicyV1Get", (const void*)xl6ContinuationProducerPolicyV1Get) ||
        !visit(user, "xl6ContinuationWatchPolicyV1Get", (const void*)xl6ContinuationWatchPolicyV1Get)) return 0;
    if (!visit(user, "xl6ValueIteratorRuntimeV2Get", (const void*)xl6ValueIteratorRuntimeV2Get)) return 0;
    if (!visit(user, "xl6ContinuationExecutorIsIdle", (const void*)xl6ContinuationExecutorIsIdle)) return 0;
    if (!visit(user, "xl6FrameStorageRuntimeV1Get", (const void*)xl6FrameStorageRuntimeV1Get)) return 0;
    if (!visit(user, "xl6TaskRuntimeV1Get", (const void*)xl6TaskRuntimeV1Get)) return 0;
    if (!visit(user, "xl6TaskPoolControlV1Get", (const void*)xl6TaskPoolControlV1Get)) return 0;
    if (!visit(user, "xl6ExecutorCreateLanguageV1", (const void*)xl6ExecutorCreateLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorDestroyLanguageV1", (const void*)xl6ExecutorDestroyLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSubmitLanguageV1", (const void*)xl6ExecutorSubmitLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorCloseLanguageV1", (const void*)xl6ExecutorCloseLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorCancelLanguageV1", (const void*)xl6ExecutorCancelLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorWaitLanguageV1", (const void*)xl6ExecutorWaitLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorThrowLanguageV1", (const void*)xl6ExecutorThrowLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorFailureCountLanguageV1", (const void*)xl6ExecutorFailureCountLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorFailureSaturatedLanguageV1", (const void*)xl6ExecutorFailureSaturatedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotLanguageV1", (const void*)xl6ExecutorSnapshotLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotDestroyLanguageV1", (const void*)xl6ExecutorSnapshotDestroyLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotClosedLanguageV1", (const void*)xl6ExecutorSnapshotClosedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotCancellingLanguageV1", (const void*)xl6ExecutorSnapshotCancellingLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotThreadsLanguageV1", (const void*)xl6ExecutorSnapshotThreadsLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotQueueLimitLanguageV1", (const void*)xl6ExecutorSnapshotQueueLimitLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotQueuedLanguageV1", (const void*)xl6ExecutorSnapshotQueuedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotRunningLanguageV1", (const void*)xl6ExecutorSnapshotRunningLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotSubmittedLanguageV1", (const void*)xl6ExecutorSnapshotSubmittedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotCompletedLanguageV1", (const void*)xl6ExecutorSnapshotCompletedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotExecutedLanguageV1", (const void*)xl6ExecutorSnapshotExecutedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotStolenLanguageV1", (const void*)xl6ExecutorSnapshotStolenLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotCancelledLanguageV1", (const void*)xl6ExecutorSnapshotCancelledLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorSnapshotRejectedLanguageV1", (const void*)xl6ExecutorSnapshotRejectedLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutionModuleInitializeV1", (const void*)xl6ExecutionModuleInitializeV1)) return 0;
    if (!visit(user, "xl6ExecutionModulePrepareV1", (const void*)xl6ExecutionModulePrepareV1)) return 0;
    if (!visit(user, "xl6ExecutionModuleRetireV1", (const void*)xl6ExecutionModuleRetireV1)) return 0;
    if (!visit(user, "xl6ExecutionSourceCreateV1", (const void*)xl6ExecutionSourceCreateV1)) return 0;
    if (!visit(user, "xl6ExecutionSourceCloseV1", (const void*)xl6ExecutionSourceCloseV1)) return 0;
    if (!visit(user, "xl6ExecutionSourceCheckV1", (const void*)xl6ExecutionSourceCheckV1)) return 0;
    if (!visit(user, "xl6ExecutionSourceRetryV1", (const void*)xl6ExecutionSourceRetryV1)) return 0;
    if (!visit(user, "xl6ExecutionSourceSnapshotV1Get", (const void*)xl6ExecutionSourceSnapshotV1Get)) return 0;
    if (!visit(user, "xl6ExecutionSourceDestroyV1", (const void*)xl6ExecutionSourceDestroyV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorCreateV1", (const void*)xl6ExecutionExecutorCreateV1)) return 0;
    if (!visit(user, "xl6ExecutionTaskPoolCreateV1", (const void*)xl6ExecutionTaskPoolCreateV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceRetainV1", (const void*)xl6ExecutionResourceRetainV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceReleaseV1", (const void*)xl6ExecutionResourceReleaseV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceCloseV1", (const void*)xl6ExecutionResourceCloseV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceRetryV1", (const void*)xl6ExecutionResourceRetryV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceSnapshotV1Get", (const void*)xl6ExecutionResourceSnapshotV1Get)) return 0;
    if (!visit(user, "xl6ExecutionResourceEnterV1", (const void*)xl6ExecutionResourceEnterV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceLeaveV1", (const void*)xl6ExecutionResourceLeaveV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorSubmitV1", (const void*)xl6ExecutionExecutorSubmitV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorSubmitBatchV1", (const void*)xl6ExecutionExecutorSubmitBatchV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorWaitV1", (const void*)xl6ExecutionExecutorWaitV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorThrowIfFailedV1", (const void*)xl6ExecutionExecutorThrowIfFailedV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorFailuresV1", (const void*)xl6ExecutionExecutorFailuresV1)) return 0;
    if (!visit(user, "xl6ExecutionExecutorStatsV1", (const void*)xl6ExecutionExecutorStatsV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceCancelV1", (const void*)xl6ExecutionResourceCancelV1)) return 0;
    if (!visit(user, "xl6OwnershipCatalogAddNetFileInputV1", (const void*)xl6OwnershipCatalogAddNetFileInputV1)) return 0;
    if (!visit(user, "xl6ExecutionNetFileCreateV1", (const void*)xl6ExecutionNetFileCreateV1)) return 0;
    if (!visit(user, "xl6ExecutionNetFileReadV1", (const void*)xl6ExecutionNetFileReadV1)) return 0;
    if (!visit(user, "xl6ExecutionNetFileWriteV1", (const void*)xl6ExecutionNetFileWriteV1)) return 0;
    if (!visit(user, "xl6ExecutionNetFileWriteRefV1", (const void*)xl6ExecutionNetFileWriteRefV1)) return 0;
    if (!visit(user, "xl6ExecutionNetFileCloseSourceV1", (const void*)xl6ExecutionNetFileCloseSourceV1)) return 0;
    if (!visit(user, "xl6ExecutionNetFileFamilyV1Get", (const void*)xl6ExecutionNetFileFamilyV1Get)) return 0;
    if (!visit(user, "xl6OwnershipDomainNetFileNativeV1Get", (const void*)xl6OwnershipDomainNetFileNativeV1Get)) return 0;
    if (!visit(user, "xl6OwnershipDomainExecutionNetFileV1Get", (const void*)xl6OwnershipDomainExecutionNetFileV1Get)) return 0;
    if (!visit(user, "xl6ExecutionSourceOwnershipV1", (const void*)xl6ExecutionSourceOwnershipV1)) return 0;
    if (!visit(user, "xl6ExecutionResourceOwnershipV1", (const void*)xl6ExecutionResourceOwnershipV1)) return 0;
    if (!visit(user, "xl6ExecutionOwnershipAdapterV1", (const void*)xl6ExecutionOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6ExecutionSourceQuiescenceV1Get", (const void*)xl6ExecutionSourceQuiescenceV1Get)) return 0;
    if (!visit(user, "xl6ExecutionResourceQuiescenceV1Get", (const void*)xl6ExecutionResourceQuiescenceV1Get)) return 0;
    if (!visit(user, "xl6ExecutionExecutorFamilyV1Get", (const void*)xl6ExecutionExecutorFamilyV1Get)) return 0;
    if (!visit(user, "xl6ExecutionTaskPoolFamilyV1Get", (const void*)xl6ExecutionTaskPoolFamilyV1Get)) return 0;
    if (!visit(user, "xl6TaskPoolReleaseClearedOwnerV1", (const void*)xl6TaskPoolReleaseClearedOwnerV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresCreateV1", (const void*)xl6ExecutionFailuresCreateV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresRetainV1", (const void*)xl6ExecutionFailuresRetainV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresReleaseV1", (const void*)xl6ExecutionFailuresReleaseV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresSnapshotV1", (const void*)xl6ExecutionFailuresSnapshotV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresCheckV1", (const void*)xl6ExecutionFailuresCheckV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresUnreportedV1", (const void*)xl6ExecutionFailuresUnreportedV1)) return 0;
    if (!visit(user, "xl6ExecutorSubmitClosureV1", (const void*)xl6ExecutorSubmitClosureV1)) return 0;
    if (!visit(user, "xl6ExecutorSubmitClosuresV1", (const void*)xl6ExecutorSubmitClosuresV1)) return 0;
    if (!visit(user, "xl6ExecutorPrepareBoxedBatchV1", (const void*)xl6ExecutorPrepareBoxedBatchV1)) return 0;
    if (!visit(user, "xl6ExecutorCommitBoxedBatchV1", (const void*)xl6ExecutorCommitBoxedBatchV1)) return 0;
    if (!visit(user, "xl6ExecutorDiscardBoxedBatchV1", (const void*)xl6ExecutorDiscardBoxedBatchV1)) return 0;
    if (!visit(user, "xl6ExecutorSubmitBatchLanguageV1", (const void*)xl6ExecutorSubmitBatchLanguageV1)) return 0;
    if (!visit(user, "xl6ExecutorWaitV1", (const void*)xl6ExecutorWaitV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresOwnershipV1", (const void*)xl6ExecutionFailuresOwnershipV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresRecordV1", (const void*)xl6ExecutionFailuresRecordV1)) return 0;
    if (!visit(user, "xl6ExecutionFailuresAdapterV1", (const void*)xl6ExecutionFailuresAdapterV1)) return 0;
    if (!visit(user, "xl6ExecutionWorkerExitV1", (const void*)xl6ExecutionWorkerExitV1)) return 0;
    if (!visit(user, "xl6ExecutionWorkerExitDropV1", (const void*)xl6ExecutionWorkerExitDropV1)) return 0;
    if (!visit(user, "xl6TaskPoolCreateManagedV1", (const void*)xl6TaskPoolCreateManagedV1)) return 0;
    if (!visit(user, "xl6TaskPoolWorkerExitPolicyV1Get", (const void*)xl6TaskPoolWorkerExitPolicyV1Get)) return 0;
    if (!visit(user, "xl6TaskPoolOwnershipAdapterV2", (const void*)xl6TaskPoolOwnershipAdapterV2)) return 0;
    if (!visit(user, "xl6ExecutionTaskPoolCheckExitV1", (const void*)xl6ExecutionTaskPoolCheckExitV1)) return 0;
    if (!visit(user, "xl6ExecutionTaskPoolExitFailuresV1", (const void*)xl6ExecutionTaskPoolExitFailuresV1)) return 0;
    if (!visit(user, "xl6ExecutorClosureDataPolicyV1Get", (const void*)xl6ExecutorClosureDataPolicyV1Get)) return 0;
    if (!visit(user, "xl6ExecutorWorkerExitPolicyV1Get", (const void*)xl6ExecutorWorkerExitPolicyV1Get)) return 0;
    if (!visit(user, "xl6ExecutorClosureQuiescenceV1Get", (const void*)xl6ExecutorClosureQuiescenceV1Get)) return 0;
    if (!visit(user, "xl6ExecutorJobsOwnershipAdapterV1", (const void*)xl6ExecutorJobsOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6TaskResourceInitV1", (const void*)xl6TaskResourceInitV1)) return 0;
    if (!visit(user, "xl6TaskResourceRemoveV1", (const void*)xl6TaskResourceRemoveV1)) return 0;
    if (!visit(user, "xl6TaskResourceReleaseV1", (const void*)xl6TaskResourceReleaseV1)) return 0;
    if (!visit(user, "xl6TaskNativeBeginV1", (const void*)xl6TaskNativeBeginV1)) return 0;
    if (!visit(user, "xl6TaskNativeEndV1", (const void*)xl6TaskNativeEndV1)) return 0;
    if (!visit(user, "xl6AsyncFileOpenV1", (const void*)xl6AsyncFileOpenV1)) return 0;
    if (!visit(user, "xl6AsyncFileAdoptV1", (const void*)xl6AsyncFileAdoptV1)) return 0;
    if (!visit(user, "xl6AsyncFileDestroyV1", (const void*)xl6AsyncFileDestroyV1)) return 0;
    if (!visit(user, "xl6AsyncFileCloseV1", (const void*)xl6AsyncFileCloseV1)) return 0;
    if (!visit(user, "xl6AsyncFileReadV1", (const void*)xl6AsyncFileReadV1)) return 0;
    if (!visit(user, "xl6AsyncFileWriteV1", (const void*)xl6AsyncFileWriteV1)) return 0;
    if (!visit(user, "xl6AsyncFileFlushV1", (const void*)xl6AsyncFileFlushV1)) return 0;
    if (!visit(user, "xl6AsyncFileSizeV1", (const void*)xl6AsyncFileSizeV1)) return 0;
    if (!visit(user, "xl6AsyncFileResizeV1", (const void*)xl6AsyncFileResizeV1)) return 0;
    if (!visit(user, "xl6AsyncFileFlagsV1", (const void*)xl6AsyncFileFlagsV1)) return 0;
    if (!visit(user, "xl6AsyncFileClosedV1", (const void*)xl6AsyncFileClosedV1)) return 0;
    if (!visit(user, "xl6NetworkServiceRuntimeV1Get", (const void*)xl6NetworkServiceRuntimeV1Get) ||
        !visit(user, "xl6NetworkServiceRuntimeV2Get", (const void*)xl6NetworkServiceRuntimeV2Get)) return 0;
    if (!visit(user, "xl6TaskRuntimeV2Get", (const void*)xl6TaskRuntimeV2Get)) return 0;
    if (!visit(user, "xl6TaskRuntimeV3Get", (const void*)xl6TaskRuntimeV3Get)) return 0;
    if (!visit(user, "xl6TaskProducerPolicyV1Get", (const void*)xl6TaskProducerPolicyV1Get)) return 0;
    if (!visit(user, "xl6TaskNativeDataPolicyV1Get", (const void*)xl6TaskNativeDataPolicyV1Get)) return 0;
    if (!visit(user, "xl6TaskWatchPolicyV1Get", (const void*)xl6TaskWatchPolicyV1Get)) return 0;
    if (!visit(user, "xl6TaskOwnershipAdapterV1", (const void*)xl6TaskOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6TaskPoolOwnership", (const void*)xl6TaskPoolOwnership)) return 0;
    if (!visit(user, "xl6TaskPoolOwnershipAdapterV1", (const void*)xl6TaskPoolOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6TaskSubmitPayloadV1", (const void*)xl6TaskSubmitPayloadV1)) return 0;
    if (!visit(user, "xl6TaskPoolTryQuiesceV1", (const void*)xl6TaskPoolTryQuiesceV1)) return 0;
    if (!visit(user, "xl6TaskPoolModulePrepareV1", (const void*)xl6TaskPoolModulePrepareV1)) return 0;
    if (!visit(user, "xl6FutureCallbackRuntimeV1Get", (const void*)xl6FutureCallbackRuntimeV1Get)) return 0;
    if (!visit(user, "xl6FutureCallbackRuntimeV2Get", (const void*)xl6FutureCallbackRuntimeV2Get)) return 0;
    if (!visit(user, "xl6FutureCallbackRuntimeV4Get", (const void*)xl6FutureCallbackRuntimeV4Get)) return 0;
    if (!visit(user, "xl6FutureCallbackRuntimeV5Get", (const void*)xl6FutureCallbackRuntimeV5Get)) return 0;
    if (!visit(user, "xl6FutureCallbackFailV1", (const void*)xl6FutureCallbackFailV1)) return 0;
    if (!visit(user, "xl6ChannelFutureRuntimeV1Get", (const void*)xl6ChannelFutureRuntimeV1Get)) return 0;
    if (!visit(user, "xl6ChannelDirectFutureRuntimeV1Get", (const void*)xl6ChannelDirectFutureRuntimeV1Get)) return 0;
#define XL6_DEADLINE_SYMBOL(Name) if(!visit(user,#Name,(const void*)Name))return 0;
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceCreateV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineBindTakeV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineDetachV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineTicketIsIdleV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineTicketServiceOwnershipV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceCloseV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceDestroyV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceDestroyCompletedV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceCheckV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineFromMillisecondsV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceOwnershipV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceOwnershipAdapterV1)
    XL6_DEADLINE_SYMBOL(xl6DeadlineServiceQuiescenceV1Get)
#undef XL6_DEADLINE_SYMBOL
#define XL6_CHANNEL_DEADLINE_SYMBOL(Name) if(!visit(user,#Name,(const void*)Name))return 0;
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreWakeTakeV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreWakePolicyV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreWakeDataV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreStartWakeV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreRetainV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCorePublishBuilderV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreBusyV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelDirectFutureQuiescenceV1Get)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureQuiescenceV1Get)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreSubscriptionRefV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureCoreStopV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureDeadlineAttachV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureDeadlineMatchesV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelFutureDeadlineStartV1)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelDirectDeadlineRuntimeV1Get)
    XL6_CHANNEL_DEADLINE_SYMBOL(xl6ChannelSelectDeadlineRuntimeV1Get)
#undef XL6_CHANNEL_DEADLINE_SYMBOL
    if (!visit(user, "xl6FutureCallbackRuntimeV3Get", (const void*)xl6FutureCallbackRuntimeV3Get)) return 0;
    if (!visit(user, "xl6FutureCallbackCancelPolicyV1Get", (const void*)xl6FutureCallbackCancelPolicyV1Get)) return 0;
    if (!visit(user, "xl6FutureAggregateAllV2", (const void*)xl6FutureAggregateAllV2)) return 0;
    if (!visit(user, "xl6FutureDebugNameSetV2", (const void*)xl6FutureDebugNameSetV2)) return 0;
    if (!visit(user, "xl6FutureDebugNameCopyV2", (const void*)xl6FutureDebugNameCopyV2)) return 0;
    if (!visit(user, "xl6FutureFirstObservedMsV2", (const void*)xl6FutureFirstObservedMsV2)) return 0;
    if (!visit(user, "xl6FutureFirstTerminalObservedMsV2", (const void*)xl6FutureFirstTerminalObservedMsV2)) return 0;
    if (!visit(user, "xl6FuturePendingWatchesV2", (const void*)xl6FuturePendingWatchesV2)) return 0;
    if (!visit(user, "xl6FutureAggregateRaceV2", (const void*)xl6FutureAggregateRaceV2)) return 0;
    if (!visit(user, "xl6FutureAggregateWhenAnyIndexV2", (const void*)xl6FutureAggregateWhenAnyIndexV2)) return 0;
    if (!visit(user, "xl6FutureAggregateAllArrayV2", (const void*)xl6FutureAggregateAllArrayV2)) return 0;
    if (!visit(user, "xl6FutureAggregateRaceArrayV2", (const void*)xl6FutureAggregateRaceArrayV2)) return 0;
    if (!visit(user, "xl6FutureAggregateWhenAnyIndexArrayV2", (const void*)xl6FutureAggregateWhenAnyIndexArrayV2)) return 0;
    if (!visit(user, "xl6FutureCallbackProducerPolicyV1Get", (const void*)xl6FutureCallbackProducerPolicyV1Get)) return 0;
    if (!visit(user, "xl6FutureCallbackWatchPolicyV1Get", (const void*)xl6FutureCallbackWatchPolicyV1Get)) return 0;
    if (!visit(user, "xl6FutureCallbackOwnershipAdapterV1", (const void*)xl6FutureCallbackOwnershipAdapterV1)) return 0;
    if (!visit(user, "xl6CaptureRuntimeV1Get", (const void*)xl6CaptureRuntimeV1Get)) return 0;
    if (!visit(user, "xl6ActivationCaptureRuntimeV1Get", (const void*)xl6ActivationCaptureRuntimeV1Get)) return 0;
    if (!visit(user, "xl6ClassRuntimeV1Get", (const void*)xl6ClassRuntimeV1Get)) return 0;
    if (!visit(user, "xl6ClassRuntimeV2Get", (const void*)xl6ClassRuntimeV2Get)) return 0;
    if (!visit(user, "xl6ClassRuntimeV3Get", (const void*)xl6ClassRuntimeV3Get)) return 0;
    if (!visit(user, "xl6StaticModuleCreateV1", (const void*)xl6StaticModuleCreateV1)) return 0;
    if (!visit(user, "xl6StaticModuleCreateV2", (const void*)xl6StaticModuleCreateV2)) return 0;
    if (!visit(user, "xl6StaticModuleDrainForV1", (const void*)xl6StaticModuleDrainForV1)) return 0;
    if (!visit(user, "xl6StaticModuleOwnerV1", (const void*)xl6StaticModuleOwnerV1)) return 0;
    if (!visit(user, "xl6StaticModuleOwnershipV1", (const void*)xl6StaticModuleOwnershipV1)) return 0;
    if (!visit(user, "xl6StaticModuleRetireV1", (const void*)xl6StaticModuleRetireV1)) return 0;
    if (!visit(user, "xl6StaticModuleDestroyV1", (const void*)xl6StaticModuleDestroyV1)) return 0;
    if (!visit(user, "xl6StaticModuleDrainV1", (const void*)xl6StaticModuleDrainV1)) return 0;
    if (!visit(user, "xl6StaticModuleReadBeginV1", (const void*)xl6StaticModuleReadBeginV1)) return 0;
    if (!visit(user, "xl6StaticModuleReadAdmitsV1", (const void*)xl6StaticModuleReadAdmitsV1)) return 0;
    if (!visit(user, "xl6StaticModuleReadEndV1", (const void*)xl6StaticModuleReadEndV1)) return 0;
    if (!visit(user, "xl6DynamicFieldsReadAdmitsV1", (const void*)xl6DynamicFieldsReadAdmitsV1)) return 0;
    if (!visit(user, "xl6OwnershipCatalogAdmitsReadV1", (const void*)xl6OwnershipCatalogAdmitsReadV1)) return 0;
    if (!visit(user, "xl6OwnershipCatalogCreateV1", (const void*)xl6OwnershipCatalogCreateV1)) return 0;
    if (!visit(user, "xl6OwnershipCatalogDestroyV1", (const void*)xl6OwnershipCatalogDestroyV1)) return 0;
    if (!visit(user, "xl6OwnershipCatalogResolveV1", (const void*)xl6OwnershipCatalogResolveV1)) return 0;
    if (!visit(user, "xl6OwnershipCatalogProbeV1", (const void*)xl6OwnershipCatalogProbeV1)) return 0;
    if (!visit(user, "xl6OwnershipPlanTryCreateV6", (const void*)xl6OwnershipPlanTryCreateV6)) return 0;
    if (!visit(user, "xl6XlangContextHandlePolicyV1Get", (const void*)xl6XlangContextHandlePolicyV1Get)) return 0;
    if (!visit(user, "xl6BufferHandlePolicyV1Get", (const void*)xl6BufferHandlePolicyV1Get)) return 0;
#define XL6_NATIVE_RESOURCE_POLICY(Owner, Component, Getter) \
    if (!visit(user, #Getter, (const void*)Getter)) return 0;
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
#define XL6_POLICY_ADD_SYMBOL(Name, Type) \
    if (!visit(user, "xl6OwnershipCatalogAdd" #Name "V1", (const void*)xl6OwnershipCatalogAdd##Name##V1)) return 0;
    XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_ADD_SYMBOL)
#undef XL6_POLICY_ADD_SYMBOL
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) \
    if (!visit(user, #Getter, (const void*)Getter)) return 0;
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    if (!visit(user, "xl6ExecutionTcpPrepareDialV1", (const void*)xl6ExecutionTcpPrepareDialV1) ||
        !visit(user, "xl6ExecutionTcpDialSourceV1", (const void*)xl6ExecutionTcpDialSourceV1) ||
        !visit(user, "xl6ExecutionTcpDialAddressesSourceV1", (const void*)xl6ExecutionTcpDialAddressesSourceV1) ||
        !visit(user, "xl6ExecutionTcpPromoteDialV1", (const void*)xl6ExecutionTcpPromoteDialV1)) return 0;
    if (!visit(user, "xl6ExecutionTcpProxyDialSourceV1", (const void*)xl6ExecutionTcpProxyDialSourceV1)) return 0;
    if (!visit(user, "xl6ClassDynamicFieldPathV3", (const void*)xl6ClassDynamicFieldPathV3)) return 0;
    if (!visit(user, "xl6CodeLeaseAcquire", (const void*)xl6CodeLeaseAcquire) ||
        !visit(user, "xl6CodeActivityEnterV1", (const void*)xl6CodeActivityEnterV1) ||
        !visit(user, "xl6CodeActivityLeaveV1", (const void*)xl6CodeActivityLeaveV1) ||
        !visit(user, "xl6CodeLeaseAcquireScoped", (const void*)xl6CodeLeaseAcquireScoped) ||
        !visit(user, "xl6CodeScopeEnter", (const void*)xl6CodeScopeEnter) ||
        !visit(user, "xl6CodeScopeLeave", (const void*)xl6CodeScopeLeave) ||
        !visit(user, "xl6CodeLeaseRetain", (const void*)xl6CodeLeaseRetain) ||
        !visit(user, "xl6CodeLeaseRelease", (const void*)xl6CodeLeaseRelease) ||
        !visit(user, "xl6CodeLeaseOwnership", (const void*)xl6CodeLeaseOwnership) ||
        !visit(user, "xl6CodeDropCreate", (const void*)xl6CodeDropCreate) ||
        !visit(user, "xl6CodeDropAbort", (const void*)xl6CodeDropAbort) ||
        !visit(user, "xl6CodeDropInvoke", (const void*)xl6CodeDropInvoke) ||
        !visit(user, "xl6CodeFinalizerBind", (const void*)xl6CodeFinalizerBind)) return 0;
    if ( !visit(user, "xl6ErrorScopeBegin", (const void*)xl6ErrorScopeBegin) ||
         !visit(user, "xl6ErrorScopeBeginCleanup", (const void*)xl6ErrorScopeBeginCleanup) ||
         !visit(user, "xl6ErrorScopeEnd", (const void*)xl6ErrorScopeEnd) ||
         !visit(user, "xl6ErrorScopeCaptureFailure", (const void*)xl6ErrorScopeCaptureFailure) ||
         !visit(user, "xl6ErrorScopePropagateFailure", (const void*)xl6ErrorScopePropagateFailure) ||
         !visit(user, "xl6ForeignCallBegin", (const void*)xl6ForeignCallBegin) ||
         !visit(user, "xl6ForeignCallEnd", (const void*)xl6ForeignCallEnd) ||
         !visit(user, "xl6ForeignCallbackFail", (const void*)xl6ForeignCallbackFail) ) {
        return 0;
    }
    if ( !visit(user, "xl6FutureRunThread", (const void*)xl6FutureRunThread) ||
         !visit(user, "xl6FutureRef", (const void*)xl6FutureRef) ||
         !visit(user, "xl6FutureDestroy", (const void*)xl6FutureDestroy) ||
         !visit(user, "xl6FutureWait", (const void*)xl6FutureWait) ||
         !visit(user, "xl6FutureWaitFor", (const void*)xl6FutureWaitFor) ||
         !visit(user, "xl6FutureWaitUntil", (const void*)xl6FutureWaitUntil) ||
         !visit(user, "xl6FutureSetName", (const void*)xl6FutureSetName) ||
         !visit(user, "xl6FutureCreatedMs", (const void*)xl6FutureCreatedMs) ||
         !visit(user, "xl6FutureCompletedMs", (const void*)xl6FutureCompletedMs) ||
         !visit(user, "xl6FuturePendingCount", (const void*)xl6FuturePendingCount) ||
         !visit(user, "xl6FutureState", (const void*)xl6FutureState) ||
         !visit(user, "xl6FutureStatus", (const void*)xl6FutureStatus) ||
         !visit(user, "xl6FutureValue", (const void*)xl6FutureValue) ||
         !visit(user, "xl6FutureError", (const void*)xl6FutureError) ||
         !visit(user, "xl6FutureCancel", (const void*)xl6FutureCancel) ||
         !visit(user, "xl6FutureLinkCancel", (const void*)xl6FutureLinkCancel) ||
         !visit(user, "xl6FutureAny", (const void*)xl6FutureAny) ||
         !visit(user, "xl6FutureAll", (const void*)xl6FutureAll) ||
         !visit(user, "xl6FutureRace", (const void*)xl6FutureRace) ||
         !visit(user, "xl6FutureAllXValues", (const void*)xl6FutureAllXValues) ||
         !visit(user, "xl6FutureRaceChecked", (const void*)xl6FutureRaceChecked) ||
         !visit(user, "xl6FutureWhenAnyIndex", (const void*)xl6FutureWhenAnyIndex) ||
         !visit(user, "xl6FutureAllChecked", (const void*)xl6FutureAllChecked) ||
         !visit(user, "xl6FuturePickIndex", (const void*)xl6FuturePickIndex) ||
         !visit(user, "xl6FutureAllCount", (const void*)xl6FutureAllCount) ||
         !visit(user, "xl6FutureAllValue", (const void*)xl6FutureAllValue) ||
         !visit(user, "xl6FutureThen", (const void*)xl6FutureThen) ||
         !visit(user, "xl6FutureThenOwned", (const void*)xl6FutureThenOwned) ||
         !visit(user, "xl6FutureCatchOwned", (const void*)xl6FutureCatchOwned) ||
         !visit(user, "xl6FutureContinueOwned", (const void*)xl6FutureContinueOwned) ||
         !visit(user, "xl6FutureFinally", (const void*)xl6FutureFinally) ) {
        return 0;
    }
    if ( !visit(user, "xl6TaskGroupCreate", (const void*)xl6TaskGroupCreate) ||
         !visit(user, "xl6TaskGroupOwnershipV1", (const void*)xl6TaskGroupOwnershipV1) ||
         !visit(user, "xl6TaskGroupOwnershipAdapterV1", (const void*)xl6TaskGroupOwnershipAdapterV1) ||
         !visit(user, "xl6TaskGroupOwnershipAdapterV2", (const void*)xl6TaskGroupOwnershipAdapterV2) ||
         !visit(user, "xl6TaskGroupQuiescenceV1Get", (const void*)xl6TaskGroupQuiescenceV1Get) ||
         !visit(user, "xl6TaskGroupParentPolicyV1Get", (const void*)xl6TaskGroupParentPolicyV1Get) ||
         !visit(user, "xl6TaskGroupChild", (const void*)xl6TaskGroupChild) ||
         !visit(user, "xl6TaskGroupDestroy", (const void*)xl6TaskGroupDestroy) ||
         !visit(user, "xl6TaskGroupClose", (const void*)xl6TaskGroupClose) ||
         !visit(user, "xl6TaskGroupCancel", (const void*)xl6TaskGroupCancel) ||
         !visit(user, "xl6TaskGroupBindParent", (const void*)xl6TaskGroupBindParent) ||
         !visit(user, "xl6TaskGroupAdd", (const void*)xl6TaskGroupAdd) ||
         !visit(user, "xl6TaskGroupCount", (const void*)xl6TaskGroupCount) ||
         !visit(user, "xl6TaskGroupReap", (const void*)xl6TaskGroupReap) ||
         !visit(user, "xl6TaskGroupFuture", (const void*)xl6TaskGroupFuture) ||
         !visit(user, "xl6TaskGroupJoin", (const void*)xl6TaskGroupJoin) ||
         !visit(user, "xl6TaskGroupJoinFor", (const void*)xl6TaskGroupJoinFor) ||
         !visit(user, "xl6TaskGroupJoinUntil", (const void*)xl6TaskGroupJoinUntil) ) {
        return 0;
    }
    if ( !visit(user, "xl6ChannelCreate", (const void*)xl6ChannelCreate) ||
         !visit(user, "xl6ChannelDestroy", (const void*)xl6ChannelDestroy) ||
         !visit(user, "xl6ChannelSend", (const void*)xl6ChannelSend) ||
         !visit(user, "xl6ChannelRecv", (const void*)xl6ChannelRecv) ||
         !visit(user, "xl6ChannelRecvFor", (const void*)xl6ChannelRecvFor) ||
         !visit(user, "xl6ChannelClose", (const void*)xl6ChannelClose) ||
         !visit(user, "xl6ChannelCount", (const void*)xl6ChannelCount) ||
         !visit(user, "xl6ChannelIsClosed", (const void*)xl6ChannelIsClosed) ) {
        return 0;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( !visit(user, "xlCompileScriptStringToExe", (const void*)xlCompileScriptStringToExe) ||
         !visit(user, "xlCompileScriptFileToExe", (const void*)xlCompileScriptFileToExe) ||
         !visit(user, "xlCompileCSourceFileToExe", (const void*)xlCompileCSourceFileToExe) ||
         !visit(user, "xlCompileCSourceFilesToExe", (const void*)xlCompileCSourceFilesToExe) ||
         !visit(user, "xlCompileCSourceFileToSharedLibrary", (const void*)xlCompileCSourceFileToSharedLibrary) ||
         !visit(user, "xlCompileCSourceFilesToSharedLibrary", (const void*)xlCompileCSourceFilesToSharedLibrary) ||
         !visit(user, "xlcDefaultContextOptions", (const void*)xlcDefaultContextOptions) ||
         !visit(user, "xlcCreateContext", (const void*)xlcCreateContext) ||
         !visit(user, "xlcDestroyContext", (const void*)xlcDestroyContext) ||
         !visit(user, "xlcSetWorkDir", (const void*)xlcSetWorkDir) ||
         !visit(user, "xlcSetTempDir", (const void*)xlcSetTempDir) ||
         !visit(user, "xlcSetSaveTemps", (const void*)xlcSetSaveTemps) ||
         !visit(user, "xlcSetBackend", (const void*)xlcSetBackend) ||
         !visit(user, "xlcAddIncludePath", (const void*)xlcAddIncludePath) ||
         !visit(user, "xlcAddLibraryPath", (const void*)xlcAddLibraryPath) ||
         !visit(user, "xlcAddLinkLibrary", (const void*)xlcAddLinkLibrary) ||
         !visit(user, "xlcAddResourceFile", (const void*)xlcAddResourceFile) ||
         !visit(user, "xlcAddCSourceFile", (const void*)xlcAddCSourceFile) ||
         !visit(user, "xlcLoadDependencyLibrary", (const void*)xlcLoadDependencyLibrary) ||
         !visit(user, "xlcRegisterCFunction", (const void*)xlcRegisterCFunction) ||
         !visit(user, "xlcAddSourceFile", (const void*)xlcAddSourceFile) ||
         !visit(user, "xlcAddSourceString", (const void*)xlcAddSourceString) ||
         !visit(user, "xlcSetSourceOverlay", (const void*)xlcSetSourceOverlay) ||
         !visit(user, "xlcRemoveSourceOverlay", (const void*)xlcRemoveSourceOverlay) ||
         !visit(user, "xlcClearSourceOverlays", (const void*)xlcClearSourceOverlays) ||
         !visit(user, "xlcCompile", (const void*)xlcCompile) ||
         !visit(user, "xlcCheck", (const void*)xlcCheck) ||
         !visit(user, "xlcCheckWithSources", (const void*)xlcCheckWithSources) ||
         !visit(user, "xlcCheckWithIndex", (const void*)xlcCheckWithIndex) ||
         !visit(user, "xlcComplete", (const void*)xlcComplete) ||
         !visit(user, "xlcEmitTokens", (const void*)xlcEmitTokens) ||
         !visit(user, "xlcEmitAst", (const void*)xlcEmitAst) ||
         !visit(user, "xlcEmitIr", (const void*)xlcEmitIr) ||
         !visit(user, "xlcEmitLoweredIr", (const void*)xlcEmitLoweredIr) ||
         !visit(user, "xlcEmitC", (const void*)xlcEmitC) ||
         !visit(user, "xlcEmitDebugC", (const void*)xlcEmitDebugC) ||
         !visit(user, "xlcEmitHeader", (const void*)xlcEmitHeader) ||
         !visit(user, "xlcEmitAbi", (const void*)xlcEmitAbi) ||
         !visit(user, "xlcEmitExe", (const void*)xlcEmitExe) ||
         !visit(user, "xlcEmitDebugExe", (const void*)xlcEmitDebugExe) ||
         !visit(user, "xlcRuntimeContext", (const void*)xlcRuntimeContext) ||
         !visit(user, "xlcRelocateModule", (const void*)xlcRelocateModule) ||
         !visit(user, "xlcRun", (const void*)xlcRun) ||
         !visit(user, "xlcGetLastError", (const void*)xlcGetLastError) ||
         !visit(user, "xlcClearLastError", (const void*)xlcClearLastError) ||
         !visit(user, "xlcGetLastExitCode", (const void*)xlcGetLastExitCode) ||
         !visit(user, "xlcEmitSharedLibrary", (const void*)xlcEmitSharedLibrary) ) {
        return 0;
    }
#endif

#ifdef XL_STATIC_TCC_VFS
    if ( !visit(user, "tcc_vfs_open", (const void*)tcc_vfs_open) ||
         !visit(user, "tcc_vfs_close", (const void*)tcc_vfs_close) ||
         !visit(user, "tcc_vfs_read", (const void*)tcc_vfs_read) ||
         !visit(user, "tcc_vfs_lseek", (const void*)tcc_vfs_lseek) ||
         !visit(user, "tcc_vfs_fopen", (const void*)tcc_vfs_fopen) ||
         !visit(user, "tcc_vfs_fclose", (const void*)tcc_vfs_fclose) ||
         !visit(user, "tcc_vfs_mount_memory", (const void*)tcc_vfs_mount_memory) ||
         !visit(user, "tcc_vfs_clear_dynamic", (const void*)tcc_vfs_clear_dynamic) ) {
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

int xlApiVisitDefaultRuntimeSymbols(XL_ApiSymbolVisit visit,void* user)
{
    size_t i;

    if ( visit == NULL ) {
        return 0;
    }
    if ( !visit(user, "__mzerosf", &xl__mzerosf) ||
         !visit(user, "__mzerodf", &xl__mzerodf) ) {
        return 0;
    }
    for ( i = 0; i < sizeof(xl__xrtRuntimeSymbols) / sizeof(xl__xrtRuntimeSymbols[0]); ++i ) {
        if ( !visit(
                user,
                xl__xrtRuntimeSymbols[i].name,
                xl__xrtRuntimeSymbols[i].value) ) {
            return 0;
        }
    }
    return xlVisitDefaultTccApiSymbols(visit,user);
}

static int xlAddDefaultRuntimeSymbol(void* state,const char* name,const void* address)
{
    return address==NULL || xlApiAddTccSymbol((TCCState*)state,name,address);
}

int xlApiAddDefaultRuntimeSymbols(TCCState* state)
{
    return state!=NULL && xlApiVisitDefaultRuntimeSymbols(xlAddDefaultRuntimeSymbol,state);
}
