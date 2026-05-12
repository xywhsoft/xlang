@echo off
setlocal
cd /d %~dp0

if not exist release\tmp mkdir release\tmp
if exist release\tmp\*.fallback.exe del /f /q release\tmp\*.fallback.exe >nul 2>nul
if exist release\tmp\*.trace.txt del /f /q release\tmp\*.trace.txt >nul 2>nul
set SRC_LIST=release\tmp\gcc_sources.rsp
set XL_SRC_LIST=release\tmp\gcc_xl_sources.rsp
set XLANG_SRC_LIST=release\tmp\gcc_xlang_sources.rsp
set TEST_SRC_LIST=release\tmp\gcc_test_sources.rsp
set TCC_OBJ=release\tmp\libtcc.o
set XL_BUILD_EXE=release\xl.exe
set XLDBG_BUILD_EXE=release\xldbg.exe
set XLBIND_BUILD_EXE=release\xlbind.exe
set XL5_TEST_BUILD_EXE=release\xl5_test.exe
set XLANG_BUILD_DLL=release\xlang.dll
set XLANG_BUILD_IMPLIB=release\libxlang.dll.a
set BUILD_STAMP_HEADER=release\tmp\xl_test_build_stamp.h
set GCC_COMMON_FLAGS=-std=c11 -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-format-truncation -Wno-restrict -Wno-maybe-uninitialized -D_CRT_SECURE_NO_WARNINGS -I. -Ilib\xrt -Ilib\tcc
set GCC_DEBUG_FLAGS=%GCC_COMMON_FLAGS% -DXLANG_DEBUGMODE=1
set GCC_LINK_FLAGS=-Wl,--gc-sections -s -lshell32 -lws2_32 -liphlpapi
set SRC_ROOT=%CD%\src\

for /f "delims=" %%i in ('powershell -NoProfile -Command "(Get-Date).ToString('yyyy-MM-dd HH:mm:ss.fff')"') do set BUILD_ID=%%i
set BUILD_ID=%BUILD_ID% run-%RANDOM%-%RANDOM%
> "%BUILD_STAMP_HEADER%" echo #ifndef XL_TEST_BUILD_ID
>> "%BUILD_STAMP_HEADER%" echo #define XL_TEST_BUILD_ID "%BUILD_ID%"
>> "%BUILD_STAMP_HEADER%" echo #endif

if exist release\src rmdir /s /q release\src
if exist release\lib\xrt rmdir /s /q release\lib\xrt
if exist release\tcc\xrt rmdir /s /q release\tcc\xrt
if not exist release\tcc\support\common mkdir release\tcc\support\common
if not exist release\tcc\support\runtime mkdir release\tcc\support\runtime
if not exist release\tcc\support\host mkdir release\tcc\support\host
if not exist release\tcc\include mkdir release\tcc\include
if exist examples (
	if exist release\examples rmdir /s /q release\examples
	xcopy /E /I /Y examples release\examples >nul
)
if not exist release\examples (
	echo Missing examples source: neither .\examples nor .\release\examples exists.
	exit /b 1
)
if not exist release\examples\xle_native\libs\pkg.clock\bin mkdir release\examples\xle_native\libs\pkg.clock\bin
if not exist release\examples\xle_native\libs\pkg.probe\bin mkdir release\examples\xle_native\libs\pkg.probe\bin
if not exist release\examples\xle_official_native\libs\x.image.bmp\bin mkdir release\examples\xle_official_native\libs\x.image.bmp\bin
if not exist release\examples\xle_official_native\libs\x.archive.tar\bin mkdir release\examples\xle_official_native\libs\x.archive.tar\bin
if not exist release\examples\xle_official_native\libs\x.archive.zip\bin mkdir release\examples\xle_official_native\libs\x.archive.zip\bin
if not exist release\examples\xle_official_native\libs\x.compress.gzip\bin mkdir release\examples\xle_official_native\libs\x.compress.gzip\bin
if not exist release\examples\xle_static\libs\pkg.clock\lib mkdir release\examples\xle_static\libs\pkg.clock\lib
if not exist release\examples\xle_static\libs\pkg.probe\lib mkdir release\examples\xle_static\libs\pkg.probe\lib

