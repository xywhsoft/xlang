@echo off
setlocal
cd /d %~dp0\..\..\..

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
if errorlevel 1 exit /b 1

set "EXTLIB_TEST_DIR=build\test\extlibs"
if not exist "%EXTLIB_TEST_DIR%" mkdir "%EXTLIB_TEST_DIR%"

call :run_example release\examples\extlibs\crc32.xl crc32 xcrc32_string_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\clipboard.xl clipboard x_clipboard_set_text
if errorlevel 1 exit /b 1
call :export_example release\examples\extlibs\com.xl com x_com_test_object
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\md5.xl md5 xmd5_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\lz4.xl lz4 xlz4_compress_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\zstd.xl zstd xzstd_compress_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\lzma.xl lzma xlzma_compress_hex
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\miniz.xl miniz xminiz_compress_hex
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
call :run_example release\examples\extlibs\volc_sts.xl volc.sts __xl6_import_volc_sts_assume_role_trn
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\volc_phone.xl volc.phone __xl6_import_volc_phone_product_body
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\mail_mime.xl mail.mime __xl6_import_mail_mime_build_json
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\mail_smtp.xl mail.smtp xmail_smtp_send_json_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\mail_pop3.xl mail.pop3 xmail_pop3_fetch_json_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\mail_imap.xl mail.imap xmail_imap_watch_json_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\uia.xl uia x_uia_test_window_create
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\opencv.xl opencv opencv_capture_rect_native
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\onnxruntime.xl onnxruntime __xl6_import_onnxruntime_available
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\dinov3.xl dinov3 __xl6_import_dinov3_version
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\xfeat.xl xfeat __xl6_import_xfeat_version
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\rapidocr.xl rapidocr __xl6_import_rapidocr_available
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\ui_auto.xl ui.auto __xl6_import_ui_auto_test_window
if errorlevel 1 exit /b 1
call :run_example release\examples\extlibs\redis.xl redis __xl6_import_redis_resp_encode3
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

release\xl.exe check %example%
if errorlevel 1 exit /b 1

release\xl.exe cgen %example% --emit "%EXTLIB_TEST_DIR%\ext_%name%_example.c"
if errorlevel 1 exit /b 1
findstr /C:"%symbol%" "%EXTLIB_TEST_DIR%\ext_%name%_example.c" >nul
if errorlevel 1 (
    echo ext_%name%_example.c missing %name% native symbol.
    exit /b 1
)

release\xl.exe run %example%
if errorlevel 1 exit /b 1

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

"%EXTLIB_TEST_DIR%\ext_%name%_example.exe"
if errorlevel 1 exit /b 1

exit /b 0
