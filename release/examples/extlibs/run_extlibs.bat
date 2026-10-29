@echo off
setlocal
cd /d %~dp0\..\..\..
set "REPO_ROOT=%CD%"

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
if errorlevel 1 exit /b 1

set "EXTLIB_TEST_DIR=build\test\extlibs"
if not exist "%EXTLIB_TEST_DIR%" mkdir "%EXTLIB_TEST_DIR%"

call :run_example release\examples\extlibs\crc32.xl crc32 xcrc32_hash_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\clipboard.xl clipboard x_clipboard_set_text
if errorlevel 1 exit /b 1
call :export_example release\examples\extlibs\com.xl com x_com_test_object
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\md5.xl md5 xmd5_hash_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\lz4.xl lz4 xlz4_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\zstd.xl zstd xzstd_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\lzma.xl lzma xlzma_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\miniz.xl miniz xminiz_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\brotli.xl brotli xbrotli_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\snappy.xl snappy xsnappy_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\xxhash.xl xxhash xxxhash_hash64
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\gzip.xl gzip xgzip_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\bz2.xl bz2 xbz2_compress
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\zip.xl zip xzip_archive_create
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\tar.xl tar xtar_reader_open_memory
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\tar_compressed.xl tar_compressed xrtTarWriterCreateMemory
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\csv.xl csv xcsv_reader_open_memory
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\ini.xl ini xini_parse
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\toml.xl toml xtoml_parse
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\netrc.xl netrc xnetrc_parse
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\plist.xl plist xplist_parse
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\xml_sax.xl xml_sax xsax_open_memory
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\xml_etree.xl xml_etree xetree_parse
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\xml_dom.xl xml_dom xdom_parse
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\md4c.xl md4c xmd4c_html
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\stb_image.xl stb_image xstbi_load
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\stb_image_resize2.xl stb_image_resize2 xstbir_resize_uint8_srgb_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\stb_image_write.xl stb_image_write xstbiw_write_png
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\imagehash.xl imagehash xihash_ahash
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\stb_truetype.xl stb_truetype xstbtt_load
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\volc_core.xl volc.core x_volc_core_sha256_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\sqlite.xl sqlite xsqlite3_open_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\pkg_math.xl pkg.math xpkg_math_add_one
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\volc_sts.xl volc.sts xl6_export_____xl6__import__volc__sts__assume__role__trn
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\volc_phone.xl volc.phone xl6_export_____xl6__import__volc__phone__product__body
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\uia.xl uia x_uia_test_window_create
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\opencv.xl opencv opencv_capture_rect_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\onnxruntime.xl onnxruntime xl6_export_____xl6__import__onnxruntime__available
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\dinov3.xl dinov3 xl6_export_____xl6__import__dinov3__version
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\xfeat.xl xfeat xl6_export_____xl6__import__xfeat__version
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\rapidocr.xl rapidocr xl6_export_____xl6__import__rapidocr__available
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\ui_auto.xl ui.auto xl6_export_____xl6__import__ui__auto__test__window
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\redis.xl redis xr_encode
if errorlevel 1 exit /b 1

rem Wires: release/examples/extlibs/*.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_examples.ps1
if errorlevel 1 exit /b 1

echo DEMO6 extension library examples ok.
exit /b 0

:export_example
set "example=%~1"
set "name=%~2"
set "symbol=%~3"

release\xl.exe check %example%
if errorlevel 1 exit /b 1

release\xl.exe cgen %example% --emit "%EXTLIB_TEST_DIR%\ext_%name%_example.c" --emit-header "%EXTLIB_TEST_DIR%\ext_%name%_example.h"
if errorlevel 1 exit /b 1
findstr /C:"%symbol%" "%EXTLIB_TEST_DIR%\ext_%name%_example.c" >nul
if errorlevel 1 (
    echo ext_%name%_example.c missing %name% native symbol.
    exit /b 1
)

exit /b 0

:run_example
set "example=%~1"
set "name=%~2"
set "symbol=%~3"
set "runtime_example=%example:release\=%"

release\xl.exe check %example%
if errorlevel 1 exit /b 1

release\xl.exe cgen %example% --emit "%EXTLIB_TEST_DIR%\ext_%name%_example.c"
if errorlevel 1 exit /b 1
findstr /C:"%symbol%" "%EXTLIB_TEST_DIR%\ext_%name%_example.c" >nul
if errorlevel 1 (
    echo ext_%name%_example.c missing %name% native symbol.
    exit /b 1
)

if /I "%name%"=="clipboard" (
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_named_mutex.ps1 -Name "Local\XLangDemo6ClipboardExample" -FilePath "%REPO_ROOT%\release\xl.exe" -WorkingDirectory "%REPO_ROOT%\release" -Argument0 "run" -Argument1 "%runtime_example%"
    set "run_result=%errorlevel%"
) else (
    pushd release
    xl.exe run "%runtime_example%"
    set "run_result=%errorlevel%"
    popd
)
if not "%run_result%"=="0" exit /b %run_result%

if exist "%EXTLIB_TEST_DIR%\ext_%name%_example.exe" (
    del /f /q "%EXTLIB_TEST_DIR%\ext_%name%_example.exe"
    if exist "%EXTLIB_TEST_DIR%\ext_%name%_example.exe" (
        echo ext_%name%_example.exe is still present before build.
        exit /b 1
    )
)
release\xl.exe build %example% -o "%EXTLIB_TEST_DIR%\ext_%name%_example.exe"
if errorlevel 1 exit /b 1
if not exist "%EXTLIB_TEST_DIR%\ext_%name%_example.exe" (
    echo ext_%name%_example.exe missing after build, retrying once.
    release\xl.exe build %example% -o "%EXTLIB_TEST_DIR%\ext_%name%_example.exe"
    if errorlevel 1 exit /b 1
)
if not exist "%EXTLIB_TEST_DIR%\ext_%name%_example.exe" (
    echo ext_%name%_example.exe build returned success but output is missing.
    exit /b 1
)

if /I "%name%"=="clipboard" (
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_named_mutex.ps1 -Name "Local\XLangDemo6ClipboardExample" -FilePath "%REPO_ROOT%\%EXTLIB_TEST_DIR%\ext_%name%_example.exe" -WorkingDirectory "%REPO_ROOT%\release"
    set "run_result=%errorlevel%"
) else (
    pushd release
    "%REPO_ROOT%\%EXTLIB_TEST_DIR%\ext_%name%_example.exe"
    set "run_result=%errorlevel%"
    popd
)
if not "%run_result%"=="0" exit /b %run_result%

exit /b 0