copy /y src\common\xl_common.h release\tcc\support\common\xl_common.h >nul
copy /y src\common\xl_config.h release\tcc\support\common\xl_config.h >nul
copy /y src\common\xl_rc.h release\tcc\support\common\xl_rc.h >nul
copy /y src\runtime\xlrt_profile.h release\tcc\support\runtime\xlrt_profile.h >nul
copy /y src\runtime\xlrt_host.c release\tcc\support\runtime\xlrt_host.c >nul
copy /y src\runtime\xlrt_generated.h release\tcc\support\runtime\xlrt_generated.h >nul
copy /y src\runtime\xlrt_generated.c release\tcc\support\runtime\xlrt_generated.c >nul
copy /y src\runtime\xl_object_runtime.h release\tcc\support\runtime\xl_object_runtime.h >nul
copy /y src\runtime\xl_object_runtime.c release\tcc\support\runtime\xl_object_runtime.c >nul
copy /y src\runtime\xl_channel_runtime.c release\tcc\support\runtime\xl_channel_runtime.c >nul
copy /y src\runtime\xl_sys_lib_runtime.c release\tcc\support\runtime\xl_sys_lib_runtime.c >nul
copy /y src\runtime\xl_url_runtime.c release\tcc\support\runtime\xl_url_runtime.c >nul
copy /y src\runtime\xl_http_runtime.c release\tcc\support\runtime\xl_http_runtime.c >nul
copy /y src\runtime\xl_httpd_runtime.c release\tcc\support\runtime\xl_httpd_runtime.c >nul
copy /y src\runtime\xl_mime_runtime.c release\tcc\support\runtime\xl_mime_runtime.c >nul
copy /y src\runtime\xl_multipart_runtime.c release\tcc\support\runtime\xl_multipart_runtime.c >nul
copy /y src\runtime\xl_net_dns_runtime.c release\tcc\support\runtime\xl_net_dns_runtime.c >nul
copy /y src\runtime\xl_net_ip_runtime.c release\tcc\support\runtime\xl_net_ip_runtime.c >nul
copy /y src\runtime\xl_net_tcp_runtime.c release\tcc\support\runtime\xl_net_tcp_runtime.c >nul
copy /y src\runtime\xl_net_udp_runtime.c release\tcc\support\runtime\xl_net_udp_runtime.c >nul
copy /y src\runtime\xl_net_tls_runtime.c release\tcc\support\runtime\xl_net_tls_runtime.c >nul
copy /y src\runtime\xl_net_proxy_runtime.c release\tcc\support\runtime\xl_net_proxy_runtime.c >nul
copy /y src\runtime\xl_net_ws_runtime.c release\tcc\support\runtime\xl_net_ws_runtime.c >nul
copy /y src\host\xl_host_runtime.h release\tcc\support\host\xl_host_runtime.h >nul
copy /y src\host\xl_host_runtime.c release\tcc\support\host\xl_host_runtime.c >nul
copy /y lib\xrt\xrt.h release\tcc\include\xrt.h >nul
if not exist release\include mkdir release\include
copy /y src\sdk\xlang.h release\include\xlang.h >nul

echo Generating lexer with Flex...
tools\flex.exe --wincompat --noline -o src\lexer\xl_lexer_gen.c --header-file=src\lexer\xl_lexer_gen.h src\lexer\xl_lexer.l

if errorlevel 1 (
	echo Flex generation failed.
	exit /b 1
)

echo Generating parser with Lemon...
pushd src\parser
..\..\tools\lemon.exe -q xl_parser.y
set LEMON_RC=%errorlevel%
popd

if not "%LEMON_RC%"=="0" (
	if exist src\parser\xl_parser.c if exist src\parser\xl_parser.h (
		echo Lemon reported conflicts, continuing with generated parser artifacts.
	) else (
		echo Lemon generation failed.
		exit /b 1
	)
)

setlocal EnableDelayedExpansion
> "%SRC_LIST%" (
	echo main_debug.c
	for /r src %%f in (*.c) do (
		set "SRC_FILE=%%f"
		set "SRC_FILE=!SRC_FILE:%SRC_ROOT%=src\!"
		set "SRC_FILE=!SRC_FILE:\=/!"
		echo !SRC_FILE!
	)
)

