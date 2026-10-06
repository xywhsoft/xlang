param(
    [string]$Root = "",
    [string]$StartAt = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$generated = Join-Path $Root "build\test\xlir_cgen_runtime_matrix_generated.c"
$fixtures = @(
    @{ Name = "resident derived class exact NUL char and Unicode text"; Source = "test\cases\stdlib_class_text.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "minimal resident class text descriptor and skip partition"; Source = "test\cases\stdlib_class_text_derive_only.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "resident error and exact dynamic text conversion"; Source = "test\cases\stdlib_error_text.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "contextual literal element conversion before collection erasure"; Source = "test\cases\stdlib_collection_contextual_literals.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "nullable TypeId narrowed equality and short circuit ABI"; Source = "test\cases\stdlib_nullable_type_equality.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "exact function-value expression calls and managed products"; Source = "test\cases\xlir_function_value_calls.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "native C scalar and type callable bridges"; Source = "test\cases\stdlib_callable_native_scalars.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "length-bearing Buffer, dictionary and embedded xlang boundaries"; Source = "test\cases\stdlib_string_bridge_boundaries.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true; XlangLinks = $true },
    @{ Name = "task race source and loser cancellation"; Source = "test\cases\cgen_task_race_source_cancel.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_task_race_source_cancel_support.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "scalar ABI"; Source = "test\cases\xlir_cgen_differential_scalar.xl"; Host = "test\cases\xlir_cgen_differential_scalar_host.c" },
    @{ Name = "exported primitive-width and F32 value ABI"; Source = "test\cases\export_primitive_widths.xl"; Host = "test\cases\export_primitive_widths_host.c"; SeparateGenerated = $true },
    @{ Name = "float-to-integer scalar conversion ABI"; Source = "test\cases\xlir_cgen_differential_scalar_conversion.xl"; Host = "test\cases\xlir_cgen_differential_scalar_conversion_host.c" },
    @{ Name = "explicit checked and static wrapping integer conversion ABI"; Source = "test\cases\xlir_integer_conversion_source_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "narrow integer storage normalization ABI"; Source = "test\cases\type_int_narrowing_wrap.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical scalar conversion family ABI"; Source = "test\cases\xlir_scalar_conversion_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "explicit dynamic unary operation ABI"; Source = "test\cases\xlir_dynamic_unary_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked extended dynamic binary operation ABI"; Source = "test\cases\xlir_dynamic_binary_extended_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked scalar integer arithmetic effect ABI"; Source = "test\cases\xlir_checked_integer_arithmetic_abi.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "exact no-raise i64 power ABI"; Source = "test\cases\cgen_pow.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked integer async/generator continuation ABI"; Source = "test\cases\xlir_checked_integer_continuation_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "distinct time conversion ABI"; Source = "test\cases\xlir_time_conversion_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; Support = "test\cases\xlir_time_conversion_abi_support.c"; IncludeGenerated = $true },
    @{ Name = "exact no-raise time approximate ABI"; Source = "test\cases\cgen_time_literals.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "special f64 constant and stdlib math ABI"; Source = "test\cases\stdlib_math.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "managed error ABI"; Source = "test\cases\xlir_cgen_differential_managed_error.xl"; Host = "test\cases\xlir_cgen_differential_managed_error_host.c" },
    @{ Name = "immutable Error source-location and member ABI"; Source = "test\cases\xlir_error_location_abi.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "xvalue conditional scalar ABI"; Source = "test\cases\xlir_cgen_differential_xvalue_conditional.xl"; Host = "test\cases\xlir_cgen_differential_xvalue_conditional_host.c" },
    @{ Name = "tagged nullable scalar ABI"; Source = "test\cases\xlir_nullable_scalar.xl"; Host = "test\cases\xlir_nullable_scalar_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "xvalue-sentinel nullable coalesce ABI"; Source = "test\cases\xlir_nullable_sentinel_coalesce.xl"; Host = "test\cases\xlir_nullable_sentinel_coalesce_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "xvalue indexed null-aware assignment ABI"; Source = "test\cases\xlir_nullable_index_assign_if_null.xl"; Host = "test\cases\xlir_nullable_index_assign_if_null_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "indexed null-aware assignment raising RHS"; Source = "test\cases\xlir_nullable_index_assign_if_null_may_raise.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "pointer-sentinel nullable-string lifecycle ABI"; Source = "test\cases\nullable_string_lifecycle.xl"; Host = "test\cases\xlir_class_static_storage_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true },
    @{ Name = "safe nullable-collection integer index ABI"; Source = "test\cases\xlir_safe_index_nullable_collection_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "safe typed/dynamic collection count ABI"; Source = "test\cases\cgen_safe_member.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked scalar Array reads and loop cleanup ABI"; Source = "test\cases\stdlib_array_scalar_read.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "checked scalar Array continuation reads ABI"; Source = "test\cases\stdlib_array_scalar_read_continuation.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "resident owned string and consuming Array/index boundaries"; Source = "test\cases\stdlib_array_value_boundaries.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "resident Array boundaries in continuations"; Source = "test\cases\stdlib_array_value_boundaries_continuation.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "checked resident cursor status and owned error routes"; Source = "test\cases\stdlib_value_iterator_checked.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "checked resident cursor continuation lifetime"; Source = "test\cases\stdlib_value_iterator_checked_continuation.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "safe nullable declared-class field ABI"; Source = "test\cases\nullable_safe_navigation.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "typed dict literal-key projection and safe-index ABI"; Source = "test\cases\xlir_typed_dict_literal_index_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "xvalue dynamic-key borrowed index ABI"; Source = "test\cases\xlir_xvalue_dynamic_key_index.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "xvalue dynamic-key consuming index write ABI"; Source = "test\cases\xlir_xvalue_dynamic_key_index_set.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "typed dict scalar boxing write ABI"; Source = "test\cases\xlir_typed_dict_scalar_set.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "typed raw-pointer collection index projection ABI"; Source = "test\cases\cgen_typed_point.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "xvalue scalar bridge, flow projection, and may-raise constructor ABI"; Source = "test\cases\hir_xvalue_scalar_bridge_matrix.xl"; Host = "test\cases\hir_xvalue_scalar_bridge_matrix_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "canonical typed index write ABI"; Source = "test\cases\xlir_index_write_generic_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "owned expression-result discard ABI"; Source = "test\cases\xlir_owned_expression_discard.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "owned collection expression-result discard ABI"; Source = "test\cases\cgen_array_literal_expr_elements.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "xvalue collection membership ABI"; Source = "test\cases\xlir_xvalue_membership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "managed string membership ABI"; Source = "test\cases\xlir_string_membership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "managed string UTF-8 length member ABI"; Source = "test\cases\xlir_member_abi_string_length.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical UTF-8 string iterator ABI"; Source = "test\cases\xlir_string_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "typed nullable erased-container projection ABI"; Source = "test\cases\typed_nullable_container_matrix.xl"; Host = "test\cases\xlir_typed_nullable_container_matrix_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "canonical class length protocol member ABI"; Source = "test\cases\xlir_length_protocol_member.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\xlir_length_protocol_member_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "canonical scalar/time/raw/native class-field projection ABI"; Source = "test\cases\xlir_class_field_member_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\xlir_class_field_member_abi_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "inherited and flow-refined class-field projection ABI"; Source = "test\cases\xlir_class_field_inherited_narrowed_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "mixed string and xvalue container membership ABI"; Source = "test\cases\cgen_in_containers.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical range descriptor membership ABI"; Source = "test\cases\xlir_range_membership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "established range membership ABI"; Source = "test\cases\cgen_in_range.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical range-for target and zero-step error ABI"; Source = "test\cases\xlir_range_for_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical stable array<i64> for snapshot ABI"; Source = "test\cases\xlir_array_i64_for_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical xvalue list<i64> snapshot iterator ABI"; Source = "test\cases\xlir_list_i64_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical xvalue set<i64> snapshot iterator ABI"; Source = "test\cases\xlir_set_i64_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical xvalue scalar iterator projection ABI"; Source = "test\cases\xlir_xvalue_iterator_scalar_projection.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "managed list/set iterator target lifetime ABI"; Source = "test\cases\xlir_xvalue_iterator_managed_projection.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    # Full-width dict<uint64> has its own representation-specific gate. This
    # fixture preserves key/item, managed-lifetime, and finally semantics.
    @{ Name = "dict key/item iterator projection ABI"; Source = "test\cases\xlir_dict_iterator_projection_differential.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "dict key-only iterator projection ABI"; Source = "test\cases\xlir_dict_key_only_iterator.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "statically shaped erased collection iterator ABI"; Source = "test\cases\xlir_erased_collection_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "runtime-checked dynamic var iterator ABI"; Source = "test\cases\xlir_dynamic_var_iterator_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    # The dedicated U64 ABI gate separately proves full-width tags and
    # modulo-2^64 boundaries; this fixture covers its shared exact range.
    @{ Name = "dynamic U64 arithmetic shared-range ABI"; Source = "test\cases\xlir_dynamic_u64_arithmetic.xl"; Host = "test\cases\xlir_dynamic_u64_arithmetic_host.c"; IncludeGenerated = $true },
    @{ Name = "canonical Future cancellation intrinsic ABI"; Source = "test\cases\xlir_future_cancel.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "explicit Future wait/status member ABI"; Source = "test\cases\xlir_future_member_status_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true; XlangLinks = $true },
    @{ Name = "flattened literal task.whenAny index ABI"; Source = "test\cases\xlir_task_when_any_literal_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed-array task.whenAny retained Future ABI"; Source = "test\cases\xlir_task_when_any_array_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed-array task.all/task.race aggregate ABI"; Source = "test\cases\xlir_task_aggregate_array_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true; XlangLinks = $true },
    @{ Name = "typed task.spawn closure/frame ABI"; Source = "test\cases\xlir_task_spawn_closure_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed thread resource identity/capture ABI"; Source = "test\cases\xlir_thread_resource_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed channel resource/payload/storage ABI"; Source = "test\cases\xlir_channel_resource_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed mutex/sem/cond/rwlock resource ABI"; Source = "test\cases\cgen_sync.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed concurrent queue resource/payload/capture ABI"; Source = "test\cases\stdlib_special_collections_concurrent.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed Stack resource/payload/class-field/capture ABI"; Source = "test\cases\xlir_stack_resource_abi.xl"; Host = "test\cases\xlir_stack_resource_abi_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed AvlTree key/value/order/class-field/capture ABI"; Source = "test\cases\xlir_avl_resource_abi.xl"; Host = "test\cases\xlir_avl_resource_abi_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed Future then/catch/finally continuation ABI"; Source = "test\cases\stdlib_future_continuation.xl"; Host = "test\cases\xlir_future_continuation_abi_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed callable index projection ABI"; Source = "test\cases\xlir_typed_callable_index_abi.xl"; Host = "test\cases\xlir_generated_module_diagnostic_host.c"; IncludeGenerated = $true; GenericHost = $true; XlangLinks = $true },
    @{ Name = "typed native-handle index projection ABI"; Source = "test\cases\xlir_typed_native_handle_index_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed native-handle iterator projection ABI"; Source = "test\cases\xlir_typed_native_handle_iterator_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "async ready-Future ABI"; Source = "test\cases\xlir_async_ready_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_scalar_host.c" },
    @{ Name = "async ready-Future local dynamic ABI"; Source = "test\cases\xlir_await_future_xvalue.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c" },
    @{ Name = "async generic expression planner ABI"; Source = "test\cases\xlir_async_generic_expression_planner.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\xlir_async_generic_expression_planner_support.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "async scalar mixed-capture ABI"; Source = "test\cases\xlir_async_future_scalar_mixed_capture_chain.xl"; Host = "test\cases\xlir_cgen_differential_async_scalar_host.c" },
    @{ Name = "async dynamic continuation ABI"; Source = "test\cases\xlir_async_future_xvalue_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c" },
    @{ Name = "async dynamic local-parameter ABI"; Source = "test\cases\xlir_async_future_xvalue_local_parameter_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c" },
    @{ Name = "async extern Future capture ABI"; Source = "test\cases\xlir_async_future_direct_call_capture.xl"; Host = "test\cases\xlir_async_future_direct_call_capture_host.c"; GeneratedInclude = "build\test\xlir_async_future_direct_call_capture_generated.c" },
    @{ Name = "async extern Future string-capture ABI"; Source = "test\cases\xlir_async_future_direct_string_capture.xl"; Host = "test\cases\xlir_async_future_direct_string_capture_host.c"; GeneratedInclude = "build\test\xlir_async_future_direct_string_capture_generated.c" },
    @{ Name = "async extern Future dynamic-capture ABI"; Source = "test\cases\xlir_async_future_direct_xvalue_differential.xl"; Host = "test\cases\xlir_cgen_differential_async_xvalue_host.c" },
    @{ Name = "async extern Future forward ABI"; Source = "test\cases\xlir_async_future_direct_call.xl"; Host = "test\cases\xlir_async_future_direct_call_host.c"; GeneratedInclude = "build\test\xlir_async_future_direct_call_generated.c" },
    @{ Name = "async managed/dynamic string ownership ABI"; Source = "test\cases\cgen_async_string_ownership.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "function-value dynamic borrow and compound assignment ABI"; Source = "test\cases\cgen_function_direct_owned_args.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "function-value by-value return ABI"; Source = "test\cases\cgen_function_param_modes.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "runtime callable xcall ABI"; Source = "test\cases\cgen_extern_function_value.xl"; Host = "test\cases\xlir_xcall_i64_host.c"; IncludeGenerated = $true },
    @{ Name = "runtime callable scalar ABI"; Source = "test\cases\xlir_xcall_scalar_values.xl"; Host = "test\cases\xlir_xcall_scalar_host.c"; IncludeGenerated = $true },
    @{ Name = "managed product field ownership ABI"; Source = "test\cases\xlir_managed_product_result.xl"; Host = "test\cases\xlir_managed_product_differential_host.c"; IncludeGenerated = $true },
    @{ Name = "owned empty-container default initializer ABI"; Source = "test\cases\definite_assignment_default_values.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "scalar/time/union class-field constructor initializer ABI"; Source = "test\cases\xlir_value_initializer_class_default_differential.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "declaration-driven implicit class receiver ABI"; Source = "test\cases\xlir_class_implicit_receiver.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "base receiver direct dispatch ABI"; Source = "test\cases\cgen_base_method_static_dispatch.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "base receiver typed call ABI"; Source = "test\cases\cgen_base_method_xlir.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "implicit class-field namespace shadow ABI"; Source = "test\cases\cgen_stdlib_namespace_shadow.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "class operator/contains/call syntax CallSite ABI"; Source = "test\cases\cgen_protocol_ops.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "class rvalue item/field/call syntax CallSite ABI"; Source = "test\cases\cgen_protocol_complex_receiver.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "generic class field syntax specialization ABI"; Source = "test\cases\xlir_class_protocol_syntax_generic.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "generic class method and operator/index protocol specialization ABI"; Source = "test\cases\cgen_class_generic_handle.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "overloaded class index getter/setter CallSite ABI"; Source = "test\cases\cgen_protocol_overload_index.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "resolved native property getter/setter CallTarget ABI"; Source = "test\cases\cgen_native_class_property.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\xlir_resolved_property_call_abi_support.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "trivial same-TypeId point conversion ABI"; Source = "test\cases\xlir_intrinsic_trivial_identity_convert.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\xlir_intrinsic_trivial_identity_convert_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "package-export module-global initializer ABI"; Source = "test\cases\xlir_value_initializer_package_exports.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "explicit-constructor class-null initializer ABI"; Source = "test\cases\xlir_value_initializer_class_null.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "explicit constructor own-field initializer ABI"; Source = "test\cases\xlir_explicit_constructor_field_initializer.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "class reference-slot initializer ABI"; Source = "test\cases\xlir_class_reference_field_initializer.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "nested class constructor ABI"; Source = "test\cases\cgen_nested_class_constructor.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "owned native class-field destruction ABI"; Source = "test\cases\cgen_class_native_field.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_native_field_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "derived class and dynamic shallow/deep copy ABI"; Source = "test\cases\cgen_copy_clone.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "custom class copy protocol call ABI"; Source = "test\cases\cgen_class_custom_copy_clone.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "TypeId, derived class and explicit class-protocol string conversion ABI"; Source = "test\cases\xlir_string_conversion_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "derived class structural hash/equality and typed-set identity ABI"; Source = "test\cases\cgen_derive_string_hash_equals.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "field-only native-backed managed object identity ABI"; Source = "test\cases\cgen_typed_container_objects.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "managed native extern receiver physical field-borrow ABI"; Source = "test\cases\xlir_managed_native_extern_receiver_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\xlir_managed_native_extern_receiver_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "class JSON persistence stringify/parse schema-projection ABI"; Source = "test\cases\xlir_persistence_json_abi.xl"; Host = "test\cases\xlir_generated_module_host.c"; Support = "test\cases\cgen_class_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "managed class by-value and replacement-return ABI"; Source = "test\cases\cgen_param_modes_class.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "managed container by-value and replacement-return ABI"; Source = "test\cases\cgen_param_modes_container.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "managed container parameter mutation and collection-member intrinsic ABI"; Source = "test\cases\cgen_param_modes_container_matrix.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "typed container borrowed-parameter lexical ownership ABI"; Source = "test\cases\cgen_typed_container_param_mutation.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "explicit dynamic collection-method operation ABI"; Source = "test\cases\cgen_dynamic_container_methods.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "closed typed collection algebra ABI"; Source = "test\cases\cgen_container_ops.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked resident Array core and query methods"; Source = "test\cases\stdlib_array_methods.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked resident Array methods in async and generator"; Source = "test\cases\stdlib_array_methods_continuation.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked resident Array defaults and resize"; Source = "test\cases\stdlib_array_resident_defaults.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked resident Array resize in async and generator"; Source = "test\cases\stdlib_array_resident_defaults_continuation.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "checked resident Array extraction and List/Dict misses"; Source = "test\cases\stdlib_array_extract.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "checked basic Array extraction in async and generator"; Source = "test\cases\stdlib_array_extract_continuation.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "checked trivial Array extraction with nullable and C scalars"; Source = "test\cases\stdlib_array_extract_trivial.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "managed class byval ownership ABI"; Source = "test\cases\cgen_class_param_value_ownership.xl"; Host = "test\cases\xlir_generated_module_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "plain union value and replacement-return ABI"; Source = "test\cases\cgen_union_value_semantics.xl"; Host = "test\cases\xlir_program_entry_host.c"; IncludeGenerated = $true; GenericHost = $true },
    @{ Name = "trivial union dynamic box and checked projection ABI"; Source = "test\cases\xlir_union_dynamic_box_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "trivial union set iterator checked projection ABI"; Source = "test\cases\cgen_union_var_box.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "trivial union class-field read/write-back ABI"; Source = "test\cases\xlir_union_class_field_write_abi.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "explicit and inherited default base constructor ABI"; Source = "test\cases\cgen_base_create.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true },
    @{ Name = "overloaded inherited default base constructor ABI"; Source = "test\cases\cgen_base_create_overloads.xl"; Host = "test\cases\xlir_default_class_layout_host.c"; IncludeGenerated = $true }
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
        throw "Unknown XLIR runtime-matrix fixture: $StartAt"
    }
    $fixtures = @($fixtures[$startIndex..($fixtures.Count - 1)])
}

# This is now a canonical generated-C runtime matrix.  It keeps the broad
# fixture coverage that was useful during migration without retaining a dead
# backend as an oracle.
if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "XLIR generated-C runtime check requires $xl"
}

Push-Location $Root
$compilerEnvironmentBefore = @{}
$fixtureFailures = [System.Collections.Generic.List[string]]::new()
function Invoke-MatrixNative([string]$File, [string[]]$Arguments) {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $File @Arguments | Out-Host
        return $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
}
try {
    # All rows use one frozen source/runtime revision. Prepare the same compiler,
    # objects and declarations once, rather than rebuilding XRT for every row.
    $toolchainOutput = & cmd.exe /d /v:on /c 'call test\suites\common.bat && set XL6_CGEN'
    if ($LASTEXITCODE -ne 0) { throw 'XLIR runtime matrix toolchain preparation failed' }
    foreach ($line in $toolchainOutput) {
        if ($line -match '^(XL6_CGEN_[A-Z_]+)=(.*)$') {
            $key = $Matches[1]
            $compilerEnvironmentBefore[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            [Environment]::SetEnvironmentVariable($key, $Matches[2], 'Process')
        } else { Write-Output $line }
    }
    foreach ($key in @('XL6_CGEN_CC','XL6_CGEN_FLAGS','XL6_CGEN_INCLUDES','XL6_CGEN_LINKS','XL6_CGEN_XLANG_LINKS')) {
        if (![Environment]::GetEnvironmentVariable($key, 'Process')) { throw "Missing runtime matrix compiler variable: $key" }
    }
    foreach ( $fixture in $fixtures ) {
      try {
        $source = Join-Path $Root $fixture.Source
        if ((Invoke-MatrixNative $xl @('cgen', $source, '--backend', 'xlir', '--emit', $generated)) -ne 0) {
            throw "XLIR Cgen failed for $($fixture.Name)"
        }
        if ( $null -ne $fixture.GeneratedInclude ) {
            $includedGenerated = Join-Path $Root $fixture.GeneratedInclude
            [System.IO.File]::Copy($generated, $includedGenerated, $true)
        } elseif ( !$fixture.SeparateGenerated -and !$fixture.IncludeGenerated ) {
            # Self-including differential hosts share this canonical path.
            # Refresh it for every fixture so the matrix never consumes a
            # stale artifact produced by another gate or an earlier row.
            $includedGenerated = Join-Path $Root "build\test\xlir_cgen_differential_generated.c"
            [System.IO.File]::Copy($generated, $includedGenerated, $true)
        }
        if ( $fixture.SeparateGenerated ) {
            $command = "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXL6_DIFFERENTIAL_INLINE_DECL $generated $($fixture.Host) !XL6_CGEN_LINKS! -o build\test\xlir_cgen_runtime.exe && build\test\xlir_cgen_runtime.exe"
        } elseif ( $fixture.IncludeGenerated ) {
            # XLIR emits an internal module initializer, so the complete
            # generated module and its host intentionally share one TU.
            $support = if ( $null -ne $fixture.Support ) {
                if ( $fixture.SeparateSupport ) {
                    $supportObject = "build\test\xlir_cgen_runtime_support.o"
                    $supportCommand = "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c $($fixture.Support) -o $supportObject"
                    # Forward the native compiler log without allowing stdout
                    # to become an accidental linker argument.
                    if ((Invoke-MatrixNative 'cmd.exe' @('/d', '/v:on', '/c', $supportCommand)) -ne 0) {
                        throw "XLIR runtime support compilation failed for $($fixture.Name)"
                    }
                    $supportObject
                } else {
                    "-include $($fixture.Support)"
                }
            } else {
                ""
            }
            $preincludedDefine = if ( $fixture.GenericHost ) {
                "-DXLIR_GENERATED_MODULE_PREINCLUDED"
            } else {
                ""
            }
            $links = if ( $fixture.XlangLinks ) {
                "!XL6_CGEN_XLANG_LINKS!"
            } else {
                "!XL6_CGEN_LINKS!"
            }
            $command = "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $preincludedDefine -include $generated $support $($fixture.Host) $links -o build\test\xlir_cgen_runtime.exe && build\test\xlir_cgen_runtime.exe"
        } else {
            $command = "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $($fixture.Host) !XL6_CGEN_LINKS! -o build\test\xlir_cgen_runtime.exe && build\test\xlir_cgen_runtime.exe"
        }
        if ((Invoke-MatrixNative 'cmd.exe' @('/d', '/v:on', '/c', $command)) -ne 0) {
            throw "XLIR generated-C runtime failed for $($fixture.Name)"
        }
        Write-Output "PASS: $($fixture.Name)"
      } catch {
        $fixtureFailures.Add($_.Exception.Message)
        Write-Output "FAIL: $($fixture.Name): $($_.Exception.Message)"
      }
    }
} finally {
    foreach ($key in $compilerEnvironmentBefore.Keys) {
        [Environment]::SetEnvironmentVariable($key, $compilerEnvironmentBefore[$key], 'Process')
    }
    Pop-Location
}

if ($fixtureFailures.Count -ne 0) {
    throw "XLIR runtime matrix recorded $($fixtureFailures.Count) failed rows after collecting all $($fixtures.Count):`n$($fixtureFailures -join "`n")"
}
Write-Output "XLIR canonical Cgen runtime matrix ok: generated C compiled and executed across scalar, ownership, collection, callable, class, async, generator, task, synchronization and persistence ABI fixtures."
