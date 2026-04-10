@echo off
setlocal
cd /d %~dp0

if not exist release\bin mkdir release\bin
if not exist release\tmp mkdir release\tmp

set SRC_LIST=release\tmp\gcc_sources.rsp
set TEST_SRC_LIST=release\tmp\gcc_test_sources.rsp

echo Generating lexer with Flex...
tools\flex.exe --wincompat --noline -o src\lexer\xl_lexer_gen.c --header-file=src\lexer\xl_lexer_gen.h src\lexer\xl_lexer.l

if errorlevel 1 (
	echo Flex generation failed.
	exit /b 1
)

echo Generating parser with Lemon...
pushd src\parser
..\..\tools\lemon.exe -q -T..\..\tools\lempar.c xl_parser.y
set LEMON_RC=%errorlevel%
popd

if not "%LEMON_RC%"=="0" (
	echo Lemon generation failed.
	exit /b 1
)

> "%SRC_LIST%" (
	echo main.c
	echo "test/xl_test.c"
	for /r src %%f in (*.c) do @echo "%%f"
)

> "%TEST_SRC_LIST%" (
	echo "test/main.c"
	echo "test/xl_test.c"
	for /r src %%f in (*.c) do @echo "%%f"
)

echo Building demo5 with GCC...
gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -D_CRT_SECURE_NO_WARNINGS -I. -Ilib\xrt @"%SRC_LIST%" -o release\bin\xl5.exe -lshell32 -lws2_32 -liphlpapi

if errorlevel 1 (
	echo Build failed.
	exit /b 1
)

echo Building demo5 test runner with GCC...
gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -D_CRT_SECURE_NO_WARNINGS -I. -Ilib\xrt @"%TEST_SRC_LIST%" -o release\bin\xl5_test.exe -lshell32 -lws2_32 -liphlpapi

if errorlevel 1 (
	echo Test runner build failed.
	exit /b 1
)

echo Build completed: release\bin\xl5.exe
exit /b 0
