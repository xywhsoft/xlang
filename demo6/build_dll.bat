@echo off
setlocal
cd /d %~dp0

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist release mkdir release
if not exist release\tmp mkdir release\tmp

if not exist "%XL6_XRT_OBJ%" (
    echo DEMO6 xl.dll build failed: missing XRT object "%XL6_XRT_OBJ%".
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

echo Building DEMO6 xl.dll...
"%XL6_TCC%" -std=c11 -Wall -shared -DXL_BUILD_DLL -D_CRT_SECURE_NO_WARNINGS -I. -Iinclude -Isrc %XL6_TCC_SOURCE_INCLUDE% %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl.dll ^
    src\api\xl_api.c ^
    src\abi\xl6_callable.c ^
    src\abi\xl6_xapi_dump.c ^
    src\abi\xl6_xapi_load.c ^
    src\backend\xl6_build_backend.c ^
    src\compile\xl6_compile_unit.c ^
    src\compile\xl6_imports.c ^
    src\common\xl6_console.c ^
    src\common\xl6_output.c ^
    src\diag\xl6_diag.c ^
    src\ast\xl6_ast.c ^
    src\cgen\xl6_cgen_entry.c ^
    src\cgen\xl6_cgen_lifecycle.c ^
    src\cgen\xl6_cgen_runtime.c ^
    src\cgen\xl6_cgen_header.c ^
    src\cgen\xl6_cgen_needs_basic.c ^
    src\cgen\xl6_cgen.c ^
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
    %XL6_DEFAULT_LINKS%

if errorlevel 1 (
    echo DEMO6 xl.dll build failed.
    exit /b 1
)

copy /Y "%XL6_TCC_DLL%" release\libtcc.dll >nul
if errorlevel 1 (
    echo DEMO6 xl.dll build failed: cannot copy libtcc.dll.
    exit /b 1
)

echo Generating xl.dll import definition...
"%XL6_TCC%" -impdef release\xl.dll -o release\xl.def
if errorlevel 1 (
    echo DEMO6 xl.dll import definition failed.
    exit /b 1
)

echo Building xl.dll dependency fixtures...
"%XL6_TCC%" %XL6_TCC_INCLUDES% -c examples\xl_dll_link_host.c -o release\tmp\xl_dll_link_host.o
if errorlevel 1 (
    echo DEMO6 xl.dll static dependency fixture build failed.
    exit /b 1
)
"%XL6_TCC%" -ar rcs release\tmp\libxl6dlllink.a release\tmp\xl_dll_link_host.o
if errorlevel 1 (
    echo DEMO6 xl.dll static dependency fixture archive failed.
    exit /b 1
)
"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% examples\xl_dll_loadlib_host.c -o release\tmp\xl_dll_loadlib_host.dll
if errorlevel 1 (
    echo DEMO6 xl.dll loadlib dependency fixture build failed.
    exit /b 1
)
"%XL6_TCC%" -impdef release\tmp\xl_dll_loadlib_host.dll -o release\tmp\xl_dll_loadlib_host.def
if errorlevel 1 (
    echo DEMO6 xl.dll loadlib dependency fixture import definition failed.
    exit /b 1
)

echo Building xl.dll embed verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Iinclude %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_embed_string.exe ^
    examples\xl_dll_embed_string.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll verification program build failed.
    exit /b 1
)

echo Building xl.dll API boundary verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Iinclude %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_null_context.exe ^
    examples\xl_dll_null_context.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll API boundary verification program build failed.
    exit /b 1
)

echo Building xl.dll threaded host verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Iinclude %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_threaded_host.exe ^
    examples\xl_dll_threaded_host.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll threaded host verification program build failed.
    exit /b 1
)

echo DEMO6 xl.dll build ok: release\xl.dll
exit /b 0