> "%XL_SRC_LIST%" (
	echo main.c
	for /r src %%f in (*.c) do (
		set "SRC_FILE=%%f"
		set "SRC_FILE=!SRC_FILE:%SRC_ROOT%=src\!"
		set "SRC_FILE=!SRC_FILE:\=/!"
		set "SRC_FILE_SKIP=0"
		if /I "!SRC_FILE:~0,10!"=="src/debug/" set "SRC_FILE_SKIP=1"
		if "!SRC_FILE_SKIP!"=="0" if /I not "!SRC_FILE!"=="src/diag/xl_dump.c" if /I not "!SRC_FILE!"=="src/ast/xl_ast_dump.c" if /I not "!SRC_FILE!"=="src/sem/xl_sem_dump.c" if /I not "!SRC_FILE!"=="src/lower/xl_lower_dump.c" if /I not "!SRC_FILE!"=="src/driver/xl_xle_dump.c" if /I not "!SRC_FILE!"=="src/runtime/xl_std_trace.c" if /I not "!SRC_FILE!"=="src/runtime/xl_std_debug.c" if /I not "!SRC_FILE!"=="src/runtime/xlrt_generated_debug.c" if /I not "!SRC_FILE!"=="src/runtime/xl_native_registry_debug.c" if /I not "!SRC_FILE!"=="src/driver/xl_driver_debug.c" if /I not "!SRC_FILE!"=="src/driver/xl_cli_debug.c" (
			echo !SRC_FILE!
		)
	)
)

> "%XLANG_SRC_LIST%" (
	for /r src %%f in (*.c) do (
		set "SRC_FILE=%%f"
		set "SRC_FILE=!SRC_FILE:%SRC_ROOT%=src\!"
		set "SRC_FILE=!SRC_FILE:\=/!"
		set "SRC_FILE_SKIP=0"
		if /I "!SRC_FILE:~0,10!"=="src/debug/" set "SRC_FILE_SKIP=1"
		if "!SRC_FILE_SKIP!"=="0" if /I not "!SRC_FILE!"=="src/diag/xl_dump.c" if /I not "!SRC_FILE!"=="src/ast/xl_ast_dump.c" if /I not "!SRC_FILE!"=="src/sem/xl_sem_dump.c" if /I not "!SRC_FILE!"=="src/lower/xl_lower_dump.c" if /I not "!SRC_FILE!"=="src/driver/xl_xle_dump.c" if /I not "!SRC_FILE!"=="src/runtime/xl_std_trace.c" if /I not "!SRC_FILE!"=="src/runtime/xl_std_debug.c" if /I not "!SRC_FILE!"=="src/runtime/xlrt_generated_debug.c" if /I not "!SRC_FILE!"=="src/runtime/xl_native_registry_debug.c" if /I not "!SRC_FILE!"=="src/driver/xl_driver_debug.c" if /I not "!SRC_FILE!"=="src/driver/xl_cli_debug.c" (
			echo !SRC_FILE!
		)
	)
)

> "%TEST_SRC_LIST%" (
	echo test/main.c
	echo test/xl_test.c
	for /r src %%f in (*.c) do (
		set "SRC_FILE=%%f"
		set "SRC_FILE=!SRC_FILE:%SRC_ROOT%=src\!"
		set "SRC_FILE=!SRC_FILE:\=/!"
		echo !SRC_FILE!
	)
)

echo Building demo5 with GCC...
echo Building embedded TCC object...
gcc -std=c11 -O2 -ffunction-sections -fdata-sections -w -D_CRT_SECURE_NO_WARNINGS -Ilib\tcc -c lib\tcc\libtcc.c -o "%TCC_OBJ%"

if errorlevel 1 (
	echo Embedded TCC build failed.
	exit /b 1
)

if exist "%XLANG_BUILD_DLL%" del /f /q "%XLANG_BUILD_DLL%" >nul
if exist "%XLANG_BUILD_IMPLIB%" del /f /q "%XLANG_BUILD_IMPLIB%" >nul
echo Building xlang SDK DLL with GCC...
gcc %GCC_COMMON_FLAGS% -DXLANG_BUILD_DLL -shared @"%XLANG_SRC_LIST%" "%TCC_OBJ%" -o "%XLANG_BUILD_DLL%" -Wl,--out-implib,"%XLANG_BUILD_IMPLIB%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo xlang SDK DLL build failed.
	exit /b 1
)

if not exist "%XLANG_BUILD_DLL%" (
	echo xlang SDK DLL output missing: %XLANG_BUILD_DLL%
	exit /b 1
)

if exist "%XL_BUILD_EXE%" del /f /q "%XL_BUILD_EXE%" >nul
echo Building xl with GCC...
gcc %GCC_COMMON_FLAGS% @"%XL_SRC_LIST%" "%TCC_OBJ%" -o "%XL_BUILD_EXE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo xl build failed.
	exit /b 1
)

if not exist "%XL_BUILD_EXE%" (
	echo xl build output missing: %XL_BUILD_EXE%
	exit /b 1
)

