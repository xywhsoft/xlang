param([string]$Directory = 'build/test/xlir_public_source', [switch]$RuntimeOnly, [switch]$NoCache,
    [string]$CacheDirectory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_c_object_cache.ps1')
$rootPath = (Get-Location).Path
$batchPath = if ([IO.Path]::IsPathRooted($Directory)) {
    [IO.Path]::GetFullPath($Directory)
} else {
    [IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
}
if (Test-Path -LiteralPath $batchPath) { throw 'Choose a fresh public build directory; prior evidence must not be overwritten' }
$buildTimer = [Diagnostics.Stopwatch]::StartNew()
$buildContext = New-XlirCBuildContext -Root $rootPath -Directory $batchPath -NoCache:$NoCache -CacheDirectory $CacheDirectory
$driverSource = if ($RuntimeOnly) { 'test/cases/xlir_public_runtime_driver.c' } else { 'test/cases/xlir_public_source_driver.c' }
$binaryName = if ($RuntimeOnly) { 'runtime_source.exe' } else { 'public_source.exe' }
$enableExe = if ($RuntimeOnly) { 0 } else { 1 }
$sources = @('src/api/xl_api.c','src/api/xl_api_symbols.c','src/api/xl_api_path.c',
    'tools/xrt_single_header_impl.c','src/api/xl_protocol_registry.c','src/api/xl_channel_bridge.c',
    'src/runtime/xl6_value_numeric.c','src/runtime/xl6_value_add.c',
    'src/api/xl_future_bridge.c','src/api/xl_task_group_bridge.c',
    'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
    'src/runtime/xl6_continuation_future.c','src/runtime/xl6_code_lease.c','src/runtime/xl6_future_payload.c',
    'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_static_module.c','src/runtime/xl6_ownership_catalog.c',
    'src/runtime/xl6_ownership_domain_code_lease.c',
    'src/runtime/xl6_ownership_domain_class.c',
    'src/runtime/xl6_ownership_domain_cell.c',
    'src/runtime/xl6_ownership_domain_tree.c',
    'src/runtime/xl6_ownership_domain_semaphore.c',
    'src/runtime/xl6_ownership_domain_sync.c',
    'src/runtime/xl6_ownership_domain_thread.c',
    'src/runtime/xl6_ownership_domain_value_iterator.c',
    'src/runtime/xl6_ownership_domain_error_value.c',
    'src/runtime/xl6_ownership_domain_slot_map_key.c',
    'src/runtime/xl6_ownership_domain_channel.c',
    'src/runtime/xl6_ownership_domain_sequence.c',
    'src/runtime/xl6_ownership_domain_list.c',
    'src/runtime/xl6_ownership_domain_set.c',
    'src/runtime/xl6_ownership_domain_dict.c',
    'src/runtime/xl6_ownership_domain_slot_map.c',
    'src/runtime/xl6_ownership_domain_spsc_queue.c',
    'src/runtime/xl6_ownership_domain_mpsc_queue.c',
    'src/runtime/xl6_ownership_domain_mpmc_queue.c',
    'src/runtime/xl6_ownership_domain_wait_queue.c',
    'src/runtime/xl6_ownership_domain_process_command.c',
    'src/runtime/xl6_ownership_domain_process_status.c',
    'src/runtime/xl6_ownership_domain_process_live.c',
    'src/runtime/xl6_ownership_domain_process_future.c',
    'src/runtime/xl6_task_group_future.c',
    'src/runtime/xl6_task_group_start.c',
    'src/runtime/xl6_ownership_domain_buffer_iterator.c',
    'src/runtime/xl6_ownership_domain_sequence_future.c',
    'src/runtime/xl6_ownership_domain_sequence_semaphore.c',
    'src/runtime/xl6_ownership_domain_sequence_sync.c',
    'src/runtime/xl6_ownership_domain_sequence_thread.c',
    'src/runtime/xl6_ownership_domain_sequence_slot_map_key.c',
    'src/runtime/xl6_ownership_domain_closure.c',
    'src/runtime/xl6_ownership_domain_dynamic_fields.c',
    'src/runtime/xl6_ownership_domain_executor.c',
    'src/runtime/xl6_ownership_domain_generator.c',
    'src/runtime/xl6_ownership_domain_queue.c',
    'src/runtime/xl6_ownership_domain_frame.c',
    'src/runtime/xl6_ownership_domain_future_callback.c',
    'src/runtime/xl6_ownership_domain_task.c',
    'src/runtime/xl6_ownership_domain_task_pool.c',
    'src/runtime/xl6_ownership_domain_native_executor.c',
    'src/runtime/xl6_ownership_domain_executor_jobs.c',
    'src/runtime/xl6_executor_jobs.c',
    'src/runtime/xl6_detached_work.c',
    'src/runtime/xl6_ownership_domain_detached_work.c',
    'src/runtime/xl6_execution_net_engine.c',
    'src/runtime/xl6_ownership_domain_execution_net_engine.c',
    'src/runtime/xl6_ownership_domain_net_engine_native.c',
    'src/runtime/xl6_net_engine_config.c',
    'src/runtime/xl6_net_engine_language.c',
    'src/runtime/xl6_net_engine_stats.c',
    'src/runtime/xl6_ownership_domain_net_engine_config.c',
    'src/runtime/xl6_ownership_domain_net_engine_language.c',
    'src/runtime/xl6_ownership_domain_net_engine_stats.c',
    'src/runtime/xl6_net_socket_owner.c',
    'src/runtime/xl6_net_socket_language.c',
    'src/runtime/xl6_net_socket_io.c',
    'src/runtime/xl6_net_datagram_values.c',
    'src/runtime/xl6_net_socket_datagram_control.c',
    'src/runtime/xl6_net_socket_datagram_io.c',
    'src/runtime/xl6_ownership_domain_net_datagram_info.c',
    'src/runtime/xl6_ownership_domain_net_datagram_control.c',
    'src/runtime/xl6_net_datagram_error_values.c',
    'src/runtime/xl6_net_socket_datagram_error_io.c',
    'src/runtime/xl6_ownership_domain_net_datagram_error_info.c',
    'src/runtime/xl6_ownership_domain_net_socket_owner.c',
    'src/runtime/xl6_ownership_domain_net_socket.c',
    'src/runtime/xl6_execution_failures.c',
    'src/runtime/xl6_ownership_domain_execution_failures.c',
    'src/runtime/xl6_execution_worker_exit.c',
    'src/runtime/xl6_task_pool_exit.c',
    'src/runtime/xl6_ownership_domain_task_pool_exit.c',
    'src/runtime/xl6_execution_resource.c',
    'src/runtime/xl6_execution_executor.c',
    'src/runtime/xl6_execution_pool.c',
    'src/runtime/xl6_execution_module.c',
    'src/runtime/xl6_executor_language.c',
    'src/runtime/xl6_executor_batch.c',
    'src/runtime/xl6_task_submit_closure.c',
    'src/runtime/xl6_task_pool_language.c',
    'src/runtime/xl6_task_pool_submit_language.c',
    'src/runtime/xl6_ownership_domain_task_pool_language.c',
    'src/runtime/xl6_ownership_domain_executor_language.c',
    'src/runtime/xl6_ownership_domain_execution_source.c',
    'src/runtime/xl6_ownership_domain_execution_executor.c',
    'src/runtime/xl6_ownership_domain_execution_pool.c',
    'src/runtime/xl6_ownership_domain_future_payload.c',
    'src/runtime/xl6_ownership_domain_native_future.c',
    'src/runtime/xl6_ownership_domain_future_aggregate.c',
    'src/runtime/xl6_ownership_domain_network.c',
    'src/runtime/xl6_xlang_context_policy.c','src/runtime/xl6_ownership_domain_xlang_context.c',
    'src/runtime/xl6_ownership_domain_buffer.c',
    'src/runtime/xl6_ownership_domain_resource_time_date_time.c',
    'src/runtime/xl6_ownership_domain_resource_unicode_validator.c',
    'src/runtime/xl6_ownership_domain_resource_text_iterator.c',
    'src/runtime/xl6_ownership_domain_resource_pattern_core.c',
    'src/runtime/xl6_ownership_domain_resource_pattern_options.c',
    'src/runtime/xl6_ownership_domain_resource_pattern_match.c',
    'src/runtime/xl6_ownership_domain_resource_pattern_builder.c',
    'src/runtime/xl6_ownership_domain_resource_process_pipeline_config.c',
    'src/runtime/xl6_ownership_domain_resource_process_pipeline.c',
    'src/runtime/xl6_ownership_domain_resource_process_run.c',
    'src/runtime/xl6_ownership_domain_resource_regex_core.c',
    'src/runtime/xl6_ownership_domain_resource_regex_match.c',
    'src/runtime/xl6_ownership_domain_resource_regex_iterator.c',
    'src/runtime/xl6_ownership_domain_resource_regex_split.c',
    'src/runtime/xl6_ownership_domain_resource_regex_set.c',
    'src/runtime/xl6_ownership_domain_resource_regex_replace_func.c',
    'src/runtime/xl6_ownership_domain_resource_signal_queue.c',
    'src/runtime/xl6_ownership_domain_resource_process_options.c',
    'src/runtime/xl6_ownership_domain_resource_random_state.c',
    'src/runtime/xl6_ownership_domain_resource_fs_file.c',
    'src/runtime/xl6_random_state.c',
    'src/runtime/xl6_fs_file_owner.c',
    'src/runtime/xl6_vfs_core.c',
    'src/runtime/xl6_vfs_memory.c',
    'src/runtime/xl6_vfs_disk.c',
    'src/runtime/xl6_vfs_pack.c',
    'src/runtime/xl6_vfs_file.c',
    'src/runtime/xl6_vfs_info.c',
    'src/runtime/xl6_vfs_dir.c',
    'src/runtime/xl6_fs_info_owner.c',
    'src/runtime/xl6_fs_dir_owner.c',
    'src/runtime/xl6_io_read_file.c',
    'src/runtime/xl6_io_write_file.c',
    'src/runtime/xl6_io_reader.c',
    'src/runtime/xl6_io_reader_data.c',
    'src/runtime/xl6_io_reader_position.c',
    'src/runtime/xl6_io_writer.c',
    'src/runtime/xl6_io_writer_data.c',
    'src/runtime/xl6_io_writer_position.c',
    'src/runtime/xl6_io_line.c',
    'src/runtime/xl6_io_copy.c',
    'src/runtime/xl6_io_file.c',
    'src/runtime/xl6_io_reader_open.c',
    'src/runtime/xl6_io_reader_memory.c',
    'src/runtime/xl6_io_reader_standard.c',
    'src/runtime/xl6_io_writer_open.c',
    'src/runtime/xl6_io_writer_buffer.c',
    'src/runtime/xl6_io_writer_standard.c',
    'src/runtime/xl6_ownership_domain_resource_io_reader.c',
    'src/runtime/xl6_ownership_domain_resource_io_writer.c',
    'src/runtime/xl6_ownership_domain_resource_io_line.c',
    'src/runtime/xl6_fs_map_owner.c',
    'src/runtime/xl6_fs_lock_owner.c',
    'src/runtime/xl6_fs_root_owner.c',
    'src/runtime/xl6_ownership_domain_resource_fs_map.c',
    'src/runtime/xl6_ownership_domain_resource_fs_lock.c',
    'src/runtime/xl6_ownership_domain_resource_fs_root.c',
    'src/runtime/xl6_ownership_domain_resource_task_group.c',
    'src/runtime/xl6_ownership_domain_native_future_payload.c',
    'src/runtime/xl6_ownership_domain_resource_vfs_core.c',
    'src/runtime/xl6_ownership_domain_resource_vfs_memory.c',
    'src/runtime/xl6_ownership_domain_resource_vfs_disk.c',
    'src/runtime/xl6_ownership_domain_resource_vfs_pack.c',
    'src/runtime/xl6_ownership_domain_resource_fs_info_owner.c',
    'src/runtime/xl6_ownership_domain_resource_fs_dir_owner.c',
    'src/runtime/xl6_network_service.c','src/runtime/xl6_log.c','src/runtime/xl6_log_console.c',
    'src/runtime/xl6_log_record.c','src/runtime/xl6_log_text.c','src/runtime/xl6_log_json.c','src/runtime/xl6_log_custom.c',
    'src/runtime/xl6_log_file.c','src/runtime/xl6_log_file_text.c','src/runtime/xl6_log_file_json.c','src/runtime/xl6_log_async.c','src/runtime/xl6_log_ring.c',
    'src/runtime/xl6_log_printf.c','src/runtime/xl6_log_text_writer.c','src/runtime/xl6_log_json_writer.c','src/runtime/xl6_text_value.c',
    'src/runtime/xl6_text_query.c','src/runtime/xl6_text_binary.c','src/runtime/xl6_text_chars.c','src/runtime/xl6_text_glob.c','src/runtime/xl6_text_distance.c',
    'src/runtime/xl6_buffer_value.c','src/runtime/xl6_buffer_iterator.c',
    'src/runtime/xl6_dynamic_fields.c',
    'src/runtime/xl6_dynamic_fields_copy.c',
    'src/runtime/xl6_buffer_hex.c','src/runtime/xl6_buffer_base64.c',
    'src/runtime/xl6_buffer_core.c','src/runtime/xl6_buffer_query.c',
    'src/runtime/xl6_sequence.c','src/runtime/xl6_array.c','src/runtime/xl6_array_iterator.c','src/runtime/xl6_array_value_iterator.c','src/runtime/xl6_stack.c','src/runtime/xl6_stack_erased.c',
    'src/runtime/xl6_stack_batch.c',
    'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_array_nominal_admission.c',
    'src/runtime/xl6_array_union_admission.c',
    'src/runtime/xl6_array_call_frame.c',
    'src/runtime/xl6_call_frame_expansion.c',
    'src/runtime/xl6_dict_call_frame.c',
    'src/runtime/xl6_array_entry.c',
    'src/runtime/xl6_array_producer.c',
    'src/runtime/xl6_tree_projection.c',
    'src/runtime/xl6_tree.c',
    'src/runtime/xl6_tree_bytes_key.c',
    'src/runtime/xl6_tree_admission.c',
    'src/runtime/xl6_semaphore.c','src/runtime/xl6_array_semaphore_admission.c',
    'src/runtime/xl6_sync.c','src/runtime/xl6_mutex.c','src/runtime/xl6_cond.c','src/runtime/xl6_rwlock.c',
    'src/runtime/xl6_array_sync_admission.c',
    'src/runtime/xl6_thread.c','src/runtime/xl6_array_thread_admission.c',
    'src/runtime/xl6_queue_spsc_admission.c',
    'src/runtime/xl6_queue_mpsc_admission.c',
    'src/runtime/xl6_queue_mpmc_admission.c',
    'src/runtime/xl6_queue_spsc.c','src/runtime/xl6_queue_mpsc.c','src/runtime/xl6_queue_mpmc.c',
    'src/runtime/xl6_channel.c',
    'src/runtime/xl6_wait_queue.c',
    'src/runtime/xl6_wait_queue_admission.c',
    'src/runtime/xl6_path_build.c',
    'src/runtime/xl6_array_os_argv.c',
    'src/runtime/xl6_array_method_remove.c','src/runtime/xl6_value_contains.c','src/runtime/xl6_dynamic_collection.c',
    'src/runtime/xl6_array_default.c',
    'src/runtime/xl6_array_primitive_admission.c',
    'src/runtime/xl6_array_c_scalar_admission.c',
    'src/runtime/xl6_array_sequence_admission.c',
    'src/runtime/xl6_array_nullable_c_scalar_admission.c',
    'src/runtime/xl6_array_numeric_enum_admission.c',
    'src/runtime/xl6_array_callable_admission.c','src/runtime/xl6_array_future_admission.c','src/runtime/xl6_array_input.c',
    'src/runtime/xl6_array_rng_shuffle.c','src/runtime/xl6_array_rand_shuffle.c',
    'src/runtime/xl6_collection_count.c',
    'src/runtime/xl6_array_scalar_read.c',
    'src/runtime/xl6_array_string_read.c',
    'src/runtime/xl6_array_value_read.c',
    'src/runtime/xl6_array_import.c',
    'src/runtime/xl6_array_import_chain.c',
    'src/runtime/xl6_restore_class.c',
    'src/runtime/xl6_restore_class_closed.c',
    'src/runtime/xl6_restore_open.c',
    'src/runtime/xl6_list.c',
    'src/runtime/xl6_dict.c',
    'src/runtime/xl6_slot_map.c',
    'src/runtime/xl6_slot_map_key.c',
    'src/runtime/xl6_slot_map_key_value.c',
    'src/runtime/xl6_native_leaf_value.c',
    'src/runtime/xl6_array_native_value_admission.c',
    'src/runtime/xl6_array_native_value_policy.c',
    'src/runtime/xl6_nullable_slot_map_key_collection.c',
    'src/runtime/xl6_cell_native.c',
    'src/runtime/xl6_stack_callable.c',
    'src/runtime/xl6_array_extract_native_value.c',
    'src/runtime/xl6_slot_map_keys.c',
    'src/runtime/xl6_slot_map_pairs.c',
    'src/runtime/xl6_slot_map_iterator.c','src/runtime/xl6_slot_map_iterator_key.c',
    'src/runtime/xl6_slot_map_iterator_pair.c','src/runtime/xl6_slot_map_iterator_keys.c',
    'src/runtime/xl6_slot_map_iterator_values.c','src/runtime/xl6_slot_map_iterator_count.c',
    'src/runtime/xl6_capture_v2.c','src/runtime/xl6_slot_map_key_capture.c',
    'src/runtime/xl6_native_future_payload.c',
    'src/runtime/xl6_nullable_slot_map_key.c',
    'src/runtime/xl6_nullable_slot_map_key_capture.c',
    'src/runtime/xl6_closure_capture_v3.c',
    'src/runtime/xl6_dict_schema.c',
    'src/runtime/xl6_dict_value_index.c',
    'src/runtime/xl6_dict_methods.c',
    'src/runtime/xl6_dict_import_chain.c',
    'src/runtime/xl6_dict_graph.c','src/runtime/xl6_dict_value_iterator.c',
    'src/runtime/xl6_list_methods.c',
    'src/runtime/xl6_list_value_iterator.c',
    'src/runtime/xl6_list_value_read.c','src/runtime/xl6_value_index_read.c','src/runtime/xl6_value_index_write.c',
    'src/runtime/xl6_value_named_read.c','src/runtime/xl6_value_named_write.c',
    'src/runtime/xl6_value_object_fields.c',
    'src/runtime/xl6_list_scalar_read.c','src/runtime/xl6_list_string_read.c','src/runtime/xl6_list_value_write.c',
    'src/runtime/xl6_list_algebra.c',
    'src/runtime/xl6_value_copy.c',
    'src/runtime/xl6_list_graph.c',
    'src/runtime/xl6_value_copy_graph.c',
    'src/runtime/xl6_persistence_project_list.c',
    'src/runtime/xl6_list_import_chain.c',
    'src/runtime/xl6_list_schema.c',
    'src/runtime/xl6_set_key_policy.c',
    'src/runtime/xl6_set.c',
    'src/runtime/xl6_set_schema.c',
    'src/runtime/xl6_set_value_iterator.c',
    'src/runtime/xl6_collection_value_iterator.c',
    'src/runtime/xl6_set_graph.c',
    'src/runtime/xl6_value_copy_collections.c',
    'src/runtime/xl6_persistence_project_collections.c',
    'src/runtime/xl6_set_methods.c',
    'src/runtime/xl6_set_import_chain.c',
    'src/runtime/xl6_collection_count_all.c',
    'src/runtime/xl6_value_copy_set_graph.c',
    'src/runtime/xl6_persistence_project_set.c',
    'src/runtime/xl6_restore_class_set.c',
    'src/runtime/xl6_restore_class_collections.c',
    'src/runtime/xl6_value_wire_contains.c',
    'src/runtime/xl6_array_algebra.c',
    'src/runtime/xl6_array_graph.c',
    'src/runtime/xl6_persistence_project.c','src/runtime/xl6_persistence_project_open.c',
    'src/runtime/xl6_array_methods.c','src/runtime/xl6_array_method_insert.c','src/runtime/xl6_array_method_find.c',
    'src/runtime/xl6_array_extract_scalar.c','src/runtime/xl6_array_extract_string.c',
    'src/runtime/xl6_array_extract_value.c','src/runtime/xl6_collection_extract_value.c',
    'src/runtime/xl6_array_extract_f32.c','src/runtime/xl6_array_extract_char.c',
    'src/runtime/xl6_array_extract_trivial.c',
    'src/runtime/xl6_future_aggregate.c','src/runtime/xl6_future_aggregate_array.c',
    'src/runtime/xl6_future_metadata.c',
    'src/runtime/xl6_nullable_native_i8.c',
    'src/runtime/xl6_nullable_native_i8_value.c',
    'src/runtime/xl6_nullable_native_u8.c',
    'src/runtime/xl6_nullable_native_u8_value.c',
    'src/runtime/xl6_nullable_native_i16.c',
    'src/runtime/xl6_nullable_native_i16_value.c',
    'src/runtime/xl6_nullable_native_u16.c',
    'src/runtime/xl6_nullable_native_u16_value.c',
    'src/runtime/xl6_nullable_native_i32.c',
    'src/runtime/xl6_nullable_native_i32_value.c',
    'src/runtime/xl6_nullable_native_u32.c',
    'src/runtime/xl6_nullable_native_u32_value.c',
    'src/runtime/xl6_nullable_string.c',
    'src/runtime/xl6_string_type.c','src/runtime/xl6_string_value.c','src/runtime/xl6_array_value.c','src/runtime/xl6_array_reference_value.c','src/runtime/xl6_array_erased.c','src/runtime/xl6_value_admission.c','src/runtime/xl6_native_scalar_value.c','src/runtime/xl6_native_scalar_convert.c','src/runtime/xl6_value_native_scalar_convert.c','src/runtime/xl6_nominal_admission.c',
    'src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c','src/runtime/xl6_char_checked.c','src/runtime/xl6_stack_erased_methods.c','src/runtime/xl6_stack_erased_try.c','src/runtime/xl6_dynamic_product.c','src/runtime/xl6_dynamic_collection_vector.c','src/runtime/xl6_float_convert.c','src/runtime/xl6_integer_convert.c','src/runtime/xl6_sequence_projection_schema.c',
    'src/runtime/xl6_container_projection.c','src/runtime/xl6_list_projection_cast.c','src/runtime/xl6_set_projection_cast.c',
    'src/runtime/xl6_dict_projection_cast.c','src/runtime/xl6_tree_projection_cast.c',
    'src/runtime/xl6_value_domain_projection.c','src/runtime/xl6_channel_admission.c','src/runtime/xl6_slot_map_admission.c',
    'src/runtime/xl6_error_value.c','src/runtime/xl6_value_text.c',
    'src/runtime/xl6_print_values.c',
    'src/runtime/xl6_class_text.c','src/runtime/xl6_error_text.c',
    'src/runtime/xl6_nullable_bool.c','src/runtime/xl6_nullable_i64.c','src/runtime/xl6_nullable_u64.c',
    'src/runtime/xl6_nullable_f32.c','src/runtime/xl6_nullable_f64.c','src/runtime/xl6_nullable_time.c','src/runtime/xl6_nullable_char.c',
    'src/runtime/xl6_nullable_bool_value.c','src/runtime/xl6_nullable_i64_value.c','src/runtime/xl6_nullable_u64_value.c',
    'src/runtime/xl6_nullable_f32_value.c','src/runtime/xl6_nullable_f64_value.c','src/runtime/xl6_nullable_time_value.c','src/runtime/xl6_nullable_char_value.c',
    'src/runtime/xl6_text_split.c','src/runtime/xl6_text_join.c','src/runtime/xl6_text_iterator.c',
    'src/runtime/xl6_number_format.c','src/runtime/xl6_number_format_buffer.c',
    'src/runtime/xl6_number_integer.c','src/runtime/xl6_number_float.c',
    'src/runtime/xl6_number_integer_buffer.c','src/runtime/xl6_number_float_buffer.c',
    'src/runtime/xl6_unicode.c','src/runtime/xl6_unicode_validator.c','src/runtime/xl6_html.c',
    'src/runtime/xl6_encoding.c','src/runtime/xl6_encoding_meta.c',
    'src/runtime/xl6_encoding_detect.c',
    'src/runtime/xl6_regex_core.c','src/runtime/xl6_regex_match.c',
    'src/runtime/xl6_regex_iterator.c','src/runtime/xl6_regex_replace.c','src/runtime/xl6_regex_replace_buffer.c',
    'src/runtime/xl6_regex_split.c','src/runtime/xl6_regex_set.c',
    'src/runtime/xl6_regex_replace_func.c','src/runtime/xl6_regex_replace_func_buffer.c',
    'src/runtime/xl6_pattern_core.c','src/runtime/xl6_pattern_options.c','src/runtime/xl6_pattern_match.c',
    'src/runtime/xl6_pattern_builder.c',
    'src/runtime/xl6_time_core.c',
    'src/runtime/xl6_time_datetime.c','src/runtime/xl6_time_local.c',
    'src/runtime/xl6_net_address.c','src/runtime/xl6_net_resolve_address.c','src/runtime/xl6_ownership_domain_net_address.c',
    'src/runtime/xl6_net_interfaces.c',
    'src/runtime/xl6_net_backends.c',
    'src/runtime/xl6_net_backend_text.c',
    'src/runtime/xl6_net_interfaces_text.c',
    'src/runtime/xl6_net_interface_query.c',
    'src/runtime/xl6_net_interface_local.c',
    'src/runtime/xl6_ownership_domain_net_interfaces.c',
    'src/runtime/xl6_ownership_domain_net_interface.c',
    'src/runtime/xl6_ownership_domain_net_interface_address.c',
    'src/runtime/xl6_time_text.c','src/runtime/xl6_time_datetime_text.c','src/runtime/xl6_environment.c','src/runtime/xl6_signal_core.c','src/runtime/xl6_signal_queue.c','src/runtime/xl6_signal_task_group.c',
    'src/runtime/xl6_process_command.c','src/runtime/xl6_process_options.c','src/runtime/xl6_process_run.c','src/runtime/xl6_process_status.c',
    'src/runtime/xl6_process_open.c','src/runtime/xl6_process_terminal.c',
    'src/runtime/xl6_native_handle.c',
    'src/runtime/xl6_process_live.c','src/runtime/xl6_process_stream.c',
    'src/runtime/xl6_process_future.c',
    'src/runtime/xl6_process_pipeline_config.c','src/runtime/xl6_process_pipeline.c',
    'src/runtime/xl6_process_file.c',
    'src/runtime/xl6_process_terminal_config.c','src/runtime/xl6_process_terminal_live.c',
    'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_task_file.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c',
    'src/abi/xl6_abi_dump.c','src/abi/xl6_abi_json.c','src/abi/xl6_callable.c',
    'src/backend/xl6_build_backend.c','src/compile/xl6_compile_unit.c','src/compile/xl6_imports.c','src/compile/xl6_source_overlay.c','src/compile/xl6_source_report.c','src/compile/xl6_build_inputs.c',
    'src/compile/xl6_xrt_trim.c','src/resource/xl6_win_resource.c','src/package/xl6_package.c',
    'src/ast/xl6_ast.c','src/ast/xl6_ast_dump.c','src/type/xl6_type_model.c',
    'src/ir/xl6_ir.c','src/ir/xl6_ir_class_identity.c','src/ir/xl6_ir_lower.c','src/ir/xl6_ir_dump.c',
    'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
    'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
    'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
    'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
    'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
    'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c','src/sem/xl6_sem_index.c','src/sem/xl6_completion.c','src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c',
    $driverSource,'lib/tcc/tcctools.c','lib/tcc/tcc_utf8_io.c')
# Close the resident population from ordinary explicit build manifests, not a
# second stale hand-maintained list, directory glob, or alternate provider.
$runtimeManifests=@('tools/compiler_dll.rsp.in','build_dll.bat')
foreach($runtimeManifest in $runtimeManifests) {
    $runtimeText=[IO.File]::ReadAllText((Join-Path $rootPath $runtimeManifest)).Replace('\','/')
    foreach($runtimeMatch in [regex]::Matches($runtimeText,'\bsrc/runtime/[A-Za-z0-9_]+\.c\b')) {
        if($sources -notcontains $runtimeMatch.Value){$sources += $runtimeMatch.Value}
    }
}
$files = @(@(Get-ChildItem src,lib,release/include,res/tcc -Recurse -File) +
    @(Get-Item $driverSource,$PSCommandPath,tools/xlir_c_object_cache.ps1,tools/xrt_single_header_impl.c,release/xl.dll,tools/tcc_vfs_manifest_win64_xlc.txt,tools/compiler_dll.rsp.in,build_dll.bat) +
    @(Get-Item -LiteralPath $buildContext.Toolchain.Path)) | Sort-Object FullName -Unique
$records = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
New-Item -ItemType Directory -Force -Path $batchPath | Out-Null
Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $batchPath 'xl.dll') -Force
$headers = @()
# Stage the declared package layout, not a generated VFS or a release update.
foreach ($entry in (Get-Content -LiteralPath tools/tcc_vfs_manifest_win64_xlc.txt -Encoding UTF8)) {
    if ($entry.StartsWith('dir|include/__xl_runtime|')) {
        $parts = $entry.Split('|'); $directory = $parts[2].Replace('%XL6_ROOT%', $rootPath)
        if ($parts.Count -ne 4 -or $parts[3] -ne '*.h') { throw 'Unsupported private runtime header directory mapping' }
        foreach ($source in (Get-ChildItem -LiteralPath $directory -File -Filter '*.h' | Sort-Object Name)) {
            $target = Join-Path $batchPath ('include/__xl_runtime/' + $source.Name)
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            if (Test-Path -LiteralPath $target) { throw "Duplicate header staging target: $target" }
            Copy-Item -LiteralPath $source.FullName -Destination $target
            $hash = (Get-FileHash -LiteralPath $source.FullName).Hash
            if ((Get-FileHash -LiteralPath $target).Hash -ne $hash) { throw "Header staging mismatch: $source" }
            $headers += [pscustomobject]@{Path=$target;Sha256=$hash;Source=$source.FullName}
        }
        continue
    }
    if (!$entry.StartsWith('file|include/__xl_runtime/')) { continue }
    $parts = $entry.Split('|'); $source = $parts[2].Replace('%XL6_ROOT%', $rootPath)
    $target = Join-Path $batchPath $parts[1]
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    if (Test-Path -LiteralPath $target) { throw "Duplicate header staging target: $target" }
    Copy-Item -LiteralPath $source -Destination $target -Force
    $hash = (Get-FileHash -LiteralPath $source).Hash
    if ((Get-FileHash -LiteralPath $target).Hash -ne $hash) { throw "Header staging mismatch: $source" }
    $headers += [pscustomobject]@{Path=$target;Sha256=$hash;Source=$source}
}
if ($headers.Count -lt 6) { throw 'Private runtime package manifest is incomplete' }
# Physical source mirror for standalone native-component validation. This is
# not a shipping SDK or the nested import-xlang compiler resource package.
foreach ($entry in Get-Content -LiteralPath tools/tcc_vfs_manifest_win64_xlc.txt -Encoding UTF8) {
    $parts = $entry.Split('|')
    if ($parts.Length -lt 3 -or !$parts[1].StartsWith('__xl_compiler/') -or !$parts[2].StartsWith('%XL6_ROOT%')) { continue }
    if ($parts[1].Contains('..')) { throw "Unsafe private source target: $entry" }
    $sourceRoot = $parts[2].Replace('%XL6_ROOT%', $rootPath)
    $sourcesToStage = if ($parts[0] -eq 'dir') {
        $patterns = $parts[3].Split(';')
        @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | Where-Object {
            $name = $_.Name; @($patterns | Where-Object { $name -like $_ }).Count -gt 0
        })
    } else { @(Get-Item -LiteralPath $sourceRoot) }
    foreach ($source in $sourcesToStage) {
        $suffix = if ($parts[0] -eq 'dir') { '/' + [IO.Path]::GetRelativePath($sourceRoot, $source.FullName) } else { '' }
        $target = Join-Path $batchPath ('include/' + $parts[1] + $suffix)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        if (Test-Path -LiteralPath $target) { throw "Duplicate private source target: $target" }
        Copy-Item -LiteralPath $source.FullName -Destination $target
        $hash = (Get-FileHash -LiteralPath $source.FullName).Hash
        if ((Get-FileHash -LiteralPath $target).Hash -ne $hash) { throw "Private source staging mismatch: $source" }
        $headers += [pscustomobject]@{Path=$target;Sha256=$hash;Source=$source.FullName}
    }
}
$flags = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-DXL_STATIC_EMBED',"-DXL_ENABLE_EXE_COMPILER=$enableExe",'-DTCC_DISABLE_BUILTIN_VFS',
    '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
    '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
    "-I$rootPath","-I$rootPath/release","-I$rootPath/release/include","-I$rootPath/lib/tcc","-I$rootPath/build/test")
