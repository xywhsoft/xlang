@echo off
setlocal
cd /d %~dp0\..\..

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
if errorlevel 1 exit /b 1

call :run_example examples\extlibs\crc32.xl crc32 xcrc32_string_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\clipboard.xl clipboard x_clipboard_set_text
if errorlevel 1 exit /b 1
call :export_example examples\extlibs\x_com.xl x.com x_com_test_object
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\md5.xl md5 xmd5_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\lz4.xl lz4 xlz4_compress_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\zstd.xl zstd xzstd_compress_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\lzma.xl lzma xlzma_compress_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\miniz.xl miniz xminiz_compress_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\md4c.xl md4c xmd4c_html
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\stb_image.xl stb_image xstbi_load
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\stb_image_resize2.xl stb_image_resize2 xstbir_resize_uint8_srgb_native
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\stb_image_write.xl stb_image_write xstbiw_write_png
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\imagehash.xl imagehash xihash_ahash
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\stb_truetype.xl stb_truetype xstbtt_load
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_volc_core.xl x.volc.core x_volc_core_sha256_hex
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\sqlite.xl sqlite xsqlite3_open_native
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\pkg_math.xl pkg.math xpkg_math_add_one
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_volc_sts.xl x.volc.sts __xl6_import_x_volc_sts_assume_role_trn
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_volc_phone.xl x.volc.phone __xl6_import_x_volc_phone_product_body
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_mail_mime.xl x.mail.mime __xl6_import_x_mail_mime_build_json
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_mail_smtp.xl x.mail.smtp xmail_smtp_send_json_native
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_mail_pop3.xl x.mail.pop3 xmail_pop3_fetch_json_native
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_mail_imap.xl x.mail.imap xmail_imap_watch_json_native
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_uia.xl x.uia x_uia_test_window_create
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_opencv.xl opencv opencv_capture_rect_native
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_onnxruntime.xl onnxruntime __xl6_import_onnxruntime_available
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_dinov3.xl dinov3 __xl6_import_dinov3_version
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_xfeat.xl xfeat __xl6_import_xfeat_version
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_rapidocr.xl rapidocr __xl6_import_rapidocr_available
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_ui_auto.xl x.ui.auto __xl6_import_x_ui_auto_test_window
if errorlevel 1 exit /b 1
call :run_example examples\extlibs\x_redis.xl x.redis __xl6_import_x_redis_resp_encode3
if errorlevel 1 exit /b 1

echo DEMO6 extension library examples ok.
exit /b 0

:export_example
set "example=%~1"
set "name=%~2"
set "symbol=%~3"

release\xl.exe check %example%
if errorlevel 1 exit /b 1

release\xl.exe xapi %example% --emit release\tmp\ext_%name%_example.xapi.json
if errorlevel 1 exit /b 1
findstr /C:"%name%" release\tmp\ext_%name%_example.xapi.json >nul
if errorlevel 1 (
    echo ext_%name%_example.xapi.json missing %name% import metadata.
    exit /b 1
)

release\xl.exe cgen %example% --emit release\tmp\ext_%name%_example.c --emit-header release\tmp\ext_%name%_example.h
if errorlevel 1 exit /b 1
findstr /C:"%symbol%" release\tmp\ext_%name%_example.c >nul
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

release\xl.exe xapi %example% --emit release\tmp\ext_%name%_example.xapi.json
if errorlevel 1 exit /b 1
findstr /C:"%name%" release\tmp\ext_%name%_example.xapi.json >nul
if errorlevel 1 (
    echo ext_%name%_example.xapi.json missing %name% import metadata.
    exit /b 1
)

release\xl.exe cgen %example% --emit release\tmp\ext_%name%_example.c
if errorlevel 1 exit /b 1
findstr /C:"%symbol%" release\tmp\ext_%name%_example.c >nul
if errorlevel 1 (
    echo ext_%name%_example.c missing %name% native symbol.
    exit /b 1
)

release\xl.exe run %example%
if errorlevel 1 exit /b 1

release\xl.exe build %example% -o release\tmp\ext_%name%_example.exe
if errorlevel 1 exit /b 1

release\tmp\ext_%name%_example.exe
if errorlevel 1 exit /b 1

exit /b 0
