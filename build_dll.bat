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

python tools\check_stdlib_shipping_sources.py --root "%XL6_ROOT%"
if errorlevel 1 exit /b 1

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

rem Safe native constructors are separate from lossless read/box providers.
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_template.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_template.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_template_call.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_template_call.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_template_engine.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_template_engine.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_deflater.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_deflater.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_inflater.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_inflater.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_time_date_time.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_time_date_time.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_unicode_validator.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_unicode_validator.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_text_iterator.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_text_iterator.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_pattern_core.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_pattern_core.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_pattern_options.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_pattern_options.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_pattern_match.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_pattern_match.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_pattern_builder.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_pattern_builder.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_process_pipeline_config.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_process_pipeline_config.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_process_pipeline.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_process_pipeline.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_process_run.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_process_run.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_regex_core.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_regex_core.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_regex_match.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_regex_match.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_regex_iterator.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_regex_iterator.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_regex_split.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_regex_split.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_regex_set.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_regex_set.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_regex_replace_func.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_regex_replace_func.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_signal_queue.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_signal_queue.o"
if errorlevel 1 exit /b 1

rem Audited nested-runtime context policy stays resident in both DLL targets.
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_core.c -o "%XL6_BUILD_TMP%\xl6_vfs_core.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_memory.c -o "%XL6_BUILD_TMP%\xl6_vfs_memory.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_disk.c -o "%XL6_BUILD_TMP%\xl6_vfs_disk.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_pack.c -o "%XL6_BUILD_TMP%\xl6_vfs_pack.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_file.c -o "%XL6_BUILD_TMP%\xl6_vfs_file.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_info.c -o "%XL6_BUILD_TMP%\xl6_vfs_info.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_vfs_dir.c -o "%XL6_BUILD_TMP%\xl6_vfs_dir.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_fs_info_owner.c -o "%XL6_BUILD_TMP%\xl6_fs_info_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_fs_dir_owner.c -o "%XL6_BUILD_TMP%\xl6_fs_dir_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_vfs_core.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_vfs_core.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_vfs_memory.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_vfs_memory.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_vfs_disk.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_vfs_disk.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_vfs_pack.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_vfs_pack.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_fs_info_owner.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_fs_info_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_fs_dir_owner.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_fs_dir_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_io_reader.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_io_reader.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_io_writer.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_io_writer.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_io_line.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_io_line.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_fs_map_owner.c -o "%XL6_BUILD_TMP%\xl6_fs_map_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_fs_lock_owner.c -o "%XL6_BUILD_TMP%\xl6_fs_lock_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_fs_root_owner.c -o "%XL6_BUILD_TMP%\xl6_fs_root_owner.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_fs_map.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_fs_map.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_fs_lock.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_fs_lock.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_fs_root.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_fs_root.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_task_group.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_task_group.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_process_future.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_process_future.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_task_group_future.c -o "%XL6_BUILD_TMP%\xl6_task_group_future.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_task_group_start.c -o "%XL6_BUILD_TMP%\xl6_task_group_start.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_native_future_payload.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_native_future_payload.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_read_file.c -o "%XL6_BUILD_TMP%\xl6_io_read_file.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_write_file.c -o "%XL6_BUILD_TMP%\xl6_io_write_file.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_reader.c -o "%XL6_BUILD_TMP%\xl6_io_reader.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_reader_data.c -o "%XL6_BUILD_TMP%\xl6_io_reader_data.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_reader_position.c -o "%XL6_BUILD_TMP%\xl6_io_reader_position.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_writer.c -o "%XL6_BUILD_TMP%\xl6_io_writer.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_writer_data.c -o "%XL6_BUILD_TMP%\xl6_io_writer_data.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_writer_position.c -o "%XL6_BUILD_TMP%\xl6_io_writer_position.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_line.c -o "%XL6_BUILD_TMP%\xl6_io_line.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_copy.c -o "%XL6_BUILD_TMP%\xl6_io_copy.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_file.c -o "%XL6_BUILD_TMP%\xl6_io_file.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_reader_open.c -o "%XL6_BUILD_TMP%\xl6_io_reader_open.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_reader_memory.c -o "%XL6_BUILD_TMP%\xl6_io_reader_memory.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_reader_standard.c -o "%XL6_BUILD_TMP%\xl6_io_reader_standard.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_writer_open.c -o "%XL6_BUILD_TMP%\xl6_io_writer_open.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_writer_buffer.c -o "%XL6_BUILD_TMP%\xl6_io_writer_buffer.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_io_writer_standard.c -o "%XL6_BUILD_TMP%\xl6_io_writer_standard.o"
if errorlevel 1 exit /b 1

"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_process_options.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_process_options.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_random_state.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_random_state.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_resource_fs_file.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_resource_fs_file.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_random_state.c -o "%XL6_BUILD_TMP%\xl6_random_state.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_fs_file_owner.c -o "%XL6_BUILD_TMP%\xl6_fs_file_owner.o"
if errorlevel 1 exit /b 1

