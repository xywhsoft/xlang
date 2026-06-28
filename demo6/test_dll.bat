@echo off
setlocal
cd /d %~dp0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

call build_dll.bat
if errorlevel 1 exit /b 1

release\xl_dll_null_context.exe
if errorlevel 1 exit /b 1

release\xl_dll_threaded_host.exe
if errorlevel 1 exit /b 1

release\xlc_dll_embed_string.exe
if errorlevel 1 exit /b 1

if exist release\libtcc.dll (
    echo release\libtcc.dll should not be generated.
    exit /b 1
)

cmd /c "set XL6_TCC_DLL=__missing_libtcc.dll&& set XL6_TCC_ROOT=__missing_root__&& set XL6_TCC_INCLUDE=__missing_include__&& set XL6_TCC_WIN_INCLUDE=__missing_win_include__&& set XL6_TCC_WINAPI_INCLUDE=__missing_winapi_include__&& set XL6_TCC_LIB=__missing_lib__&& set XL6_TCC_LIBTCC1=__missing_libtcc1.a&& set XL6_XRT_OBJ=__missing_xrt.o&& release\xl_dll_null_context.exe"
if errorlevel 1 (
    echo xl.dll static libtcc/runtime VFS verification failed.
    exit /b 1
)

cmd /c "set XL6_TCC_DLL=__missing_libtcc.dll&& set XL6_TCC_ROOT=__missing_root__&& set XL6_TCC_INCLUDE=__missing_include__&& set XL6_TCC_WIN_INCLUDE=__missing_win_include__&& set XL6_TCC_WINAPI_INCLUDE=__missing_winapi_include__&& set XL6_TCC_LIB=__missing_lib__&& set XL6_TCC_LIBTCC1=__missing_libtcc1.a&& set XL6_XRT_OBJ=__missing_xrt.o&& release\xlc_dll_embed_string.exe"
set XL6_XLC_STATIC_RC=%errorlevel%
if not "%XL6_XLC_STATIC_RC%"=="0" (
    echo xlc.dll static libtcc/VFS verification failed.
    exit /b 1
)

if not exist release\xlc.dll (
    echo xlc.dll missing.
    exit /b 1
)
if not exist release\xlc.def (
    echo xlc.def missing.
    exit /b 1
)

release\tmp\xl_dll_embed_string_script.exe
if errorlevel 1 exit /b 1

release\tmp\xl_dll_embed_file_script.exe
if errorlevel 1 exit /b 1

if not exist release\tmp\xl_dll_link_host.o (
    echo xl_dll_link_host.o missing.
    exit /b 1
)
if not exist release\tmp\libxl6dlllink.a (
    echo libxl6dlllink.a missing.
    exit /b 1
)
if not exist release\tmp\xl_dll_loadlib_host.dll (
    echo xl_dll_loadlib_host.dll missing.
    exit /b 1
)
if not exist release\tmp\xl_dll_loadlib_host.def (
    echo xl_dll_loadlib_host.def missing.
    exit /b 1
)

echo DEMO6 xl.dll/xlc.dll tests ok.
exit /b 0
