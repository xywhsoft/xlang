@echo off
setlocal
cd /d %~dp0

set XL_BUILD_EXE=release\xl.exe
set XLDBG_BUILD_EXE=release\xldbg.exe
set XLBIND_BUILD_EXE=release\xlbind.exe
set XL5_TEST_BUILD_EXE=release\xl5_test.exe
set XLANG_BUILD_DLL=release\xlang.dll
set XLANG_BUILD_IMPLIB=release\libxlang.dll.a
set XLANG_SDK_SMOKE=release\bin\sdk_host_basic.exe
set BUILD_STAMP_HEADER=release\tmp\xl_test_build_stamp.h
set XL_SMOKE_LOG=release\tmp\xl_smoke.log
set XLDBG_SMOKE_LOG=release\tmp\xldbg_smoke.log
set XL5_TEST_SMOKE_LOG=release\tmp\xl5_test_smoke.log
set XLBIND_SMOKE_LOG=release\tmp\xlbind_smoke.log
set XL5_SMOKE_FILE=release\examples\full\for_in_range.xl
set XLBIND_SMOKE_HEADER=release\examples\native\binding_probe.h
set XLBIND_SMOKE_OUT=release\tmp\bind_smoke
set SMOKE_SELECTOR=lower.async_generator_loop_control_hybrid_groups_json
set GCC_COMMON_FLAGS=-std=c11 -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-format-truncation -Wno-restrict -Wno-maybe-uninitialized -D_CRT_SECURE_NO_WARNINGS -I. -Ilib\xrt -Ilib\tcc
set GCC_LINK_FLAGS=-Wl,--gc-sections -s -lshell32 -lws2_32 -liphlpapi

if not exist release\tmp mkdir release\tmp
if not exist "%XL_BUILD_EXE%" (
	echo Missing %XL_BUILD_EXE%. Run build_gcc.bat first.
	exit /b 1
)
if not exist "%XLDBG_BUILD_EXE%" (
	echo Missing %XLDBG_BUILD_EXE%. Run build_gcc.bat first.
	exit /b 1
)
if not exist "%XLBIND_BUILD_EXE%" (
	echo Missing %XLBIND_BUILD_EXE%. Run build_gcc.bat first.
	exit /b 1
)
if not exist "%XL5_TEST_BUILD_EXE%" (
	echo Missing %XL5_TEST_BUILD_EXE%. Run build_gcc.bat first.
	exit /b 1
)
if not exist "%XLANG_BUILD_DLL%" (
	echo Missing %XLANG_BUILD_DLL%. Run build_gcc.bat first.
	exit /b 1
)
if not exist "%XLANG_BUILD_IMPLIB%" (
	echo Missing %XLANG_BUILD_IMPLIB%. Run build_gcc.bat first.
	exit /b 1
)

if exist "%XL_SMOKE_LOG%" del /f /q "%XL_SMOKE_LOG%" >nul
if exist "%XLDBG_SMOKE_LOG%" del /f /q "%XLDBG_SMOKE_LOG%" >nul
if exist "%XL5_TEST_SMOKE_LOG%" del /f /q "%XL5_TEST_SMOKE_LOG%" >nul
if exist "%XLBIND_SMOKE_LOG%" del /f /q "%XLBIND_SMOKE_LOG%" >nul
if exist "%XLBIND_SMOKE_OUT%" rmdir /s /q "%XLBIND_SMOKE_OUT%"

echo Running xl.exe smoke test...
"%XL_BUILD_EXE%" run "%XL5_SMOKE_FILE%" > "%XL_SMOKE_LOG%" 2>&1

if errorlevel 1 (
	echo xl.exe smoke test failed for %XL5_SMOKE_FILE%.
	if exist "%BUILD_STAMP_HEADER%" type "%BUILD_STAMP_HEADER%"
	type "%XL_SMOKE_LOG%"
	exit /b 1
)

echo Running xldbg.exe smoke test...
"%XLDBG_BUILD_EXE%" sem "%XL5_SMOKE_FILE%" > "%XLDBG_SMOKE_LOG%" 2>&1

if errorlevel 1 (
	echo xldbg.exe smoke test failed for %XL5_SMOKE_FILE%.
	if exist "%BUILD_STAMP_HEADER%" type "%BUILD_STAMP_HEADER%"
	type "%XLDBG_SMOKE_LOG%"
	exit /b 1
)

echo Running xlbind.exe smoke test...
"%XLBIND_BUILD_EXE%" "%XLBIND_SMOKE_HEADER%" --emit "%XLBIND_SMOKE_OUT%" > "%XLBIND_SMOKE_LOG%" 2>&1

if errorlevel 1 (
	echo xlbind.exe smoke test failed for %XLBIND_SMOKE_HEADER%.
	if exist "%BUILD_STAMP_HEADER%" type "%BUILD_STAMP_HEADER%"
	type "%XLBIND_SMOKE_LOG%"
	exit /b 1
)

if not exist "%XLBIND_SMOKE_OUT%\xlang.json" (
	echo xlbind.exe smoke output missing package file.
	if exist "%BUILD_STAMP_HEADER%" type "%BUILD_STAMP_HEADER%"
	type "%XLBIND_SMOKE_LOG%"
	exit /b 1
)

echo Running xlang SDK smoke test...
if not exist release\bin mkdir release\bin
gcc %GCC_COMMON_FLAGS% release\examples\sdk\sdk_host_basic.c -Irelease\include -Lrelease -lxlang -o "%XLANG_SDK_SMOKE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo xlang SDK smoke host build failed.
	exit /b 1
)

set PATH=%CD%\release;%PATH%
"%XLANG_SDK_SMOKE%"

if errorlevel 1 (
	echo xlang SDK smoke host failed.
	exit /b 1
)

echo Running xl5_test.exe smoke test...
"%XL5_TEST_BUILD_EXE%" %SMOKE_SELECTOR% > "%XL5_TEST_SMOKE_LOG%" 2>&1

if errorlevel 1 (
	echo xl5_test.exe smoke test failed for %SMOKE_SELECTOR%.
	if exist "%BUILD_STAMP_HEADER%" type "%BUILD_STAMP_HEADER%"
	type "%XL5_TEST_SMOKE_LOG%"
	exit /b 1
)

echo GCC smoke tests passed.
exit /b 0
