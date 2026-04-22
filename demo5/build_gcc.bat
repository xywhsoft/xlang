@echo off
setlocal
cd /d %~dp0

if not exist release\bin mkdir release\bin
if not exist release\tmp mkdir release\tmp
if exist release\tmp\*.fallback.exe del /f /q release\tmp\*.fallback.exe >nul 2>nul
if exist release\tmp\*.trace.txt del /f /q release\tmp\*.trace.txt >nul 2>nul
if not exist examples\xlp_native\packages\pkg.clock\bin mkdir examples\xlp_native\packages\pkg.clock\bin
if not exist examples\xlp_native\packages\pkg.probe\bin mkdir examples\xlp_native\packages\pkg.probe\bin
if not exist examples\xlp_official_native\packages\x.image.bmp\bin mkdir examples\xlp_official_native\packages\x.image.bmp\bin
if not exist examples\xlp_official_native\packages\x.archive.tar\bin mkdir examples\xlp_official_native\packages\x.archive.tar\bin
if not exist examples\xlp_official_native\packages\x.archive.zip\bin mkdir examples\xlp_official_native\packages\x.archive.zip\bin
if not exist examples\xlp_official_native\packages\x.compress.gzip\bin mkdir examples\xlp_official_native\packages\x.compress.gzip\bin
if not exist examples\xlp_static\packages\pkg.clock\lib mkdir examples\xlp_static\packages\pkg.clock\lib
if not exist examples\xlp_static\packages\pkg.probe\lib mkdir examples\xlp_static\packages\pkg.probe\lib

set SRC_LIST=release\tmp\gcc_sources.rsp
set TEST_SRC_LIST=release\tmp\gcc_test_sources.rsp
set TCC_OBJ=release\tmp\libtcc.o
set XL5_BUILD_EXE=release\tmp\xl5.build.exe
set XL5_TEST_BUILD_EXE=release\tmp\xl5_test.build.exe
set BUILD_STAMP_HEADER=release\tmp\xl_test_build_stamp.h
set XL5_SMOKE_LOG=release\tmp\xl5_smoke.log
set XL5_TEST_SMOKE_LOG=release\tmp\xl5_test_smoke.log
set XL5_SMOKE_FILE=examples\full\for_in_range.xl
set SMOKE_SELECTOR=lower.async_generator_loop_control_hybrid_groups_json
set GCC_COMMON_FLAGS=-std=c11 -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-format-truncation -Wno-restrict -Wno-maybe-uninitialized -D_CRT_SECURE_NO_WARNINGS -I. -Ilib\xrt -Ilib\tcc
set GCC_LINK_FLAGS=-Wl,--gc-sections -s -lshell32 -lws2_32 -liphlpapi

for /f "delims=" %%i in ('powershell -NoProfile -Command "(Get-Date).ToString('yyyy-MM-dd HH:mm:ss.fff')"') do set BUILD_ID=%%i
set BUILD_ID=%BUILD_ID% run-%RANDOM%-%RANDOM%
> "%BUILD_STAMP_HEADER%" echo #ifndef XL_TEST_BUILD_ID
>> "%BUILD_STAMP_HEADER%" echo #define XL_TEST_BUILD_ID "%BUILD_ID%"
>> "%BUILD_STAMP_HEADER%" echo #endif

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
	echo main.c
	for /r src %%f in (*.c) do (
		set "SRC_FILE=%%f"
		set "SRC_FILE=!SRC_FILE:\=/!"
		echo !SRC_FILE!
	)
)

