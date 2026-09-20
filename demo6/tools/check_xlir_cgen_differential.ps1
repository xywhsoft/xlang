param(
    [string]$Root = "",
    [string]$StartAt = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$generated = Join-Path $Root "build\test\xlir_cgen_differential_generated.c"
$fixtures = @(
    @{ Name = "scalar ABI"; Source = "test\cases\xlir_cgen_differential_scalar.xl"; Host = "test\cases\xlir_cgen_differential_scalar_host.c" },
    @{ Name = "exported primitive-width and F32 value ABI"; Source = "test\cases\export_primitive_widths.xl"; Host = "test\cases\export_primitive_widths_host.c"; SeparateGenerated = $true; LegacyRuntimeInit = $true },
    @{ Name = "float-to-integer scalar conversion ABI"; Source = "test\cases\xlir_cgen_differential_scalar_conversion.xl"; Host = "test\cases\xlir_cgen_differential_scalar_conversion_host.c" },
    @{ Name = "explicit checked and static wrapping integer conversion ABI"; Source = "test\cases\xlir_integer_conversion_source_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "narrow integer storage normalization ABI"; Source = "test\cases\type_int_narrowing_wrap.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical scalar conversion family ABI"; Source = "test\cases\xlir_scalar_conversion_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "explicit dynamic unary operation ABI"; Source = "test\cases\xlir_dynamic_unary_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "checked extended dynamic binary operation ABI"; Source = "test\cases\xlir_dynamic_binary_extended_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "checked scalar integer arithmetic effect ABI"; Source = "test\cases\xlir_checked_integer_arithmetic_abi.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "exact no-raise i64 power ABI"; Source = "test\cases\cgen_pow.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "checked integer async/generator continuation ABI"; Source = "test\cases\xlir_checked_integer_continuation_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "distinct time conversion ABI"; Source = "test\cases\xlir_time_conversion_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_time_conversion_abi_support.c"; XlirSupport = "test\cases\xlir_time_conversion_abi_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "exact no-raise time approximate ABI"; Source = "test\cases\cgen_time_literals.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "special f64 constant and stdlib math ABI"; Source = "test\cases\stdlib_math.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "managed error ABI"; Source = "test\cases\xlir_cgen_differential_managed_error.xl"; Host = "test\cases\xlir_cgen_differential_managed_error_host.c" },
    @{ Name = "immutable Error source-location and member ABI"; Source = "test\cases\xlir_error_location_abi.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "xvalue conditional scalar ABI"; Source = "test\cases\xlir_cgen_differential_xvalue_conditional.xl"; Host = "test\cases\xlir_cgen_differential_xvalue_conditional_host.c" },
    @{ Name = "tagged nullable scalar ABI"; Source = "test\cases\xlir_nullable_scalar.xl"; Host = "test\cases\xlir_nullable_scalar_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "xvalue-sentinel nullable coalesce ABI"; Source = "test\cases\xlir_nullable_sentinel_coalesce.xl"; Host = "test\cases\xlir_nullable_sentinel_coalesce_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "xvalue indexed null-aware assignment ABI"; Source = "test\cases\xlir_nullable_index_assign_if_null.xl"; Host = "test\cases\xlir_nullable_index_assign_if_null_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "indexed null-aware assignment raising RHS"; Source = "test\cases\xlir_nullable_index_assign_if_null_may_raise.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "pointer-sentinel nullable-string lifecycle ABI"; Source = "test\cases\nullable_string_lifecycle.xl"; Host = "test\cases\xlir_class_static_storage_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "safe nullable-collection integer index ABI"; Source = "test\cases\xlir_safe_index_nullable_collection_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "safe typed/dynamic collection count ABI"; Source = "test\cases\cgen_safe_member.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "safe nullable declared-class field ABI"; Source = "test\cases\nullable_safe_navigation.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "typed dict literal-key projection and safe-index ABI"; Source = "test\cases\xlir_typed_dict_literal_index_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "xvalue dynamic-key borrowed index ABI"; Source = "test\cases\xlir_xvalue_dynamic_key_index.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "xvalue dynamic-key consuming index write ABI"; Source = "test\cases\xlir_xvalue_dynamic_key_index_set.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "typed dict scalar boxing write ABI"; Source = "test\cases\xlir_typed_dict_scalar_set.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "typed raw-pointer collection index projection ABI"; Source = "test\cases\cgen_typed_point.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "xvalue scalar bridge, flow projection, and may-raise constructor ABI"; Source = "test\cases\hir_xvalue_scalar_bridge_matrix.xl"; Host = "test\cases\hir_xvalue_scalar_bridge_matrix_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "canonical typed index write ABI"; Source = "test\cases\xlir_index_write_generic_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "owned expression-result discard ABI"; Source = "test\cases\xlir_owned_expression_discard.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "owned collection expression-result discard ABI"; Source = "test\cases\cgen_array_literal_expr_elements.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "xvalue collection membership ABI"; Source = "test\cases\xlir_xvalue_membership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "managed string membership ABI"; Source = "test\cases\xlir_string_membership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "managed string UTF-8 length member ABI"; Source = "test\cases\xlir_member_abi_string_length.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical UTF-8 string iterator ABI"; Source = "test\cases\xlir_string_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "typed nullable erased-container projection ABI"; Source = "test\cases\typed_nullable_container_matrix.xl"; Host = "test\cases\xlir_typed_nullable_container_matrix_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "canonical class length protocol member ABI"; Source = "test\cases\xlir_length_protocol_member.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\xlir_length_protocol_member_host.c"; XlirSupport = "test\cases\xlir_length_protocol_member_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "canonical scalar/time/raw/native class-field projection ABI"; Source = "test\cases\xlir_class_field_member_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\xlir_class_field_member_abi_host.c"; XlirSupport = "test\cases\xlir_class_field_member_abi_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "inherited and flow-refined class-field projection ABI"; Source = "test\cases\xlir_class_field_inherited_narrowed_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "mixed string and xvalue container membership ABI"; Source = "test\cases\cgen_in_containers.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical range descriptor membership ABI"; Source = "test\cases\xlir_range_membership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "established range membership ABI"; Source = "test\cases\cgen_in_range.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical range-for target and zero-step error ABI"; Source = "test\cases\xlir_range_for_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical stable array<i64> for snapshot ABI"; Source = "test\cases\xlir_array_i64_for_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical xvalue list<i64> snapshot iterator ABI"; Source = "test\cases\xlir_list_i64_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical xvalue set<i64> snapshot iterator ABI"; Source = "test\cases\xlir_set_i64_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical xvalue scalar iterator projection ABI"; Source = "test\cases\xlir_xvalue_iterator_scalar_projection.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "managed list/set iterator target lifetime ABI"; Source = "test\cases\xlir_xvalue_iterator_managed_projection.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    # The complete dict projection fixture keeps full-width dict<uint64> as an
    # XLIR-only gate because legacy boxes that element through signed xvalue.
    # This shared fixture preserves the key/item, managed-lifetime and finally
    # semantics on the exact representations both backends implement.
    @{ Name = "dict key/item iterator projection ABI"; Source = "test\cases\xlir_dict_iterator_projection_differential.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "dict key-only iterator projection ABI"; Source = "test\cases\xlir_dict_key_only_iterator.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "statically shaped erased collection iterator ABI"; Source = "test\cases\xlir_erased_collection_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "runtime-checked dynamic var iterator ABI"; Source = "test\cases\xlir_dynamic_var_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    # Legacy historically boxes U64 through signed xvalue storage.  Keep the
    # cross-backend observable fixture inside the shared exact range; the U64
    # ABI gate separately proves full-width tags and modulo-2^64 boundaries.
    @{ Name = "dynamic U64 arithmetic shared-range ABI"; Source = "test\cases\xlir_dynamic_u64_arithmetic.xl"; Host = "test\cases\xlir_dynamic_u64_arithmetic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "canonical Future cancellation intrinsic ABI"; Source = "test\cases\xlir_future_cancel.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "explicit Future wait/status member ABI"; Source = "test\cases\xlir_future_member_status_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true; XlirXlangLinks = $true },
    @{ Name = "flattened literal task.whenAny index ABI"; Source = "test\cases\xlir_task_when_any_literal_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed-array task.whenAny retained Future ABI"; Source = "test\cases\xlir_task_when_any_array_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed-array task.all/task.race aggregate ABI"; Source = "test\cases\xlir_task_aggregate_array_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true; XlirXlangLinks = $true },
    @{ Name = "typed task.spawn closure/frame ABI"; Source = "test\cases\xlir_task_spawn_closure_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed thread resource identity/capture ABI"; Source = "test\cases\xlir_thread_resource_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed channel resource/payload/storage ABI"; Source = "test\cases\xlir_channel_resource_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; LegacyAllowWarnings = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed mutex/sem/cond/rwlock resource ABI"; Source = "test\cases\cgen_sync.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; LegacyAllowWarnings = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed concurrent queue resource/payload/capture ABI"; Source = "test\cases\stdlib_special_collections_concurrent.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; LegacyAllowWarnings = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed Stack resource/payload/class-field/capture ABI"; Source = "test\cases\xlir_stack_resource_abi.xl"; Host = "test\cases\xlir_stack_resource_abi_host.c"; LegacyStandalone = $true; LegacyAllowWarnings = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed AvlTree key/value/order/class-field/capture ABI"; Source = "test\cases\xlir_avl_resource_abi.xl"; Host = "test\cases\xlir_avl_resource_abi_host.c"; LegacyStandalone = $true; LegacyAllowWarnings = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed Future then/catch/finally continuation ABI"; Source = "test\cases\stdlib_future_continuation.xl"; Host = "test\cases\xlir_future_continuation_abi_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed callable index projection ABI"; Source = "test\cases\xlir_typed_callable_index_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true; XlirXlangLinks = $true },
    @{ Name = "typed native-handle index projection ABI"; Source = "test\cases\xlir_typed_native_handle_index_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed native-handle iterator projection ABI"; Source = "test\cases\xlir_typed_native_handle_iterator_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "async ready-Future ABI"; Source = "test\cases\xlir_async_ready_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_scalar_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_scalar_legacy_support.c" },
    @{ Name = "async ready-Future local dynamic ABI"; Source = "test\cases\xlir_await_future_xvalue.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_xvalue_legacy_support.c" },
    @{ Name = "async generic expression planner ABI"; Source = "test\cases\xlir_async_generic_expression_planner.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\xlir_async_generic_expression_planner_support.c"; XlirSupport = "test\cases\xlir_async_generic_expression_planner_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "async scalar mixed-capture ABI"; Source = "test\cases\xlir_async_future_scalar_mixed_capture_chain.xl"; Host = "test\cases\xlir_cgen_differential_async_scalar_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_scalar_legacy_support.c" },
    @{ Name = "async dynamic continuation ABI"; Source = "test\cases\xlir_async_future_xvalue_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_xvalue_legacy_support.c" },
    @{ Name = "async dynamic local-parameter ABI"; Source = "test\cases\xlir_async_future_xvalue_local_parameter_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_xvalue_legacy_support.c" },
    @{ Name = "async extern Future capture ABI"; Source = "test\cases\xlir_async_future_direct_call_capture.xl"; Host = "test\cases\xlir_async_future_direct_call_capture_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_direct_call_legacy_support.c"; GeneratedInclude = "build\test\xlir_async_future_direct_call_capture_generated.c" },
    @{ Name = "async extern Future string-capture ABI"; Source = "test\cases\xlir_async_future_direct_string_capture.xl"; Host = "test\cases\xlir_async_future_direct_string_capture_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_direct_call_legacy_support.c"; GeneratedInclude = "build\test\xlir_async_future_direct_string_capture_generated.c" },
    @{ Name = "async extern Future dynamic-capture ABI"; Source = "test\cases\xlir_async_future_direct_xvalue_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_xvalue_legacy_support.c" },
    @{ Name = "async extern Future forward ABI"; Source = "test\cases\xlir_async_future_direct_call.xl"; Host = "test\cases\xlir_async_future_direct_call_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_async_direct_call_legacy_support.c"; GeneratedInclude = "build\test\xlir_async_future_direct_call_generated.c" },
    @{ Name = "async managed/dynamic string ownership ABI"; Source = "test\cases\cgen_async_string_ownership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "function-value dynamic borrow and compound assignment ABI"; Source = "test\cases\cgen_function_direct_owned_args.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "function-value byref/out storage-reference ABI"; Source = "test\cases\cgen_function_param_modes.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "runtime callable xcall ABI"; Source = "test\cases\cgen_extern_function_value.xl"; Host = "test\cases\xlir_xcall_i64_host.c"; LegacySupport = "test\cases\xlir_xcall_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "runtime callable scalar ABI"; Source = "test\cases\xlir_xcall_scalar_values.xl"; Host = "test\cases\xlir_xcall_scalar_host.c"; LegacySupport = "test\cases\xlir_xcall_scalar_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "managed product field ownership ABI"; Source = "test\cases\xlir_managed_product_result.xl"; Host = "test\cases\xlir_managed_product_differential_host.c"; LegacySupport = "test\cases\xlir_managed_product_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "owned empty-container default initializer ABI"; Source = "test\cases\definite_assignment_default_values.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "scalar/time/union class-field constructor initializer ABI"; Source = "test\cases\xlir_value_initializer_class_default_differential.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "declaration-driven implicit class receiver ABI"; Source = "test\cases\xlir_class_implicit_receiver.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "base receiver direct dispatch ABI"; Source = "test\cases\cgen_base_method_static_dispatch.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "base receiver typed call ABI"; Source = "test\cases\cgen_base_method_typed_hir.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "implicit class-field namespace shadow ABI"; Source = "test\cases\cgen_stdlib_namespace_shadow.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "class operator/contains/call syntax CallSite ABI"; Source = "test\cases\cgen_protocol_ops.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "class rvalue item/field/call syntax CallSite ABI"; Source = "test\cases\cgen_protocol_complex_receiver.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "generic class field syntax specialization ABI"; Source = "test\cases\xlir_class_protocol_syntax_generic.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "generic class method and operator/index protocol specialization ABI"; Source = "test\cases\cgen_class_generic_handle.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "overloaded class index getter/setter CallSite ABI"; Source = "test\cases\cgen_protocol_overload_index.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "resolved native property getter/setter CallTarget ABI"; Source = "test\cases\cgen_native_class_property.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\xlir_resolved_property_call_abi_support.c"; XlirSupport = "test\cases\xlir_resolved_property_call_abi_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "trivial same-TypeId point conversion ABI"; Source = "test\cases\xlir_intrinsic_trivial_identity_convert.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\xlir_intrinsic_trivial_identity_convert_host.c"; XlirSupport = "test\cases\xlir_intrinsic_trivial_identity_convert_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "package-export module-global initializer ABI"; Source = "test\cases\xlir_value_initializer_package_exports.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "explicit-constructor class-null initializer ABI"; Source = "test\cases\xlir_value_initializer_class_null.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "explicit constructor own-field initializer ABI"; Source = "test\cases\xlir_explicit_constructor_field_initializer.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "class reference-slot initializer ABI"; Source = "test\cases\xlir_class_reference_field_initializer.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "nested class constructor ABI"; Source = "test\cases\cgen_nested_class_constructor.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "owned native class-field destruction ABI"; Source = "test\cases\cgen_class_native_field.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_native_field_host.c"; XlirSupport = "test\cases\cgen_class_native_field_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "derived class and dynamic shallow/deep copy ABI"; Source = "test\cases\cgen_copy_clone.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "custom class copy protocol call ABI"; Source = "test\cases\cgen_class_custom_copy_clone.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "TypeId, derived class and explicit class-protocol string conversion ABI"; Source = "test\cases\xlir_string_conversion_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "derived class structural hash/equality and typed-set identity ABI"; Source = "test\cases\cgen_derive_string_hash_equals.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "field-only native-backed managed object identity ABI"; Source = "test\cases\cgen_typed_container_objects.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "managed native extern receiver physical field-borrow ABI"; Source = "test\cases\xlir_managed_native_extern_receiver_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\xlir_managed_native_extern_receiver_host.c"; XlirSupport = "test\cases\xlir_managed_native_extern_receiver_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "class JSON persistence stringify/parse schema-projection ABI"; Source = "test\cases\xlir_persistence_json_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacySupport = "test\cases\cgen_class_host.c"; XlirSupport = "test\cases\cgen_class_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "managed class byval/byref/out parameter ABI"; Source = "test\cases\cgen_param_modes_class.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "managed container byval/byref/out parameter ABI"; Source = "test\cases\cgen_param_modes_container.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "managed container parameter mutation and collection-member intrinsic ABI"; Source = "test\cases\cgen_param_modes_container_matrix.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "typed container borrowed-parameter lexical ownership ABI"; Source = "test\cases\cgen_typed_container_param_mutation.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "explicit dynamic collection-method operation ABI"; Source = "test\cases\cgen_dynamic_container_methods.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "closed typed collection algebra ABI"; Source = "test\cases\cgen_container_ops.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "managed class byval ownership ABI"; Source = "test\cases\cgen_class_param_value_ownership.xl"; Host = "test\cases\xlir_generated_module_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "plain union value and byval/byref/out storage-reference ABI"; Source = "test\cases\cgen_union_value_semantics.xl"; Host = "test\cases\xlir_program_entry_host.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true; XlirGenericHost = $true },
    @{ Name = "trivial union dynamic box and checked projection ABI"; Source = "test\cases\xlir_union_dynamic_box_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "trivial union set iterator checked projection ABI"; Source = "test\cases\cgen_union_var_box.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "trivial union class-field read/write-back ABI"; Source = "test\cases\xlir_union_class_field_write_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "explicit and inherited default base constructor ABI"; Source = "test\cases\cgen_base_create.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true },
    @{ Name = "overloaded inherited default base constructor ABI"; Source = "test\cases\cgen_base_create_overloads.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; LegacySupport = "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"; LegacyStandalone = $true; XlirIncludeGenerated = $true }
)

if ( $StartAt -ne "" ) {
    $startIndex = -1
    for ( $i = 0; $i -lt $fixtures.Count; ++$i ) {
        if ( $fixtures[$i].Name -eq $StartAt ) {
            $startIndex = $i
            break
        }
    }
    if ( $startIndex -lt 0 ) {
        throw "Unknown XLIR differential fixture: $StartAt"
    }
    $fixtures = @($fixtures[$startIndex..($fixtures.Count - 1)])
}

# Both `--backend legacy` and `--backend xlir` execute this same fixture.
if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "XLIR differential check requires $xl"
}

Push-Location $Root
try {
    foreach ( $fixture in $fixtures ) {
        $source = Join-Path $Root $fixture.Source
        foreach ( $backend in @("legacy", "xlir") ) {
            & $xl cgen $source --backend $backend --emit $generated
            if ( $LASTEXITCODE -ne 0 ) {
                throw "XLIR differential Cgen failed for $($fixture.Name), backend: $backend"
            }
            if ( $null -ne $fixture.GeneratedInclude ) {
                $includedGenerated = Join-Path $Root $fixture.GeneratedInclude
                [System.IO.File]::Copy($generated, $includedGenerated, $true)
            }
            if ( $fixture.SeparateGenerated ) {
                $legacyDefine = if ( $backend -eq "legacy" -and
                    $fixture.LegacyRuntimeInit ) {
                    "-DXL6_LEGACY_RUNTIME"
                } else {
                    ""
                }
                $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXL6_DIFFERENTIAL_INLINE_DECL $legacyDefine $generated $($fixture.Host) !XL6_CGEN_LINKS! -o build\test\xlir_cgen_differential_$backend.exe && build\test\xlir_cgen_differential_$backend.exe"
            } elseif ( $backend -eq "legacy" -and $fixture.LegacyStandalone ) {
                # Legacy owns main for complete source-program fixtures.  The
                # executable lives beside the prepared xl.dll so its historic
                # async runtime imports resolve to the same build under test.
                # A fixture may additionally provide extern assertions without
                # replacing that generated process entry.
                $legacySupport = if ( $null -ne $fixture.LegacySupport ) {
                    $fixture.LegacySupport
                } else {
                    ""
                }
                $legacyWarningMode = if ( $fixture.LegacyAllowWarnings ) { "" } else { "-Werror" }
                $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! $legacyWarningMode $generated $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_cgen_differential_$backend.exe && build\test\xlir_cgen_differential_$backend.exe"
            } elseif ( $backend -eq "legacy" -and $null -ne $fixture.LegacySupport ) {
                # Legacy emits the process entry and retains its historic
                # xl.dll async adapter. XLIR emits module-init and links
                # directly to XRT. Both execute the identical XL source and
                # factory value; only the generated-C ABI harness differs.
                $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $generated $($fixture.LegacySupport) !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_cgen_differential_$backend.exe && build\test\xlir_cgen_differential_$backend.exe"
            } elseif ( $backend -eq "xlir" -and $fixture.XlirIncludeGenerated ) {
                # XLIR emits an internal module initializer, so the complete
                # generated module and its host intentionally share one TU.
                $xlirSupport = if ( $null -ne $fixture.XlirSupport ) {
                    if ( $fixture.XlirSeparateSupport ) {
                        $supportObject = "build\test\xlir_cgen_differential_support.o"
                        $supportCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c $($fixture.XlirSupport) -o $supportObject"
                        # This command runs inside the value-producing `if`
                        # expression.  Forward its log without allowing native
                        # stdout to become part of `$xlirSupport` and, later,
                        # accidental linker arguments.
                        & cmd.exe /v:on /c $supportCommand | Out-Host
                        if ( $LASTEXITCODE -ne 0 ) {
                            throw "XLIR differential support compilation failed for $($fixture.Name)"
                        }
                        $supportObject
                    } else {
                        "-include $($fixture.XlirSupport)"
                    }
                } else {
                    ""
                }
                $preincludedDefine = if ( $fixture.XlirGenericHost ) {
                    "-DXLIR_GENERATED_MODULE_PREINCLUDED"
                } else {
                    ""
                }
                $xlirLinks = if ( $fixture.XlirXlangLinks ) {
                    "!XL6_CGEN_XLANG_LINKS!"
                } else {
                    "!XL6_CGEN_LINKS!"
                }
                $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $preincludedDefine -include $generated $xlirSupport $($fixture.Host) $xlirLinks -o build\test\xlir_cgen_differential_$backend.exe && build\test\xlir_cgen_differential_$backend.exe"
            } else {
                $legacyDefine = ""
                if ( $backend -eq "legacy" ) { $legacyDefine = "-DXLIR_DIFFERENTIAL_LEGACY" }
                $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyDefine $($fixture.Host) !XL6_CGEN_LINKS! -o build\test\xlir_cgen_differential_$backend.exe && build\test\xlir_cgen_differential_$backend.exe"
            }
            & cmd.exe /v:on /c $command
            if ( $LASTEXITCODE -ne 0 ) {
                throw "XLIR differential generated-C runtime failed for $($fixture.Name), backend: $backend"
            }
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR differential Cgen runtime ok: legacy and xlir agree for scalar, explicit checked/static-wrapping and narrow-storage integer conversion, checked scalar div/mod/shift effects across signed/unsigned widths, exact no-raise signed i64 power and canonical async/generator machines, seven canonical base scalar conversion families, four distinct time conversion families and exact time approximate equality, special f64 constants and the stdlib math surface, managed-error, xvalue conditional, tagged nullable scalar, xvalue-sentinel var/class lazy coalesce, indexed null-aware assignment, pointer-sentinel nullable-string local/global/parameter/capture/class-field lifecycle, safe nullable-collection integer indexing and flow unwrap, safe typed/dynamic collection count, dynamic-key borrowed reads and consuming writes, typed dict scalar boxing writes, owned expression-result temporary release, xvalue array/list/set/dict/object/dynamic-string membership, managed-string membership, UTF-8 length and canonical UTF-8 string iteration, canonical scalar/time/raw/native class-field projection, mixed-container membership, canonical range descriptor membership, canonical range-for target/error semantics, stable array<i64> shallow-snapshot target/continue/break semantics, canonical list/set managed-item lifetime, dict key-only and key/item iterator projections, statically shaped erased array/list/set/dict cursor/item ABI, typed callable and nominal native-handle index/iterator projection, dynamic U64 arithmetic in the shared exact range, canonical task.cancel(future)/future.cancel() semantics, explicit Future wait/timeout/deadline/status semantics, flattened-literal and typed-array task.whenAny index semantics with retained Future xvalue handles, typed-array task.all/task.race aggregate semantics, typed Future then/catch/finally continuation semantics including payload changes and task-boundary errors, async ready-Future, async ready-Future-local-dynamic, async mixed-capture, dynamic-continuation, dynamic-local-parameter, extern-Future-capture, extern-Future-string-capture, extern-Future-dynamic-capture, extern-Future-forward, async managed/dynamic string ownership, function-value dynamic-borrow/compound-assignment, runtime callable xcall and scalar adapters, managed product ownership, owned empty-container defaults, scalar/time/union class-field constructor defaults, package-export module-global initialization, explicit-constructor class-null initialization, declaration-driven class receivers, class operator/contains/item/field/call syntax CallSites with rvalue receiver ownership, overloaded index getter/setter calls, resolved native property getter/setter member and indexed calls, trivial same-TypeId point conversion, generic field specialization, trivial union class-field read/write-back, explicit-constructor and reference-slot field initialization, nested class construction, owned native field destruction, derived shallow/deep copy, and custom copy-protocol ABIs. Advanced tagged nullable closure/cell/async/continuation/class-field contracts are XLIR-only and execute in check_xlir_null_abi_boundary.ps1 because legacy cannot emit valid C for those representations."
