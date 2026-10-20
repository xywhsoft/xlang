param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$caseDir = Join-Path $Root "test\cases"
$jsonValueGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_json_value_bridge.ps1"))
$jsonStreamGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_json_stream_bridge.ps1"))
$xlonValueGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_xlon_value_bridge.ps1"))
$xlonStreamGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_xlon_stream_bridge.ps1"))
$hostErrorBoundaryGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_host_error_boundary.ps1"))
$serializationCallbackGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_serialization_callback_error.ps1"))
$textOutputGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_text_output_bridge.ps1"))
$systemBridgeGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_system_bridges.ps1"))
$dynamicCompareGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_dynamic_compare_contract.ps1"))
$evaluationLifetimeGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_evaluation_lifetime.ps1"))
$openObjectFieldGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_core_type_closure_contract.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_base_07.bat"))
$namedOwnerGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_core_callable_control_contract.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_base_08.bat"))
$membershipOwnerGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_membership_evaluation_lifetime.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_base_09.bat"))
$binaryOwnerGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_binary_evaluation_lifetime.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_base_10.bat"))
$moduleFinalizationGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_module_finalization.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_base_10.bat"))
$classCleanupGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_class_cleanup_outcome.ps1"))
$exportCallbackGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_export_callback.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_function_abi.bat"))
$closureArgumentConversionGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_closure_argument_conversion.ps1"))
$foreignErrorScopeGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_foreign_error_scope.ps1")) +
    [System.IO.File]::ReadAllText((Join-Path $Root "test\suites\03_core_function_abi.bat"))
$cleanupDetachGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_cleanup_detach.ps1"))
$scopeExitPlanGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_scope_exit_plan.ps1"))
$scopeCleanupOutcomeGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_scope_cleanup_outcome.ps1"))
$exitRouteGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_exit_route.ps1"))
$abruptCleanupGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_abrupt_cleanup_outcome.ps1"))
$parameterCleanupGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_parameter_cleanup_outcome.ps1"))
$loopCleanupGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_loop_cleanup_outcome.ps1"))
$pendingExceptionRouteGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_pending_exception_route.ps1"))
$returnProjectionGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_return_projection_owner.ps1"))
$conditionalNullableJoinGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_conditional_nullable_join.ps1"))
$managedStorageInputsGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_managed_storage_inputs.ps1"))
$ownerConsumptionGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_owner_consumption.ps1"))
$returnCommitGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_return_commit.ps1"))
$xcallResultFailureGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_xcall_result_failure.ps1"))
$erasedCollectionMemberGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_erased_collection_members.ps1"))
$nativeFactoryDeclarationGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_native_factory_declaration_abi.ps1"))
$determinism = Join-Path $Root "tools\check_xlir_cgen_determinism.ps1"
$runtimeMatrix = Join-Path $Root "tools\check_xlir_cgen_runtime_matrix.ps1"
$defaultClassGate = Join-Path $Root "tools\check_xlir_default_class_layout.ps1"
$instanceClassMethodGate = Join-Path $Root "tools\check_xlir_class_instance_method.ps1"
$implicitClassReceiverGate = Join-Path $Root "tools\check_xlir_class_implicit_receiver.ps1"
$explicitConstructorFieldGate = Join-Path $Root "tools\check_xlir_explicit_constructor_field_initializer.ps1"
$classReferenceFieldGate = Join-Path $Root "tools\check_xlir_class_reference_field_initializer.ps1"
$persistenceJsonGate = Join-Path $Root "tools\check_xlir_persistence_json_abi.ps1"
$recursivePersistenceGate = Join-Path $Root "tools\check_xlir_persistence_recursive_class_values.ps1"
$persistenceProjectionRootGate = Join-Path $Root "tools\check_xlir_persistence_projection_roots.ps1"
$recursiveCollectionPersistenceGate = Join-Path $Root "tools\check_xlir_persistence_recursive_collection_values.ps1"
$explicitClassConstructorGate = Join-Path $Root "tools\check_xlir_class_explicit_constructor_base.ps1"
$staticClassGate = Join-Path $Root "tools\check_xlir_class_static_storage.ps1"
$virtualClassModelGate = Join-Path $Root "tools\check_xlir_class_virtual_dispatch_model.ps1"
$protocolDispatchGate = Join-Path $Root "tools\check_xlir_protocol_dispatch_model.ps1"
$protocolWitnessAdapterGate = Join-Path $Root "tools\check_xlir_protocol_witness_adapter_abi.ps1"
$asyncClassMethodGate = Join-Path $Root "tools\check_xlir_async_class_method.ps1"
$cScalarAbiGate = Join-Path $Root "tools\check_xlir_c_scalar_abi.ps1"
$exportPrimitiveWidthGate = Join-Path $Root "tools\check_xlir_export_primitive_width_abi.ps1"
$u64AbiGate = Join-Path $Root "tools\check_xlir_u64_abi.ps1"
$integerConversionAbiGate = Join-Path $Root "tools\check_xlir_integer_conversion_abi.ps1"
$checkedFloatConversionGate = Join-Path $Root "tools\check_xlir_checked_float_conversion.ps1"
$checkedTimeConversionGate = Join-Path $Root "tools\check_xlir_checked_time_conversion.ps1"
$boolConversionAbiGate = Join-Path $Root "tools\check_xlir_bool_conversion_abi.ps1"
$checkedCastAbiGate = Join-Path $Root "tools\check_xlir_checked_cast_abi.ps1"
$nullableCastAbiGate = Join-Path $Root "tools\check_xlir_nullable_cast_abi.ps1"
$nominalCastAbiGate = Join-Path $Root "tools\check_xlir_nominal_cast_abi.ps1"
$dynamicConditionalTruthGate = Join-Path $Root "tools\check_xlir_dynamic_conditional_truth.ps1"
$dynamicUnaryAbiGate = Join-Path $Root "tools\check_xlir_dynamic_unary_abi.ps1"
$dynamicBinaryExtendedAbiGate = Join-Path $Root "tools\check_xlir_dynamic_binary_extended_abi.ps1"
$checkedIntegerArithmeticAbiGate = Join-Path $Root "tools\check_xlir_checked_integer_arithmetic_abi.ps1"
$i64PowAbiGate = Join-Path $Root "tools\check_xlir_i64_pow_abi.ps1"
$timeApproxAbiGate = Join-Path $Root "tools\check_xlir_time_approx_abi.ps1"
$specialF64ConstantAbiGate = Join-Path $Root "tools\check_xlir_special_f64_constant_abi.ps1"
$scalarConversionAbiGate = Join-Path $Root "tools\check_xlir_scalar_conversion_abi.ps1"
$timeConversionAbiGate = Join-Path $Root "tools\check_xlir_time_conversion_abi.ps1"
$stringConversionAbiGate = Join-Path $Root "tools\check_xlir_string_conversion_abi.ps1"
$classValueIdentityAbiGate = Join-Path $Root "tools\check_xlir_class_value_identity_abi.ps1"
$nativeClassObjectIdentityAbiGate = Join-Path $Root "tools\check_xlir_native_class_object_identity_abi.ps1"
$managedNativeExternReceiverGate = Join-Path $Root "tools\check_xlir_managed_native_extern_receiver_abi.ps1"
$managedNativeLifecycleGate = Join-Path $Root "tools\check_xlir_managed_native_lifecycle_abi.ps1"
$classLifecycleGate = Join-Path $Root "tools\check_xlir_class_lifecycle_abi.ps1"
$functionValueOwnershipGate = Join-Path $Root "tools\check_xlir_function_value_ownership.ps1"
$openFunctionGate = Join-Path $Root "tools\check_xlir_open_function_abi.ps1"
$foreignFunctionValueGate = Join-Path $Root "tools\check_xlir_foreign_function_value_abi.ps1"
$copyCloneGate = Join-Path $Root "tools\check_xlir_copy_clone.ps1"
$exportBoundaryGate = Join-Path $Root "tools\check_xlir_export_boundary.ps1"
$globalExportBoundaryGate = Join-Path $Root "tools\check_xlir_global_export_boundary.ps1"
$externGlobalBoundaryGate = Join-Path $Root "tools\check_xlir_extern_global_boundary.ps1"
$nullAbiBoundaryGate = Join-Path $Root "tools\check_xlir_null_abi_boundary.ps1"
$callSiteBindingGate = Join-Path $Root "tools\check_xlir_callsite_binding.ps1"
$callSiteVariadicGate = Join-Path $Root "tools\check_xlir_callsite_variadic.ps1"
$callSiteCVarargGate = Join-Path $Root "tools\check_xlir_callsite_c_vararg.ps1"
$callSiteGenericGate = Join-Path $Root "tools\check_xlir_callsite_generic_specialization.ps1"
$classProtocolSyntaxGate = Join-Path $Root "tools\check_xlir_class_protocol_syntax.ps1"
$intrinsicCallGate = Join-Path $Root "tools\check_xlir_intrinsic_calls.ps1"
$nativeNominalReceiverGate = Join-Path $Root "tools\check_xlir_native_nominal_receiver.ps1"
$nativeHandleBindGate = Join-Path $Root "tools\check_xlir_native_handle_bind.ps1"
$valueInitializerGate = Join-Path $Root "tools\check_xlir_value_initializer.ps1"
$reachabilityGate = Join-Path $Root "tools\check_xlir_reachability.ps1"
$productResultGate = Join-Path $Root "tools\check_xlir_product_result.ps1"
$namedFunctionAdapterGate = Join-Path $Root "tools\check_xlir_named_function_adapter.ps1"
$collectionMemberGate = Join-Path $Root "tools\check_xlir_collection_member_intrinsics.ps1"
$erasedArrayAppendGate = Join-Path $Root "tools\check_xlir_erased_array_append.ps1"
$collectionAlgebraGate = Join-Path $Root "tools\check_xlir_collection_algebra.ps1"
$dynamicCollectionMethodGate = Join-Path $Root "tools\check_xlir_dynamic_collection_methods.ps1"
$xcallGate = Join-Path $Root "tools\check_xlir_xcall.ps1"
$asyncGenericPlannerGate = Join-Path $Root "tools\check_xlir_async_generic_planner.ps1"
$asyncCfgLivenessGate = Join-Path $Root "tools\check_xlir_async_cfg_liveness.ps1"
$asyncMultiAwaitMachineGate = Join-Path $Root "tools\check_xlir_async_machine_multi_await.ps1"
$asyncMachineAdmissionGate = Join-Path $Root "tools\check_xlir_async_machine_admission.ps1"
$asyncErrorAbiGate = Join-Path $Root "tools\check_xlir_async_error_abi.ps1"
$futureCancelGate = Join-Path $Root "tools\check_xlir_future_cancel.ps1"
$futureMemberStatusGate = Join-Path $Root "tools\check_xlir_future_member_status_abi.ps1"
$taskWhenAnyLiteralGate = Join-Path $Root "tools\check_xlir_task_when_any_literal_abi.ps1"
$taskWhenAnyArrayGate = Join-Path $Root "tools\check_xlir_task_when_any_array_abi.ps1"
$taskAggregateArrayGate = Join-Path $Root "tools\check_xlir_task_aggregate_array_abi.ps1"
$taskNestedFutureGate = Join-Path $Root "tools\check_xlir_task_nested_future_abi.ps1"
$taskSpawnClosureGate = Join-Path $Root "tools\check_xlir_task_spawn_closure_abi.ps1"
$threadResourceGate = Join-Path $Root "tools\check_xlir_thread_resource_abi.ps1"
$channelResourceGate = Join-Path $Root "tools\check_xlir_channel_resource_abi.ps1"
$syncResourceGate = Join-Path $Root "tools\check_xlir_sync_resource_abi.ps1"
$specialQueueResourceGate = Join-Path $Root "tools\check_xlir_special_queue_resource_abi.ps1"
$stackResourceGate = Join-Path $Root "tools\check_xlir_stack_resource_abi.ps1"
$avlResourceGate = Join-Path $Root "tools\check_xlir_avl_resource_abi.ps1"
$futureContinuationGate = Join-Path $Root "tools\check_xlir_future_continuation_abi.ps1"
$typedCallableIndexGate = Join-Path $Root "tools\check_xlir_typed_callable_index_abi.ps1"
$typedCallableContainerGate = Join-Path $Root "tools\check_xlir_typed_callable_container_abi.ps1"
$typedSafeIndexNominalGate = Join-Path $Root "tools\check_xlir_typed_container_safe_index_objects_functions_abi.ps1"
$typedIndexAssignNominalGate = Join-Path $Root "tools\check_xlir_typed_container_index_assign_objects_functions_abi.ps1"
$dynamicKeyScalarProjectionGate = Join-Path $Root "tools\check_xlir_dynamic_key_scalar_projection.ps1"
$typedPointProjectionGate = Join-Path $Root "tools\check_xlir_typed_point_projection_abi.ps1"
$xvalueScalarBridgeGate = Join-Path $Root "tools\check_xlir_xvalue_scalar_bridge_abi.ps1"
$typedDictLiteralIndexGate = Join-Path $Root "tools\check_xlir_typed_dict_literal_index_abi.ps1"
$typedNativeHandleIndexGate = Join-Path $Root "tools\check_xlir_typed_native_handle_index_abi.ps1"
$typedNativeHandleIteratorGate = Join-Path $Root "tools\check_xlir_typed_native_handle_iterator_abi.ps1"
$dynamicKeyIndexGate = Join-Path $Root "tools\check_xlir_dynamic_key_index.ps1"
$indexWriteAbiGate = Join-Path $Root "tools\check_xlir_index_write_abi.ps1"
$ownedExpressionDiscardGate = Join-Path $Root "tools\check_xlir_owned_expression_discard.ps1"
$xvalueMembershipGate = Join-Path $Root "tools\check_xlir_xvalue_membership.ps1"
$stringMembershipGate = Join-Path $Root "tools\check_xlir_string_membership.ps1"
$rangeMembershipGate = Join-Path $Root "tools\check_xlir_range_membership.ps1"
$rangeForAbiGate = Join-Path $Root "tools\check_xlir_range_for_abi.ps1"
$generatorMachineGate = Join-Path $Root "tools\check_xlir_generator_machine.ps1"
$interpolatedStringGate = Join-Path $Root "tools\check_xlir_interpolated_string.ps1"
$arrayI64ForAbiGate = Join-Path $Root "tools\check_xlir_array_i64_for_abi.ps1"
$xvalueIteratorAbiGate = Join-Path $Root "tools\check_xlir_xvalue_iterator_abi.ps1"
$setI64IteratorAbiGate = Join-Path $Root "tools\check_xlir_set_i64_iterator_abi.ps1"
$iteratorScalarProjectionGate = Join-Path $Root "tools\check_xlir_xvalue_iterator_scalar_projection.ps1"
$iteratorManagedProjectionGate = Join-Path $Root "tools\check_xlir_xvalue_iterator_managed_projection.ps1"
$arrayIteratorProjectionGate = Join-Path $Root "tools\check_xlir_xvalue_array_iterator_projection.ps1"
$dictIteratorProjectionGate = Join-Path $Root "tools\check_xlir_dict_iterator_projection.ps1"
$erasedCollectionIteratorGate = Join-Path $Root "tools\check_xlir_erased_collection_iterator_abi.ps1"
$dynamicVarIteratorGate = Join-Path $Root "tools\check_xlir_dynamic_var_iterator_abi.ps1"
$stringIteratorGate = Join-Path $Root "tools\check_xlir_string_iterator_abi.ps1"
$charBytesAbiGate = Join-Path $Root "tools\check_xlir_char_bytes_abi.ps1"
$protocolIteratorGate = Join-Path $Root "tools\check_xlir_protocol_iterator_abi.ps1"
$collectionTypeRebindGate = Join-Path $Root "tools\check_xlir_collection_type_rebind.ps1"
$nullableContainerProjectionGate = Join-Path $Root "tools\check_xlir_nullable_container_projection.ps1"
$safeIndexNullableCollectionGate = Join-Path $Root "tools\check_xlir_safe_index_nullable_collection_abi.ps1"
$nullableSafeClassFieldGate = Join-Path $Root "tools\check_xlir_nullable_safe_class_field_abi.ps1"
$memberAbiGate = Join-Path $Root "tools\check_xlir_member_abi.ps1"
$lengthProtocolMemberGate = Join-Path $Root "tools\check_xlir_length_protocol_member.ps1"
$classFieldMemberAbiGate = Join-Path $Root "tools\check_xlir_class_field_member_abi.ps1"
$classFieldInheritedNarrowedGate = Join-Path $Root "tools\check_xlir_class_field_inherited_narrowed_abi.ps1"
$nullableStringLifecycleGate = Join-Path $Root "tools\check_xlir_nullable_string_lifecycle_abi.ps1"
$resolvedPropertyCallGate = Join-Path $Root "tools\check_xlir_resolved_property_call_abi.ps1"

