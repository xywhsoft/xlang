@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: typed func, XCall and C callback ABI...

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_value_lifetime.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_index_conditional_assign.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_storage_conditional_assign.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_member_assignment_transaction.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_storage_assignment_transaction.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_storage_exchange.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_product_await_owner.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_generator_frame_loan.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_continuation_owner_views.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_continuation_owner_cfg.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_continuation_owner_calls.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_result_commit.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_frame_completion.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_worker_completion.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_cancel_watch.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_source_lease.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_string_allocation_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_enum_allocation_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_boxing_allocation_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_adapter_cleanup_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_closure_allocation_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cell_allocation_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cell_loop_binding.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_iteration_allocation.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_generator_close_routes.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cell_continuation_outcome.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cell_thread_lifetime.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cell_cancel_race.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_property_assignment_transaction.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_virtual_call_outcome.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_virtual_activation.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_owned_activation.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_module_task_pool.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_checked_projection.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_owned_projection.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_evaluation_cleanup.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_match_cleanup.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_terminal_outcome.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_async_pool_outcome.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callback_boundary.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_task_value_types.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_task_byval.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callable_byval.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_xcall_embedded_nul.xl --emit build\test\cgen_string_xcall_embedded_nul.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -fsyntax-only build\test\cgen_string_xcall_embedded_nul.c
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_string_xcall_embedded_nul.xl -o build\test\cgen_string_xcall_embedded_nul.exe
if errorlevel 1 exit /b 1
build\test\cgen_string_xcall_embedded_nul.exe
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callable_result.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_protocol_call_outcome.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_evaluation.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_indexed_accessor.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_property_update.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_member_get.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_bound_method.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_member_set.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_owner_completion.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_product_owner.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_owner_consumption.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_managed_storage_inputs.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_managed_storage_inputs.xl --emit build\test\xlir_managed_storage_inputs.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_return_commit.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_return_commit.xl --emit build\test\xlir_return_commit.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_return_projection_owner.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_return_projection_owner.xl --emit build\test\xlir_return_projection_owner.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_conditional_nullable_join.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_conditional_nullable_join.xl --emit build\test\xlir_conditional_nullable_join.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_exit_route.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_exit_route.xl --emit build\test\xlir_exit_route.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_abrupt_cleanup_outcome.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_abrupt_cleanup_outcome.xl --emit build\test\xlir_abrupt_cleanup_outcome.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_loop_cleanup_outcome.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_loop_cleanup_outcome.xl --emit build\test\xlir_loop_cleanup_outcome.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_pending_exception_route.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_pending_exception_outcome.xl --emit build\test\xlir_pending_exception_outcome.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_pending_exception_control.xl --emit build\test\xlir_pending_exception_control.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_parameter_cleanup_outcome.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_parameter_cleanup_outcome.xl --emit build\test\xlir_parameter_cleanup_outcome.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scope_exit_plan.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_scope_exit_plan.xl --emit build\test\xlir_scope_exit_plan.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scope_cleanup_outcome.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_scope_cleanup_outcome.xl --emit build\test\xlir_scope_cleanup_outcome.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_xcall_result_failure.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_xcall_result_failure.xl --emit build\test\xlir_xcall_result_failure.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_foreign_error_scope.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_foreign_scope_nested.xl --emit build\test\xlir_foreign_scope_nested.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_foreign_raw_callback_error.xl --emit build\test\xlir_foreign_raw_callback_error.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_cleanup_detach.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_native_take_reentry.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_foreign_result_rollback.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_export_transaction.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_error_unwind.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_module_cleanup_outcome.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_class_cleanup_outcome.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_class_cleanup_outcome.xl --emit build\test\xlir_class_cleanup_outcome.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_module_cleanup_outcome.xl --emit build\test\xlir_module_cleanup_outcome.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_error_unwind.xl --emit build\test\xlir_error_unwind.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_export_transaction.xl --emit build\test\xlir_export_transaction.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_foreign_result_rollback.xl --emit build\test\xlir_foreign_result_rollback.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_cleanup_detach_reentry.xl --emit build\test\xlir_cleanup_detach_reentry.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_closure_argument_conversion.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_closure_argument_conversion.xl --emit build\test\xlir_closure_argument_conversion.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_closure_argument_evaluation_order.xl --emit build\test\xlir_closure_argument_evaluation_order.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_export_callback.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_export_callback_lifetime.xl --emit build\test\xlir_export_callback_lifetime.c
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\cgen_c_callback_interop.xl --lowered --emit build\test\xlir_c_callback_interop.lowered.xlir
if errorlevel 1 exit /b 1
findstr /C:"borrow/raw-c-callback" build\test\xlir_c_callback_interop.lowered.xlir >nul
if errorlevel 1 (
    echo XLIR callback call target is missing its raw C physical ABI.
    exit /b 1
)
findstr /C:"raw-callback=direct-thunk" build\test\xlir_c_callback_interop.lowered.xlir >nul
if errorlevel 1 (
    echo XLIR callback closure is missing its verified direct thunk ABI.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_c_callback_interop.xl --backend xlir --emit build\test\xlir_c_callback_interop.c
if errorlevel 1 exit /b 1
findstr /C:"typedef int (*xlir_raw_callback_t" build\test\xlir_c_callback_interop.c >nul
if errorlevel 1 (
    echo XLIR generated C is missing its exact raw callback typedef.
    exit /b 1
)
findstr /C:"xlir_raw_callback_entry_" build\test\xlir_c_callback_interop.c >nul
if errorlevel 1 (
    echo XLIR generated C is missing its verified static callback entry.
    exit /b 1
)
findstr /C:"->raw_callback : NULL" build\test\xlir_c_callback_interop.c >nul
if errorlevel 1 (
    echo XLIR generated C is not consuming the closure raw callback ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_c_callback_interop_host.c %XL6_CGEN_LINKS% -o build\test\xlir_c_callback_interop.exe
if errorlevel 1 exit /b 1
build\test\xlir_c_callback_interop.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_xcall_class.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_xcall_error_bridge.xl test\cases\cgen_xcall_error_bridge_host.c
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_callback_multi_return_invalid.xl >nul 2> build\test\sem_c_callback_multi_return_invalid.err.txt
if not errorlevel 1 (
    echo sem_c_callback_multi_return_invalid.xl should fail.
    exit /b 1
)
findstr /C:"multiple return values" build\test\sem_c_callback_multi_return_invalid.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_callback_string_return_invalid.xl >nul 2> build\test\sem_c_callback_string_return_invalid.err.txt
if not errorlevel 1 (
    echo sem_c_callback_string_return_invalid.xl should fail.
    exit /b 1
)
findstr /C:"ownership protocol" build\test\sem_c_callback_string_return_invalid.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_callback_byref_invalid.xl >nul 2> build\test\sem_c_callback_byref_invalid.err.txt
if not errorlevel 1 (
    echo sem_c_callback_byref_invalid.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_c_callback_byref_invalid.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_callback_signature_mismatch_invalid.xl >nul 2> build\test\sem_c_callback_signature_mismatch_invalid.err.txt
if not errorlevel 1 (
    echo sem_c_callback_signature_mismatch_invalid.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=[System.IO.File]::ReadAllText('build\test\sem_c_callback_signature_mismatch_invalid.err.txt',[System.Text.Encoding]::UTF8); $n=[string]([char]0x76EE)+[char]0x6807+[char]0x7B7E+[char]0x540D; if($t.IndexOf($n)-lt 0){exit 1}"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_callback_vararg_invalid.xl >nul 2> build\test\sem_c_callback_vararg_invalid.err.txt
if not errorlevel 1 (
    echo sem_c_callback_vararg_invalid.xl should fail.
    exit /b 1
)
findstr /C:"raw C function pointer" build\test\sem_c_callback_vararg_invalid.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_c_callback_capture_invalid.xl --backend xlir --emit build\test\xlir_c_callback_capture_invalid.c > build\test\xlir_c_callback_capture_invalid.err.txt 2>&1
if not errorlevel 1 (
    echo XLIR must reject a captured closure at a raw C callback boundary.
    exit /b 1
)
findstr /C:"XLIR raw C callback argument has no verified direct-thunk provenance or propagated parameter contract" build\test\xlir_c_callback_capture_invalid.err.txt >nul
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_foreign_bridges.ps1
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: typed func, XCall and C callback ABI
exit /b 0