$objects = @($sources | ForEach-Object { Join-Path $batchPath ([IO.Path]::GetFileNameWithoutExtension($_) + '.o') })
if (@($objects | Select-Object -Unique).Count -ne $objects.Count) { throw 'Object basename collision' }
$objectRecords = @()
for ($index = 0; $index -lt $sources.Count; ++$index) {
    $source = $sources[$index]
    $sourceFlags = $flags
    if ($source -match '^src/runtime/xl6_(sync|mutex|cond|rwlock|array_sync_admission|thread|array_thread_admission)\.c$') {
        $sourceFlags = @($flags | Where-Object { !$_.StartsWith('-Wno-') }) + '-Wshadow'
    }
    if ($source -match '^lib/tcc/') { $sourceFlags += '-Wno-error' }
    $record = Invoke-XlirCObject -Context $buildContext -Source (Join-Path $rootPath $source) -Object $objects[$index] -Flags $sourceFlags
    $objectRecords += $record
    Write-Output "[$($index + 1)/$($sources.Count)] $(if ($record.CacheHit) {'reuse'} else {'compile'}) $source ($($record.TotalSeconds)s)"
}
Push-Location $batchPath
try {
    # Hundreds of absolute object paths exceed the Windows native command
    # limit in external evidence directories. GCC's response grammar owns
    # quoting; keep every argument and the actual consumed bytes in the receipt.
    $linkArguments=@($flags)+@($objects)+@((Join-Path $rootPath 'release/xl.dll'),
        '-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$binaryName)
    $linkResponse=Join-Path $batchPath 'link.rsp'
    $linkRows=@($linkArguments|ForEach-Object{'"'+$_.Replace('\','/').Replace('"','\"')+'"'})
    [IO.File]::WriteAllText($linkResponse,($linkRows -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    & $buildContext.Compiler ('@'+$linkResponse) *> link.log
    if ($LASTEXITCODE -ne 0) { Get-Content link.log -Tail 40; throw 'Public source compiler link failed' }
} finally { Pop-Location }
if (@($records | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) { throw 'Build inputs changed' }
Assert-XlirCToolchainUnchanged $buildContext
$manifest = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$records;Sources=$sources;RuntimeOnly=[bool]$RuntimeOnly;EnableExeCompiler=$enableExe;
    Driver=(Join-Path $batchPath $binaryName);DriverSha256=(Get-FileHash (Join-Path $batchPath $binaryName)).Hash;Headers=$headers;
    Objects=$objectRecords;ToolchainSha256=$buildContext.ToolchainSha256;
    LinkResponse=$linkResponse;LinkResponseSha256=(Get-FileHash -LiteralPath $linkResponse).Hash;
    CacheHits=@($objectRecords | Where-Object CacheHit).Count;CacheMisses=@($objectRecords | Where-Object {!$_.CacheHit}).Count;
    BuildSeconds=[Math]::Round($buildTimer.Elapsed.TotalSeconds,3);
    RuntimeSha256=(Get-FileHash release/xl.dll).Hash;
    SourceXrt=$true;
    Scope='Current-source public API/compiler/import/stdlib/Cgen/continuation runtime, generated XRT/xruntime implementation and TCC utilities. Released TCC core loaded dynamically; no release/VFS rebuild.'}
[IO.File]::WriteAllText((Join-Path $batchPath 'build.json'), ($manifest | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output "Public source driver built from $($sources.Count) translation units: $batchPath"