if exist "%XLDBG_BUILD_EXE%" del /f /q "%XLDBG_BUILD_EXE%" >nul
echo Building xldbg with GCC...
gcc %GCC_DEBUG_FLAGS% @"%SRC_LIST%" "%TCC_OBJ%" -o "%XLDBG_BUILD_EXE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo xldbg build failed.
	exit /b 1
)

if not exist "%XLDBG_BUILD_EXE%" (
	echo xldbg build output missing: %XLDBG_BUILD_EXE%
	exit /b 1
)

if exist "%XLBIND_BUILD_EXE%" del /f /q "%XLBIND_BUILD_EXE%" >nul
echo Building xlbind with GCC...
gcc %GCC_COMMON_FLAGS% tools\xlbind\main.c tools\xlbind\xlbind_tool.c src\common\xl_app_path.c -o "%XLBIND_BUILD_EXE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo xlbind build failed.
	exit /b 1
)

if not exist "%XLBIND_BUILD_EXE%" (
	echo xlbind build output missing: %XLBIND_BUILD_EXE%
	exit /b 1
)

echo Building demo5 test runner with GCC...
gcc %GCC_DEBUG_FLAGS% @"%TEST_SRC_LIST%" "%TCC_OBJ%" -o "%XL5_TEST_BUILD_EXE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo Test runner build failed.
	exit /b 1
)

if not exist "%XL5_TEST_BUILD_EXE%" (
	echo Test runner build output missing: %XL5_TEST_BUILD_EXE%
	exit /b 1
)

echo Building xle example artifacts...
gcc -std=c11 -shared -o release\examples\xle_native\libs\pkg.clock\bin\pkg_clock.dll release\examples\xle_artifacts\pkg_clock_artifact.c

if errorlevel 1 (
	echo Shared xle artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o release\examples\xle_native\libs\pkg.probe\bin\pkg_probe.dll release\examples\xle_artifacts\pkg_clock_artifact.c

if errorlevel 1 (
	echo Shared xle probe artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o release\examples\xle_official_native\libs\x.image.bmp\bin\x_image_bmp.dll release\examples\xle_artifacts\x_image_bmp_artifact.c

if errorlevel 1 (
	echo Official xle image bmp artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o release\examples\xle_official_native\libs\x.archive.tar\bin\x_archive_tar.dll release\examples\xle_artifacts\x_archive_tar_artifact.c

if errorlevel 1 (
	echo Official xle archive tar artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o release\examples\xle_official_native\libs\x.archive.zip\bin\x_archive_zip.dll release\examples\xle_artifacts\x_archive_zip_artifact.c

if errorlevel 1 (
	echo Official xle archive zip artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o release\examples\xle_official_native\libs\x.compress.gzip\bin\x_compress_gzip.dll release\examples\xle_artifacts\x_compress_gzip_artifact.c

if errorlevel 1 (
	echo Official xle compress gzip artifact build failed.
	exit /b 1
)

call "%XL_BUILD_EXE%" build-c release\examples\xle_artifacts\pkg_clock_artifact.c --emit release\tmp\pkg_clock_artifact.obj >nul

if errorlevel 1 (
	echo Static xle artifact object build failed.
	exit /b 1
)

ar rcs release\tmp\pkg_clock_static.a release\tmp\pkg_clock_artifact.obj

if errorlevel 1 (
	echo Static xle artifact archive build failed.
	exit /b 1
)

copy /y release\tmp\pkg_clock_static.a release\examples\xle_static\libs\pkg.clock\lib\pkg_clock.a >nul

if errorlevel 1 (
	echo Static xle artifact copy failed.
	exit /b 1
)

copy /y release\tmp\pkg_clock_static.a release\examples\xle_static\libs\pkg.probe\lib\pkg_probe.a >nul

if errorlevel 1 (
	echo Static xle probe artifact archive build failed.
	exit /b 1
)

if exist release\xl5.exe del /f /q release\xl5.exe >nul
if exist release\bin\xl.exe del /f /q release\bin\xl.exe >nul
if exist release\bin\xldbg.exe del /f /q release\bin\xldbg.exe >nul
if exist release\bin\xl5.exe del /f /q release\bin\xl5.exe >nul
if exist release\bin\xl5_test.exe del /f /q release\bin\xl5_test.exe >nul

echo Build completed: release\xl.exe, release\xldbg.exe, release\xlbind.exe and release\xlang.dll
echo Run test_gcc.bat to execute GCC smoke tests.
exit /b 0