# This is an admission matrix, not a claim that every language feature has
# migrated.  Each row names the smallest fixtures that prove the corresponding
# XLIR semantic ABI is present.  The determinism gate then emits and strictly
# compiles every xlir_ fixture, while the marked runtime fixtures execute the
# canonical XLIR product against their observable contracts.
$matrix = [ordered]@{
    "Unicode-scalar char and managed bytes ABI" = @(
        "char_bytes_literals", "char_bytes_advanced",
        "char_bytes_invalid_char_width", "char_bytes_invalid_char_surrogate",
        "char_bytes_invalid_bf_prefix", "char_bytes_invalid_brf_prefix"
    )
    "typed intrinsic identity and effect ABI" = @(
        "cgen_var_numeric_convert", "cgen_async_string_ownership", "c_abi_types",
        "cgen_copy_clone", "xlir_class_virtual_dispatch", "xlir_intrinsic_range",
        "xlir_intrinsic_trivial_identity_convert", "cgen_extern_function_value",
        "xlir_foreign_function_value_abi"
    )
    "foreign XL6_Function descriptor and raw-pointer null physical ABI" = @(
        "xlir_foreign_function_value_abi", "stdlib_tls_server_fast_path",
        "stdlib_tls_stream_fast_path"
    )
    "scalar and CFG" = @(
        "xlir_short_circuit_scalar", "xlir_loop_exit_scalar",
        "xlir_do_while_scalar", "xlir_float_to_int_scalar", "xlir_float_to_int_checked_catch", "xlir_dynamic_int_convert", "xlir_dynamic_int_convert_failure", "xlir_dynamic_int_width_convert", "xlir_dynamic_int_width_failure", "xlir_dynamic_wide_int_convert", "xlir_dynamic_wide_int_failure", "xlir_for_range_negative_step", "xlir_range_for_abi",
        "xlir_match_string", "xlir_conditional_xvalue", "xlir_typed_int_array_read", "xlir_typed_bool_array_read", "xlir_typed_float_array_read",
        "xlir_u64_scalar", "xlir_dynamic_u64_arithmetic",
        "xlir_integer_conversion_source_abi", "type_int_narrowing_wrap",
        "xlir_scalar_conversion_abi", "xlir_time_conversion_abi",
        "xlir_dynamic_conditional_truth", "xlir_dynamic_unary_abi",
        "xlir_dynamic_binary_extended_abi",
        "xlir_checked_integer_arithmetic_abi",
        "xlir_checked_integer_compound_error_abi"
    )
    "checked managed-string float conversion ABI" = @(
        "xlir_checked_float_string"
    )
    "exact no-raise signed i64 power ABI" = @(
        "cgen_pow"
    )
    "exact no-raise time approximate comparison ABI" = @(
        "cgen_time_literals"
    )
    "special f64 constant and stdlib math ABI" = @(
        "stdlib_math"
    )
    "checked string/xvalue time conversion ABI" = @(
        "xlir_checked_time_conversion"
    )
    "explicit scalar/null/xvalue bool truth conversion ABI" = @(
        "xlir_bool_conversion_abi"
    )
    "closed dynamic checked cast and recoverable failure ABI" = @(
        "xlir_checked_cast_abi", "xlir_checked_cast_differential"
    )
    "closed nullable try/lift/checked-unwrap cast ABI" = @(
        "xlir_nullable_cast_abi"
    )
    "closed class/protocol nominal identity-set cast ABI" = @(
        "xlir_nominal_cast_abi"
    )
    "exact foreign C scalar ABI" = @(
        "c_abi_types", "async_class_method"
    )
    "language F32 and exported fixed-width public ABI" = @(
        "export_primitive_widths"
    )
    "managed values" = @(
        "xlir_string_temp_operands", "xlir_string_self_assign",
        "xlir_xvalue_nested_collection", "xlir_xvalue_object_variable_key_set",
        "xlir_xvalue_array_set_read", "xlir_typed_string_array_read",
        "xlir_owned_expression_discard", "cgen_array_literal_expr_elements"
    )
    "closed TypeId and nominal class string conversion ABI" = @(
        "xlir_string_conversion_abi"
    )
    "class structural hash/equality and typed-set identity ABI" = @(
        "cgen_derive_string_hash_equals"
    )
    "field-only native-backed managed object identity ABI" = @(
        "cgen_typed_container_objects"
    )
    "managed native extern receiver physical field-borrow ABI" = @(
        "xlir_managed_native_extern_receiver_abi"
    )
    "managed native owned-field lifecycle and noncopyable ABI" = @(
        "xlir_managed_native_explicit_destroy", "cgen_native_class_lifecycle",
        "cgen_native_class"
    )
    "user class destroy-plan and finalizer-borrow lifecycle ABI" = @(
        "cgen_var_destroy_class_init", "cgen_class_destroy",
        "cgen_class_inherit_destroy", "xlir_class_cleanup_outcome"
    )
    "canonical interpolated-string ABI" = @(
        "cgen_fstring", "cgen_loop_fstring_needs",
        "cgen_generator_fstring_needs"
    )
    "erased and typed index ownership ABI" = @(
        "xlir_xvalue_dynamic_key_index", "xlir_xvalue_dynamic_key_index_set",
        "xlir_typed_dict_scalar_set", "xlir_index_write_abi",
        "xlir_index_write_generic_abi", "cgen_index_access",
        "index_ownership_top_level_combined"
    )
    "xvalue collection membership ABI" = @(
        "xlir_xvalue_membership", "cgen_typed_container_copy_assign"
    )
    "managed string membership ABI" = @(
        "xlir_string_membership", "cgen_in_containers"
    )
    "canonical non-call member ABI" = @(
        "xlir_member_abi_string_length", "xlir_length_protocol_member",
        "stdlib_buffer", "extlib_argon2"
    )
    "canonical class-field projection ABI" = @(
        "xlir_class_field_member_abi", "cgen_native_class_field",
        "extlib_xbot", "extlib_dinov3"
    )
    "inherited and flow-refined class-field projection ABI" = @(
        "xlir_class_field_inherited_narrowed_abi",
        "cgen_class", "export_class_inherit",
        "typed_container_iteration_ownership"
    )
    "pointer-sentinel nullable-string lifecycle and class-field ownership ABI" = @(
        "xlir_nullable_string_assignment_coalesce_abi",
        "nullable_string_lifecycle"
    )
    "canonical range membership ABI" = @(
        "xlir_range_membership", "cgen_in_range"
    )
    "canonical storage-backed array iteration ABI" = @(
        "xlir_array_i64_for_abi", "cgen_export_typed_container_abi"
    )
    "canonical xvalue snapshot iterator ABI" = @(
        "xlir_list_i64_iterator_abi", "xlir_set_i64_iterator_abi",
        "xlir_xvalue_iterator_scalar_projection",
        "xlir_xvalue_iterator_managed_projection",
        "xlir_xvalue_array_iterator_projection",
        "xlir_dict_iterator_projection", "xlir_dict_key_only_iterator"
    )
    "canonical class and native protocol iterator ABI" = @(
        "cgen_protocol_iter", "xlir_protocol_iterator_managed_item",
        "extlib_native_iter_protocol_min", "stdlib_buffer_file_runtime"
    )
    "statically shaped erased collection iterator ABI" = @(
        "xlir_erased_collection_iterator_abi"
    )
    "runtime-checked dynamic var iterator ABI" = @(
        "xlir_dynamic_var_iterator_abi", "cgen_for_in_containers",
        "cgen_for_in_var_non_container"
    )
    "canonical Future cancellation intrinsic ABI" = @(
        "xlir_future_cancel"
    )
    "explicit Future wait and status member ABI" = @(
        "xlir_future_member_status_abi"
    )
    "flattened heterogeneous task.whenAny index ABI" = @(
        "xlir_task_when_any_literal_abi"
    )
    "typed-array Future reference and task.whenAny index ABI" = @(
        "xlir_task_when_any_array_abi"
    )
    "typed-array task.all aggregate result ABI" = @(
        "xlir_task_aggregate_array_abi"
    )
    "typed-array task.race aggregate result ABI" = @(
        "xlir_task_aggregate_array_abi"
    )
    "nested Future task.spawn/all/race payload and argument-frame ABI" = @(
        "xlir_task_nested_future_abi", "sem_task_all_return_future",
        "sem_task_all_typed_array_return_future"
    )
    "typed task.spawn closure frame and managed capture ABI" = @(
        "xlir_task_spawn_closure_abi",
        "cgen_task_literal_single_eval", "cgen_task_shared_closure_refcount",
        "cgen_task_spawn_fstring", "cgen_task_spawn_range_contains"
    )
    "typed thread runtime resource, capture, and handle identity ABI" = @(
        "xlir_thread_resource_abi", "cgen_thread"
    )
    "typed channel resource, generic payload, storage-ref, capture, and handle identity ABI" = @(
        "xlir_channel_resource_abi", "cgen_channel", "cgen_channel_string"
    )
    "typed mutex, semaphore, condition and rwlock resource identity and lifetime ABI" = @(
        "cgen_sync"
    )
    "typed concurrent queue payload, storage-ref, thread capture, and handle identity ABI" = @(
        "stdlib_special_collections_concurrent"
    )
    "typed callable collection index projection ABI" = @(
        "xlir_typed_callable_index_abi"
    )
    "typed callable container conversion effect, projection, and expression-call ABI" = @(
        "cgen_typed_container_functions",
        "cgen_typed_container_copy_objects_functions"
    )
    "typed safe-index canonical carrier, nominal presence, and exact nullable projection ABI" = @(
        "cgen_typed_container_safe_index_objects_functions"
    )
    "typed nominal index assignment boxing, effect, and non-null/scalar projection ABI" = @(
        "cgen_typed_container_index_assign_objects_functions",
        "cgen_typed_container_index_assign"
    )
    "typed raw-pointer collection index projection ABI" = @(
        "cgen_typed_point"
    )
    "xvalue scalar bridge, exact type flow, and may-raise constructor cleanup ABI" = @(
        "hir_xvalue_scalar_bridge_matrix"
    )
    "typed dict literal-key projection and safe nullable index ABI" = @(
        "xlir_typed_dict_literal_index_abi",
        "cgen_safe_index", "cgen_static_typed_containers"
    )
    "typed managed native-backed class collection index retention ABI" = @(
        "xlir_typed_native_handle_index_abi",
        "cgen_typed_container_native_class_items"
    )
    "typed managed native-backed class collection iterator retention ABI" = @(
        "xlir_typed_native_handle_iterator_abi"
    )
    "canonical UTF-8 string iterator ABI" = @(
        "xlir_string_iterator_abi", "stdlib_string"
    )
    "checked collection TypeId rebind ABI" = @(
        "xlir_dynamic_typed_collection_projection",
        "cgen_dynamic_to_typed_containers"
    )
    "typed nullable erased-container projection ABI" = @(
        "typed_nullable_container_matrix"
    )
    "safe nullable-collection integer index and flow unwrap ABI" = @(
        "xlir_safe_index_nullable_collection_abi",
        "cgen_array_negative_index", "nullable_flow_narrowing"
    )
    "safe nullable declared-class field lookup and projection ABI" = @(
        "nullable_safe_navigation"
    )
    "contextual null representation ABI" = @(
        "xlir_null_abi_boundary", "xlir_nullable_scalar", "xlir_nullable_closure_capture", "xlir_nullable_cell_capture",
        "xlir_nullable_async", "xlir_nullable_async_continuation", "xlir_nullable_class_field",
        "xlir_nullable_sentinel_coalesce", "xlir_nullable_sentinel_assign_if_null",
        "xlir_nullable_index_assign_if_null"
    )
    "canonical CallSite binding and variadic ABI" = @(
        "xlir_callsite_binding", "call_defaults",
        "xlir_callsite_variadic", "cgen_extern_xvalue_vararg",
        "xlir_callsite_c_vararg", "xlir_callsite_generic_specialization"
    )
    "class protocol syntax CallSite ABI" = @(
        "cgen_protocol_ops", "cgen_protocol_complex_receiver",
        "xlir_class_protocol_syntax_generic", "cgen_protocol_overload_index"
    )
    "copy and clone ownership ABI" = @(
        "cgen_copy_clone", "cgen_generic_class_copy_clone",
        "cgen_class_copy", "cgen_class_custom_copy_clone",
        "cgen_generic_class_custom_copy_clone"
    )
    "errors and cleanup CFG" = @(
        "xlir_try_catch_finally_rethrow", "xlir_multi_typed_catch_finally_outer_route",
        "xlir_try_finally_xvalue_return", "xlir_catch_error_runtime_fields",
        "xlir_catch_error_cause_ref"
    )
    "generic and closure ABI" = @(
        "xlir_generic_inferred_array", "xlir_function_value",
        "xlir_closure_nested_capture", "xlir_closure_closure_capture",
        "xlir_closure_mutable_xvalue_multi_catch_finally_return",
        "cgen_function_direct_owned_args", "hir_closure_for_capture",
        "hir_closure_for_named_typed_array", "hir_closure_for_string_capture"
    )
    "open runtime callable erasure ABI" = @(
        "cgen_function_bare_call"
    )
    "native standard-library handle ABI" = @(
        "xlir_native_dynamic_fields_receiver", "xlir_native_dynamic_fields_values",
        "xlir_native_dynamic_fields_destroy", "xlir_native_dynamic_fields_auto_destroy",
        "xlir_native_dynamic_fields_explicit_destroy", "cgen_native_class"
    )
    "native handle bind/take ownership ABI" = @(
        "cgen_native_class_bind_point", "cgen_native_class_bind_point_basic"
    )
    "resolved native property CallTarget ABI" = @(
        "cgen_native_class_property"
    )
    "canonical value/default initializer ABI" = @(
        "xlir_value_initializer_defaults", "xlir_value_initializer_class_default",
        "xlir_value_initializer_class_null",
        "xlir_value_initializer_containers", "xlir_value_initializer_package_exports",
        "function_types",
        "cgen_native_class_ownership",
        "cgen_native_class_null_destroy"
    )
    "callable reachability and emission closure" = @(
        "xlir_callable_reachability", "xlir_value_initializer_class_default",
        "xlir_class_virtual_dispatch", "protocol_dynamic_dispatch",
        "xlir_async_ready_differential"
    )
    "canonical product result ownership ABI" = @(
        "cgen_multi_return", "cgen_class_method_multi_return",
        "xlir_product_closure_result", "xlir_product_generic_result",
        "xlir_product_declaration_only", "xlir_managed_product_result"
    )
    "named function adapter CFG ABI" = @(
        "cgen_function_adapter_dedupe", "cgen_toplevel_typed_func_value",
        "cgen_named_function_argument", "cgen_function_multi_return",
        "cgen_c_callback_export", "cgen_function_value_error_propagation",
        "cgen_extern_function_value",
        "xlir_named_function_extern_adapter"
    )
    "canonical collection-member and borrowed parameter owner ABI" = @(
        "cgen_param_modes_container_matrix",
        "cgen_typed_container_param_mutation",
        "xlir_typed_collection_methods",
        "cgen_dynamic_container_methods"
    )
    "statically shaped erased array insertion ABI" = @(
        "xlir_erased_array_append"
    )
    "closed typed collection algebra ABI" = @(
        "cgen_container_ops"
    )
    "dynamic class-member and collection fallback precedence" = @(
        "cgen_dynamic_method_arguments"
    )
    "runtime callable xcall ABI" = @(
        "cgen_extern_function_value", "xlir_xcall_scalar_values",
        "xlir_xcall_scalar_effect", "xlir_xcall_nullable_closure"
    )
    "default xvalue class layout ABI" = @(
        "xlir_default_class_layout"
    )
    "instance xvalue class method ABI" = @(
        "xlir_class_instance_method"
    )
    "declaration-driven class receiver ABI" = @(
        "xlir_class_implicit_receiver"
    )
    "explicit constructor own-field initializer ABI" = @(
        "xlir_explicit_constructor_field_initializer"
    )
    "class reference-slot and owned-field destruction ABI" = @(
        "xlir_class_reference_field_initializer",
        "cgen_nested_class_constructor", "cgen_class_native_field"
    )
    "class JSON persistence schema and reconstruction ABI" = @(
        "xlir_persistence_json_abi", "xlir_persistence_recursive_class_values",
        "xlir_persistence_projection_roots",
        "xlir_persistence_recursive_collection_values"
    )
    "explicit class constructor/default-base ABI" = @(
        "xlir_class_explicit_constructor_base"
    )
    "static class namespace/storage ABI" = @(
        "xlir_class_static_storage", "xlir_class_static_storage_write"
    )
    "virtual class dispatch ABI" = @(
        "xlir_class_virtual_dispatch"
    )
    "protocol dispatch ABI" = @(
        "protocol_dynamic_dispatch", "protocol_overload_dispatch",
        "protocol_generic_dispatch", "protocol_property_dispatch",
        "protocol_generic_property_dispatch", "protocol_async_dispatch",
        "protocol_inherited_dispatch", "protocol_native_backed_dispatch"
    )
    "synchronous generator continuation machine" = @(
        "cgen_generator", "cgen_generator_two_targets",
        "cgen_generator_string_resource", "cgen_generator_fstring_needs",
        "xlir_generator_string_machine", "xlir_checked_integer_continuation_abi"
    )
    "future and async state machine" = @(
        "xlir_await_future_i64_finally", "xlir_async_generic_expression_planner",
        "xlir_async_raw_pointer_abi",
        "xlir_async_cfg_liveness", "xlir_async_future_two_awaits",
        "xlir_async_future_three_awaits", "xlir_async_future_three_awaits_mixed",
        "xlir_async_future_three_strings",
        "xlir_async_future_then_string_mixed_capture_chain",
        "xlir_async_future_direct_xvalue_capture", "xlir_async_task_worker_error_catch",
        "xlir_async_task_xvalue_worker_error", "xlir_async_class_method",
        "async_generic_class_method", "async_class_receiver_lifetime",
        "cgen_async_class_conditional_await", "cgen_async_param_ownership",
        "cgen_async_string_ownership", "xlir_u64_async_closure",
        "xlir_checked_integer_continuation_abi", "stdlib_future_continuation",
        "xlir_async_error_abi", "xlir_async_error_race_abi"
    )
    "module and export ABI" = @(
        "xlir_global_scalar", "xlir_void_call_global", "xlir_export_auto_scalar",
        "xlir_export_error_void", "xlir_export_nominal_overload", "export_class", "export_vars",
        "export_primitive_widths", "extern_var", "xlir_extern_global_function"
    )
}

$matrix["JSON private value bridge ABI"] = @("xlir_json_value_bridge", "xlir_json_valid_only")
$matrix["XLON private value bridge ABI"] = @("xlir_xlon_value_bridge", "xlir_xlon_valid_only")
$matrix["XLON native writer and visitor ABI"] = @("xlir_xlon_writer_bridge", "xlir_xlon_writer_type_only", "xlir_xlon_writer_factory_only", "xlir_xlon_visit_bridge", "xlir_xlon_visit_capture_lifetime")
$matrix["JSON native writer and visitor ABI"] = @("xlir_json_writer_bridge", "xlir_json_visit_bridge", "xlir_json_visit_capture_lifetime")
$matrix["native factory declaration ABI"] = @("xlir_native_factory_declaration")
$matrix["host entry error outcome ABI"] = @("xlir_host_error_boundary_script", "xlir_host_error_boundary_main_no_raise", "xlir_host_error_boundary_main_may_raise", "xlir_host_error_boundary_script_throw", "xlir_host_error_boundary_main_throw", "xlir_host_error_boundary_init_throw")
$matrix["serialization callback error result ABI"] = @("xlir_serialization_callback_error", "xlir_serialization_callback_async_error", "xlir_serialization_callback_generator_error", "xlir_serialization_callback_lifetime", "xlir_serialization_callback_json_uncaught", "xlir_serialization_callback_xlon_uncaught")
$matrix["text stdout output ABI"] = @("xlir_text_output_write_only", "xlir_text_output_writeln_only", "xlir_text_output_bridge", "xlir_text_output_continuation")
$matrix["direct directory and process bridge ABI"] = @("xlir_process_exec_bridge", "xlir_process_success_bridge", "xlir_process_exit_code_bridge", "xlir_dir_list_bridge", "xlir_dir_glob_bridge", "xlir_dir_entries_bridge", "xlir_dir_empty_bridge", "xlir_dir_count_bridge", "xlir_dir_size_bridge", "xlir_directory_bridge", "xlir_system_bridge_continuation")
$matrix["statically shaped erased collection member ABI"] = @("xlir_erased_collection_members", "xlir_erased_generic_collection_members", "xlir_erased_collection_member_continuation", "xlir_erased_collection_member_lifetime")