> "%TEST_SRC_LIST%" (
	echo test/main.c
	echo test/xl_test.c
	for /r src %%f in (*.c) do (
		set "SRC_FILE=%%f"
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

echo Building demo5 with GCC...
gcc %GCC_COMMON_FLAGS% @"%SRC_LIST%" "%TCC_OBJ%" -o "%XL5_BUILD_EXE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo Build failed.
	exit /b 1
)

echo Building demo5 test runner with GCC...
gcc %GCC_COMMON_FLAGS% @"%TEST_SRC_LIST%" "%TCC_OBJ%" -o "%XL5_TEST_BUILD_EXE%" %GCC_LINK_FLAGS%

if errorlevel 1 (
	echo Test runner build failed.
	exit /b 1
)

echo Building xlp example artifacts...
gcc -std=c11 -shared -o examples\xlp_native\packages\pkg.clock\bin\pkg_clock.dll examples\xlp_artifacts\pkg_clock_artifact.c

if errorlevel 1 (
	echo Shared xlp artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o examples\xlp_native\packages\pkg.probe\bin\pkg_probe.dll examples\xlp_artifacts\pkg_clock_artifact.c

if errorlevel 1 (
	echo Shared xlp probe artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o examples\xlp_official_native\packages\x.image.bmp\bin\x_image_bmp.dll examples\xlp_artifacts\x_image_bmp_artifact.c

if errorlevel 1 (
	echo Official xlp image bmp artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o examples\xlp_official_native\packages\x.archive.tar\bin\x_archive_tar.dll examples\xlp_artifacts\x_archive_tar_artifact.c

if errorlevel 1 (
	echo Official xlp archive tar artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o examples\xlp_official_native\packages\x.archive.zip\bin\x_archive_zip.dll examples\xlp_artifacts\x_archive_zip_artifact.c

if errorlevel 1 (
	echo Official xlp archive zip artifact build failed.
	exit /b 1
)

gcc -std=c11 -shared -o examples\xlp_official_native\packages\x.compress.gzip\bin\x_compress_gzip.dll examples\xlp_artifacts\x_compress_gzip_artifact.c

if errorlevel 1 (
	echo Official xlp compress gzip artifact build failed.
	exit /b 1
)

"%XL5_BUILD_EXE%" build-c examples\xlp_artifacts\pkg_clock_artifact.c --emit release\tmp\pkg_clock_artifact.obj >nul

if errorlevel 1 (
	echo Static xlp artifact object build failed.
	exit /b 1
)

ar rcs release\tmp\pkg_clock_static.a release\tmp\pkg_clock_artifact.obj

if errorlevel 1 (
	echo Static xlp artifact archive build failed.
	exit /b 1
)

copy /y release\tmp\pkg_clock_static.a examples\xlp_static\packages\pkg.clock\lib\pkg_clock.a >nul

if errorlevel 1 (
	echo Static xlp artifact copy failed.
	exit /b 1
)

copy /y release\tmp\pkg_clock_static.a examples\xlp_static\packages\pkg.probe\lib\pkg_probe.a >nul

if errorlevel 1 (
	echo Static xlp probe artifact archive build failed.
	exit /b 1
)

if exist release\bin\xl5.exe del /f /q release\bin\xl5.exe >nul
move /y "%XL5_BUILD_EXE%" release\bin\xl5.exe >nul

if errorlevel 1 (
	echo xl5.exe publish failed.
	exit /b 1
)

if exist release\bin\xl5_test.exe del /f /q release\bin\xl5_test.exe >nul
move /y "%XL5_TEST_BUILD_EXE%" release\bin\xl5_test.exe >nul

if errorlevel 1 (
	echo xl5_test.exe publish failed.
	exit /b 1
)

if exist "%XL5_SMOKE_LOG%" del /f /q "%XL5_SMOKE_LOG%" >nul
if exist "%XL5_TEST_SMOKE_LOG%" del /f /q "%XL5_TEST_SMOKE_LOG%" >nul

release\bin\xl5.exe sem "%XL5_SMOKE_FILE%" > "%XL5_SMOKE_LOG%" 2>&1

if errorlevel 1 (
	echo xl5.exe smoke test failed for %XL5_SMOKE_FILE%.
	type "%BUILD_STAMP_HEADER%"
	type "%XL5_SMOKE_LOG%"
	exit /b 1
)

release\bin\xl5_test.exe %SMOKE_SELECTOR% > "%XL5_TEST_SMOKE_LOG%" 2>&1

if errorlevel 1 (
	echo xl5_test.exe smoke test failed for %SMOKE_SELECTOR%.
	type "%BUILD_STAMP_HEADER%"
	type "%XL5_TEST_SMOKE_LOG%"
	exit /b 1
)

echo Build completed: release\bin\xl5.exe
exit /b 0
