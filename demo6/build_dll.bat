@echo off
setlocal
cd /d %~dp0

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1
set "XL6_DLL_LINKS="%XL6_TCC_LIBTCC1%" -lWs2_32 -lIPHLPAPI -lShell32"
set "XL6_BUILD_TMP=build\tmp"
set "XL6_RELEASE_CC=gcc"
set "XL6_RELEASE_CFLAGS=-std=c11 -Wall -Wno-pointer-sign -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format-truncation -Wno-dangling-pointer -Os -ffunction-sections -fdata-sections"
set "XL6_RELEASE_LDFLAGS=-shared -Wl,--gc-sections -s"

where "%XL6_RELEASE_CC%" >nul 2>nul
if errorlevel 1 (
    echo DEMO6 DLL build failed: MinGW GCC was not found in PATH.
    exit /b 1
)

if not exist release mkdir release
if not exist build mkdir build
if not exist "%XL6_BUILD_TMP%" mkdir "%XL6_BUILD_TMP%"

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

echo Building DEMO6 xl.dll runtime...
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% %XL6_RELEASE_LDFLAGS% -DXL_BUILD_DLL -DXL_RUNTIME_ONLY_DLL -DXL_STATIC_TCC -DLIBTCC_AS_DLL -D_CRT_SECURE_NO_WARNINGS -DCONFIG_TCC_STATIC -I. -Irelease -Isrc -Ilib\lzma -I"%XL6_EMBED_TCC_SOURCE_ROOT%" %XL6_TCC_LIB_FLAGS% -o release\xl.dll ^
    src\api\xl_api.c ^
    src\api\xl_api_symbols.c ^
    src\api\xl_api_path.c ^
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
    src\ir\xl6_hir.c ^
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

if exist release\libtcc.dll del /f /q release\libtcc.dll >nul 2>nul

echo Generating xl.dll import definition...
"%XL6_TCC%" -impdef release\xl.dll -o release\xl.def
if errorlevel 1 (
    echo DEMO6 xl.dll import definition failed.
    exit /b 1
)

echo Building DEMO6 xlc.dll compiler runtime...
"%XL6_RELEASE_CC%" %XL6_RELEASE_CFLAGS% %XL6_RELEASE_LDFLAGS% -DXL_BUILD_DLL -DXL_STATIC_TCC -DLIBTCC_AS_DLL -D_CRT_SECURE_NO_WARNINGS -DCONFIG_TCC_STATIC -I. -Irelease -Isrc -Ilib\lzma -I"%XL6_EMBED_TCC_SOURCE_ROOT%" %XL6_TCC_LIB_FLAGS% -o release\xlc.dll ^
    src\api\xl_api.c ^
    src\api\xl_api_symbols.c ^
    src\api\xl_api_path.c ^
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
    src\ir\xl6_hir.c ^
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
"%XL6_TCC%" -impdef release\xlc.dll -o release\xlc.def
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
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xlc_dll_embed_string.exe ^
    release\examples\xl_dll_embed_string.c ^
    release\xlc.def

if errorlevel 1 (
    echo DEMO6 xlc.dll verification program build failed.
    exit /b 1
)

echo Building xlc.dll compiler API verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xlc_api_embed.exe ^
    release\examples\xlc_api_embed.c ^
    release\xlc.def

if errorlevel 1 (
    echo DEMO6 xlc.dll compiler API verification program build failed.
    exit /b 1
)

echo Building xl.dll XRT export verification program...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_xrt_exports.exe ^
    release\examples\xl_dll_xrt_exports.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll XRT export verification program build failed.
    exit /b 1
)

echo Building xl.dll dynamic run GUI demo...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -mwindows -o release\xl_run_demo.exe ^
    release\examples\xl_run_demo_win32.c ^
    release\xl.def ^
    -luser32 -lgdi32

if errorlevel 1 (
    echo DEMO6 xl.dll dynamic run GUI demo build failed.
    exit /b 1
)

echo Building xl.dll API boundary verification program...
"%XL6_TCC%" -std=c11 -Wall -DXL_RUNTIME_ONLY_API -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_null_context.exe ^
    release\examples\xl_dll_null_context.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll API boundary verification program build failed.
    exit /b 1
)

echo Building xl.dll threaded host verification program...
"%XL6_TCC%" -std=c11 -Wall -DXL_RUNTIME_ONLY_API -D_CRT_SECURE_NO_WARNINGS -DXL6_EXAMPLE_BOUNDARY_TEST -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_threaded_host.exe ^
    release\examples\xl_dll_threaded_host.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll threaded host verification program build failed.
    exit /b 1
)

echo Building xl.dll module dependency lifetime verification program...
"%XL6_TCC%" -std=c11 -Wall -DXL_RUNTIME_ONLY_API -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl_dll_module_dependency_lifetime.exe ^
    test\cases\xl_dll_module_dependency_lifetime.c ^
    release\xl.def

if errorlevel 1 (
    echo DEMO6 xl.dll module dependency lifetime verification program build failed.
    exit /b 1
)

echo DEMO6 xl.dll/xlc.dll build ok: release\xl.dll release\xlc.dll
exit /b 0