$matrix["dynamic comparison execution contract"] = @("xlir_dynamic_compare_contract")
$matrix["partial expression ownership and local product cleanup"] = @("xlir_core_call_value_lifetime")
$matrix["managed getter and open object ownership with public factory ABI"] = @("xlir_open_object_field_lifetime")
$matrix["refcounted local global and static storage replacement ownership"] = @("xlir_refcounted_storage_assignment_lifetime")
$matrix["named callable owned materialization and physical allocation balance"] = @("xlir_named_callable_owner_lifetime")
$matrix["membership source order and partial evaluation ownership"] = @("xlir_membership_operand_lifetime")
$matrix["managed binary evaluation ownership and string boxing"] = @("xlir_binary_operand_lifetime")
$matrix["verified module finalization and initialization failure ownership"] = @("xlir_module_finalization", "xlir_module_native_finalization", "xlir_module_scalar_finalization", "xlir_module_finalization_cli", "xlir_module_finalization_cli_error")
$matrix["public raw C callback export ownership ABI"] = @("cgen_c_callback_export", "xlir_export_callback_lifetime")
$matrix["first class callable typed argument conversion boundary"] = @("xlir_closure_argument_conversion", "xlir_closure_argument_evaluation_order")
$matrix["shared foreign error scope and raw callback error ABI"] = @("cgen_xcall_error_bridge", "xlir_foreign_scope_nested", "xlir_foreign_raw_callback_error")
$matrix["detach before destroy reentrant storage ownership"] = @("xlir_cleanup_detach_reentry")
$matrix["normal scope exit lifetime plans and scalar cell promotion"] = @("xlir_scope_exit_plan")
$matrix["normal lexical cleanup outcomes and exact handler ownership"] = @("xlir_scope_cleanup_outcome")
$matrix["abrupt exit route topology and handler boundaries (IR contract, not runtime outcome)"] = @("xlir_exit_route")
$matrix["abrupt lexical cleanup outcomes, pending returns and fallible custom copy completion"] = @("xlir_abrupt_cleanup_outcome")
$matrix["parameter activation cleanup outcomes and explicit promoted-cell ownership"] = @("xlir_parameter_cleanup_outcome")
$matrix["loop item/resource cleanup outcomes and generator next result carrier"] = @("xlir_loop_cleanup_outcome")
$matrix["post-try exception routing and managed-finally pending-error ownership"] = @("xlir_pending_exception_outcome")
$matrix["pending-error cancellation, nested capture and await/yield continuation storage"] = @("xlir_pending_exception_control")
$matrix["pending return ownership through cleanup and exact terminal commit"] = @("xlir_return_commit")
$matrix["physical return carrier, narrowed observations and independent product field owners"] = @("xlir_return_projection_owner")
$matrix["conditional nullable domains, branch flow intersection and exact owned merge values"] = @("xlir_conditional_nullable_join")
$matrix["independent managed storage inputs and explicit match owner merges"] = @("xlir_managed_storage_inputs")
$matrix["path-sensitive SSA owner consumption and closed operand effects (not complete borrow liveness)"] = @("xlir_managed_storage_inputs", "xlir_parameter_cleanup_outcome", "xlir_loop_cleanup_outcome")
$matrix["runtime callable partial result rollback and original failure ownership"] = @("xlir_xcall_result_failure")

$runtimeContracts = @(
    "xlir_return_commit",
    "xlir_closure_argument_conversion",
    "xlir_process_exec_bridge", "xlir_process_success_bridge", "xlir_process_exit_code_bridge",
    "xlir_dir_list_bridge", "xlir_dir_glob_bridge", "xlir_dir_entries_bridge",
    "xlir_directory_bridge", "xlir_system_bridge_continuation",
    "xlir_erased_collection_members", "xlir_erased_generic_collection_members", "xlir_erased_collection_member_continuation",
    "xlir_text_output_write_only", "xlir_text_output_writeln_only", "xlir_text_output_bridge", "xlir_text_output_continuation",
    "xlir_serialization_callback_error",
    "xlir_serialization_callback_json_uncaught", "xlir_serialization_callback_xlon_uncaught",
    "xlir_json_value_bridge", "xlir_json_valid_only", "xlir_json_writer_bridge", "xlir_json_visit_bridge",
    "xlir_xlon_value_bridge", "xlir_xlon_valid_only", "xlir_xlon_writer_bridge", "xlir_xlon_visit_bridge",
    "xlir_xlon_writer_type_only", "xlir_xlon_writer_factory_only",
    "xlir_host_error_boundary_script", "xlir_host_error_boundary_main_no_raise", "xlir_host_error_boundary_main_may_raise",
    "xlir_host_error_boundary_script_throw", "xlir_host_error_boundary_main_throw", "xlir_host_error_boundary_init_throw",
    "xlir_cgen_differential_scalar",
    "export_primitive_widths",
    "xlir_cgen_differential_scalar_conversion",
    "xlir_integer_conversion_source_abi",
    "type_int_narrowing_wrap",
    "xlir_scalar_conversion_abi",
    "xlir_dynamic_unary_abi",
    "xlir_dynamic_binary_extended_abi",
    "xlir_checked_integer_arithmetic_abi",
    "cgen_pow",
    "cgen_time_literals",
    "stdlib_math",
    "xlir_checked_integer_continuation_abi",
    "xlir_cgen_differential_managed_error",
    "xlir_cgen_differential_xvalue_conditional",
    "xlir_nullable_scalar",
    "xlir_nullable_sentinel_coalesce",
    "xlir_nullable_index_assign_if_null",
    "xlir_nullable_index_assign_if_null_may_raise",
    "nullable_string_lifecycle",
    "xlir_xvalue_dynamic_key_index",
    "xlir_xvalue_dynamic_key_index_set",
    "xlir_typed_dict_scalar_set",
    "cgen_typed_point",
    "hir_xvalue_scalar_bridge_matrix",
    "xlir_index_write_generic_abi",
    "xlir_owned_expression_discard",
    "cgen_array_literal_expr_elements",
    "xlir_xvalue_membership",
    "xlir_string_membership",
    "xlir_member_abi_string_length",
    "xlir_length_protocol_member",
    "xlir_class_field_member_abi",
    "xlir_class_field_inherited_narrowed_abi",
    "cgen_dynamic_container_methods",
    "cgen_container_ops",
    "cgen_in_containers",
    "xlir_range_membership",
    "cgen_in_range",
    "xlir_range_for_abi",
    "xlir_array_i64_for_abi",
    "xlir_list_i64_iterator_abi",
    "xlir_set_i64_iterator_abi",
    "xlir_xvalue_iterator_scalar_projection",
    "xlir_xvalue_iterator_managed_projection",
    "xlir_dict_iterator_projection_differential",
    "xlir_dict_key_only_iterator",
    "xlir_erased_collection_iterator_abi",
    "xlir_dynamic_var_iterator_abi",
    "xlir_string_iterator_abi",
    "typed_nullable_container_matrix",
    "xlir_safe_index_nullable_collection_abi",
    "xlir_typed_dict_literal_index_abi",
    "xlir_dynamic_u64_arithmetic",
    "xlir_future_cancel",
    "xlir_future_member_status_abi",
    "xlir_task_when_any_literal_abi",
    "xlir_task_when_any_array_abi",
    "xlir_task_aggregate_array_abi",
    "xlir_task_spawn_closure_abi",
    "xlir_thread_resource_abi",
    "xlir_channel_resource_abi",
    "cgen_sync",
    "stdlib_special_collections_concurrent",
    "stdlib_future_continuation",
    "xlir_typed_callable_index_abi",
    "xlir_typed_native_handle_index_abi",
    "xlir_typed_native_handle_iterator_abi",
    "xlir_managed_native_extern_receiver_abi",
    "xlir_async_ready_differential",
    "xlir_await_future_xvalue",
    "xlir_async_generic_expression_planner",
    "xlir_async_future_scalar_mixed_capture_chain",
    "xlir_async_future_xvalue_differential",
    "xlir_async_future_xvalue_local_parameter_differential",
    "xlir_async_future_direct_call_capture",
    "xlir_async_future_direct_string_capture",
    "xlir_async_future_direct_xvalue_differential",
    "xlir_async_future_direct_call",
    "cgen_async_string_ownership",
    "cgen_function_direct_owned_args",
    "hir_closure_for_capture",
    "hir_closure_for_named_typed_array",
    "hir_closure_for_string_capture",
    "cgen_function_bare_call",
    "cgen_extern_function_value",
    "xlir_xcall_scalar_values",
    "xlir_managed_product_result",
    "definite_assignment_default_values",
    "xlir_class_implicit_receiver",
    "cgen_protocol_ops",
    "cgen_protocol_complex_receiver",
    "xlir_class_protocol_syntax_generic",
    "cgen_protocol_overload_index",
    "cgen_native_class_property",
    "xlir_intrinsic_trivial_identity_convert",
    "xlir_value_initializer_package_exports",
    "xlir_value_initializer_class_null",
    "xlir_explicit_constructor_field_initializer",
    "xlir_class_reference_field_initializer",
    "cgen_nested_class_constructor",
    "cgen_class_native_field",
    "cgen_copy_clone",
    "cgen_class_custom_copy_clone",
    "xlir_string_conversion_abi",
    "cgen_derive_string_hash_equals",
    "xlir_persistence_json_abi",
    "xlir_persistence_projection_roots"
)
$xlirOnlyRuntimeContracts = @(
    "xlir_member_assignment_transaction",
    "xlir_storage_assignment_transaction",
    "xlir_property_assignment_transaction",
    "xlir_virtual_call_outcome", "xlir_virtual_activation", "xlir_owned_activation", "xlir_module_task_pool",
    "xlir_protocol_call_outcome",
    "xlir_callsite_evaluation", "xlir_indexed_accessor", "xlir_property_update",
    "xlir_dynamic_member_get",
    "xlir_dynamic_bound_method",
    "xlir_dynamic_member_set",
    "xlir_nullable_storage_assign_if_null",
    "xlir_storage_conditional_assign",
    "xlir_index_conditional_assign",
    "xlir_scope_cleanup_outcome",
    "xlir_abrupt_cleanup_outcome",
    "xlir_parameter_cleanup_outcome",
    "xlir_loop_cleanup_outcome",
    "xlir_pending_exception_outcome",
    "xlir_pending_exception_control",
    "xlir_return_projection_owner",
    "xlir_conditional_nullable_join",
    "xlir_managed_storage_inputs",
    "xlir_class_cleanup_outcome",
    "xlir_scope_exit_plan",
    "xlir_xcall_result_failure",
    "xlir_cleanup_detach_reentry",
    "xlir_foreign_scope_nested", "xlir_foreign_raw_callback_error",
    "xlir_export_callback_lifetime",
    "xlir_closure_argument_evaluation_order",
    "xlir_module_finalization", "xlir_module_native_finalization", "xlir_module_scalar_finalization", "xlir_module_finalization_cli", "xlir_module_finalization_cli_error",
    "xlir_binary_operand_lifetime",
    "xlir_membership_operand_lifetime",
    "xlir_named_callable_owner_lifetime",
    "xlir_open_object_field_lifetime",
    "xlir_refcounted_storage_assignment_lifetime",
    "xlir_core_call_value_lifetime",
    "xlir_dynamic_compare_contract",
    "xlir_dir_empty_bridge", "xlir_dir_count_bridge", "xlir_dir_size_bridge",
    "xlir_erased_collection_member_lifetime",
    "xlir_serialization_callback_lifetime",
    "xlir_serialization_callback_async_error",
    "xlir_serialization_callback_generator_error",
    "xlir_json_visit_capture_lifetime",
    "xlir_xlon_visit_capture_lifetime",
    "xlir_typed_collection_methods",
    "xlir_nullable_closure_capture",
    "xlir_nullable_cell_capture",
    "xlir_nullable_async",
    "xlir_nullable_async_continuation",
    "xlir_nullable_class_field",
    "xlir_nullable_sentinel_assign_if_null",
    "xlir_index_write_abi",
    "xlir_xvalue_array_iterator_projection",
    "xlir_dynamic_typed_collection_projection",
    "cgen_dynamic_to_typed_containers",
    "xlir_generator_string_machine",
    "xlir_checked_float_string",
    "xlir_checked_time_conversion",
    "xlir_bool_conversion_abi",
    "xlir_checked_cast_abi",
    "xlir_nullable_cast_abi",
    "xlir_nominal_cast_abi",
    "xlir_dynamic_conditional_truth",
    "xlir_erased_array_append",
    "xlir_checked_integer_compound_error_abi",
    "xlir_foreign_function_value_abi",
    "xlir_managed_native_explicit_destroy",
    "xlir_xcall_nullable_closure",
    "xlir_async_error_abi",
    "xlir_async_error_race_abi",
    "xlir_task_nested_future_abi",
    "xlir_persistence_recursive_collection_values"
)

foreach ( $path in @($caseDir, $determinism, $runtimeMatrix, $defaultClassGate, $instanceClassMethodGate, $implicitClassReceiverGate, $explicitConstructorFieldGate, $classReferenceFieldGate, $explicitClassConstructorGate, $staticClassGate, $virtualClassModelGate, $protocolDispatchGate, $protocolWitnessAdapterGate, $asyncClassMethodGate, $cScalarAbiGate, $exportPrimitiveWidthGate, $u64AbiGate, $integerConversionAbiGate, $checkedFloatConversionGate, $checkedTimeConversionGate, $boolConversionAbiGate, $checkedCastAbiGate, $nullableCastAbiGate, $nominalCastAbiGate, $dynamicConditionalTruthGate, $dynamicUnaryAbiGate, $dynamicBinaryExtendedAbiGate, $checkedIntegerArithmeticAbiGate, $i64PowAbiGate, $timeApproxAbiGate, $specialF64ConstantAbiGate, $scalarConversionAbiGate, $timeConversionAbiGate, $functionValueOwnershipGate, $openFunctionGate, $foreignFunctionValueGate, $copyCloneGate, $exportBoundaryGate, $globalExportBoundaryGate, $externGlobalBoundaryGate, $nullAbiBoundaryGate, $callSiteBindingGate, $callSiteVariadicGate, $callSiteCVarargGate, $callSiteGenericGate, $classProtocolSyntaxGate, $intrinsicCallGate, $nativeNominalReceiverGate, $nativeHandleBindGate, $resolvedPropertyCallGate, $valueInitializerGate, $reachabilityGate, $productResultGate, $namedFunctionAdapterGate, $collectionMemberGate, $erasedArrayAppendGate, $collectionAlgebraGate, $dynamicCollectionMethodGate, $xcallGate, $asyncGenericPlannerGate, $asyncCfgLivenessGate, $asyncMultiAwaitMachineGate, $asyncMachineAdmissionGate, $asyncErrorAbiGate, $futureCancelGate, $futureMemberStatusGate, $dynamicKeyIndexGate, $indexWriteAbiGate, $ownedExpressionDiscardGate, $xvalueMembershipGate, $stringMembershipGate, $rangeMembershipGate, $rangeForAbiGate, $generatorMachineGate, $interpolatedStringGate, $arrayI64ForAbiGate, $xvalueIteratorAbiGate, $setI64IteratorAbiGate, $iteratorScalarProjectionGate, $iteratorManagedProjectionGate, $arrayIteratorProjectionGate, $dictIteratorProjectionGate, $erasedCollectionIteratorGate, $dynamicVarIteratorGate, $stringIteratorGate, $charBytesAbiGate, $collectionTypeRebindGate, $nullableContainerProjectionGate, $safeIndexNullableCollectionGate, $nullableSafeClassFieldGate, $memberAbiGate, $lengthProtocolMemberGate, $classFieldMemberAbiGate, $classFieldInheritedNarrowedGate, $nullableStringLifecycleGate) ) {
    if ( ! (Test-Path -LiteralPath $path) ) {
        throw "XLIR migration coverage input is missing: $path"
    }
}
foreach ( $path in @(
    $persistenceJsonGate, $recursivePersistenceGate,
    $persistenceProjectionRootGate, $recursiveCollectionPersistenceGate
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR migration coverage input is missing: $path"
    }
}

foreach ( $path in @(
    $taskWhenAnyLiteralGate, $taskWhenAnyArrayGate, $taskAggregateArrayGate,
    $taskNestedFutureGate,
    $taskSpawnClosureGate, $threadResourceGate, $channelResourceGate,
    $syncResourceGate, $specialQueueResourceGate, $stackResourceGate,
    $avlResourceGate,
    $futureContinuationGate,
    $typedCallableIndexGate, $typedCallableContainerGate,
    $typedSafeIndexNominalGate, $typedIndexAssignNominalGate,
    $dynamicKeyScalarProjectionGate, $typedPointProjectionGate,
    $xvalueScalarBridgeGate,
    $stringConversionAbiGate,
    $classValueIdentityAbiGate,
    $nativeClassObjectIdentityAbiGate,
    $managedNativeExternReceiverGate,
    $managedNativeLifecycleGate,
    $classLifecycleGate,
    $typedDictLiteralIndexGate,
    $typedNativeHandleIndexGate,
    $typedNativeHandleIteratorGate
) ) {
    if ( ! (Test-Path -LiteralPath $path) ) {
        throw "XLIR task aggregate migration gate is missing: $path"
    }
}

$allXlirFixtures = @(Get-ChildItem -LiteralPath $caseDir -File -Filter "xlir_*.xl" | Sort-Object Name)
if ( $allXlirFixtures.Count -lt 220 ) {
    throw "XLIR migration fixture baseline regressed: $($allXlirFixtures.Count) < 220"
}

$allFixtures = @(Get-ChildItem -LiteralPath $caseDir -File -Filter "*.xl")
$names = New-Object System.Collections.Generic.HashSet[string]
foreach ( $fixture in $allFixtures ) {
    $names.Add($fixture.BaseName) | Out-Null
}
foreach ( $row in $matrix.GetEnumerator() ) {
    foreach ( $name in $row.Value ) {
        if ( ! $names.Contains($name) ) {
            throw "XLIR migration matrix row '$($row.Key)' is missing fixture: $name"
        }
    }
}

$determinismText = [System.IO.File]::ReadAllText($determinism, [System.Text.Encoding]::UTF8)
if ( ! $determinismText.Contains('-Filter "xlir_*.xl"') -or
     ! $determinismText.Contains('byte-for-byte deterministic') -or
     ! $determinismText.Contains('strict C compile failed') -or
     ! $determinismText.Contains('XLIR Cgen batch found') ) {
    throw "XLIR migration matrix requires the all-fixture deterministic strict-C gate"
}

