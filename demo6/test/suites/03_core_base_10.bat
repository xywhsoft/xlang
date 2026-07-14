@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 10/10...

release\xl.exe run release\examples\stdlib_random.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_random_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_random_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_random_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_random_c.exe > build\test\stdlib_random_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_random_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_random_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib random xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_buffer.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_buffer.xl --emit build\test\stdlib_buffer.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Buffer" build\test\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing Buffer class bridge.
    exit /b 1
)
findstr /C:"xbuffer xrtBufferCreate" build\test\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing xrtBufferCreate extern prototype.
    exit /b 1
)
findstr /C:"bool xrtBufferAppend" build\test\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing xrtBufferAppend extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_buffer_create" build\test\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing Buffer create runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_buffer_append_bytes" build\test\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing Buffer appendBytes runtime helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_buffer.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_buffer.exe
if errorlevel 1 exit /b 1
build\test\stdlib_buffer.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_buffer.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_buffer_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_buffer_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_buffer_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_buffer_c.exe > build\test\stdlib_buffer_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_buffer_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_buffer_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib buffer xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_file_async.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_file_async.xl --emit build\test\stdlib_file_async.c
if errorlevel 1 exit /b 1
findstr /C:"xfuture* xl6_xrt_file_read_text_async_utf8" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing async file read helper.
    exit /b 1
)
findstr /C:"xfuture* xrtFileReadAllAsync" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing direct XRT async file API.
    exit /b 1
)
findstr /C:"xFutureThenSource" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing XRT future result adapter.
    exit /b 1
)
findstr /C:"xl6_task_run_thread_tracked(xl6_xrt_file" build\test\stdlib_file_async.c >nul
if not errorlevel 1 (
    echo stdlib_file_async.c still contains the removed duplicate thread-backed file path.
    exit /b 1
)
findstr /C:"xfuture* xl6_xrt_file_delete_async" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing async file delete helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_file_async.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_file_async.exe
if errorlevel 1 exit /b 1
build\test\stdlib_file_async.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_file_async.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_file_async_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_file_async_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_file_async_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_file_async_c.exe > build\test\stdlib_file_async_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_file_async_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_file_async_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib file async xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_tcp_udp.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_tcp_udp.xl --emit build\test\stdlib_net_tcp_udp.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_TcpStream" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing TcpStream class bridge.
    exit /b 1
)
findstr /C:"void* xrtNetTcpConnect" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing xrtNetTcpConnect extern prototype.
    exit /b 1
)
findstr /C:"void* xrtNetUdpBind" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing xrtNetUdpBind extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_tcp_stream_recv_buffer" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing TCP binary buffer bridge.
    exit /b 1
)
findstr /C:"void* xl6_xrt_udp_packet_buffer" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing UDP packet buffer bridge.
    exit /b 1
)
findstr /C:"xl6_xrt_tls_session_create" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c unexpectedly contains raw TLS wrappers.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_net_tcp_udp.xl -o build\test\stdlib_net_tcp_udp.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_tcp_udp.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_tcp_lifecycle.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_tcp_lifecycle.xl --emit build\test\stdlib_tcp_lifecycle.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_point_take((void**)&(clientHandle))" build\test\stdlib_tcp_lifecycle.c >nul
if errorlevel 1 (
    echo stdlib_tcp_lifecycle.c missing TcpStream ownership transfer.
    exit /b 1
)
findstr /C:"xl_class_TcpStream__close__void(((XL_Class_TcpStream*)xl6_class_data(client" build\test\stdlib_tcp_lifecycle.c >nul
if errorlevel 1 (
    echo stdlib_tcp_lifecycle.c missing graceful close call.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_tcp_lifecycle.xl -o build\test\stdlib_tcp_lifecycle.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    build\test\stdlib_tcp_lifecycle.exe > build\test\stdlib_tcp_lifecycle_%%I.out.txt
    if errorlevel 1 exit /b 1
)
release\xl.exe run release\examples\stdlib_net_tcp_udp.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_net_tcp_udp_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_net_tcp_udp_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_net_tcp_udp_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_tcp_udp_c.exe > build\test\stdlib_net_tcp_udp_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_net_tcp_udp_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_net_tcp_udp_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib net tcp/udp xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_path_file.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_path_file_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_path_file_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_path_file_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_path_file_c.exe > build\test\stdlib_path_file_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_path_file_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_path_file_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib path/file xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_path_file.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_path_file.xl --emit build\test\stdlib_path_file.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtPathExists" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathExists extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathRandom" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathRandom extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathJoin" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathJoin extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_path_join" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing path.join runtime helper.
    exit /b 1
)
findstr /C:"const char* __xl_std_path_join" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing path.join helper.
    exit /b 1
)
findstr /C:"void* xrtPathNormalize" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathNormalize extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathAbs" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathAbs extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathRel" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathRel extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathCwd" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathCwd extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathTemp" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathTemp extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathWithExt" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathWithExt extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathWithName" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathWithName extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileRename" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileRename extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileReplace" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileReplace extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileTouch" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileTouch extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileWriteAllAtomic" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileWriteAllAtomic extern prototype.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_FileInfo" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing FileInfo class bridge.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_File" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing File class bridge.
    exit /b 1
)
findstr /C:"void* xrtOpen" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtOpen extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_file_open" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing file.open runtime helper.
    exit /b 1
)
findstr /C:"int xrtDirDelete" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirDelete extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirCopy" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirCopy extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirMove" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirMove extern prototype.
    exit /b 1
)
findstr /C:"int __xl_std_dir_delete" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.delete helper.
    exit /b 1
)
findstr /C:"int __xl_std_dir_copy" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.copy helper.
    exit /b 1
)
findstr /C:"int __xl_std_dir_move" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.move helper.
    exit /b 1
)
findstr /C:"int xrtDirScanEx" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirScanEx runtime prototype.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_dir_list" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.list runtime helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_dir_files" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.files helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_dir_dirs" build\test\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.dirs helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_path_file.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_path_file.exe
if errorlevel 1 exit /b 1
build\test\stdlib_path_file.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_file_meta.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_file_meta.xl --emit build\test\stdlib_file_meta.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtFileSetSize" build\test\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileSetSize extern prototype.
    exit /b 1
)
findstr /C:"int xrtFileGetAttr" build\test\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileGetAttr extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtFileGetAccessTime" build\test\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileGetAccessTime extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_file_meta.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_file_meta.exe
if errorlevel 1 exit /b 1
build\test\stdlib_file_meta.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_loop_fstring_needs.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_loop_fstring_needs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_loop_fstring_needs.xl --emit build\test\cgen_loop_fstring_needs.c
if errorlevel 1 exit /b 1
findstr /C:"static const char* xl6_xrt_i64_to_string" build\test\cgen_loop_fstring_needs.c >nul
if errorlevel 1 (
    echo cgen_loop_fstring_needs.c missing typed-loop f-string conversion runtime.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string((long long)(value))" build\test\cgen_loop_fstring_needs.c >nul
if errorlevel 1 (
    echo cgen_loop_fstring_needs.c missing typed-loop f-string conversion.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_loop_fstring_needs.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_loop_fstring_needs.exe
if errorlevel 1 exit /b 1
build\test\cgen_loop_fstring_needs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_generator_fstring_needs.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_generator_fstring_needs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generator_fstring_needs.xl --emit build\test\cgen_generator_fstring_needs.c
if errorlevel 1 exit /b 1
findstr /C:"static const char* xl6_xrt_i64_to_string" build\test\cgen_generator_fstring_needs.c >nul
if errorlevel 1 (
    echo cgen_generator_fstring_needs.c missing generator-loop f-string conversion runtime.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string((long long)(index))" build\test\cgen_generator_fstring_needs.c >nul
if errorlevel 1 (
    echo cgen_generator_fstring_needs.c missing generator index f-string conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string((long long)(value))" build\test\cgen_generator_fstring_needs.c >nul
if errorlevel 1 (
    echo cgen_generator_fstring_needs.c missing generator value f-string conversion.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generator_fstring_needs.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generator_fstring_needs.exe
if errorlevel 1 exit /b 1
build\test\cgen_generator_fstring_needs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_string_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_class_string_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_string_ownership.xl --emit build\test\cgen_class_string_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTempScope __xl6_temp_scope_" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing nested XRT temporary scope.
    exit /b 1
)
findstr /C:"return xrtTempStringTake((char*)__xl6_return_" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing returned string promotion.
    exit /b 1
)
findstr /C:".text = xrtCopyStr(" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing owned class string copy.
    exit /b 1
)
findstr /C:"xrtFree((void*)self->text); self->text = 0;" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing class string destruction.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_string_ownership.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_string_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_string_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_closure_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_closure_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_closure_ownership.xl --emit build\test\cgen_closure_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"static const char* xl6_xrt_i64_to_string" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing captured-value conversion runtime.
    exit /b 1
)
findstr /C:"xtarray values;" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing static typed-container capture storage.
    exit /b 1
)
findstr /C:"__env->values = xl6_xrt_typed_array_copy_mode" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing shared typed-container capture copy.
    exit /b 1
)
findstr /C:"__env->prefix = xrtCopyStr(prefix, 0u);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing owned string capture copy.
    exit /b 1
)
findstr /C:"__env->dynamicValue = xl6_xrt_retain(dynamicValue);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing retained var capture.
    exit /b 1
)
findstr /C:"__env->box = xl6_xrt_retain(box);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing retained class handle capture.
    exit /b 1
)
findstr /C:"xl6_class_data(__env->box" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing captured class receiver rewrite.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(__env->values);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing typed-container capture destruction.
    exit /b 1
)
findstr /C:"xvoUnref(__env->box);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing class handle capture release.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_closure_ownership.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_closure_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_closure_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_string_value_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_string_value_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_value_ownership.xl --emit build\test\cgen_string_value_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"text = xrtCopyStr(text != 0 ? text : \"\", 0u);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing owned string parameter copy.
    exit /b 1
)
findstr /C:"globalText = xrtCopyStr(\"global\", 0u);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing owned module string initialization.
    exit /b 1
)
findstr /C:"char* __xl6_string_assign_" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing copy-before-release string assignment.
    exit /b 1
)
findstr /C:"char* __xl6_multi_string_value_" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing owned multi-return string assignment.
    exit /b 1
)
findstr /C:"part = xrtCopyStr(part, 0u);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing generator string value copy.
    exit /b 1
)
findstr /C:"xrtFree((void*)globalText);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing module string cleanup.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_string_value_ownership.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_string_value_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_string_value_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_contextual_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_contextual_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_contextual_identifiers.xl --emit build\test\cgen_contextual_identifiers.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_fn_xtime(long long xl_kw_stdout, long long xl_kw_errno)" build\test\cgen_contextual_identifiers.c >nul
if errorlevel 1 (
    echo cgen_contextual_identifiers.c missing private function symbol or protected C macro parameters.
    exit /b 1
)
findstr /C:"xvalue __xl6_discard_" build\test\cgen_contextual_identifiers.c >nul
if errorlevel 1 (
    echo cgen_contextual_identifiers.c missing independent discard slots.
    exit /b 1
)
findstr /C:"void* xl6_xrt_path_join(const char* first, const char* second, xvalue rest)" build\test\cgen_contextual_identifiers.c >nul
if errorlevel 1 (
    echo cgen_contextual_identifiers.c missing variadic path.join runtime.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_contextual_identifiers.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_contextual_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_contextual_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_stdout_crlf.xl > build\test\cgen_stdout_crlf.out.bin
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$actual=[System.IO.File]::ReadAllBytes('build\test\cgen_stdout_crlf.out.bin');$expected=[System.Text.Encoding]::UTF8.GetBytes(\"first`r`nsecond`r`n\");if($actual.Length -ne $expected.Length){exit 1};for($i=0;$i -lt $actual.Length;$i++){if($actual[$i] -ne $expected[$i]){exit 1}}"
if errorlevel 1 (
    echo print must preserve an existing CRLF byte sequence exactly once.
    exit /b 1
)

release\xl.exe check test\cases\cgen_json_recursive_lifecycle.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_json_recursive_lifecycle.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_json_recursive_lifecycle.xl -o build\test\cgen_json_recursive_lifecycle.exe
if errorlevel 1 exit /b 1
build\test\cgen_json_recursive_lifecycle.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\perf_var_recursive_alloc.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\perf_var_recursive_alloc.xl -- dict 20 20
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_typed_container_param_reassign.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_param_reassign.xl --emit build\test\cgen_typed_container_param_reassign.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_param_reassign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_param_reassign.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_param_reassign.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\perf_json_ast_lifecycle.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\perf_json_ast_lifecycle.xl --format json > build\test\perf_json_ast_input.json
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\perf_json_ast_lifecycle.xl -- build\test\perf_json_ast_input.json 3 > build\test\perf_json_ast_lifecycle.out
if errorlevel 1 exit /b 1
findstr /B /C:"total " build\test\perf_json_ast_lifecycle.out >nul
if errorlevel 1 (
    echo perf_json_ast_lifecycle.xl missing total summary.
    exit /b 1
)



echo DEMO6 suite ok: core C generation and runtime baseline 10/10
exit /b 0