"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_xlang_context_policy.c -o "%XL6_BUILD_TMP%\xl6_xlang_context_policy.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_xlang_context.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_xlang_context.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_ownership_domain_buffer.c -o "%XL6_BUILD_TMP%\xl6_ownership_domain_buffer.o"
if errorlevel 1 exit /b 1

"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_native_scalar_convert.c -o "%XL6_BUILD_TMP%\xl6_native_scalar_convert.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_value_native_scalar_convert.c -o "%XL6_BUILD_TMP%\xl6_value_native_scalar_convert.o"
if errorlevel 1 exit /b 1

rem Compile the resident Channel provider once for both DLL targets. Keeping
rem this object in DLL_LINKS also covers locally supplied response templates.
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_channel.c -o "%XL6_BUILD_TMP%\xl6_channel.o"
if errorlevel 1 (
    echo DEMO6 DLL build failed: resident Channel provider compilation failed.
    exit /b 1
)
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_wait_queue.c -o "%XL6_BUILD_TMP%\xl6_wait_queue.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_wait_queue_admission.c -o "%XL6_BUILD_TMP%\xl6_wait_queue_admission.o"
if errorlevel 1 exit /b 1

rem Keep the private optional-record supplier in both resident DLL targets.
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_nullable_slot_map_key.c -o "%XL6_BUILD_TMP%\xl6_nullable_slot_map_key.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_nullable_slot_map_key_capture.c -o "%XL6_BUILD_TMP%\xl6_nullable_slot_map_key_capture.o"
if errorlevel 1 (
    echo DEMO6 DLL build failed: optional native record compilation failed.
    exit /b 1
)
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_closure_capture_v3.c -o "%XL6_BUILD_TMP%\xl6_closure_capture_v3.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_array_native_value_policy.c -o "%XL6_BUILD_TMP%\xl6_array_native_value_policy.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_nullable_slot_map_key_collection.c -o "%XL6_BUILD_TMP%\xl6_nullable_slot_map_key_collection.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_cell_native.c -o "%XL6_BUILD_TMP%\xl6_cell_native.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_stack_callable.c -o "%XL6_BUILD_TMP%\xl6_stack_callable.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_stack_erased.c -o "%XL6_BUILD_TMP%\xl6_stack_erased.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_char_checked.c -o "%XL6_BUILD_TMP%\xl6_char_checked.o"
if errorlevel 1 exit /b 1
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_stack_erased_methods.c -o "%XL6_BUILD_TMP%\xl6_stack_erased_methods.o"
if errorlevel 1 exit /b %errorlevel%
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_stack_erased_try.c -o "%XL6_BUILD_TMP%\xl6_stack_erased_try.o"
if errorlevel 1 exit /b %errorlevel%
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_dynamic_product.c -o "%XL6_BUILD_TMP%\xl6_dynamic_product.o"
if errorlevel 1 exit /b %errorlevel%
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_dynamic_collection_vector.c -o "%XL6_BUILD_TMP%\xl6_dynamic_collection_vector.o"
if errorlevel 1 exit /b %errorlevel%
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_float_convert.c -o "%XL6_BUILD_TMP%\xl6_float_convert.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_integer_convert.c -o "%XL6_BUILD_TMP%\xl6_integer_convert.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_sequence_projection_schema.c -o "%XL6_BUILD_TMP%\xl6_sequence_projection_schema.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_container_projection.c -o "%XL6_BUILD_TMP%\xl6_container_projection.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_list_projection_cast.c -o "%XL6_BUILD_TMP%\xl6_list_projection_cast.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_set_projection_cast.c -o "%XL6_BUILD_TMP%\xl6_set_projection_cast.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_dict_projection_cast.c -o "%XL6_BUILD_TMP%\xl6_dict_projection_cast.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_tree_projection_cast.c -o "%XL6_BUILD_TMP%\xl6_tree_projection_cast.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_value_domain_projection.c -o "%XL6_BUILD_TMP%\xl6_value_domain_projection.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_channel_admission.c -o "%XL6_BUILD_TMP%\xl6_channel_admission.o"
if errorlevel 1 goto :fail
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% -DXL_BUILD_DLL -I. -Irelease -Isrc -c src\runtime\xl6_slot_map_admission.c -o "%XL6_BUILD_TMP%\xl6_slot_map_admission.o"
if errorlevel 1 goto :fail

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

    python tools\precompile_builtin.py --root "%XL6_ROOT%" --tcc "%XL6_TCC%" --tcc-dist "%XL6_EMBED_TCC_DIST_X64%" --output "%XL6_BUILD_TMP%\precompiled"
    if errorlevel 1 exit /b 1
    if not errorlevel 0 exit /b 1
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

powershell -NoProfile -ExecutionPolicy Bypass -File tools\gen_dll_object_response.ps1 -Directory "%XL6_BUILD_TMP%"
if errorlevel 1 exit /b 1
set "XL6_DLL_LINKS=%XL6_DLL_LINKS% @"%XL6_BUILD_TMP%\dll_objects.rsp""

echo Building DEMO6 xl.dll runtime...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_release_dll.ps1 -Target runtime

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
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_release_dll.ps1 -Target compiler

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