$runtimeMatrixText = [System.IO.File]::ReadAllText($runtimeMatrix, [System.Text.Encoding]::UTF8)
$defaultClassGateText = [System.IO.File]::ReadAllText($defaultClassGate, [System.Text.Encoding]::UTF8)
$instanceClassMethodGateText = [System.IO.File]::ReadAllText($instanceClassMethodGate, [System.Text.Encoding]::UTF8)
$implicitClassReceiverGateText = [System.IO.File]::ReadAllText($implicitClassReceiverGate, [System.Text.Encoding]::UTF8)
$explicitConstructorFieldGateText = [System.IO.File]::ReadAllText($explicitConstructorFieldGate, [System.Text.Encoding]::UTF8)
$classReferenceFieldGateText = [System.IO.File]::ReadAllText($classReferenceFieldGate, [System.Text.Encoding]::UTF8)
$persistenceJsonGateText = [System.IO.File]::ReadAllText($persistenceJsonGate, [System.Text.Encoding]::UTF8)
$recursivePersistenceGateText = [System.IO.File]::ReadAllText($recursivePersistenceGate, [System.Text.Encoding]::UTF8)
$persistenceProjectionRootGateText = [System.IO.File]::ReadAllText($persistenceProjectionRootGate, [System.Text.Encoding]::UTF8)
$recursiveCollectionPersistenceGateText = [System.IO.File]::ReadAllText($recursiveCollectionPersistenceGate, [System.Text.Encoding]::UTF8)
$explicitClassConstructorGateText = [System.IO.File]::ReadAllText($explicitClassConstructorGate, [System.Text.Encoding]::UTF8)
$staticClassGateText = [System.IO.File]::ReadAllText($staticClassGate, [System.Text.Encoding]::UTF8)
$virtualClassModelGateText = [System.IO.File]::ReadAllText($virtualClassModelGate, [System.Text.Encoding]::UTF8)
$protocolDispatchGateText = [System.IO.File]::ReadAllText($protocolDispatchGate, [System.Text.Encoding]::UTF8)
$protocolWitnessAdapterGateText = [System.IO.File]::ReadAllText($protocolWitnessAdapterGate, [System.Text.Encoding]::UTF8)
$asyncClassMethodGateText = [System.IO.File]::ReadAllText($asyncClassMethodGate, [System.Text.Encoding]::UTF8)
$cScalarAbiGateText = [System.IO.File]::ReadAllText($cScalarAbiGate, [System.Text.Encoding]::UTF8)
$exportPrimitiveWidthGateText = [System.IO.File]::ReadAllText($exportPrimitiveWidthGate, [System.Text.Encoding]::UTF8)
$u64AbiGateText = [System.IO.File]::ReadAllText($u64AbiGate, [System.Text.Encoding]::UTF8)
$integerConversionAbiGateText = [System.IO.File]::ReadAllText($integerConversionAbiGate, [System.Text.Encoding]::UTF8)
$checkedFloatConversionGateText = [System.IO.File]::ReadAllText($checkedFloatConversionGate, [System.Text.Encoding]::UTF8)
$checkedTimeConversionGateText = [System.IO.File]::ReadAllText($checkedTimeConversionGate, [System.Text.Encoding]::UTF8)
$boolConversionAbiGateText = [System.IO.File]::ReadAllText($boolConversionAbiGate, [System.Text.Encoding]::UTF8)
$checkedCastAbiGateText = [System.IO.File]::ReadAllText($checkedCastAbiGate, [System.Text.Encoding]::UTF8)
$nullableCastAbiGateText = [System.IO.File]::ReadAllText($nullableCastAbiGate, [System.Text.Encoding]::UTF8)
$nominalCastAbiGateText = [System.IO.File]::ReadAllText($nominalCastAbiGate, [System.Text.Encoding]::UTF8)
$dynamicConditionalTruthGateText = [System.IO.File]::ReadAllText($dynamicConditionalTruthGate, [System.Text.Encoding]::UTF8)
$dynamicUnaryAbiGateText = [System.IO.File]::ReadAllText($dynamicUnaryAbiGate, [System.Text.Encoding]::UTF8)
$dynamicBinaryExtendedAbiGateText = [System.IO.File]::ReadAllText($dynamicBinaryExtendedAbiGate, [System.Text.Encoding]::UTF8)
$checkedIntegerArithmeticAbiGateText = [System.IO.File]::ReadAllText($checkedIntegerArithmeticAbiGate, [System.Text.Encoding]::UTF8)
$scalarConversionAbiGateText = [System.IO.File]::ReadAllText($scalarConversionAbiGate, [System.Text.Encoding]::UTF8)
$timeConversionAbiGateText = [System.IO.File]::ReadAllText($timeConversionAbiGate, [System.Text.Encoding]::UTF8)
$stringConversionAbiGateText = [System.IO.File]::ReadAllText($stringConversionAbiGate, [System.Text.Encoding]::UTF8)
$classValueIdentityAbiGateText = [System.IO.File]::ReadAllText($classValueIdentityAbiGate, [System.Text.Encoding]::UTF8)
$nativeClassObjectIdentityAbiGateText = [System.IO.File]::ReadAllText($nativeClassObjectIdentityAbiGate, [System.Text.Encoding]::UTF8)
$managedNativeExternReceiverGateText = [System.IO.File]::ReadAllText($managedNativeExternReceiverGate, [System.Text.Encoding]::UTF8)
$managedNativeLifecycleGateText = [System.IO.File]::ReadAllText($managedNativeLifecycleGate, [System.Text.Encoding]::UTF8)
$classLifecycleGateText = [System.IO.File]::ReadAllText($classLifecycleGate, [System.Text.Encoding]::UTF8)
$functionValueOwnershipGateText = [System.IO.File]::ReadAllText($functionValueOwnershipGate, [System.Text.Encoding]::UTF8)
$openFunctionGateText = [System.IO.File]::ReadAllText($openFunctionGate, [System.Text.Encoding]::UTF8)
$foreignFunctionValueGateText = [System.IO.File]::ReadAllText($foreignFunctionValueGate, [System.Text.Encoding]::UTF8)
$copyCloneGateText = [System.IO.File]::ReadAllText($copyCloneGate, [System.Text.Encoding]::UTF8)
$exportBoundaryGateText = [System.IO.File]::ReadAllText($exportBoundaryGate, [System.Text.Encoding]::UTF8)
$globalExportBoundaryGateText = [System.IO.File]::ReadAllText($globalExportBoundaryGate, [System.Text.Encoding]::UTF8)
$externGlobalBoundaryGateText = [System.IO.File]::ReadAllText($externGlobalBoundaryGate, [System.Text.Encoding]::UTF8)
$nullAbiBoundaryGateText = [System.IO.File]::ReadAllText($nullAbiBoundaryGate, [System.Text.Encoding]::UTF8)
$callSiteBindingGateText = [System.IO.File]::ReadAllText($callSiteBindingGate, [System.Text.Encoding]::UTF8)
$callSiteVariadicGateText = [System.IO.File]::ReadAllText($callSiteVariadicGate, [System.Text.Encoding]::UTF8)
$callSiteCVarargGateText = [System.IO.File]::ReadAllText($callSiteCVarargGate, [System.Text.Encoding]::UTF8)
$callSiteGenericGateText = [System.IO.File]::ReadAllText($callSiteGenericGate, [System.Text.Encoding]::UTF8)
$classProtocolSyntaxGateText = [System.IO.File]::ReadAllText($classProtocolSyntaxGate, [System.Text.Encoding]::UTF8)
$intrinsicCallGateText = [System.IO.File]::ReadAllText($intrinsicCallGate, [System.Text.Encoding]::UTF8)
$nativeNominalReceiverGateText = [System.IO.File]::ReadAllText($nativeNominalReceiverGate, [System.Text.Encoding]::UTF8)
$nativeHandleBindGateText = [System.IO.File]::ReadAllText($nativeHandleBindGate, [System.Text.Encoding]::UTF8)
$nativeTakeReentryGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_native_take_reentry.ps1"), [System.Text.Encoding]::UTF8)
$foreignResultRollbackGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_foreign_result_rollback.ps1"), [System.Text.Encoding]::UTF8)
$exportTransactionGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_export_transaction.ps1"), [System.Text.Encoding]::UTF8)
$errorUnwindGateText = [System.IO.File]::ReadAllText((Join-Path $Root "tools\check_xlir_error_unwind.ps1"), [System.Text.Encoding]::UTF8)
$valueInitializerGateText = [System.IO.File]::ReadAllText($valueInitializerGate, [System.Text.Encoding]::UTF8)
$reachabilityGateText = [System.IO.File]::ReadAllText($reachabilityGate, [System.Text.Encoding]::UTF8)
$productResultGateText = [System.IO.File]::ReadAllText($productResultGate, [System.Text.Encoding]::UTF8)
$namedFunctionAdapterGateText = [System.IO.File]::ReadAllText($namedFunctionAdapterGate, [System.Text.Encoding]::UTF8)
$collectionMemberGateText = [System.IO.File]::ReadAllText($collectionMemberGate, [System.Text.Encoding]::UTF8)
$erasedArrayAppendGateText = [System.IO.File]::ReadAllText($erasedArrayAppendGate, [System.Text.Encoding]::UTF8)
$collectionAlgebraGateText = [System.IO.File]::ReadAllText($collectionAlgebraGate, [System.Text.Encoding]::UTF8)
$dynamicCollectionMethodGateText = [System.IO.File]::ReadAllText($dynamicCollectionMethodGate, [System.Text.Encoding]::UTF8)
$xcallGateText = [System.IO.File]::ReadAllText($xcallGate, [System.Text.Encoding]::UTF8)
$asyncGenericPlannerGateText = [System.IO.File]::ReadAllText($asyncGenericPlannerGate, [System.Text.Encoding]::UTF8)
$asyncCfgLivenessGateText = [System.IO.File]::ReadAllText($asyncCfgLivenessGate, [System.Text.Encoding]::UTF8)
$asyncMultiAwaitMachineGateText = [System.IO.File]::ReadAllText($asyncMultiAwaitMachineGate, [System.Text.Encoding]::UTF8)
$asyncMachineAdmissionGateText = [System.IO.File]::ReadAllText($asyncMachineAdmissionGate, [System.Text.Encoding]::UTF8)
$asyncErrorAbiGateText = [System.IO.File]::ReadAllText($asyncErrorAbiGate, [System.Text.Encoding]::UTF8)
$futureCancelGateText = [System.IO.File]::ReadAllText($futureCancelGate, [System.Text.Encoding]::UTF8)
$futureMemberStatusGateText = [System.IO.File]::ReadAllText($futureMemberStatusGate, [System.Text.Encoding]::UTF8)
$taskWhenAnyLiteralGateText = [System.IO.File]::ReadAllText($taskWhenAnyLiteralGate, [System.Text.Encoding]::UTF8)
$taskWhenAnyArrayGateText = [System.IO.File]::ReadAllText($taskWhenAnyArrayGate, [System.Text.Encoding]::UTF8)
$taskAggregateArrayGateText = [System.IO.File]::ReadAllText($taskAggregateArrayGate, [System.Text.Encoding]::UTF8)
$taskNestedFutureGateText = [System.IO.File]::ReadAllText($taskNestedFutureGate, [System.Text.Encoding]::UTF8)
$taskSpawnClosureGateText = [System.IO.File]::ReadAllText($taskSpawnClosureGate, [System.Text.Encoding]::UTF8)
$threadResourceGateText = [System.IO.File]::ReadAllText($threadResourceGate, [System.Text.Encoding]::UTF8)
$threadPublicGateText = [System.IO.File]::ReadAllText((Join-Path $Root 'tools/check_xlir_runtime_operand_catalog.py'), [System.Text.Encoding]::UTF8)
$threadLanguageGateText = [System.IO.File]::ReadAllText((Join-Path $Root 'tools/check_stdlib_thread_language.py'), [System.Text.Encoding]::UTF8)
$channelResourceGateText = [System.IO.File]::ReadAllText($channelResourceGate, [System.Text.Encoding]::UTF8)
$channelOwnedGateText = [System.IO.File]::ReadAllText((Join-Path $Root 'tools/check_stdlib_channel_regressions.py'), [System.Text.Encoding]::UTF8)
$syncResourceGateText = [System.IO.File]::ReadAllText($syncResourceGate, [System.Text.Encoding]::UTF8)
$specialQueueResourceGateText = [System.IO.File]::ReadAllText($specialQueueResourceGate, [System.Text.Encoding]::UTF8)
$stackResourceGateText = [System.IO.File]::ReadAllText($stackResourceGate, [System.Text.Encoding]::UTF8)
$avlResourceGateText = [System.IO.File]::ReadAllText($avlResourceGate, [System.Text.Encoding]::UTF8)
$futureContinuationGateText = [System.IO.File]::ReadAllText($futureContinuationGate, [System.Text.Encoding]::UTF8)
$typedCallableIndexGateText = [System.IO.File]::ReadAllText($typedCallableIndexGate, [System.Text.Encoding]::UTF8)
$typedCallableContainerGateText = [System.IO.File]::ReadAllText($typedCallableContainerGate, [System.Text.Encoding]::UTF8)
$typedSafeIndexNominalGateText = [System.IO.File]::ReadAllText($typedSafeIndexNominalGate, [System.Text.Encoding]::UTF8)
$typedIndexAssignNominalGateText = [System.IO.File]::ReadAllText($typedIndexAssignNominalGate, [System.Text.Encoding]::UTF8)
$dynamicKeyScalarProjectionGateText = [System.IO.File]::ReadAllText($dynamicKeyScalarProjectionGate, [System.Text.Encoding]::UTF8)
$typedPointProjectionGateText = [System.IO.File]::ReadAllText($typedPointProjectionGate, [System.Text.Encoding]::UTF8)
$xvalueScalarBridgeGateText = [System.IO.File]::ReadAllText($xvalueScalarBridgeGate, [System.Text.Encoding]::UTF8)
$typedDictLiteralIndexGateText = [System.IO.File]::ReadAllText($typedDictLiteralIndexGate, [System.Text.Encoding]::UTF8)
$typedNativeHandleIndexGateText = [System.IO.File]::ReadAllText($typedNativeHandleIndexGate, [System.Text.Encoding]::UTF8)
$typedNativeHandleIteratorGateText = [System.IO.File]::ReadAllText($typedNativeHandleIteratorGate, [System.Text.Encoding]::UTF8)
$dynamicKeyIndexGateText = [System.IO.File]::ReadAllText($dynamicKeyIndexGate, [System.Text.Encoding]::UTF8)
$indexWriteAbiGateText = [System.IO.File]::ReadAllText($indexWriteAbiGate, [System.Text.Encoding]::UTF8)
$ownedExpressionDiscardGateText = [System.IO.File]::ReadAllText($ownedExpressionDiscardGate, [System.Text.Encoding]::UTF8)
$xvalueMembershipGateText = [System.IO.File]::ReadAllText($xvalueMembershipGate, [System.Text.Encoding]::UTF8)
$stringMembershipGateText = [System.IO.File]::ReadAllText($stringMembershipGate, [System.Text.Encoding]::UTF8)
$rangeMembershipGateText = [System.IO.File]::ReadAllText($rangeMembershipGate, [System.Text.Encoding]::UTF8)
$rangeForAbiGateText = [System.IO.File]::ReadAllText($rangeForAbiGate, [System.Text.Encoding]::UTF8)
$generatorMachineGateText = [System.IO.File]::ReadAllText($generatorMachineGate, [System.Text.Encoding]::UTF8)
$interpolatedStringGateText = [System.IO.File]::ReadAllText($interpolatedStringGate, [System.Text.Encoding]::UTF8)
$arrayI64ForAbiGateText = [System.IO.File]::ReadAllText($arrayI64ForAbiGate, [System.Text.Encoding]::UTF8)
$arrayLanguageLifetimeGateText = [System.IO.File]::ReadAllText((Join-Path $Root 'tools\check_stdlib_collection_continuations.py'), [System.Text.Encoding]::UTF8)
$xvalueIteratorAbiGateText = [System.IO.File]::ReadAllText($xvalueIteratorAbiGate, [System.Text.Encoding]::UTF8)
$setI64IteratorAbiGateText = [System.IO.File]::ReadAllText($setI64IteratorAbiGate, [System.Text.Encoding]::UTF8)
$iteratorScalarProjectionGateText = [System.IO.File]::ReadAllText($iteratorScalarProjectionGate, [System.Text.Encoding]::UTF8)
$iteratorManagedProjectionGateText = [System.IO.File]::ReadAllText($iteratorManagedProjectionGate, [System.Text.Encoding]::UTF8)
$arrayIteratorProjectionGateText = [System.IO.File]::ReadAllText($arrayIteratorProjectionGate, [System.Text.Encoding]::UTF8)
$dictIteratorProjectionGateText = [System.IO.File]::ReadAllText($dictIteratorProjectionGate, [System.Text.Encoding]::UTF8)
$erasedCollectionIteratorGateText = [System.IO.File]::ReadAllText($erasedCollectionIteratorGate, [System.Text.Encoding]::UTF8)
$dynamicVarIteratorGateText = [System.IO.File]::ReadAllText($dynamicVarIteratorGate, [System.Text.Encoding]::UTF8)
$stringIteratorGateText = [System.IO.File]::ReadAllText($stringIteratorGate, [System.Text.Encoding]::UTF8)
$charBytesAbiGateText = [System.IO.File]::ReadAllText($charBytesAbiGate, [System.Text.Encoding]::UTF8)
$protocolIteratorGateText = [System.IO.File]::ReadAllText($protocolIteratorGate, [System.Text.Encoding]::UTF8)
$collectionTypeRebindGateText = [System.IO.File]::ReadAllText($collectionTypeRebindGate, [System.Text.Encoding]::UTF8)
$nullableContainerProjectionGateText = [System.IO.File]::ReadAllText($nullableContainerProjectionGate, [System.Text.Encoding]::UTF8)
$safeIndexNullableCollectionGateText = [System.IO.File]::ReadAllText($safeIndexNullableCollectionGate, [System.Text.Encoding]::UTF8)
$nullableSafeClassFieldGateText = [System.IO.File]::ReadAllText($nullableSafeClassFieldGate, [System.Text.Encoding]::UTF8)
$memberAbiGateText = [System.IO.File]::ReadAllText($memberAbiGate, [System.Text.Encoding]::UTF8)
$lengthProtocolMemberGateText = [System.IO.File]::ReadAllText($lengthProtocolMemberGate, [System.Text.Encoding]::UTF8)
$classFieldMemberAbiGateText = [System.IO.File]::ReadAllText($classFieldMemberAbiGate, [System.Text.Encoding]::UTF8)
$classFieldInheritedNarrowedGateText = [System.IO.File]::ReadAllText($classFieldInheritedNarrowedGate, [System.Text.Encoding]::UTF8)
$nullableStringLifecycleGateText = [System.IO.File]::ReadAllText($nullableStringLifecycleGate, [System.Text.Encoding]::UTF8)
$i64PowAbiGateText = [System.IO.File]::ReadAllText($i64PowAbiGate, [System.Text.Encoding]::UTF8)
$timeApproxAbiGateText = [System.IO.File]::ReadAllText($timeApproxAbiGate, [System.Text.Encoding]::UTF8)
$specialF64ConstantAbiGateText = [System.IO.File]::ReadAllText($specialF64ConstantAbiGate, [System.Text.Encoding]::UTF8)
$resolvedPropertyCallGateText = [System.IO.File]::ReadAllText($resolvedPropertyCallGate, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_async_generic_expression_planner.xl",
    "source-shape planners disconnected", "compatibility model removed",
    "canonical lowered await CFG", "asyncDispatch", "direct target links",
    "ordered frame actions", "lexical cleanup transitions",
    "explicit ownership/take points",
    "destroyAbiName", "transferAbiName", "firstTransferAction", "transferActionCount",
    "transfer-before-instruction",
    "SequenceEqual", "-Werror", "runtime gate failed",
    "Legacy async source-shape selector was not deleted"
) ) {
    if ( ! $asyncGenericPlannerGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the generic async expression planner gate: $required"
    }
}
foreach ( $required in @(
    "xlir_persistence_recursive_class_values.xl",
    'persistenceValueAbiName -ne "nullable"',
    'persistenceValueAbiName -ne "array"',
    'persistenceConstructionAbiName -ne "schema-default"',
    'persistenceDefaultAbiName -eq "xvalue-null"',
    'persistenceDefaultAbiName -eq "empty-array"',
    "persistence-json-stringify", "persistence-json-parse",
    "xlir_persist_json_project_t", "xlir_persist_json_restore_t",
    "xrtValueArrayGet(source, index)", "xrtValueTypeIdBind(result",
    "xl6_xrt_json_stringify", "SequenceEqual", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $recursivePersistenceGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the recursive class persistence ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_persistence_projection_roots.xl",
    '"list-sequence"', '"set-sequence"', '"dict-object"',
    "persistence-json-stringify", "persistence-xlon-stringify",
    "xrtValueIterAdvance(iterator, NULL, &borrowed)",
    "xrtValueObjectAt(source, index, &key)",
    "xl6_xrt_typed_list_to_sequence_value", "SequenceEqual", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $persistenceProjectionRootGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed persistence projection-root ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_persistence_recursive_collection_values.xl",
    '"xvalue-container-empty"', '"empty-list"', '"empty-set"',
    '"empty-dict"', '"list-sequence"', '"set-sequence"',
    '"dict-object"', "persistence-json-stringify",
    "persistence-json-parse", 'xlir_persist_json_${direction}_t',
    "xrtValueIntMapSetNew(result, (int64_t)index, item)",
    "xrtValueSetAddNew(result, item)",
    "xrtValueObjectSetNew(result, key, item)",
    "SequenceEqual", "-Werror", "strict-C/runtime gate failed",
    "XLIR-only runtime contract"
) ) {
    if ( ! $recursiveCollectionPersistenceGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the recursive collection persistence ABI gate: $required"
    }
}
foreach ( $required in @(
    "protocol_inherited_dispatch.xl",
    "protocol_native_backed_dispatch.xl",
    "native-class-handle-borrow", "nominalIdentitySets",
    "byte-for-byte deterministic", "strict runtime"

) ) {
    if ( ! $protocolWitnessAdapterGateText.Contains($required) ) {
        throw "XLIR migration matrix requires inherited/native protocol witness adapter coverage: $required"
    }
}
foreach ( $required in @(
    "xlir_nullable_cast_abi.xl", "xvalue-cast-nullable-tagged",
    "xvalue-cast-nullable-string", "castAbiName", "payloadType",
    "mismatch-null semantics", "adjacent ERROR_TEST", "TypeError",
    "owned string transfer", "ReadAllBytes", "-Werror"
) ) {
    if ( ! $nullableCastAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the closed nullable cast ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_nominal_cast_abi.xl", "nominalIdentitySets",
    "xvalue-nominal-test", "xvalue-nominal-cast-checked",
    "xvalue-nominal-cast-nullable", "inheritance/witness identity sets",
    "adjacent ERROR_TEST", "explicit TypeError CFG",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $nominalCastAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the closed nominal cast ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_async_future_direct_call_catch", "xlir_closure_future_capture",
    "xlir_nullable_async_continuation", "xlir_checked_integer_continuation_abi",
    "one direct callback per canonical suspension", "nested async launch",
    "checked integer effects",
    "full supported XLIR async inventory", "non-analyzed machine",
    "orphaned or ambiguous suspension",
    "fell back to legacy continuation rendering",
    "byte-for-byte deterministic C",
    "catch recovery", "closure/future ownership", "tagged nullable/null construction",
    "-Werror", "runtime semantics"
) ) {
    if ( ! $asyncMachineAdmissionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the fail-closed async machine admission gate: $required"
    }
}
foreach ( $required in @(
    "xlir_async_error_abi.xl", "xlir_async_error_race_abi.xl",
    "xlir_task_all_error_abi.xl",
    "retained-error", "retain-error", "future-race", "future-all-xvalues",
    "canonical TypeValueId binding", "xrtErrorRef(a0)",
    "xl6TaskRuntimeV2Get()->Cleanup", "xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_ERROR)",
    "byte-for-byte deterministic",
    "xl6ErrorValueRefV1", "xvalue-project-error-ref",
    "-Werror", "repeated-await runtime", "source re-await ABI"
) ) {
    if ( ! $asyncErrorAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the retained-error async ownership gate: $required"
    }
}
foreach ( $required in @(
    "xlir_future_cancel.xl", 'kindName -eq "future-cancel"',
    'admissionName -eq "verified"', 'runtimeOpName -eq "future-cancel"',
    "task.cancel(future)", "future.cancel()", "temporary ownership",
    "-Werror", "strict-C/runtime gate failed"
) ) {
    if ( ! $futureCancelGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical Future cancellation intrinsic gate: $required"
    }
}
foreach ( $required in @(
    "xlir_future_member_status_abi.xl", "fifteen distinct verified IntrinsicCalls and RuntimeOps",
    '"future-wait"', '"future-wait-timeout"', '"future-wait-until"',
    '"future-is-done"', '"future-is-pending"', '"future-is-cancelled"',
    '"future-is-failed"', "borrowed Future", "I64 millisecond operands",
    '"future-state"', '"future-status"', '"future-error"',
    '"future-set-debug-name"', '"future-debug-name"',
    '"future-create-time-ms"', '"future-complete-time-ms"',
    '"future-pending-continuations"', "owned string/error projection",
    "SequenceEqual", "mechanical RuntimeOp emission", "-Werror"

) ) {
    if ( ! $futureMemberStatusGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the explicit Future wait/status member ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_task_when_any_literal_abi.xl", 'kindName -eq "task-when-any"',
    'runtimeOpName -eq "future-when-any-index"', "heterogeneous Future payload identities",
    "xl6FutureAggregateWhenAnyIndexV2(fa", "xrtFutureThenOwnedCancelSource",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $taskWhenAnyLiteralGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the flattened task.whenAny literal ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_task_when_any_array_abi.xl", 'kindName -eq "task-when-any-array"',
    'runtimeOpName -eq "xvalue-future-ref"',
    'runtimeOpName -eq "future-when-any-index-array"',
    "borrowed array<Future<T>>", "xrtValueFuture(v", "xl6FutureAggregateWhenAnyIndexArrayV2(v",
    "xl6_future_aggregate.h", "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $taskWhenAnyArrayGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed-array task.whenAny ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_task_aggregate_array_abi.xl",
    'kindName -eq "task-all-array"', 'kindName -eq "task-race-array"',
    'runtimeOpName -eq "xvalue-future-ref"',
    'runtimeOpName -eq "future-all-xvalues-array"',
    'runtimeOpName -eq "future-race-array"',
    "borrowed array<Future<T>>", "xrtValueFuture(v",
    "xl6_future_aggregate.h", "xlir_future_aggregate_owner",
    "xl6FutureAggregateAllArrayV2(v",
    "xl6FutureAggregateRaceArrayV2(v",
    "SequenceEqual", "XL6_CGEN_XLANG_LINKS",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $taskAggregateArrayGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed-array task.all/task.race ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_task_nested_future_abi.xl", 'payloadAbiName -eq "retained-future"',
    'runtimeOpName -eq "task-spawn-closure"',
    '"future-all-xvalues", "future-all-xvalues-array"',
    '"future-race", "future-race-array"',
    'asyncResultAbiName -eq "retained-future"',
    "task.spawn Future argument frame retain", "independent await reference",
    "xlir_async_future_free", "xlir_future_await_future",
    "xrtValueFuture(*(xfuture* const*)payload)", "xrtFutureRef(a0)",
    "SequenceEqual", "strict-C/runtime gate failed", "XLIR-only runtime contract",
    "no legacy source-shape fallback"
) ) {
    if ( ! $taskNestedFutureGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the nested Future task ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_task_spawn_closure_abi.xl", 'kindName -eq "task-spawn"',
    'runtimeOpName -eq "task-spawn-closure"',
    "canonical operand ABI", "managed captures/results",
    "byte-for-byte deterministic", "strict-C/runtime gate failed"

) ) {
    if ( ! $taskSpawnClosureGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed task.spawn closure/frame ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_thread_resource_abi.xl", "runtimeResourceAbiName", "runtimeResourceOpName",
    "runtimeOpName", "retain-xvalue", "conditional", "repeat.read_bytes()",
    "xl6ThreadCreateV1", "xl6ThreadWaitV1", "StandaloneImports", "ThreadLanguage"

) ) {
    if ( ! ($threadPublicGateText+$threadLanguageGateText).Contains($required) ) {
        throw "XLIR migration matrix requires the typed thread resource/capture ABI gate: $required"
    }
}
if (!$threadResourceGateText.Contains('check_xlir_runtime_operand_catalog.ps1')) {
    throw 'Thread migration coverage must execute current-source compiled/resident contract acceptance'
}
foreach ( $required in @(
    "xlir_channel_resource_abi", "xlir_channel_captured_output_abi",
    "runtimeResourceOpName", "runtimeOpName", "'storage-ref'", "'storage-address'",
    "'__tuple'", "repeat.read_bytes()", "NativeGraphPrefixSweeps", "-aot-run",
    "InputsUnchanged", "WaitQueueComplete=False", "ShippingArtifact=False"

) ) {
    if ( ! $channelOwnedGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed channel resource/payload ABI gate: $required"
    }
}
if (!$channelResourceGateText.Contains('check_stdlib_channel_regressions.py')) {
    throw 'The Channel migration gate must run current-source owning result acceptance'
}
foreach ( $required in @(
    "cgen_sync.xl", '"mutex", "sem", "cond", "rwlock"',
    "runtimeResourceOpName -match", "runtimeOpName -match",
    "runtime-resource-default", "22 explicit error edges",
    "xrtValueHandleTake", "xrtValueTypeIdBind", "xrtValueGetHandle",
    "byte-for-byte deterministic", "strict-C/runtime gate failed"

) ) {
    if ( ! $syncResourceGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed synchronization resource ABI gate: $required"
    }
}
foreach ( $required in @(
    "stdlib_special_collections_concurrent.xl",
    'runtimeResourceOpName -like "special-queue-*"',
    'runtimeOpName -like "special-queue-*"',
    'kindName -ne "storage-address"', 'abiName -ne "retain-xvalue"',
    "explicit error edges", "xrtChannelDrain",
    "byte-for-byte deterministic", "strict-C/runtime gate failed"

) ) {
    if ( ! $specialQueueResourceGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed concurrent queue resource ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_stack_resource_abi.xl", "xlir_stack_class_default_abi.xl",
    'runtimeResourceOpName -like "stack-*"',
    'runtimeOpName -like "stack-*"', "Where-Object abiName -eq 'storage-ref').Count -ne 0",
    'initializerAbiName -eq "runtime-resource-default"',
    'errorAbiName -ne "may-raise"', 'abiName -ne "retain-xvalue"',
    "partial-object cleanup", "byte-for-byte deterministic",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $stackResourceGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed Stack resource/class-field ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_avl_resource_abi.xl", "xlir_avl_class_default_abi.xl",
    'runtimeResourceOpName -like "avl-tree-*"',
    'runtimeOpName -like "avl-tree-*"', '"storage-ref"',
    'runtimeResourceKeyOrderAbiName -eq "invalid"',
    'operandCount -ne 3', 'abiName -ne "retain-xvalue"',
    "partial-object cleanup", "byte-for-byte deterministic",
    "declaration-only C", "strict-C/runtime gate failed"

) ) {
    if ( ! $avlResourceGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed OrderedMap resource/class-field ABI gate: $required"
    }
}
foreach ( $required in @(
    "stdlib_future_continuation.xl",
    'kindName -in @("future-then", "future-catch", "future-finally")',
    "sourcePayloadAbiName", 'runtimeOpName -eq "future-continue-closure"',
    "futureContinuationKindName", "Future<T> to Future<U>",
    "byte-for-byte deterministic", "task-boundary AsyncError cause",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $futureContinuationGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed Future continuation ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_typed_callable_index_abi.xl",
    'runtimeOpName -eq "xvalue-project-closure-copy"',
    "calleeType", "owned closure result ABI", "xvalue-array-at-i64",
    "xlir_xvalue_project_closure_try_copy", "xrtValueIsCallable(source)",
    "xrtValueRetain(source)", "xlir_closure_new(entry, NULL, retained",
    "xlir_xvalue_object_set_literal_closure",
    "xlir_xvalue_object_get_literal_closure",
    "SequenceEqual", "XL6_CGEN_XLANG_LINKS",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedCallableIndexGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed callable index ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_functions.xl",
    "implicitConversionErrorAbiName", 'kindName -eq "collection"',
    'runtimeOpName -eq "xvalue-callable-from-closure"',
    'runtimeOpName -eq "xvalue-project-closure-copy"',
    'loopItemAbiName -eq "closure-copy"',
    'callKindName -eq "function-value"', 'targetSymbol -eq 0',
    "calleeType", "callableErasureAbiName", "CLOSURE_CALL",
    "SequenceEqual", "byte-for-byte deterministic",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedCallableContainerGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed callable-container ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_safe_index_objects_functions.xl",
    '"xvalue-array-at-i64-or-null"',
    '"xvalue-int-map-at-i64-or-null"',
    '"xvalue-object-get-literal"',
    '"xvalue-index-get-xvalue-key"',
    'runtimeOpName -eq "xvalue-project-nullable-xvalue-retain"',
    'runtimeOpName -eq "xvalue-project-nullable-union-copy"',
    'runtimeOpName -eq "xvalue-project-nullable-closure-copy"',
    "nullable native class did not preserve presence",
    "payloadType", "typeValueId", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedSafeIndexNominalGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed safe-index nominal ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_index_assign_objects_functions.xl",
    "implicitConversionErrorAbiName", "unique THROW ExitPlan",
    'runtimeOpName -eq "xvalue-callable-from-closure"',
    'runtimeOpName -eq "xvalue-project-xvalue-retain"',
    "runtimeIdentityType", "xvalue-index-set-xvalue-key-new",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $typedIndexAssignNominalGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed nominal index-assignment ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_index_assign.xl",
    'runtimeOpName -eq "xvalue-index-get-xvalue-key"',
    'runtimeOpName -eq "xvalue-project-scalar-copy"',
    "payloadType", "canonical borrowed carrier ABI",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $dynamicKeyScalarProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the dynamic-key scalar projection gate: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_point.xl", "xvalue-array-i64-set-new",
    'runtimeOpName -eq "xvalue-raw-pointer-borrow"',
    'runtimeOpName -eq "xvalue-project-raw-pointer-borrow"',
    "payloadType", "canonical borrowed carrier ABI", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedPointProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed point projection gate: $required"
    }
}
foreach ( $required in @(
    "hir_xvalue_scalar_bridge_matrix.xl",
    'runtimeOpName -eq "xvalue-project-string-copy"',
    'runtimeOpName -eq "xvalue-type-id"',
    'runtimeOpName -eq "integer-convert-checked"',
    'errorAbiName -ne "may-raise"',
    'kindName -eq "error-test"',
    'kindName -eq "move"',
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $xvalueScalarBridgeGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the xvalue scalar-bridge ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_string_conversion_abi.xl",
    "stringConversionAbiName", "stringClassType",
    "runtime-derived", "internal-call", "type-id-name",
    "class-derived-to-string", "type-id-to-string",
    "stringDeriveIncluded", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $stringConversionAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the closed string conversion ABI gate: $required"
    }
}
foreach ( $required in @(
    "check_xlir_class_identity_plan.py", "PublicDirectory", "--public", "--out"

) ) {
    if ( ! $classValueIdentityAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the closed class value identity ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_objects.xl", "class-field-borrow",
    "fieldDeclaration", "runtime-reference", "xvalue-class-new",
    "xvalue-class-identity-bind", "xvalue-raw-pointer-borrow",
    "class-field-init-new", "executionOrigin", "instantiations",
    "SequenceEqual", "strict-C/runtime gate failed",
    "standalone artifact runtime gate failed"
) ) {
    if ( ! $nativeClassObjectIdentityAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed native class object identity ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_managed_native_extern_receiver_abi.xl", "class-field-borrow",
    "parameterPhysicalAbis", "native-class-handle-borrow",
    "parameterPhysicalFieldDeclarations",
    "xvalue-object-get-literal-raw-pointer", "direct nominal-handle projector",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $managedNativeExternReceiverGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed native extern receiver physical ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_managed_native_explicit_destroy.xl", "noncopyable-null",
    "native-class-field-take",
    "xvalue-object-get-literal-native-handle-borrow",
    "xvalue-object-take-literal-native-handle",
    "SequenceEqual", "strict-C/runtime gate failed",
    "automatic cleanup"
) ) {
    if ( ! $managedNativeLifecycleGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed native owned-field lifecycle ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_var_destroy_class_init.xl", "cgen_class_destroy.xl",
    "cgen_class_inherit_destroy.xl", "hasExplicitCreate",
    "class-destroy-borrow", "xvalue-class-construction-commit",
    "derived-to-base", "retained projection", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $classLifecycleGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the user-class lifecycle ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_typed_dict_literal_index_abi.xl",
    '"xvalue-dict-get-literal-i64"',
    '"xvalue-dict-get-literal-u64"',
    '"xvalue-dict-get-literal-bool"',
    '"xvalue-dict-get-literal-f64"',
    '"xvalue-dict-get-literal-string-copy"',
    'receiverType.params[0] -ne $operation.type',
    'runtimeOpName -eq "xvalue-object-get-literal"',
    'runtimeOpName -eq "xvalue-project-tagged-nullable-copy"',
    "five exact direct projection identities", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedDictLiteralIndexGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed-dict literal-index ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_typed_native_handle_index_abi.xl",
    'runtimeOpName -eq "xvalue-retain"',
    'runtimeOpName -eq "xvalue-object-get-literal-raw-pointer"',
    'cAbi -ne 26', 'ownershipAbi -ne 2', 'nullAbi -ne 2',
    "nominal managed xvalue owner",
    "xvalue-array-at-i64", "xvalue-int-map-at-i64",
    "xvalue-object-get-literal",
    "xrtValueRetain", "xlir_xvalue_object_get_literal_raw_pointer",
    "regressed to a bare native-handle projection", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedNativeHandleIndexGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed native-backed class index ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_typed_native_handle_iterator_abi.xl",
    'loopItemAbiName -eq "xvalue-retain"',
    'loopAbiName -ne "xvalue-iterator-value"',
    'loopItemLifetimeAbiName -ne "owned-iteration-release"',
    'loopResourceAbiName -ne "xvalue-iterator-destroy"',
    'runtimeOpName -eq "xvalue-iterator-item-xvalue-retain"',
    'runtimeOpName -eq "xvalue-object-get-literal-raw-pointer"',
    'cAbi -ne 26', 'ownershipAbi -ne 2', 'nullAbi -ne 2',
    "retained nominal managed item",
    "xlir_xvalue_iterator_create(", "xlir_xvalue_iterator_advance(",
    "xlir_xvalue_iterator_item_xvalue_retain(",
    "xlir_xvalue_object_get_literal_raw_pointer(",
    "xlir_xvalue_iterator_destroy(", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedNativeHandleIteratorGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed native-backed class iterator ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_async_cfg_liveness.xl", "block program", "direct call-target machine link",
    "unique machine frame layout",
    "per-state membership", "frame residence/ranges/init bits", "ordered frame actions",
    "lexical cleanup transitions",
    "exact transfer/destroy ABI", "direct branch machine Cgen",
    "deterministic C", "task-pool compatibility",
    "runtime semantics"
) ) {
    if ( ! $asyncCfgLivenessGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the async CFG liveness gate: $required"
    }
}
foreach ( $required in @(
    "xlir_async_future_two_awaits", "xlir_async_future_three_awaits",
    "xlir_async_future_three_awaits_mixed", "xlir_async_future_three_strings",
    "direct scalar/managed two/three-await machine CFG",
    "canonical single-layout activation handoff",
    "forward/reverse transactional resident transfer",
    "exact managed operand owners and anchored borrow-storage observations with success/failure cleanup",
    "XLIR multi-await suspension has no exact handoff/rollback transaction",
    "XLIR multi-await handoff must move residents forward and roll them back in reverse order",
    "Multi-await borrowed spill does not name its exact operand owner",
    "Multi-await failure lost its managed operand cleanup",
    "Multi-await managed consumer success/failure lost its owner cleanup",
    "legacy async-chain renderer forbidden", "SequenceEqual", "-Werror",
    "runtime terminal-state propagation"
) ) {
    if ( ! $asyncMultiAwaitMachineGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the multi-await machine gate: $required"
    }
}
foreach ( $required in @(
    "cgen_var_numeric_convert.xl", "cgen_async_string_ownership.xl", "c_abi_types.xl",
    "cgen_copy_clone.xl", "xlir_class_virtual_dispatch.xl", "xlir_intrinsic_range.xl",
    "xlir_intrinsic_trivial_identity_convert.xl", "trivial-identity-convert",
    "cgen_extern_function_value.xl", "kindName", "resultAbiName", "errorAbiName",
    "intrinsicCalls", "intrinsicOperands", "identity/admission separation",
    "-Werror", "strict-C runtime failed"
) ) {
    if ( ! $intrinsicCallGateText.Contains($required) ) {
        throw "XLIR IntrinsicCall admission gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_string_membership.xl",
    "cgen_in_containers.xl",
    "string-contains",
    "two-borrow bool",
    "xrtStrContains",
    "TEMPORARY/RELEASE",
    "mixed static-string/xvalue container composition",
    "strict generated-C runtime failed",
    "-Werror"
) ) {
    if ( ! $stringMembershipGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed-string membership gate: $required"
    }
}
foreach ( $required in @(
    "xlir_range_membership.xl",
    "cgen_in_range.xl",
    "i64-range-contains",
    "item/start/stop/step i64",
    "RANGE descriptor consumption",
    "runtime-zero step",
    "overflow-safe unsigned distance",
    "strict generated-C runtime failed",
    "-Werror"
) ) {
    if ( ! $rangeMembershipGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical range membership gate: $required"
    }
}
foreach ( $required in @(
    "xlir_range_for_abi.xl", "cgen_range_assert.xl",
    "loopAbiName", "range-i64-value", "range-i64-index-value",
    "loopErrorAbiName", "descriptor", "evaluate-once operands",
    "dynamic-zero RangeError CFG", "mechanical C",
    "strict generated-C runtime failed", "-Werror"
) ) {
    if ( ! $rangeForAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical range-for ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_generator.xl", "cgen_generator_two_targets.xl",
    "cgen_generator_string_resource.xl", "cgen_generator_fstring_needs.xl",
    "xlir_generator_string_machine.xl",
    "generatorMachines", "generatorMachineEntries", "generatorMachineBlocks",
    "generatorFrameSlots", "generatorSuspensions", "generatorLiveSlots",
    "generatorFrameActions", "yieldInstructionIndex", "yieldInstructionId",
    "copy-string,copy-value", "generator-item-get", "generator-item-take",
    "byte-for-byte deterministic", "XL6_Generator", "xrtCoCreate", "xcoro",
    "-Werror", "strict generated-C runtime failed"
) ) {
    if ( ! $generatorMachineGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the synchronous generator continuation-machine gate: $required"
    }
}
foreach ( $required in @(
    "cgen_fstring.xl", "cgen_loop_fstring_needs.xl",
    "cgen_generator_fstring_needs.xl", "interpolated-string",
    "interp_string", "interp_text", "nodeChildCount", "nodeChildren",
    "string-from-utf8", "explicit-length decoded UTF-8 payload",
    "string-copy", "bool-to-string", "i64-to-string",
    "u64-to-string", "f64-to-string", "xvalue-to-string",
    "explicit protected load", "adjacent allocation failure edge",
    "left-to-right", "byte-for-byte deterministic",
    "xl6_xrt_fstr_join", "-Werror", "strict generated-C runtime failed"
) ) {
    if ( ! $interpolatedStringGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical interpolated-string ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_array_i64_for_abi.xl",
    "cgen_xlir_array_i64_for_escape_reject.xl",
    "cgen_export_typed_container_abi.xl",
    "xvalue-array-i64-value", "xvalue-array-i64-index-value",
    "xvalue-shallow-snapshot-release", "xvalue-owned-iterator-release",
    "xvalue-clone", "xvalue-count", "xvalue-array-at-i64-integer",
    "exact named and owned-rvalue admission", "clone-or-transfer acquisition",
    "unique canonical exit release", "call/return admission",
    "local and outward exception routing", "return/throw/finally cleanup",
    "non-container excluded from array ABI",
    "runtime snapshot/transfer semantics", "mechanical C", "check_stdlib_collection_continuations.py", "--array-only"
) ) {
    if ( ! $arrayI64ForAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical array<i64> for ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_list_i64_iterator_abi.xl",
    "xvalue-iterator-key-value", "integer-i64",
    "xvalue-iterator-destroy", "value-iterator",
    "xvalue-iterator-create", "xvalue-iterator-advance",
    "xvalue-iterator-item-i64", "xvalue-iterator-key-i64",
    "dedicated cursor TypeId", "orthogonal key/item projection",
    "stable named and owned-rvalue", "LoopResource destroy on return/throw/finally",
    "mechanical C", "-Werror"
) ) {
    if ( ! $xvalueIteratorAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical xvalue iterator ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_set_i64_iterator_abi.xl", "cgen_xlir_set_i64_key_reject.xl",
    "xvalue-iterator-value", "xvalue-iterator-key-i64",
    "compiler-owned cursor TypeId", "shared xvalue cursor and C helpers",
    "exact value-only projection", "no invented key ABI",
    "stable named and owned-rvalue snapshots",
    "LoopResource destroy on return/throw/finally", "mechanical C", "-Werror"
) ) {
    if ( ! $setI64IteratorAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical set<i64> iterator ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_iterator_scalar_projection.xl",
    "xlir_set_u64_iterator_abi.xl",
    "xvalue-iterator-item-u64", "xvalue-iterator-item-bool",
    "xvalue-iterator-item-f64", "integer-i64",
    "list u64 plus list/set bool/f64", "native set<u64> admission is exact",
    "Cgen is mechanical", "-Werror"
) ) {
    if ( ! $iteratorScalarProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the xvalue iterator scalar projection gate: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_iterator_managed_projection.xl",
    "xvalue-iterator-item-string-copy", "owned-iteration-release",
    "per-iteration release on fallthrough/continue/break/return/throw/finally",
    "return ownership preserved through finally", "cursor teardown ordering",
    "mechanical C", "-Werror"
) ) {
    if ( ! $iteratorManagedProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the managed iterator projection gate: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_array_iterator_projection.xl",
    "xvalue-iterator-key-ordinal-i64",
    "xvalue-iterator-item-xvalue-retain",
    "ordinal/u64/bool/f64/string/xvalue projections",
    "snapshot stability", "managed cleanup across exits",
    "cursor/item frame ownership across await", "launch/resume completion",
    "deterministic mechanical C", "-Werror"
) ) {
    if ( ! $arrayIteratorProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the xvalue-array iterator projection gate: $required"
    }
}
foreach ( $required in @(
    "xlir_dict_iterator_projection.xl", "xlir_dict_key_only_iterator.xl",
    "xvalue-iterator-key", "xvalue-iterator-key-string-copy",
    "i64/u64/bool/f64/string item projections",
    "double-managed reverse cleanup on all exits/finally",
    "independent return ownership", "mechanical C", "-Werror"
) ) {
    if ( ! $dictIteratorProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the dict iterator projection gate: $required"
    }
}
foreach ( $required in @(
    "xlir_erased_collection_iterator_abi.xl",
    "collectionAbiName", "runtimeTypeMap.fallback",
    "xvalue-iterator-item-xvalue-retain",
    "xvalue-iterator-key-ordinal-i64", "xvalue-iterator-key-i64",
    "xvalue-iterator-key-string-copy", "source-bound cursor",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $erasedCollectionIteratorGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the statically shaped erased collection iterator gate: $required"
    }
}
foreach ( $required in @(
    "xlir_dynamic_var_iterator_abi.xl",
    "cgen_for_in_containers", "cgen_for_in_var_non_container",
    "xvalue-iterator-create-checked", "ERROR_TEST",
    "xvalue-iterator-key-dynamic-xvalue",
    "xvalue-iterator-dynamic-logical-xvalue",
    "canonical managed var TypeId", "source-bound cursor",
    "owned-rvalue checked-create release",
    "continue/break/return/finally", "TypeError",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $dynamicVarIteratorGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the runtime-checked dynamic var iterator gate: $required"
    }
}
foreach ( $required in @(
    "xlir_string_iterator_abi.xl", "stdlib_string.xl",
    "string-utf8-value", "string-utf8-index-value",
    "string-snapshot-release", "string-utf8-count", "string-scalar-at",
    "trivial char projection", "checked status edges",
    "copy-vs-transfer acquisition semantics",
    "empty/continue/break/return/finally/throw cleanup",
    "emoji runtime semantics", "ReadAllBytes", "-Werror"

) ) {
    if ( ! $stringIteratorGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical UTF-8 string iterator gate: $required"
    }
}
foreach ( $required in @(
    "char_bytes_literals.xl", "char_bytes_advanced.xl",
    "cAbi -ne 43", "cAbi -ne 44", "unicode-scalar",
    "string-scalar-set-copy", "bytes-set-copy", "bytes-index-value",
    "xvalue-char", "persistence-xlon-stringify",
    "future<char>", "generator<bytes>",
    "char_bytes_invalid_char_width.xl",
    "char_bytes_invalid_char_surrogate.xl",
    "char_bytes_invalid_bf_prefix.xl",
    "char_bytes_invalid_brf_prefix.xl",
    "formatted backticks must preserve ordinary escapes",
    "formatted triple strings must use common-indent framing",
    "SequenceEqual", "-Werror", "XL6_CGEN_LINKS"
) ) {
    if ( ! $charBytesAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the complete char/bytes ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_dynamic_typed_collection_projection.xl",
    "cgen_dynamic_to_typed_containers.xl",
    "xvalue-collection-checked-cast",
    "xvalue-collection-checked-rebind",
    "xvalue-type-rebind-owned", "xvalue-clone",
    "runtimeIdentityType", "xrtValueTypeIdRebind",
    "ReadAllBytes", "-Werror", "runtime contract"
) ) {
    if ( ! $collectionTypeRebindGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the checked collection TypeId rebind gate: $required"
    }
}
foreach ( $required in @(
    "typed_nullable_container_matrix.xl",
    "tagged/string/closure/protocol layouts",
    "xvalue-int-map-at-i64-or-null",
    "xvalue-iterator-item-borrow",
    "ownership-only exact-TypeId retain",
    "explicit VALUE rebinds", "resident print ABI",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $nullableContainerProjectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the typed nullable container projection gate: $required"
    }
}
foreach ( $required in @(
    "xlir_safe_index_nullable_collection_abi.xl",
    'runtimeOpName -eq "xvalue-array-at-i64-or-null"',
    'runtimeOpName -eq "xvalue-int-map-at-i64-or-null"',
    'runtimeOpName -eq "xvalue-project-tagged-nullable-copy"',
    'runtimeOpName -eq "xvalue-project-nullable-string-copy"',
    'kindName -ne "nullable-unwrap"',
    "nullable receiver coverage", "static collection payload",
    "xrtValueArrayAt(v", "xrtValueIntMapGet(v", "xrtValueNull()",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $safeIndexNullableCollectionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the safe nullable-collection index ABI gate: $required"
    }
}
foreach ( $required in @(
    "nullable_safe_navigation.xl",
    'memberAbiName -eq "safe-class-field-nullable"',
    'runtimeOpName -eq "xvalue-safe-class-field-get-literal"',
    '"xvalue-project-tagged-nullable-copy"',
    '"xvalue-project-nullable-string-copy"',
    "runtimeIdentityType", "payloadType",
    "lookup/project/release", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $nullableSafeClassFieldGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the nullable safe declared-class field ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_protocol_iterator_managed_item.xl",
    "extlib_native_iter_protocol_min.xl", "stdlib_buffer_file_runtime.xl",
    "protocol-iterator-value", "protocol-iterator-index-value",
    "protocol-product-take", "native-backed-class-managed-iterator-release",
    'PSObject.Properties.Name -contains "loopResourceDestroySymbol"',
    "continue/break/return/throw", "direct CALL targets",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $protocolIteratorGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical class protocol iterator gate: $required"
    }
}
foreach ( $required in @(
    "xlir_member_abi_string_length.xl", "memberAbiName",
    "string-utf8-count", "one-borrow/i64", "xrtUtf8Count",
    "Unicode runtime semantics", "strict generated-C runtime failed", "-Werror"
) ) {
    if ( ! $memberAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical non-call member ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_length_protocol_member.xl", "stdlib_buffer.xl",
    "length-protocol-call", "source-class/native-class",
    "one exact direct CALL", "strict generated-C runtime failed", "-Werror"
) ) {
    if ( ! $lengthProtocolMemberGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical length protocol member gate: $required"
    }
}
foreach ( $required in @(
    "xlir_class_field_member_abi.xl", "class-field-bool",
    "class-field-i64", "class-field-u64", "class-field-f64",
    "class-field-string-copy", "class-field-raw-pointer-borrow",
    "class-field-xvalue-borrow",
    "xvalue-object-get-literal-native-handle-borrow",
    "xvalue-raw-pointer-borrow", "strict generated-C runtime failed", "-Werror"
) ) {
    if ( ! $classFieldMemberAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical class-field member ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_class_field_inherited_narrowed_abi.xl",
    'baseType -ne 0', 'memberAbiName -eq "class-field-i64"',
    'memberAbiName -eq "class-field-string-copy"',
    'receiver.type -ne $receiver.storageType',
    '"load", "nullable-unwrap"',
    "explicit base-layout lookup", "semantic nullable flow type",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $classFieldInheritedNarrowedGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the inherited/narrowed class-field ABI gate: $required"
    }
}
foreach ( $required in @(
    "nullable_string_lifecycle.xl",
    "structurally canonical",
    "class-field-nullable-string-copy",
    "xvalue-project-nullable-string-copy",
    "string-nullable-copy",
    "constructor borrowed nullable-string lvalue",
    "closure capture return",
    "SequenceEqual", "strict-C/runtime gate failed"

) ) {
    if ( ! $nullableStringLifecycleGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the nullable-string lifecycle ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_pow.xl", 'runtimeOpName -eq "i64-pow"',
    'integerTargetName -ne "i64"', 'operatorErrorAbiName -ne "no-raise"',
    'kindName -eq "binary"', 'opcodeName -eq "pow"',
    '"load", "runtime-call", "store"', "SequenceEqual",
    "xlir_i64_pow", "strict-C runtime failed"
) ) {
    if ( ! $i64PowAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the exact i64 power ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_time_literals.xl", 'runtimeOpName -eq "time-approx"',
    'opcodeName -ne "approx"', 'operatorErrorAbiName -ne "no-raise"',
    'name -ne "time"', 'cAbi -ne 40',
    "xrtTimeNear", "UINT64_C(10000000)", "SequenceEqual",
    "strict-C runtime failed"
) ) {
    if ( ! $timeApproxAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the exact time approximate ABI gate: $required"
    }
}
foreach ( $required in @(
    "stdlib_math.xl", '"f64-nan", "f64-positive-infinity"',
    'runtimeOpName -eq "f64-nan"',
    'runtimeOpName -eq "f64-positive-infinity"',
    'kindName -eq "const"', "arbitrary CONST C payload",
    "DBL_MANT_DIG == 53", "memcpy(&value, &bits, sizeof(value))", "SequenceEqual",
    "Special f64 CLI run failed", "Special f64 CLI executable failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $specialF64ConstantAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the special f64 constant ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_u64_scalar.xl",
    "xlir_u64_async_closure.xl",
    "xlir_u64_generic_intrinsic.xl",
    "xlir_u64_dynamic.xl",
    "cabi=34 native-slot=u64", "callable=invalid ownership=1",
    "18446744073709551615ULL",
    "runtime=u64-to-string",
    "xlir_result_u64",
    "xlir_future_wait_checked", "xrtFutureValue(",
    "xl6CellRuntimeV1Get()->Store", "XL6_CELL_U64",
    "xvalue-u64",
    "xvalue-array-at-i64-u64",
    "xvalue-int-map-at-i64-u64",
    "xvalue-object-get-literal-u64",
    "xvalue-binary-add",
    "xvalue-binary-sub",
    "xvalue-binary-mul",
    "xl6IntegerValueConvertV1",
    "xrtValueUInt",
    "xrtValueGetUInt",
    "xl6FutureAggregateAllV2(",
    "generic specialization",
    "xrtUIntFormatTo",
    "-Werror",
    "generated-C runtime failed"
) ) {
    if ( ! $u64AbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the lossless U64 execution/runtime gate: $required"
    }
}
foreach ( $required in @(
    "xlir_integer_conversion_source_abi.xl", "type_int_narrowing_wrap.xl",
    "checked-integer-convert", "wrapping-integer-convert",
    "integerSourceAbiName", "integerTargetName",
    "integer-convert-checked", "integer-convert-wrapping",
    "ERROR_TEST", "ReadAllBytes", "-Werror",
    "Static U64 conversion incorrectly re-entered the checked ABI",
    "narrow integer strict-C runtime failed"

) ) {
    if ( ! $integerConversionAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the explicit integer conversion ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_checked_float_string.xl", "checked-float-convert",
    "string-to-f64-checked", "borrow", "may-raise", "ERROR_TEST",
    "xl6Float64TextConvertV1", "projection_ok_",
    "ReadAllBytes", "-Werror", "strict-C runtime failed"
) ) {
    if ( ! $checkedFloatConversionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the checked managed-string/F64 conversion gate: $required"
    }
}
foreach ( $required in @(
    "xlir_checked_time_conversion.xl", "checked-time-convert",
    "string-to-time-checked", "xvalue-to-time-checked",
    "time-to-string", "xvalue-time", "borrow", "may-raise", "ERROR_TEST",
    "xrtTimeParseAny", "xrtValueGetTime", "xrtValueTime",
    "ValueError", "TypeError", "ReadAllBytes", "-Werror",
    "strict XLIR failure recovery"
) ) {
    if ( ! $checkedTimeConversionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the checked string/xvalue time conversion gate: $required"
    }
}
foreach ( $required in @(
    "xlir_bool_conversion_abi.xl", "bool-convert", "xvalue-truthy",
    "opcodeName", "borrow", "value", "xrtValueTruthy(",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $boolConversionAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the explicit bool truth conversion ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_checked_cast_abi.xl",
    "xvalue-cast-checked", "castAbiName", "payloadType", "may-raise",
    "adjacent ERROR_TEST", "xrtValueGetUInt", "TypeError", "MemoryError",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $checkedCastAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the closed checked cast ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_dynamic_conditional_truth.xl", "xvalue-truthy", "adjacent branch",
    "exact result TypeId", "xrtValueTruthy(", "ReadAllBytes", "-Werror",
    "strict-C runtime failed"
) ) {
    if ( ! $dynamicConditionalTruthGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the dynamic conditional truth gate: $required"
    }
}
foreach ( $required in @(
    "xlir_dynamic_unary_abi.xl", "xvalue-unary-pos", "xvalue-unary-neg",
    "xvalue-unary-bit-not", "no ambiguous UNARY fallback", "xl6ValueNumericUnaryV1(",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $dynamicUnaryAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the explicit dynamic unary ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_dynamic_binary_extended_abi.xl", "xvalue-binary-div",
    "xvalue-binary-mod", "xvalue-binary-bit-and", "xvalue-binary-bit-or",
    "xvalue-binary-bit-xor", "xvalue-binary-shl", "xvalue-binary-shr",
    "operatorErrorAbiName", "error-test", "xl6ValueNumericBinaryV1(",
    "xl6ValueAddV1(", "ReadAllBytes", "-Werror"

) ) {
    if ( ! $dynamicBinaryExtendedAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the checked extended dynamic binary ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_checked_integer_arithmetic_abi.xl",
    "xlir_checked_integer_compound_error_abi.xl",
    "xlir_checked_integer_continuation_abi.xl",
    "integer-div-checked", "integer-mod-checked",
    "integer-shl-checked", "integer-shr-checked",
    "integerTargetName", "operatorErrorAbiName", "error-test",
    "xlir_i64_binary_checked(", "xlir_u64_binary_checked(",
    "INT64_MIN && right == -1", "ReadAllBytes", "-Werror",
    "nested async launch plus async/generator machine ownership"

) ) {
    if ( ! $checkedIntegerArithmeticAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the checked scalar integer arithmetic ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_scalar_conversion_abi.xl", "bool-to-bool", "bool-to-integer", "bool-to-float",
    "integer-to-bool", "integer-to-integer", "integer-to-float",
    "float-to-integer", "float-to-float", "scalarConversionAbiName",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $scalarConversionAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical scalar conversion ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_time_conversion_abi.xl", "TIME_I64",
    "time-to-integer", "time-to-float", "integer-to-time",
    "separate typed bool truth semantics",
    "extern parameter/return ABI", "ReadAllBytes", "-Werror"

) ) {
    if ( ! $timeConversionAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the distinct time conversion ABI gate: $required"
    }
}
if ( ! $defaultClassGateText.Contains("xlir_default_class_layout.xl") -or
     ! $defaultClassGateText.Contains("xrtValueObjectSetNew") -or
     ! $defaultClassGateText.Contains("generated-C runtime failed") ) {
    throw "XLIR migration matrix requires the default-class generated-C runtime gate"
}
if ( ! $instanceClassMethodGateText.Contains("xlir_class_instance_method.xl") -or
     ! $instanceClassMethodGateText.Contains("xrtValueObjectSetNew") -or
     ! $instanceClassMethodGateText.Contains("generated-C runtime failed") ) {
    throw "XLIR migration matrix requires the instance-class method generated-C runtime gate"
}
foreach ( $required in @(
    "xlir_class_implicit_receiver.xl", "parameterAbis", "constructor/getter/setter/method",
    "canonical slot-zero borrow ABI", "-Werror", "generated-C runtime failed"
) ) {
    if ( ! $implicitClassReceiverGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the declaration-driven class receiver gate: $required"
    }
}
foreach ( $required in @(
    "xlir_explicit_constructor_field_initializer.xl", "name=declared init=explicit",
    "name=untouched init=scalar-zero", "before the create body", "-Werror", "runtime failed"
) ) {
    if ( ! $explicitConstructorFieldGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the explicit-constructor field-initializer gate: $required"
    }
}
foreach ( $required in @(
    "xlir_class_reference_field_initializer.xl",
    "cgen_nested_class_constructor.xl", "cgen_class_native_field.xl",
    "runtime=xvalue-null", "runtime=xvalue-class-new",
    'runtimeOpName -eq "xvalue-native-handle-take"', "xlir_native_handle_take_s",
    "xrtValueObjectLifo", "-Werror", "generated-C runtime failed"
) ) {
    if ( ! $classReferenceFieldGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the class reference-field/owned-destruction gate: $required"
    }
}
if ( ! $explicitClassConstructorGateText.Contains("xlir_class_explicit_constructor_base.xl") -or
     ! $explicitClassConstructorGateText.Contains('$xlir.class.runtime-type') -or
     ! $explicitClassConstructorGateText.Contains("generated-C runtime failed") ) {
    throw "XLIR migration matrix requires the explicit-class constructor/default-base runtime gate"
}
if ( ! $staticClassGateText.Contains("xlir_class_static_storage.xl") -or
     ! $staticClassGateText.Contains("xlang_main") -or
     ! $staticClassGateText.Contains("generated-C runtime failed") ) {
    throw "XLIR migration matrix requires the static-class generated-C runtime gate"
}
if ( ! $virtualClassModelGateText.Contains("xlir_class_virtual_dispatch.xl") -or
     ! $virtualClassModelGateText.Contains("cgen_class_polymorphism.xl") -or
     ! $virtualClassModelGateText.Contains("class-dispatch-slot") -or
     ! $virtualClassModelGateText.Contains("xlir_vdispatch_") -or
     ! $virtualClassModelGateText.Contains("xrtValueArrayAppendNew") -or
     ! $virtualClassModelGateText.Contains("case 0x{0:X16}ULL") -or
     ! $virtualClassModelGateText.Contains("generated-C runtime failed") ) {
    throw "XLIR migration matrix requires the virtual-class and full-polymorphism runtime ABI gate"
}
foreach ( $required in @(
    "protocol_dynamic_dispatch.xl",
    "protocol_overload_dispatch.xl",
    "protocol_generic_dispatch.xl",
    "protocol_property_dispatch.xl",
    "protocol_generic_property_dispatch.xl",
    "protocol_async_dispatch.xl",
    "--lowered",
    "generic-instance source=n24",
    "generic class/property lowered ABI",
    "case 1: return xlir_async_start_4(receiver);",
    "-Werror",
    "generated-C runtime gate failed"
) ) {
    if ( ! $protocolDispatchGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the protocol dispatch runtime ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_function_adapter_dedupe", "cgen_toplevel_typed_func_value",
    "cgen_named_function_argument", "cgen_function_multi_return",
    "cgen_c_callback_export", "cgen_function_value_error_propagation",
    "cgen_extern_function_value", "xlir_named_function_extern_adapter", "adaptedTargetSymbol",
    "runtimeBridgeName", "may-raise", "strict-C runtime failed"
) ) {
    if ( ! $namedFunctionAdapterGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the named-function adapter CFG gate: $required"
    }
}
foreach ( $required in @(
    "cgen_param_modes_container_matrix", "cgen_typed_container_param_mutation",
    "xlir_typed_collection_methods", "array-append", "set-add",
    "collection-contains", "collection-remove", "typedCollectionOpName",
    "xvalue-typed-collection-method",
    "collectionAbiName", "functionParameters", "lexical-owner",
    "-Werror", "strict-C runtime failed"
) ) {
    if ( ! $collectionMemberGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the collection-member/parameter-owner gate: $required"
    }
}
foreach ( $required in @(
    "xlir_erased_array_append.xl", "array-append", "paramCount=0", "array<var>",
    "consume", "xvalue-i64", "xvalue-string-copy",
    "xvalue-array-append-new", "TRAP_IF_FALSE", "ReadAllBytes",
    "-Werror", "strict generated-C runtime failed"
) ) {
    if ( ! $erasedArrayAppendGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the erased array append/push ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_dynamic_container_methods", "cgen_dynamic_method_arguments",
    "12 explicit operations", "class-first dynamic dispatch",
    "fallbackCollectionOpName", "xvalue-dynamic-method-call",
    "dynamicCollectionOpName", "xvalue-dynamic-collection-method",
    "consume", "owned", "-Werror", "strict-C runtime failed"
) ) {
    if ( ! $dynamicCollectionMethodGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the dynamic collection-method gate: $required"
    }
}
foreach ( $required in @(
    "cgen_container_ops.xl", "22 explicit array/list/dict/set operations",
    "exact TypeId signatures", "typed array insert",
    "byte-for-byte deterministic mechanical C", "no generic/legacy fallback",
    "-Werror", "strict-C runtime failed"
) ) {
    if ( ! $collectionAlgebraGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the closed collection-algebra gate: $required"
    }
}
foreach ( $required in @(
    "cgen_extern_function_value", "xlir_xcall_scalar_effect",
    "xlir_xcall_nullable_closure.xl", "nullable-unwrap",
    "narrowed nullable closure storage",
    "xvalue-callable-from-closure", "xvalue-dynamic-call",
    "xl6ClosureRuntimeV1Get()->Box", "xrtValueInvoke", "projected callable error causes",
    "-Werror", "runtime failed"
) ) {
    if ( ! $xcallGateText.Contains($required) ) {
        throw "XLIR xcall gate is not bound to the migration matrix: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_dynamic_key_index.xl",
    "xlir_xvalue_dynamic_key_index_set.xl",
    "xlir_typed_dict_scalar_set.xl",
    "xlir_xvalue_dynamic_key_temporary_receiver.xl",
    "xvalue-index-get-xvalue-key",
    "xvalue-index-set-xvalue-key-new",
    "two-borrow operation ABI",
    "borrowed-key/consumed-item ABI",
    "typed dict scalar write",
    "runtime i64/string dispatch",
    "explicit temporary store/load/borrow/retain/release ownership",
    "strict generated-C runtime failed",
    "-Werror"
) ) {
    if ( ! $dynamicKeyIndexGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the dynamic-key index ownership gate: $required"
    }
}
foreach ( $required in @(
    "xlir_index_write_abi.xl", "xlir_index_write_generic_abi.xl",
    "first-class verified IndexWriteAbi",
    "exact array/list/object/dynamic consuming RuntimeOps",
    "uint64 boxing", "null-aware list reuse",
    "generic specialization refresh", "one RangeError CFG edge per write",
    "mechanical C", "-Werror"
) ) {
    if ( ! $indexWriteAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical IndexWriteAbi gate: $required"
    }
}
foreach ( $required in @(
    "xlir_owned_expression_discard.xl",
    "cgen_array_literal_expr_elements.xl",
    "bad_xlir_borrowed_managed_expression_discard.xl",
    "xvalue-array-new",
    "xvalue-array-append-new",
    "collection expression discard strict generated-C runtime failed",
    "kind=temporary",
    "release",
    "temporary-storage release ABI",
    "XLIRexpressionstatementhasnoexactowned-rvaluestoragereleaseABI",
    "strict generated-C runtime failed",
    "-Werror"
) ) {
    if ( ! $ownedExpressionDiscardGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the owned expression-result discard gate: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_membership.xl",
    "xvalue-contains",
    "xvalue-set-has",
    "two-borrow bool",
    "xrtValueIterCreate",
    "xrtValueObjectHas",
    "xrtStrContains",
    "runtime tags",
    "strict generated-C runtime failed",
    "-Werror"
) ) {
    if ( ! $xvalueMembershipGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the xvalue collection membership gate: $required"
    }
}
foreach ( $required in @(
    "export_vars.xl", "globalExportCount", "globalExports",
    "static scalar initialization", "direct host access", "generated-C runtime failed", "-Werror"
) ) {
    if ( ! $globalExportBoundaryGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the verified GlobalExport ABI gate: $required"
    }
}
foreach ( $required in @(
    "extern_var.xl", "xlir_extern_global_function.xl", "externGlobalCount", "externGlobals",
    "header isolation", "host mutation", "generated-C runtime failed", "-Werror"
) ) {
    if ( ! $externGlobalBoundaryGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the verified ExternGlobal ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_null_abi_boundary.xl", "xlir_nullable_scalar.xl", "xlir_nullable_closure_capture.xl", "xlir_nullable_cell_capture.xl", "xlir_nullable_async.xl", "xlir_nullable_async_continuation.xl", "xlir_nullable_class_field.xl", "xlir_nullable_sentinel_coalesce.xl", "xlir_nullable_sentinel_assign_if_null.xl", "xlir_nullable_index_assign_if_null.xl", "xlir_nullable_index_assign_if_null_may_raise.xl", "nullAbi",
    "cabi=36 callable=invalid ownership=1 null=3", "nullable-wrap", "nullable-unwrap",
    "nullable xvalue/raw-pointer sentinels", "tagged bool/int/u64/float payloads",
    "explicit class wrap", "canonical may-raise carriers", "xvalue-sentinel var/class lazy coalesce and null-aware assignment", "static non-null ?= RHS elimination", "borrowed index current with one-shot key, protected address, raising lazy RHS and consuming replacement", "immutable closure capture/result", "mutable capture cells", "async result/frame copy", "typed continuation coalesce", "tagged class-field initialization/box/projection lifecycle", "JSON/text IR",
    "generated-C runtime failed", "-Werror"
) ) {
    if ( ! $nullAbiBoundaryGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the contextual null representation ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_callsite_binding.xl", "call_defaults.xl", "functions/constructors",
    "explicit/default", "canonical parameter order", "JSON CallSite", "-Werror"
) ) {
    if ( ! $callSiteBindingGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the fixed/default/named CallSite ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_callsite_variadic.xl", "cgen_extern_xvalue_vararg.xl",
    "array-vararg-item", "dict-vararg-item", "empty aggregates", "-Werror"
) ) {
    if ( ! $callSiteVariadicGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the ARRAY/DICT variadic CallSite ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_callsite_c_vararg.xl", "c-vararg-item", "extern int xlir_c_vararg_sum(const char*, ...);",
    "va_list runtime", "native operands", "-Werror"
) ) {
    if ( ! $callSiteCVarargGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the native C variadic CallSite ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_callsite_generic_specialization.xl", "generic-instance source=",
    "cloned arguments", "retargeted site", "JSON/text IR", "-Werror",
    "generated-C runtime failed"
) ) {
    if ( ! $callSiteGenericGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the generic-specialization CallSite ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_protocol_ops.xl", "cgen_protocol_complex_receiver.xl",
    "xlir_class_protocol_syntax_generic.xl", "cgen_protocol_overload_index.xl",
    "operator/contains/call", "implicit-string", "__setItem__",
    "materialize/call/release ownership lowering", "-Werror",
    "generated-C runtime failed"
) ) {
    if ( ! $classProtocolSyntaxGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the class protocol syntax CallSite gate: $required"
    }
}
foreach ( $required in @(
    "cgen_native_class.xl", "managed-object ABI", "native-class-field-take",
    "native-class-handle-borrow", "class-field-owned-raw-pointer-borrow",
    "xvalue-object-get-literal-native-handle-borrow", "lifecycle", "-Werror",
    "generated-C runtime failed"
) ) {
    if ( ! $nativeNominalReceiverGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the native nominal receiver ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_native_class_bind_point.xl", "cgen_native_class_bind_point_basic.xl",
    "nativeHandleBinds", "Typed direct/managed-field TAKE ABI", "source clearing",
    "temporary owner", "destroy-directed ownership", "-Werror", "runtime failed"
) ) {
    if ( ! $nativeHandleBindGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the native handle bind/take ownership ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_native_class_bind_point.xl", "nativeHandleBinds", "xlir_native_take_helper.h",
    "xlir_native_take_reentry_host.c", "xlir_native_take_reentry_runtime.c",
    "xlir_native_take_reentry_smoke.c", "-Werror", "--smoke", "standalone AOT"
) ) {
    if ( ! $nativeTakeReentryGateText.Contains($required) ) {
        throw "XLIR native TAKE ownership requires allocation-failure/reentry coverage: $required"
    }
}
foreach ( $required in @(
    "foreign-result-rollback", "xlir_foreign_result_rollback.xl",
    "xlir_foreign_result_rollback_host.c", "xlir_foreign_result_rollback_runtime.c",
    "xlir_foreign_result_rollback_contract.c", "-Werror", "--smoke", "standalone AOT"
) ) {
    if ( ! $foreignResultRollbackGateText.Contains($required) ) {
        throw "XLIR native factory ownership requires failed-result rollback coverage: $required"
    }
}
foreach ( $required in @(
    "xlir_export_transaction.xl", "bodyErrorAbi", "errorAbi",
    "xlir_export_transaction_host.c", "xlir_export_transaction_runtime.c",
    "xlir_export_transaction_contract.c", "xlir_export_transaction_header_host.c",
    "-Werror", "--smoke", "standalone AOT"
) ) {
    if ( ! $exportTransactionGateText.Contains($required) ) {
        throw "XLIR exports require diagnostic-independent transaction coverage: $required"
    }
}
foreach ( $required in @(
    "xlir_error_unwind.xl", "error-unwind-begin", "error-unwind-end", "errorScope",
    "xlir_error_unwind_host.c", "xlir_error_unwind_runtime.c", "xlir_error_unwind_contract.c",
    "-Werror", "--smoke", ".aot.exe"
) ) {
    if ( ! $errorUnwindGateText.Contains($required) ) {
        throw "XLIR exceptional cleanup requires first-error and region validation: $required"
    }
}
foreach ( $required in @(
    "cgen_native_class_property.xl", "property-get", "property-set",
    "parameterTypes", "parameterAbis", "parameterPhysicalAbis",
    "native-class-handle-borrow", "native_property_get_cell",
    "native_property_set_cell", "direct member/single-index/multi-index CALLs",
    "managed-field physical ABI", "deterministic strict-C", "-Werror"

) ) {
    if ( ! $resolvedPropertyCallGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the resolved native property CallTarget ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_value_initializer_defaults.xl", "xlir_value_initializer_class_default.xl",
    "xlir_value_initializer_class_null.xl",
    "xlir_value_initializer_containers.xl", "xlir_value_initializer_package_exports.xl",
    "cgen_native_class_ownership.xl",
    "cgen_native_class_null_destroy.xl", "function_types.xl",
    "Typed explicit/default ABI",
    "scalar-zero", "string-empty", "null-sentinel", "xvalue-container-empty",
    "xvalue-int-map-new", "xvalue-object-new", "module package exports",
    "explicit-constructor class-null semantics", "destroy-aware release",
    "representation-specific callable nulls", "xvalue-null",
    "-Werror", "runtime failed"
) ) {
    if ( ! $valueInitializerGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical ValueInitializer ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_callable_reachability.xl", "reachability -ne 'unreachable'",
    "xlir_reachability_dead_external", "xl6TextCountV1",
    "virtual-call", "protocol-call", "xlir_async_start_",
    "generated-C runtime failed", "-Werror"
) ) {
    if ( ! $reachabilityGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the callable reachability gate: $required"
    }
}
foreach ( $required in @(
    "cgen_multi_return.xl", "cgen_class_method_multi_return.xl",
    "xlir_product_closure_result.xl", "xlir_product_generic_result.xl",
    "xlir_product_declaration_only.xl", "xlir_managed_product_result.xl",
    "callableResultType", "product-make", "product-get",
    "product-take", "product-drop", "-Werror", "runtime failed"
) ) {
    if ( ! $productResultGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the canonical product-result ABI gate: $required"
    }
}
foreach ( $required in @(
    "export_class.xl", "xlir_export_nominal_overload.xl",
    "returnAbiName", "returnValueAbi", "parameterAbis",
    "parameterValueAbis", "parameterNames", "borrow", "owned",
    "IR-derived headers", "generated-C runtime failed", "-Werror"
) ) {
    if ( ! $exportBoundaryGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the verified public export ABI gate: $required"
    }
}
foreach ( $required in @(
    "export_primitive_widths.xl", 'name -eq "float32"', "xlir_result_f32",
    "parameterValueAbis", "parameterNames", "int8_t i8", "float f32",
    "(int64_t)xlir_input_0", "public header", "-Werror"
) ) {
    if ( ! $exportPrimitiveWidthGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the language F32/export fixed-width ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_copy_clone.xl",
    "cgen_generic_class_copy_clone.xl",
    "cgen_generic_class_custom_copy_clone.xl",
    "cgen_class_copy.xl",
    "cgen_class_custom_copy_clone.xl",
    "copy=runtime-shallow/s0 clone=runtime-deep/s0",
    "copy=internal-call/s2 clone=internal-call/s3",
    "init=scalar-zero",
    "runtime=xvalue-clone",
    "runtime=xvalue-deep-clone",
    "classLayouts",
    "classFields",
    "DynamicTargets",
    "ownerDeclaration",
    "cleanup actions have no explicit callable owner",
    "= xlir_fn_2(",
    "-Werror",
    "generated-C runtime failed"
) ) {
    if ( ! $copyCloneGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the copy/clone ownership and runtime ABI gate: $required"
    }
}
foreach ( $required in @(
    "check_persistence_project_language.py", "--fixture abi", "PublicDirectory", "--out"

) ) {
    if ( ! $persistenceJsonGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the JSON persistence schema/reconstruction ABI gate: $required"
    }
}
foreach ( $required in @(
    "cgen_function_direct_owned_args.xl",
    "hir_closure_for_capture.xl", "hir_closure_for_named_typed_array.xl",
    "hir_closure_for_string_capture.xl", 'sourceKindName -ne "for_targets"',
    "lexical for-target frame transfers",
    "callable=fixed", "name=function",
    "JSON type/storage table",
    "kind=temporary",
    "runtime=xvalue-retain",
    "binary opcode=add",
    "generated-C runtime failed",
    "-Werror"
) ) {
    if ( ! $functionValueOwnershipGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the function-value ownership/runtime gate: $required"
    }
}
foreach ( $required in @(
    "cgen_function_bare_call.xl", "callableErasureAbiName",
    "array-variadic", "xrtValueArrayAppend(", "deterministic", "-Werror"
) ) {
    if ( ! $openFunctionGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the open-function callable runtime gate: $required"
    }
}
foreach ( $required in @(
    "xlir_foreign_function_value_abi.xl",
    "parameterPhysicalAbis", "language-function-value",
    "raw-pointer-null-convert", "XL6_Function",
    "closure adapter invocation", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $foreignFunctionValueGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the foreign function-value physical ABI gate: $required"
    }
}
foreach ( $required in @(
    "c_abi_types.xl",
    'kindName -eq "scalar-convert"', 'opcodeName -eq "none"',
    'runtimeOpName -eq "none"',
    "extern unsigned int xl6_c_echo_uint(unsigned int);",
    "extern uintptr_t xl6_c_echo_uintptr(uintptr_t);",
    "(unsigned int)",
    "-Werror",
    "generated-C runtime gate failed"
) ) {
    if ( ! $cScalarAbiGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the exact foreign C scalar runtime ABI gate: $required"
    }
}
foreach ( $required in @(
    "xlir_async_class_method.xl",
    "async_generic_class_method.xl",
    "async_class_receiver_lifetime.xl",
    "cgen_async_class_conditional_await.xl",
    "cgen_async_param_ownership.xl",
    "cgen_async_string_ownership.xl",
    "--lowered",
    "fn[0-9]+ kind=async-worker",
    "worker/effect/result-carrier ABI",
    "kind=temporary",
    "xrtValueRetain(a0)",
    "XL6_CAPTURE_XVALUE",
    "XL6_CAPTURE_CLOSURE",
    "xl6TaskRuntimeV2Get()->Cleanup",
    "xlir_async_pool_submit_language",
    "xlir_result_future",
    "retain opcode=none runtime=none",
    "await on null future",
    'xlir_error_create_at("StateError"',
    "xvalue-to-string",
    "xlir_closure_retain(a2)",
    "bypasses the verified capture runtime",
    "bypasses the verified capture/payload runtime",
    "runtime=string-concat",
    "repeated-await IR",
    "generic-instance source=n16",
    "-Werror",
    "generated-C runtime gate failed"
) ) {
    if ( ! $asyncClassMethodGateText.Contains($required) ) {
        throw "XLIR migration matrix requires the async class-method receiver/frame gate: $required"
    }
}
foreach ( $name in $runtimeContracts ) {
    if ( ! $names.Contains($name) ) {
        throw "XLIR runtime contract fixture is missing: $name"
    }
    $runtimeGateText = if ( $name -eq "xlir_return_commit" ) {
        $returnCommitGateText
    } elseif ( $name -eq "xlir_closure_argument_conversion" ) {
        $closureArgumentConversionGateText
    } elseif ( $name -in @("xlir_json_value_bridge", "xlir_json_valid_only") ) {
        $jsonValueGateText
    } elseif ( $name -in @("xlir_json_writer_bridge", "xlir_json_visit_bridge") ) {
        $jsonStreamGateText
    } elseif ( $name -in @("xlir_xlon_value_bridge", "xlir_xlon_valid_only") ) {
        $xlonValueGateText
    } elseif ( $name -in @("xlir_xlon_writer_bridge", "xlir_xlon_writer_type_only", "xlir_xlon_writer_factory_only", "xlir_xlon_visit_bridge") ) {
        $xlonStreamGateText
    } elseif ( $name -like "xlir_host_error_boundary_*" ) {
        $hostErrorBoundaryGateText
    } elseif ( $name -like "xlir_serialization_callback_*" ) {
        $serializationCallbackGateText
    } elseif ( $name -like "xlir_text_output_*" ) {
        $textOutputGateText
    } elseif ( $name -in $matrix["direct directory and process bridge ABI"] ) {
        $systemBridgeGateText
    } elseif ( $name -in $matrix["statically shaped erased collection member ABI"] ) {
        $erasedCollectionMemberGateText
    } elseif ( $name -eq "xlir_persistence_projection_roots" ) {
        $persistenceProjectionRootGateText
    } elseif ( $name -eq "cgen_function_bare_call" ) {
        $openFunctionGateText
    } elseif ( $name -eq "hir_closure_for_capture" -or
               $name -eq "hir_closure_for_named_typed_array" -or
               $name -eq "hir_closure_for_string_capture" ) {
        $functionValueOwnershipGateText
    } else {
        $runtimeMatrixText
    }
    if ( ! $runtimeGateText.Contains("$name.xl") -and
         !(($name -like "xlir_json_*" -or $name -like "xlir_xlon_*" -or $name -like "xlir_host_error_boundary_*" -or $name -like "xlir_serialization_callback_*" -or $name -like "xlir_text_output_*" -or $name -in $matrix["direct directory and process bridge ABI"] -or $name -in $matrix["statically shaped erased collection member ABI"]) -and $runtimeGateText.Contains('"' + $name + '"')) ) {
        throw "XLIR runtime contract gate does not execute: $name"
    }
}
foreach ( $name in $xlirOnlyRuntimeContracts ) {
    if ( ! $names.Contains($name) ) {
        throw "XLIR-only runtime contract fixture is missing: $name"
    }
    $runtimeGateText = if ($name -in @("xlir_dynamic_member_set", "xlir_dynamic_bound_method", "xlir_dynamic_member_get", "xlir_property_assignment_transaction", "xlir_virtual_call_outcome", "xlir_virtual_activation", "xlir_owned_activation", "xlir_module_task_pool", "xlir_protocol_call_outcome", "xlir_callsite_evaluation", "xlir_indexed_accessor", "xlir_property_update", "xlir_storage_assignment_transaction", "xlir_member_assignment_transaction", "xlir_index_conditional_assign", "xlir_storage_conditional_assign", "xlir_nullable_storage_assign_if_null")) {
        $conditionalName = if ($name -eq "xlir_nullable_storage_assign_if_null") { "xlir_storage_conditional_assign" } else { $name }
        $conditionalGate = [IO.File]::ReadAllText((Join-Path $Root "tools/check_$conditionalName.ps1"))
        if (!$conditionalGate.Contains("test/cases/$($conditionalName)_contract.c")) {
            throw "Conditional source contract is not compiled by its runtime gate: $name"
        }
        $conditionalGate + [IO.File]::ReadAllText((Join-Path $caseDir "$($conditionalName)_contract.c"))
    } elseif ( $name -eq "xlir_class_cleanup_outcome" ) {
        $classCleanupGateText
    } elseif ( $name -eq "xlir_scope_exit_plan" ) {
        $scopeExitPlanGateText
    } elseif ( $name -eq "xlir_scope_cleanup_outcome" ) {
        $scopeCleanupOutcomeGateText
    } elseif ( $name -eq "xlir_abrupt_cleanup_outcome" ) {
        $abruptCleanupGateText
    } elseif ( $name -eq "xlir_parameter_cleanup_outcome" ) {
        $parameterCleanupGateText
    } elseif ( $name -eq "xlir_loop_cleanup_outcome" ) {
        $loopCleanupGateText
    } elseif ( $name -in @("xlir_pending_exception_outcome", "xlir_pending_exception_control") ) {
        $pendingExceptionRouteGateText
    } elseif ( $name -eq "xlir_return_projection_owner" ) {
        $returnProjectionGateText
    } elseif ( $name -eq "xlir_conditional_nullable_join" ) {
        $conditionalNullableJoinGateText
    } elseif ( $name -eq "xlir_managed_storage_inputs" ) {
        $managedStorageInputsGateText + $ownerConsumptionGateText
    } elseif ( $name -eq "xlir_xcall_result_failure" ) {
        $xcallResultFailureGateText
    } elseif ( $name -eq "xlir_cleanup_detach_reentry" ) {
        $cleanupDetachGateText
    } elseif ( $name -in @("xlir_foreign_scope_nested", "xlir_foreign_raw_callback_error") ) {
        $foreignErrorScopeGateText
    } elseif ( $name -eq "xlir_closure_argument_evaluation_order" ) {
        $closureArgumentConversionGateText
    } elseif ( $name -eq "xlir_export_callback_lifetime" ) {
        $exportCallbackGateText
    } elseif ( $name -in @("xlir_module_finalization", "xlir_module_native_finalization", "xlir_module_scalar_finalization", "xlir_module_finalization_cli", "xlir_module_finalization_cli_error") ) {
        $moduleFinalizationGateText
    } elseif ( $name -eq "xlir_binary_operand_lifetime" ) {
        $binaryOwnerGateText
    } elseif ( $name -eq "xlir_membership_operand_lifetime" ) {
        $membershipOwnerGateText
    } elseif ( $name -eq "xlir_named_callable_owner_lifetime" ) {
        $namedOwnerGateText
    } elseif ( $name -in @("xlir_open_object_field_lifetime", "xlir_refcounted_storage_assignment_lifetime") ) {
        $openObjectFieldGateText
    } elseif ( $name -eq "xlir_core_call_value_lifetime" ) {
        $evaluationLifetimeGateText
    } elseif ( $name -eq "xlir_dynamic_compare_contract" ) {
        $dynamicCompareGateText
    } elseif ( $name -in @("xlir_dir_empty_bridge", "xlir_dir_count_bridge", "xlir_dir_size_bridge") ) {
        $systemBridgeGateText
    } elseif ( $name -eq "xlir_json_visit_capture_lifetime" ) {
        $jsonStreamGateText
    } elseif ( $name -eq "xlir_erased_collection_member_lifetime" ) {
        $erasedCollectionMemberGateText
    } elseif ( $name -in @("xlir_serialization_callback_lifetime", "xlir_serialization_callback_async_error", "xlir_serialization_callback_generator_error") ) {
        $serializationCallbackGateText
    } elseif ( $name -eq "xlir_xlon_visit_capture_lifetime" ) {
        $xlonStreamGateText
    } elseif ( $name -eq
            "xlir_persistence_recursive_collection_values" ) {
        $recursiveCollectionPersistenceGateText
    } elseif ( $name -eq "xlir_typed_collection_methods" ) {
        $collectionMemberGateText
    } elseif ( $name -eq "xlir_index_write_abi" ) {
        $indexWriteAbiGateText
    } elseif ( $name -eq "xlir_xvalue_array_iterator_projection" ) {
        $arrayIteratorProjectionGateText
    } elseif ( $name -eq "xlir_dynamic_typed_collection_projection" -or
               $name -eq "cgen_dynamic_to_typed_containers" ) {
        $collectionTypeRebindGateText
    } elseif ( $name -eq "xlir_generator_string_machine" ) {
        $generatorMachineGateText
    } elseif ( $name -eq "xlir_checked_float_string" ) {
        $checkedFloatConversionGateText
    } elseif ( $name -eq "xlir_checked_time_conversion" ) {
        $checkedTimeConversionGateText
    } elseif ( $name -eq "xlir_bool_conversion_abi" ) {
        $boolConversionAbiGateText
    } elseif ( $name -eq "xlir_checked_cast_abi" ) {
        $checkedCastAbiGateText
    } elseif ( $name -eq "xlir_nullable_cast_abi" ) {
        $nullableCastAbiGateText
    } elseif ( $name -eq "xlir_nominal_cast_abi" ) {
        $nominalCastAbiGateText
    } elseif ( $name -eq "xlir_dynamic_conditional_truth" ) {
        $dynamicConditionalTruthGateText
    } elseif ( $name -eq "xlir_erased_array_append" ) {
        $erasedArrayAppendGateText
    } elseif ( $name -eq "xlir_checked_integer_compound_error_abi" ) {
        $checkedIntegerArithmeticAbiGateText
    } elseif ( $name -eq "xlir_foreign_function_value_abi" ) {
        $foreignFunctionValueGateText
    } elseif ( $name -eq "xlir_managed_native_explicit_destroy" ) {
        $managedNativeLifecycleGateText
    } elseif ( $name -eq "xlir_xcall_nullable_closure" ) {
        $xcallGateText
    } elseif ( $name -eq "xlir_async_error_abi" -or
               $name -eq "xlir_async_error_race_abi" ) {
        $asyncErrorAbiGateText
    } elseif ( $name -eq "xlir_task_nested_future_abi" ) {
        $taskNestedFutureGateText
    } else {
        $nullAbiBoundaryGateText
    }
    if ( ! $runtimeGateText.Contains("$name.xl") -and
         !($name -in @("xlir_dir_empty_bridge", "xlir_dir_count_bridge", "xlir_dir_size_bridge", "xlir_json_visit_capture_lifetime", "xlir_xlon_visit_capture_lifetime", "xlir_serialization_callback_lifetime", "xlir_serialization_callback_async_error", "xlir_serialization_callback_generator_error", "xlir_erased_collection_member_lifetime") -and $runtimeGateText.Contains('"' + $name + '"')) ) {
        throw "XLIR dedicated gate does not execute XLIR-only contract: $name"
    }
}

