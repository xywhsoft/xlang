@echo off
setlocal
cd /d %~dp0

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1
set "XL6_DLL_LINKS="%XL6_TCC_LIBTCC1%" -lWs2_32 -lIPHLPAPI -lShell32"
set "XL6_RELEASE_CC=gcc"
set "XL6_RELEASE_CFLAGS=-std=c11 -Wall -Irelease/include -Wno-pointer-sign -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format-truncation -Wno-dangling-pointer -Os -ffunction-sections -fdata-sections"
set "XL6_RELEASE_LDFLAGS=-shared -Wl,--gc-sections -s"
if "%XL6_DEBUG_BUILD%"=="1" (
    rem Preserve source locations and disable optimization for debugger-only builds.
    set "XL6_RELEASE_CFLAGS=-std=c11 -Wall -Irelease/include -Wno-pointer-sign -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format-truncation -Wno-dangling-pointer -O0 -g3"
    set "XL6_RELEASE_LDFLAGS=-shared"
)

where "%XL6_RELEASE_CC%" >nul 2>nul
if errorlevel 1 (
    echo DEMO6 DLL build failed: MinGW GCC was not found in PATH.
    exit /b 1
)

if not exist "%XL6_RELEASE_DIR%" mkdir "%XL6_RELEASE_DIR%"
if not exist build mkdir build
if not exist "%XL6_BUILD_TMP%" mkdir "%XL6_BUILD_TMP%"

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xrt_snapshot.ps1 -Root "%XL6_ROOT%"
if errorlevel 1 (
    echo DEMO6 DLL build failed: XRT snapshot is stale.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File tools\gen_xrt_symbol_table.ps1 -Root "%XL6_ROOT%"
if errorlevel 1 (
    echo DEMO6 DLL build failed: cannot generate the XRT runtime symbol table.
    exit /b 1
)

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

if not exist "lib\lzma\LzmaDec.c" (
    echo DEMO6 xl.dll build failed: missing LZMA decoder source "lib\lzma\LzmaDec.c".
    exit /b 1
)

if "%XL6_REGEN_GRAMMAR%"=="1" (
    echo Generating DEMO6 lexer...
    "%XL6_FLEX%" --noline -o src\lexer\xl6_lexer_gen.c --header-file=src\lexer\xl6_lexer_gen.h src\lexer\xl6_lexer.l
    if errorlevel 1 (
        echo DEMO6 lexer generation failed.
        exit /b 1
    )

    echo Generating DEMO6 parser...
    pushd src\parser
    "%XL6_ROOT%\%XL6_LEMON%" -q xl6_parser.y
    if errorlevel 1 (
        popd
        echo DEMO6 parser generation failed.
        exit /b 1
    )
    popd
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\normalize_lemon_parser.ps1 -Path src\parser\xl6_parser.c
    if errorlevel 1 (
        echo DEMO6 generated parser normalization failed.
        exit /b 1
    )
) else (
    if not exist src\lexer\xl6_lexer_gen.c (
        echo DEMO6 lexer source missing: src\lexer\xl6_lexer_gen.c.
        echo Set XL6_REGEN_GRAMMAR=1 and provide flex/lemon to regenerate it.
        exit /b 1
    )
    if not exist src\lexer\xl6_lexer_gen.h (
        echo DEMO6 lexer header missing: src\lexer\xl6_lexer_gen.h.
        echo Set XL6_REGEN_GRAMMAR=1 and provide flex/lemon to regenerate it.
        exit /b 1
    )
    if not exist src\parser\xl6_parser.c (
        echo DEMO6 parser source missing: src\parser\xl6_parser.c.
        echo Set XL6_REGEN_GRAMMAR=1 and provide flex/lemon to regenerate it.
        exit /b 1
    )
    if not exist src\parser\xl6_parser.h (
        echo DEMO6 parser header missing: src\parser\xl6_parser.h.
        echo Set XL6_REGEN_GRAMMAR=1 and provide flex/lemon to regenerate it.
        exit /b 1
    )
)

if "%XL6_REUSE_VFS_RESOURCES%"=="1" (
    if not exist "%XL6_BUILD_TMP%\tcc_builtin_resources_runtime.c" (
        echo DEMO6 DLL build failed: reusable runtime VFS resources are missing.
        exit /b 1
    )
    if not exist "%XL6_BUILD_TMP%\tcc_builtin_resources.c" (
        echo DEMO6 DLL build failed: reusable compiler VFS resources are missing.
        exit /b 1
    )
    echo Reusing generated embedded TCC VFS resources...
) else (
    echo Building embedded TCC VFS LZMA packer...
    "%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -DZ7_ST -Ilib\lzma %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_BUILD_TMP%\tcc_vfs_lzma_pack.exe" ^
        tools\tcc_vfs_lzma_pack.c ^
        lib\lzma\LzmaEnc.c ^
        lib\lzma\LzFind.c ^
        lib\lzma\CpuArch.c

    if errorlevel 1 (
        echo DEMO6 embedded TCC VFS LZMA packer build failed.
        exit /b 1
    )

    echo Generating runtime embedded TCC VFS resources...
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\gen_tcc_vfs_resources.ps1 -Root "%XL6_ROOT%" -Manifest tools\tcc_vfs_manifest_win64_runtime.txt -Output "%XL6_BUILD_TMP%\tcc_builtin_resources_runtime.c" -TccDist "%XL6_EMBED_TCC_DIST_X64%" -XrtObj "%XL6_XRT_OBJ%" -PackTool "%XL6_BUILD_TMP%\tcc_vfs_lzma_pack.exe"
    if errorlevel 1 (
        echo DEMO6 runtime embedded TCC VFS resource generation failed.
        exit /b 1
    )

    echo Generating compiler embedded TCC VFS resources...
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\gen_tcc_vfs_resources.ps1 -Root "%XL6_ROOT%" -Manifest tools\tcc_vfs_manifest_win64_xlc.txt -Output "%XL6_BUILD_TMP%\tcc_builtin_resources.c" -TccDist "%XL6_EMBED_TCC_DIST_X64%" -XrtObj "%XL6_XRT_OBJ%" -PackTool "%XL6_BUILD_TMP%\tcc_vfs_lzma_pack.exe"
    if errorlevel 1 (
        echo DEMO6 compiler embedded TCC VFS resource generation failed.
        exit /b 1
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_tcc_vfs_resources.ps1 -Root "%XL6_ROOT%" -Resource "%XL6_BUILD_TMP%\tcc_builtin_resources_runtime.c"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_tcc_vfs_resources.ps1 -Root "%XL6_ROOT%" -Resource "%XL6_BUILD_TMP%\tcc_builtin_resources.c"
if errorlevel 1 exit /b 1

echo Building DEMO6 xl.dll runtime...
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% %XL6_RELEASE_LDFLAGS% -DXL_BUILD_DLL -DXL_RUNTIME_ONLY_DLL -DXL_STATIC_TCC -DLIBTCC_AS_DLL -D_CRT_SECURE_NO_WARNINGS -DCONFIG_TCC_STATIC -I. -Irelease -Isrc -Ilib\lzma -I"%XL6_EMBED_TCC_SOURCE_ROOT%" %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xl.dll" ^
    src\api\xl_api.c ^
    src\api\xl_api_symbols.c ^
    src\api\xl_api_path.c ^
    src\api\xl_protocol_registry.c ^
    src\api\xl_channel_bridge.c ^
    src\api\xl_future_bridge.c ^
    src\api\xl_task_group_bridge.c ^
    src\runtime\xl6_continuation_queue.c ^
    src\runtime\xl6_continuation_executor.c ^
    src\runtime\xl6_continuation_future.c ^
    src\runtime\xl6_code_lease.c ^
    src\runtime\xl6_native_handle.c ^
    src\runtime\xl6_closure.c ^
    src\runtime\xl6_cell.c ^
    src\runtime\xl6_slot.c ^
    src\runtime\xl6_frame_storage.c ^
    src\runtime\xl6_task.c ^
    src\runtime\xl6_task_file.c ^
    src\runtime\xl6_text_value.c ^
    src\runtime\xl6_text_query.c ^
    src\runtime\xl6_text_binary.c ^
    src\runtime\xl6_buffer_value.c ^
    src\runtime\xl6_buffer_iterator.c ^
    src\runtime\xl6_buffer_hex.c ^
    src\runtime\xl6_buffer_base64.c ^
    src\runtime\xl6_buffer_core.c ^
    src\runtime\xl6_buffer_query.c ^
    src\runtime\xl6_sequence.c src\runtime\xl6_array.c src\runtime\xl6_array_iterator.c src\runtime\xl6_stack.c src\runtime\xl6_stack_batch.c src\runtime\xl6_array_reference_admission.c src\runtime\xl6_array_nominal_admission.c src\runtime\xl6_array_primitive_admission.c src\runtime\xl6_array_c_scalar_admission.c src\runtime\xl6_array_sequence_admission.c src\runtime\xl6_array_nullable_c_scalar_admission.c src\runtime\xl6_array_numeric_enum_admission.c src\runtime\xl6_array_callable_admission.c src\runtime\xl6_array_future_admission.c src\runtime\xl6_nullable_native_i8.c src\runtime\xl6_nullable_native_i8_value.c src\runtime\xl6_nullable_native_u8.c src\runtime\xl6_nullable_native_u8_value.c src\runtime\xl6_nullable_native_i16.c src\runtime\xl6_nullable_native_i16_value.c src\runtime\xl6_nullable_native_u16.c src\runtime\xl6_nullable_native_u16_value.c src\runtime\xl6_nullable_native_i32.c src\runtime\xl6_nullable_native_i32_value.c src\runtime\xl6_nullable_native_u32.c src\runtime\xl6_nullable_native_u32_value.c src\runtime\xl6_array_default.c ^
    src\runtime\xl6_nullable_bool.c ^
    src\runtime\xl6_nullable_i64.c ^
    src\runtime\xl6_nullable_u64.c ^
    src\runtime\xl6_nullable_f32.c ^
    src\runtime\xl6_nullable_f64.c ^
    src\runtime\xl6_nullable_time.c ^
    src\runtime\xl6_nullable_char.c ^
    src\runtime\xl6_nullable_bool_value.c src\runtime\xl6_nullable_i64_value.c src\runtime\xl6_nullable_u64_value.c ^
    src\runtime\xl6_nullable_f32_value.c src\runtime\xl6_nullable_f64_value.c src\runtime\xl6_nullable_time_value.c src\runtime\xl6_nullable_char_value.c ^
    src\runtime\xl6_nullable_string.c src\runtime\xl6_string_type.c src\runtime\xl6_string_value.c src\runtime\xl6_array_value.c src\runtime\xl6_array_reference_value.c src\runtime\xl6_array_erased.c src\runtime\xl6_array_input.c src\runtime\xl6_array_rng_shuffle.c src\runtime\xl6_array_rand_shuffle.c src\runtime\xl6_value_admission.c src\runtime\xl6_native_scalar_value.c src\runtime\xl6_nominal_admission.c src\runtime\xl6_char_type.c src\runtime\xl6_char_value.c ^
    src\runtime\xl6_future_aggregate.c src\runtime\xl6_future_aggregate_array.c src\runtime\xl6_future_metadata.c ^
    src\runtime\xl6_text_chars.c ^
    src\runtime\xl6_text_glob.c ^
    src\runtime\xl6_text_distance.c ^
    src\runtime\xl6_text_split.c ^
    src\runtime\xl6_text_join.c ^
    src\runtime\xl6_text_iterator.c ^
    src\runtime\xl6_number_format.c ^
    src\runtime\xl6_number_format_buffer.c ^
    src\runtime\xl6_number_integer.c ^
    src\runtime\xl6_number_float.c ^
    src\runtime\xl6_number_integer_buffer.c ^
    src\runtime\xl6_number_float_buffer.c ^
    src\runtime\xl6_unicode.c ^
    src\runtime\xl6_unicode_validator.c ^
    src\runtime\xl6_html.c ^
    src\runtime\xl6_encoding.c ^
    src\runtime\xl6_encoding_meta.c ^
    src\runtime\xl6_encoding_detect.c ^
    src\runtime\xl6_regex_core.c ^
    src\runtime\xl6_regex_match.c ^
    src\runtime\xl6_regex_iterator.c ^
    src\runtime\xl6_regex_replace.c ^
    src\runtime\xl6_regex_replace_buffer.c ^
    src\runtime\xl6_regex_split.c ^
    src\runtime\xl6_regex_set.c ^
    src\runtime\xl6_regex_replace_func.c ^
    src\runtime\xl6_regex_replace_func_buffer.c ^
    src\runtime\xl6_pattern_core.c ^
    src\runtime\xl6_pattern_builder.c ^
    src\runtime\xl6_time_core.c ^
    src\runtime\xl6_time_datetime.c ^
    src\runtime\xl6_time_local.c ^
    src\runtime\xl6_time_text.c ^
    src\runtime\xl6_time_datetime_text.c ^
    src\runtime\xl6_environment.c ^
    src\runtime\xl6_signal_core.c ^
    src\runtime\xl6_signal_queue.c ^
    src\runtime\xl6_signal_task_group.c ^
    src\runtime\xl6_process_command.c ^
    src\runtime\xl6_process_options.c ^
    src\runtime\xl6_process_run.c ^
    src\runtime\xl6_process_status.c ^
    src\runtime\xl6_process_open.c ^
    src\runtime\xl6_process_terminal.c ^
    src\runtime\xl6_process_live.c ^
    src\runtime\xl6_process_stream.c ^
    src\runtime\xl6_process_future.c ^
    src\runtime\xl6_process_pipeline_config.c ^
    src\runtime\xl6_process_pipeline.c ^
    src\runtime\xl6_process_file.c ^
    src\runtime\xl6_process_terminal_config.c ^
    src\runtime\xl6_process_terminal_live.c ^
    src\runtime\xl6_pattern_options.c ^
    src\runtime\xl6_pattern_match.c ^
    src\runtime\xl6_log.c ^
    src\runtime\xl6_log_console.c ^
    src\runtime\xl6_log_record.c ^
    src\runtime\xl6_log_text.c ^
    src\runtime\xl6_log_json.c ^
    src\runtime\xl6_log_custom.c ^
    src\runtime\xl6_log_file.c ^
    src\runtime\xl6_log_file_text.c ^
    src\runtime\xl6_log_file_json.c ^
    src\runtime\xl6_log_async.c ^
    src\runtime\xl6_log_ring.c ^
    src\runtime\xl6_log_printf.c ^
    src\runtime\xl6_log_text_writer.c ^
    src\runtime\xl6_log_json_writer.c ^
    src\runtime\xl6_network_service.c ^
    src\runtime\xl6_continuation_frame.c src\runtime\xl6_future_wait.c ^
    src\runtime\xl6_capture.c ^
    src\runtime\xl6_value_iterator.c ^
    src\runtime\xl6_future_callback.c ^
    src\runtime\xl6_ownership_plan.c ^
    src\runtime\xl6_generator.c ^
    src\runtime\xl6_code_finalizer.c ^
    src\runtime\xl6_future_payload.c ^
    src\api\xl_xrt_dll.c ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\libtcc.c" ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\tcctools.c" ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\tcc_builtin_vfs.c" ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\tcc_utf8_io.c" ^
    lib\lzma\LzmaDec.c ^
    "%XL6_BUILD_TMP%\tcc_builtin_resources_runtime.c" ^
    src\abi\xl6_abi_json.c ^
    src\abi\xl6_callable.c ^
    src\backend\xl6_build_backend.c ^
    src\compile\xl6_compile_unit.c ^
    src\compile\xl6_imports.c ^
    src\compile\xl6_xrt_trim.c ^
    src\common\xl6_console.c ^
    src\common\xl6_output.c ^
    src\diag\xl6_diag.c ^
    src\ast\xl6_ast.c ^
 src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\lexer\xl6_token.c ^
    src\lexer\xl6_lexer_api.c ^
    src\lexer\xl6_lexer_gen.c ^
    src\parser\xl6_parser.c ^
    src\parser\xl6_parser_api.c ^
    src\package\xl6_package.c ^
    src\stdlib\xl6_stdlib.c ^
    src\stdlib\xl6_stdlib_data.c ^
    src\type\xl6_type_model.c ^
    src\sem\xl6_protocol.c ^
    src\sem\xl6_type_registry.c ^
    src\sem\xl6_sem_docs.c ^
    src\sem\xl6_sem_field_attrs.c ^
    src\sem\xl6_sem.c ^
    %XL6_DLL_LINKS%

if errorlevel 1 (
    echo DEMO6 xl.dll build failed.
    exit /b 1
)

if exist "%XL6_RELEASE_DIR%\libtcc.dll" del /f /q "%XL6_RELEASE_DIR%\libtcc.dll" >nul 2>nul

echo Generating xl.dll import definition...
"%XL6_TCC%" -impdef "%XL6_RELEASE_DIR%\xl.dll" -o "%XL6_RELEASE_DIR%\xl.def"
if errorlevel 1 (
    echo DEMO6 xl.dll import definition failed.
    exit /b 1
)

echo Building DEMO6 xlc.dll compiler runtime...
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% %XL6_RELEASE_LDFLAGS% -DXL_BUILD_DLL -DXL_STATIC_TCC -DLIBTCC_AS_DLL -D_CRT_SECURE_NO_WARNINGS -DCONFIG_TCC_STATIC -I. -Irelease -Isrc -Ilib\lzma -I"%XL6_EMBED_TCC_SOURCE_ROOT%" %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xlc.dll" ^
    src\api\xl_api.c ^
    src\api\xl_api_symbols.c ^
    src\api\xl_api_path.c ^
    src\api\xl_protocol_registry.c ^
    src\api\xl_channel_bridge.c ^
    src\api\xl_future_bridge.c ^
    src\api\xl_task_group_bridge.c ^
    src\runtime\xl6_continuation_queue.c ^
    src\runtime\xl6_continuation_executor.c ^
    src\runtime\xl6_continuation_future.c ^
    src\runtime\xl6_code_lease.c ^
    src\runtime\xl6_native_handle.c ^
    src\runtime\xl6_closure.c ^
    src\runtime\xl6_cell.c ^
    src\runtime\xl6_slot.c ^
    src\runtime\xl6_frame_storage.c ^
    src\runtime\xl6_task.c ^
    src\runtime\xl6_task_file.c ^
    src\runtime\xl6_text_value.c ^
    src\runtime\xl6_text_query.c ^
    src\runtime\xl6_text_binary.c ^
    src\runtime\xl6_buffer_value.c ^
    src\runtime\xl6_buffer_iterator.c ^
    src\runtime\xl6_buffer_hex.c ^
    src\runtime\xl6_buffer_base64.c ^
    src\runtime\xl6_buffer_core.c ^
    src\runtime\xl6_buffer_query.c ^
    src\runtime\xl6_sequence.c src\runtime\xl6_array.c src\runtime\xl6_array_iterator.c src\runtime\xl6_stack.c src\runtime\xl6_stack_batch.c src\runtime\xl6_array_reference_admission.c src\runtime\xl6_array_nominal_admission.c src\runtime\xl6_array_primitive_admission.c src\runtime\xl6_array_c_scalar_admission.c src\runtime\xl6_array_sequence_admission.c src\runtime\xl6_array_nullable_c_scalar_admission.c src\runtime\xl6_array_numeric_enum_admission.c src\runtime\xl6_array_callable_admission.c src\runtime\xl6_array_future_admission.c src\runtime\xl6_nullable_native_i8.c src\runtime\xl6_nullable_native_i8_value.c src\runtime\xl6_nullable_native_u8.c src\runtime\xl6_nullable_native_u8_value.c src\runtime\xl6_nullable_native_i16.c src\runtime\xl6_nullable_native_i16_value.c src\runtime\xl6_nullable_native_u16.c src\runtime\xl6_nullable_native_u16_value.c src\runtime\xl6_nullable_native_i32.c src\runtime\xl6_nullable_native_i32_value.c src\runtime\xl6_nullable_native_u32.c src\runtime\xl6_nullable_native_u32_value.c src\runtime\xl6_array_default.c ^
    src\runtime\xl6_nullable_bool.c ^
    src\runtime\xl6_nullable_i64.c ^
    src\runtime\xl6_nullable_u64.c ^
    src\runtime\xl6_nullable_f32.c ^
    src\runtime\xl6_nullable_f64.c ^
    src\runtime\xl6_nullable_time.c ^
    src\runtime\xl6_nullable_char.c ^
    src\runtime\xl6_nullable_bool_value.c src\runtime\xl6_nullable_i64_value.c src\runtime\xl6_nullable_u64_value.c ^
    src\runtime\xl6_nullable_f32_value.c src\runtime\xl6_nullable_f64_value.c src\runtime\xl6_nullable_time_value.c src\runtime\xl6_nullable_char_value.c ^
    src\runtime\xl6_nullable_string.c src\runtime\xl6_string_type.c src\runtime\xl6_string_value.c src\runtime\xl6_array_value.c src\runtime\xl6_array_reference_value.c src\runtime\xl6_array_erased.c src\runtime\xl6_array_input.c src\runtime\xl6_array_rng_shuffle.c src\runtime\xl6_array_rand_shuffle.c src\runtime\xl6_value_admission.c src\runtime\xl6_native_scalar_value.c src\runtime\xl6_nominal_admission.c src\runtime\xl6_char_type.c src\runtime\xl6_char_value.c ^
    src\runtime\xl6_future_aggregate.c src\runtime\xl6_future_aggregate_array.c src\runtime\xl6_future_metadata.c ^
    src\runtime\xl6_text_chars.c ^
    src\runtime\xl6_text_glob.c ^
    src\runtime\xl6_text_distance.c ^
    src\runtime\xl6_text_split.c ^
    src\runtime\xl6_text_join.c ^
    src\runtime\xl6_text_iterator.c ^
    src\runtime\xl6_number_format.c ^
    src\runtime\xl6_number_format_buffer.c ^
    src\runtime\xl6_number_integer.c ^
    src\runtime\xl6_number_float.c ^
    src\runtime\xl6_number_integer_buffer.c ^
    src\runtime\xl6_number_float_buffer.c ^
    src\runtime\xl6_unicode.c ^
    src\runtime\xl6_unicode_validator.c ^
    src\runtime\xl6_html.c ^
    src\runtime\xl6_encoding.c ^
    src\runtime\xl6_encoding_meta.c ^
    src\runtime\xl6_encoding_detect.c ^
    src\runtime\xl6_regex_core.c ^
    src\runtime\xl6_regex_match.c ^
    src\runtime\xl6_regex_iterator.c ^
    src\runtime\xl6_regex_replace.c ^
    src\runtime\xl6_regex_replace_buffer.c ^
    src\runtime\xl6_regex_split.c ^
    src\runtime\xl6_regex_set.c ^
    src\runtime\xl6_regex_replace_func.c ^
    src\runtime\xl6_regex_replace_func_buffer.c ^
    src\runtime\xl6_pattern_core.c ^
    src\runtime\xl6_pattern_builder.c ^
    src\runtime\xl6_time_core.c ^
    src\runtime\xl6_time_datetime.c ^
    src\runtime\xl6_time_local.c ^
    src\runtime\xl6_time_text.c ^
    src\runtime\xl6_time_datetime_text.c ^
    src\runtime\xl6_environment.c ^
    src\runtime\xl6_signal_core.c ^
    src\runtime\xl6_signal_queue.c ^
    src\runtime\xl6_signal_task_group.c ^
    src\runtime\xl6_process_command.c ^
    src\runtime\xl6_process_options.c ^
    src\runtime\xl6_process_run.c ^
    src\runtime\xl6_process_status.c ^
    src\runtime\xl6_process_open.c ^
    src\runtime\xl6_process_terminal.c ^
    src\runtime\xl6_process_live.c ^
    src\runtime\xl6_process_stream.c ^
    src\runtime\xl6_process_future.c ^
    src\runtime\xl6_process_pipeline_config.c ^
    src\runtime\xl6_process_pipeline.c ^
    src\runtime\xl6_process_file.c ^
    src\runtime\xl6_process_terminal_config.c ^
    src\runtime\xl6_process_terminal_live.c ^
    src\runtime\xl6_pattern_options.c ^
    src\runtime\xl6_pattern_match.c ^
    src\runtime\xl6_log.c ^
    src\runtime\xl6_log_console.c ^
    src\runtime\xl6_log_record.c ^
    src\runtime\xl6_log_text.c ^
    src\runtime\xl6_log_json.c ^
    src\runtime\xl6_log_custom.c ^
    src\runtime\xl6_log_file.c ^
    src\runtime\xl6_log_file_text.c ^
    src\runtime\xl6_log_file_json.c ^
    src\runtime\xl6_log_async.c ^
    src\runtime\xl6_log_ring.c ^
    src\runtime\xl6_log_printf.c ^
    src\runtime\xl6_log_text_writer.c ^
    src\runtime\xl6_log_json_writer.c ^
    src\runtime\xl6_network_service.c ^
    src\runtime\xl6_continuation_frame.c src\runtime\xl6_future_wait.c ^
    src\runtime\xl6_capture.c ^
    src\runtime\xl6_value_iterator.c ^
    src\runtime\xl6_future_callback.c ^
    src\runtime\xl6_ownership_plan.c ^
    src\runtime\xl6_generator.c ^
    src\runtime\xl6_code_finalizer.c ^
    src\runtime\xl6_future_payload.c ^
    src\api\xl_xrt_dll.c ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\libtcc.c" ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\tcctools.c" ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\tcc_builtin_vfs.c" ^
    "%XL6_EMBED_TCC_SOURCE_ROOT%\tcc_utf8_io.c" ^
    lib\lzma\LzmaDec.c ^
    "%XL6_BUILD_TMP%\tcc_builtin_resources.c" ^
    src\abi\xl6_abi_dump.c ^
    src\abi\xl6_abi_json.c ^
    src\abi\xl6_callable.c ^
    src\backend\xl6_build_backend.c ^
    src\compile\xl6_compile_unit.c ^
    src\compile\xl6_imports.c ^
    src\compile\xl6_xrt_trim.c ^
    src\common\xl6_console.c ^
    src\common\xl6_output.c ^
    src\diag\xl6_diag.c ^
    src\ast\xl6_ast.c ^
    src\ast\xl6_ast_dump.c ^
 src\ir\xl6_ir.c src\ir\xl6_ir_lower.c ^
    src\cgen\xl6_cgen_shared_runtime.c ^
    src\cgen\xl6_ir_cgen.c ^
    src\ir\xl6_ir_dump.c ^
    src\lexer\xl6_token.c ^
    src\lexer\xl6_lexer_api.c ^
    src\lexer\xl6_lexer_gen.c ^
    src\parser\xl6_parser.c ^
    src\parser\xl6_parser_api.c ^
    src\package\xl6_package.c ^
    src\resource\xl6_win_resource.c ^
    src\stdlib\xl6_stdlib.c ^
    src\stdlib\xl6_stdlib_data.c ^
    src\type\xl6_type_model.c ^
    src\sem\xl6_protocol.c ^
    src\sem\xl6_type_registry.c ^
    src\sem\xl6_sem_docs.c ^
    src\sem\xl6_sem_field_attrs.c ^
    src\sem\xl6_sem.c ^
    %XL6_DLL_LINKS%

if errorlevel 1 (
    echo DEMO6 xlc.dll build failed.
    exit /b 1
)

echo Generating xlc.dll import definition...
"%XL6_TCC%" -impdef "%XL6_RELEASE_DIR%\xlc.dll" -o "%XL6_RELEASE_DIR%\xlc.def"
if errorlevel 1 (
    echo DEMO6 xlc.dll import definition failed.
    exit /b 1
)

echo Building xl.dll dependency fixtures...
"%XL6_TCC%" %XL6_TCC_INCLUDES% -DXL6_EXAMPLE_NO_STANDALONE_MAIN -c release\examples\xl_dll_link_host.c -o "%XL6_BUILD_TMP%\xl_dll_link_host.o"
if errorlevel 1 (
    echo DEMO6 xl.dll static dependency fixture build failed.
    exit /b 1
)
"%XL6_TCC%" -ar rcs "%XL6_BUILD_TMP%\libxl6dlllink.a" "%XL6_BUILD_TMP%\xl_dll_link_host.o"
if errorlevel 1 (
    echo DEMO6 xl.dll static dependency fixture archive failed.
    exit /b 1
)
"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% -DXL6_EXAMPLE_NO_STANDALONE_MAIN release\examples\xl_dll_loadlib_host.c -o "%XL6_BUILD_TMP%\xl_dll_loadlib_host.dll"
if errorlevel 1 (
    echo DEMO6 xl.dll loadlib dependency fixture build failed.
    exit /b 1
)
"%XL6_TCC%" -impdef "%XL6_BUILD_TMP%\xl_dll_loadlib_host.dll" -o "%XL6_BUILD_TMP%\xl_dll_loadlib_host.def"
if errorlevel 1 (
    echo DEMO6 xl.dll loadlib dependency fixture import definition failed.
    exit /b 1
)

echo Building xlc.dll compile verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xlc_dll_embed_string.exe" ^
    release\examples\xl_dll_embed_string.c ^
    "%XL6_RELEASE_DIR%\xlc.def"

if errorlevel 1 (
    echo DEMO6 xlc.dll verification program build failed.
    exit /b 1
)

echo Building xlc.dll compiler API verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xlc_api_embed.exe" ^
    release\examples\xlc_api_embed.c ^
    "%XL6_RELEASE_DIR%\xlc.def"

if errorlevel 1 (
    echo DEMO6 xlc.dll compiler API verification program build failed.
    exit /b 1
)

echo Building xl.dll XRT export verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xl_dll_xrt_exports.exe" ^
    release\examples\xl_dll_xrt_exports.c ^
    "%XL6_RELEASE_DIR%\xl.def"

if errorlevel 1 (
    echo DEMO6 xl.dll XRT export verification program build failed.
    exit /b 1
)

echo Building xl.dll dynamic run GUI demo...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -mwindows -o "%XL6_RELEASE_DIR%\xl_run_demo.exe" ^
    release\examples\xl_run_demo_win32.c ^
    "%XL6_RELEASE_DIR%\xl.def" ^
    -luser32 -lgdi32

if errorlevel 1 (
    echo DEMO6 xl.dll dynamic run GUI demo build failed.
    exit /b 1
)

echo Building xl.dll API boundary verification program...
"%XL6_TCC%" -std=c11 -Wall -DXL_RUNTIME_ONLY_API -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xl_dll_null_context.exe" ^
    release\examples\xl_dll_null_context.c ^
    "%XL6_RELEASE_DIR%\xl.def"

if errorlevel 1 (
    echo DEMO6 xl.dll API boundary verification program build failed.
    exit /b 1
)

echo Building xl.dll threaded host verification program...
"%XL6_TCC%" -std=c11 -Wall -DXL_RUNTIME_ONLY_API -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xl_dll_threaded_host.exe" ^
    release\examples\xl_dll_threaded_host.c ^
    "%XL6_RELEASE_DIR%\xl.def"

if errorlevel 1 (
    echo DEMO6 xl.dll threaded host verification program build failed.
    exit /b 1
)

echo Building xl.dll module dependency lifetime verification program...
"%XL6_TCC%" -std=c11 -Wall -DXL_RUNTIME_ONLY_API -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xl_dll_module_dependency_lifetime.exe" ^
    test\cases\xl_dll_module_dependency_lifetime.c ^
    "%XL6_RELEASE_DIR%\xl.def"

if errorlevel 1 (
    echo DEMO6 xl.dll module dependency lifetime verification program build failed.
    exit /b 1
)

echo DEMO6 xl.dll/xlc.dll build ok: "%XL6_RELEASE_DIR%\xl.dll" "%XL6_RELEASE_DIR%\xlc.dll"
exit /b 0
