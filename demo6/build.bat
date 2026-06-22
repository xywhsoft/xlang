@echo off
setlocal
cd /d %~dp0

set XL6_ROOT=%CD%
set XL6_TCC=..\demo5\lib\tcc\tcc.exe
set XL6_FLEX=..\demo5\tools\flex.exe
set XL6_LEMON=..\demo5\tools\lemon.exe
set XL6_XRT_OBJ=..\..\xrt\release\x64\xrt.o
set XL6_TCC_LIBTCC1=..\demo5\release\tcc\lib\libtcc1-64.a

if not exist release mkdir release
if not exist release\tmp mkdir release\tmp

if not exist "%XL6_XRT_OBJ%" (
    echo DEMO6 build failed: missing XRT object "%XL6_XRT_OBJ%".
    exit /b 1
)

echo Generating DEMO6 lexer...
"%XL6_FLEX%" --wincompat --noline -o src\lexer\xl6_lexer_gen.c --header-file=src\lexer\xl6_lexer_gen.h src\lexer\xl6_lexer.l
if errorlevel 1 (
    echo DEMO6 lexer generation failed.
    exit /b 1
)

echo Generating DEMO6 parser...
pushd src\parser
"%XL6_ROOT%\%XL6_LEMON%" -q xl6_parser.y
set XL6_LEMON_RC=%errorlevel%
popd
if not "%XL6_LEMON_RC%"=="0" (
    echo DEMO6 parser generation failed.
    exit /b 1
)

echo Building DEMO6 xl.exe...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Isrc -I..\demo5\lib\tcc\include -I..\demo5\release\tcc\include_win -I..\demo5\release\tcc\include_win\winapi -L..\demo5\release\tcc\lib -o release\xl.exe ^
    main.c ^
    src\abi\xl6_abi_dump.c ^
    src\abi\xl6_callable.c ^
    src\abi\xl6_xapi_dump.c ^
    src\abi\xl6_xapi_load.c ^
    src\backend\xl6_build_backend.c ^
    src\compile\xl6_compile_unit.c ^
    src\compile\xl6_imports.c ^
    src\common\xl6_output.c ^
    src\diag\xl6_diag.c ^
    src\ast\xl6_ast.c ^
    src\ast\xl6_ast_dump.c ^
    src\cgen\xl6_cgen_entry.c ^
    src\cgen\xl6_cgen_lifecycle.c ^
    src\cgen\xl6_cgen_runtime.c ^
    src\cgen\xl6_cgen_header.c ^
    src\cgen\xl6_cgen_needs_basic.c ^
    src\cgen\xl6_cgen.c ^
    src\ir\xl6_ir_dump.c ^
    src\lexer\xl6_token.c ^
    src\lexer\xl6_lexer_api.c ^
    src\lexer\xl6_lexer_gen.c ^
    src\parser\xl6_parser.c ^
    src\parser\xl6_parser_api.c ^
    src\package\xl6_package.c ^
    src\stdlib\xl6_stdlib.c ^
    src\type\xl6_type_model.c ^
    src\sem\xl6_type_registry.c ^
    src\sem\xl6_sem.c ^
    src\driver\xl6_cli.c ^
    "%XL6_XRT_OBJ%" ^
    "%XL6_TCC_LIBTCC1%" ^
    -lWs2_32 -lIPHLPAPI -lShell32

if errorlevel 1 (
    echo DEMO6 build failed.
    exit /b 1
)

echo DEMO6 build ok: release\xl.exe
exit /b 0