foreach ( $gate in @(
    @{ Text = $systemBridgeGateText; Required = @("runtimeBridgeName", "parameterAbis", "parameterPhysicalAbis", "no-raise", "xlir_process_cleanup_host.c", "-Werror", "--backend xlir") },
    @{ Text = $textOutputGateText; Required = @("parameterAbis", "parameterPhysicalAbis", "no-raise", "void", "Output", "wrong_input", "-Werror", '@("xlir")') },
    @{ Text = $erasedCollectionMemberGateText; Required = @("runtimeTypeMap.fallback", "typedCollectionOpName", "resultAbiName", "temporary", "continuation", "-Werror", '--backend xlir') },
    @{ Text = $serializationCallbackGateText; Required = @("errorAbiName", "errorPhysicalAbiName", "result-carrier", "machineOps", "Marker", "-Werror", '@("xlir")') },
    @{ Text = $hostErrorBoundaryGateText; Required = @("errorAbiName", "xlir_module_init", "result.ok", "Marker", "-Werror", '@("xlir")') },
    @{ Text = $xlonValueGateText; Required = @("runtimeBridgeName", "parameterAbis", "parameterPhysicalAbis", "wrong_dom", "missing_option", "-Werror", '@("xlir")') },
    @{ Text = $xlonStreamGateText; Required = @("native-class-field-take", "resultPhysicalDestroySymbol", "language-function-value", "wrong_callback_result", "-Werror", "XLIR ownership contract") },
    @{ Text = $jsonValueGateText; Required = @("runtimeBridgeName", "parameterAbis", "parameterPhysicalAbis", "wrong_dom", "missing_pretty", "-Werror", '@("xlir")') },
    @{ Text = $jsonStreamGateText; Required = @("native-class-field-take", "resultPhysicalDestroySymbol", "language-function-value", "wrong_callback_result", "-Werror", "XLIR ownership contract") },
    @{ Text = $nativeFactoryDeclarationGateText; Required = @("xlir_native_factory_declaration.xl", "unreachable", "native-class-field-take", "resultPhysicalDestroySymbol", "-Werror", "--backend xlir") }
) ) {
    foreach ( $required in $gate.Required ) {
        if ( !$gate.Text.Contains($required) ) { throw "XLIR JSON/native declaration gate lost explicit contract evidence: $required" }
    }
}

foreach ($required in @('xlir_exit_route.xl', 'exitSteps', 'failureTarget', 'firstStep', 'stepCount',
    'xlir_exit_route_contract.c', '-Werror', 'does not certify CFG cleanup outcomes')) {
    if (!$exitRouteGateText.Contains($required)) { throw "XLIR exit route topology gate lost contract evidence: $required" }
}
foreach ($required in @('--array-only', '-std=c11', '-Wall', '-Wextra', '-Werror')) {
    if (!$arrayLanguageLifetimeGateText.Contains($required)) {
        throw "XLIR array language lifetime worker lost its strict-C contract: $required"
    }
}
$rows = $matrix.Count
$matrixFixtures = @($matrix.Values | ForEach-Object { $_ } | Select-Object -Unique).Count
Write-Output "XLIR migration coverage ok: $rows semantic rows, $matrixFixtures representative fixtures, $($allXlirFixtures.Count) all-fixture deterministic candidates, $($runtimeContracts.Count) canonical runtime contracts, $($xlirOnlyRuntimeContracts.Count) explicit XLIR-only runtime contracts."
exit 0
