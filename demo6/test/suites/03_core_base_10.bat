@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 10/10...

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_module_finalization.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_module_finalization.xl --emit build\test\xlir_module_finalization.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_module_native_finalization.xl --emit build\test\xlir_module_native_finalization.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_module_scalar_finalization.xl --emit build\test\xlir_module_scalar_finalization.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_module_finalization_cli.xl --emit build\test\xlir_module_finalization_cli.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_module_finalization_cli_error.xl --emit build\test\xlir_module_finalization_cli_error.c
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_binary_evaluation_lifetime.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_binary_operand_lifetime.xl --emit build\test\xlir_binary_operand_lifetime.c
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_random.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_random_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_random_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_random_c.exe
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
findstr /C:"XL6_XBuffer xrtBufferCreate" build\test\stdlib_buffer.c >nul
if not errorlevel 1 (
    echo stdlib_buffer.c duplicates the XRT buffer create declaration.
    exit /b 1
)
findstr /C:"bool xrtBufferAppend" build\test\stdlib_buffer.c >nul
if not errorlevel 1 (
    echo stdlib_buffer.c duplicates the XRT buffer append declaration.
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
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\stdlib_buffer.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_buffer.exe
if errorlevel 1 exit /b 1
build\test\stdlib_buffer.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_buffer.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_buffer_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_buffer_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_buffer_c.exe
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
findstr /C:"xrtFileReadAllAsync(xl6_async_file_pool, path)" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing direct XRT async file API.
    exit /b 1
)
findstr /C:"xtaskpool* xrtTaskPoolCreate" build\test\stdlib_file_async.c >nul
if not errorlevel 1 (
    echo stdlib_file_async.c duplicates the XRT task-pool declaration.
    exit /b 1
)
findstr /C:"xl6_async_file_pool = xrtTaskPoolCreate(0)" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing the current XRT task-pool call.
    exit /b 1
)
findstr /C:"result = xl6FutureCatchOwned" build\test\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing structured path error adaptation.
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
findstr /C:"xrtAsyncFileConfigInit" build\test\stdlib_file_async.c >nul
if not errorlevel 1 (
    echo stdlib_file_async.c still contains the removed async-file config ABI.
    exit /b 1
)
findstr /C:"xrtFileGetAllAsync" build\test\stdlib_file_async.c >nul
if not errorlevel 1 (
    echo stdlib_file_async.c still contains the removed binary whole-file ABI.
    exit /b 1
)
findstr /C:"xrtDirDeleteAsync" build\test\stdlib_file_async.c >nul
if not errorlevel 1 (
    echo stdlib_file_async.c still contains the removed directory async ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\stdlib_file_async.c test\cases\cgen_class_host.c release\xl.dll -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_file_async.exe
if errorlevel 1 exit /b 1
set "PATH=%CD%\release;%PATH%"
build\test\stdlib_file_async.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_file_async.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_file_async_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_file_async_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_file_async_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_file_async_c.exe > build\test\stdlib_file_async_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_file_async_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_file_async_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib file async xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_trim_dns_only.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_trim_dns_only.xl --emit build\test\stdlib_net_trim_dns_only.c
if errorlevel 1 exit /b 1
findstr /C:"static bool xl6_net_address_parse" build\test\stdlib_net_trim_dns_only.c >nul
if errorlevel 1 (
    echo stdlib_net_trim_dns_only.c missing address helper.
    exit /b 1
)
findstr /C:"xl6_net_deadline" build\test\stdlib_net_trim_dns_only.c >nul
if not errorlevel 1 (
    echo stdlib_net_trim_dns_only.c unexpectedly contains deadline helper.
    exit /b 1
)
findstr /C:"xrtDeadlineAfter" build\test\stdlib_net_trim_dns_only.c >nul
if not errorlevel 1 (
    echo stdlib_net_trim_dns_only.c unexpectedly depends on deadline support.
    exit /b 1
)
findstr /C:"#define XL_RUNTIME_NEED_TYPED_CONTAINER 1" build\test\stdlib_net_trim_dns_only.c >nul
if not errorlevel 1 (
    echo stdlib_net_trim_dns_only.c unexpectedly enables typed containers.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_net_trim_dns_only.xl -o build\test\stdlib_net_trim_dns_only.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_trim_dns_only.exe
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
findstr /C:"xnetstream* xrtNetConnect" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c duplicates the XRT connect declaration.
    exit /b 1
)
findstr /C:"return xrtNetConnect(xl6_net_engine, xl6_net_resolver, host, port" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing the current XRT connect call.
    exit /b 1
)
findstr /C:"xnetudp* xrtNetUdpBind" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c duplicates the XRT UDP bind declaration.
    exit /b 1
)
findstr /C:"return xrtNetUdpBind(xl6_net_engine, &address" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing the current XRT UDP bind call.
    exit /b 1
)
findstr /C:"static bool xl6_net_runtime_wait_objects" build\test\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing bounded network shutdown wait.
    exit /b 1
)
findstr /C:"xrtNetTcpConnect" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c still contains the removed TCP compatibility API.
    exit /b 1
)
findstr /C:"xrtNetUdpRecvAsync" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c still contains the removed UDP compatibility API.
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
findstr /C:"#define XL_RUNTIME_NEED_TYPED_CONTAINER 1" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c unexpectedly enables typed containers.
    exit /b 1
)
findstr /R /C:"xl6TypedArray" /C:"xl6TypedList" /C:"xl6TypedSet" /C:"xl6TypedDict" build\test\stdlib_net_tcp_udp.c >nul
if not errorlevel 1 (
    echo stdlib_net_tcp_udp.c unexpectedly contains typed-container helpers.
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
findstr /C:"server = __xl_std_tcp_listen" build\test\stdlib_tcp_lifecycle.c >nul
if errorlevel 1 (
    echo stdlib_tcp_lifecycle.c missing direct TcpServer return binding.
    exit /b 1
)
findstr /C:"client = __xl_std_tcp_connect" build\test\stdlib_tcp_lifecycle.c >nul
if errorlevel 1 (
    echo stdlib_tcp_lifecycle.c missing direct TcpStream return binding.
    exit /b 1
)
findstr /C:"xl_class_TcpStream__close(((XL_Class_TcpStream*)xl6_class_data(client" build\test\stdlib_tcp_lifecycle.c >nul
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
release\xl.exe check test\cases\stdlib_net_fast_path.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_fast_path.xl --emit build\test\stdlib_net_fast_path.c
if errorlevel 1 exit /b 1
findstr /C:"xl6TcpStreamSendVectorTextV1(" build\test\stdlib_net_fast_path.c >nul
if errorlevel 1 (
    echo stdlib_net_fast_path.c missing resident typed TCP vector call.
    exit /b 1
)
findstr /C:"xl6UdpSendVectorTextV1(" build\test\stdlib_net_fast_path.c >nul
if errorlevel 1 (
    echo stdlib_net_fast_path.c missing resident typed UDP vector call.
    exit /b 1
)
findstr /C:"xl6UdpReceiveBatchAsyncV1(" build\test\stdlib_net_fast_path.c >nul
if errorlevel 1 (
    echo stdlib_net_fast_path.c missing resident UDP batch call.
    exit /b 1
)
findstr /C:"xl6TcpStreamRecvOwnedStartV1(" build\test\stdlib_net_fast_path.c >nul
if errorlevel 1 (
    echo stdlib_net_fast_path.c missing resident owned async receive.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_net_fast_path.xl -o build\test\stdlib_net_fast_path.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_fast_path.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_net_tcp_udp.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_net_tcp_udp_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_net_tcp_udp_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_net_tcp_udp_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_tcp_udp_c.exe > build\test\stdlib_net_tcp_udp_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_net_tcp_udp_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_net_tcp_udp_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib net tcp/udp xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_path_file.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_path_file_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_path_file_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_path_file_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_path_file_c.exe > build\test\stdlib_path_file_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_path_file_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_path_file_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib path/file xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_path_file.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_path_file.xl --emit build\test\stdlib_path_file.c --emit-header build\test\stdlib_path_file.h
if errorlevel 1 exit /b 1
rem The semantic fixture validates current FileStat/typed-directory contracts.
rem Do not demand removed FileInfo layouts or legacy monolithic bridge names.
powershell -NoProfile -Command "$c=[IO.File]::ReadAllText('build/test/stdlib_path_file.c'); if ($c -match 'xl6_xrt_path_random|XL_Class_FileInfo|xrtPath(Random|Normalize)|xrtFile(Rename|Replace)|xrtDirScanEx') { Write-Host 'Legacy filesystem ABI retained.'; exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_path_file.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_path_file.exe
if errorlevel 1 exit /b 1
build\test\stdlib_path_file.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_file_meta.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_file_meta.xl --emit build\test\stdlib_file_meta.c
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$c=[IO.File]::ReadAllText('build/test/stdlib_file_meta.c'); if ($c -match 'XL6_FILE_RUNTIME|xl6_xrt_file_get_(attributes|access_time|change_time)') { Write-Host 'Legacy file metadata emitter retained.'; exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_file_meta.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_file_meta.exe
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
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_loop_fstring_needs.c %XL6_CGEN_LINKS% -o build\test\cgen_loop_fstring_needs.exe
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
findstr /C:"typedef struct XL6_Generator" build\test\cgen_generator_fstring_needs.c >nul
if errorlevel 1 (
    echo cgen_generator_fstring_needs.c missing the standalone generator runtime.
    exit /b 1
)
findstr /C:"xl6FutureCancel" build\test\cgen_generator_fstring_needs.c >nul
if not errorlevel 1 (
    echo cgen_generator_fstring_needs.c unexpectedly contains the async Future bridge.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_generator_fstring_needs.c %XL6_CGEN_LINKS% -o build\test\cgen_generator_fstring_needs.exe
if errorlevel 1 exit /b 1
build\test\cgen_generator_fstring_needs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_string_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_class_string_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_string_ownership.xl --emit build\test\cgen_class_string_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"XL6_TempScope __xl6_temp_scope_" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing nested XRT temporary scope.
    exit /b 1
)
findstr /C:"return xl6_temp_string_take((char*)__xl6_return_" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing returned string promotion.
    exit /b 1
)
findstr /C:".text = xl6_xrt_string_dup(" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing owned class string copy.
    exit /b 1
)
findstr /C:"xrtFree((void*)self->text); self->text = 0;" build\test\cgen_class_string_ownership.c >nul
if errorlevel 1 (
    echo cgen_class_string_ownership.c missing class string destruction.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_class_string_ownership.c %XL6_CGEN_LINKS% -o build\test\cgen_class_string_ownership.exe
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
findstr /C:"__env->values = xl6TypedArrayRetain((*values));" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing typed-container capture retain.
    exit /b 1
)
findstr /C:"xl6TypedArrayShare" build\test\cgen_closure_ownership.c >nul
if not errorlevel 1 (
    echo cgen_closure_ownership.c still contains obsolete typed-container sharing promotion.
    exit /b 1
)
findstr /C:"__env->prefix = xl6_xrt_string_dup(prefix, 0u);" build\test\cgen_closure_ownership.c >nul
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
findstr /C:"xl6TypedArrayRelease(__env->values);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing typed-container capture destruction.
    exit /b 1
)
findstr /C:"xrtValueRelease(__env->box);" build\test\cgen_closure_ownership.c >nul
if errorlevel 1 (
    echo cgen_closure_ownership.c missing class handle capture release.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_closure_ownership.c %XL6_CGEN_LINKS% -o build\test\cgen_closure_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_closure_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_string_value_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_string_value_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_value_ownership.xl --emit build\test\cgen_string_value_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"text = xl6_xrt_string_dup(text != 0 ? text : \"\", 0u);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing owned string parameter copy.
    exit /b 1
)
findstr /C:"globalText = xl6_xrt_string_dup(\"global\", 0u);" build\test\cgen_string_value_ownership.c >nul
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
findstr /C:"part = xl6_xrt_string_dup(part, 0u);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing generator string value copy.
    exit /b 1
)
findstr /C:"xrtFree((void*)__xl6_user_global_globalText);" build\test\cgen_string_value_ownership.c >nul
if errorlevel 1 (
    echo cgen_string_value_ownership.c missing module string cleanup.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_string_value_ownership.c %XL6_CGEN_LINKS% -o build\test\cgen_string_value_ownership.exe
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
findstr /C:"xvalue* __xl6_discard_" build\test\cgen_contextual_identifiers.c >nul
if errorlevel 1 (
    echo cgen_contextual_identifiers.c missing independent discard slots.
    exit /b 1
)
findstr /C:"void* xl6_xrt_path_join(const char* first, const char* second, xvalue* rest)" build\test\cgen_contextual_identifiers.c >nul
if errorlevel 1 (
    echo cgen_contextual_identifiers.c missing variadic path.join runtime.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_contextual_identifiers.c %XL6_CGEN_LINKS% -o build\test\cgen_contextual_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_contextual_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_stdout_crlf.xl -o build\test\cgen_stdout_crlf.exe
if errorlevel 1 exit /b 1
build\test\cgen_stdout_crlf.exe > build\test\cgen_stdout_crlf.out.bin
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
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_typed_container_param_reassign.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_typed_container_param_reassign.exe
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



release\xl.exe run test\cases\operators_int_literal_forms.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\operators_pow_int_bounds.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\type_int_minmax_bounds.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\type_int_narrowing_wrap.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\type_mixed_int_arith_infer.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\runtime_uint_small_print.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_toplevel_typed_func_value.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_global_symbol_names.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\runtime_uint64_large_print.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_toplevel_auto_anonymous_func.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_local_macro_identifier.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\runtime_int_division_zero.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\anonymous_functions.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_release_string_abi.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_release_string_abi.xl --emit build\test\stdlib_release_string_abi.c
if errorlevel 1 exit /b 1
%XL6_CGEN_CC% %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_release_string_abi.c\" test\cases\xlir_generated_module_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_release_string_abi.strict.exe
if errorlevel 1 exit /b 1
build\test\stdlib_release_string_abi.strict.exe
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_release_string_abi.xl -o build\test\stdlib_release_string_abi.aot.exe
if errorlevel 1 exit /b 1
build\test\stdlib_release_string_abi.aot.exe
if errorlevel 1 exit /b 1
echo DEMO6 suite ok: core C generation and runtime baseline 10/10
exit /b 0
