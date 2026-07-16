@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
if "%XL6_TRACE_TESTS%"=="1" echo on
echo Running DEMO6 suite: extended standard library...

powershell -NoProfile -ExecutionPolicy Bypass -Command "$root=(Resolve-Path 'build\test').Path; for($i=0; $i -lt 20; $i++){ $left=@(Get-ChildItem -LiteralPath $root -Filter 'stdlib_*.exe' -File); if($left.Count -eq 0){ break }; foreach($f in $left){ Remove-Item -LiteralPath $f.FullName -Force -ErrorAction SilentlyContinue }; if($i -lt 19){ Start-Sleep -Milliseconds 250 } }; $left=@(Get-ChildItem -LiteralPath $root -Filter 'stdlib_*.exe' -File); if($left.Count -gt 0){ Write-Host 'stdlib test executables are still present before build.'; $left.FullName; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_buffer_file_runtime.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_dev_codec_crypto_buffer_build.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_dev_codec_crypto_buffer_build.xl -o build\test\stdlib_dev_codec_crypto_buffer_build.exe
if errorlevel 1 exit /b 1
build\test\stdlib_dev_codec_crypto_buffer_build.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_json.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_json.xl --emit build\test\stdlib_json.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xrtParseJSON" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtParseJSON extern prototype.
    exit /b 1
)
findstr /C:"bool xrtJsonValid" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtJsonValid extern prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyJSON" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtStringifyJSON extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_json_stringify" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing json.stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_json.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_json.exe
if errorlevel 1 exit /b 1
build\test\stdlib_json.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_json_stream.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_json_stream.xl --emit build\test\stdlib_json_stream.c
if errorlevel 1 exit /b 1
findstr /C:"int xrtJsonVisit" build\test\stdlib_json_stream.c >nul
if errorlevel 1 (
    echo stdlib_json_stream.c missing xrtJsonVisit prototype.
    exit /b 1
)
findstr /C:"xjsonwriter xrtJsonWriterCreate" build\test\stdlib_json_stream.c >nul
if errorlevel 1 (
    echo stdlib_json_stream.c missing xrtJsonWriterCreate prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_json_stream.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_json_stream.exe
if errorlevel 1 exit /b 1
build\test\stdlib_json_stream.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_json.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_json_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_json_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_json_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_json_c.exe > build\test\stdlib_json_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_json_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_json_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib json xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_xson.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_xson.xl --emit build\test\stdlib_xson.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xrtParseXSONWithOptions" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtParseXSONWithOptions extern prototype.
    exit /b 1
)
findstr /C:"bool xrtXsonValidWithOptions" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtXsonValidWithOptions extern prototype.
    exit /b 1
)
findstr /C:"char* xrtStringifyXSONWithOptions" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtStringifyXSONWithOptions extern prototype.
    exit /b 1
)
findstr /C:"xl_fn_____xl__std__xson__stringify__var" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xson.stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_xson.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_xson.exe
if errorlevel 1 exit /b 1
build\test\stdlib_xson.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_xson.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_xson_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_xson_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_xson_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_xson_c.exe > build\test\stdlib_xson_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_xson_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_xson_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib xson xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_print.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_print.xl --emit build\test\stdlib_print.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing xl_runtime.h include.
    exit /b 1
)
findstr /C:"xl6_xrt_print(__xl6_print_args_" build\test\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing direct print runtime call.
    exit /b 1
)
findstr /C:"xl6_xrt_write" build\test\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing write runtime call.
    exit /b 1
)
findstr /C:"xl6_xrt_writeln" build\test\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing writeln runtime call.
    exit /b 1
)
findstr /C:"__xl_std_print(" build\test\stdlib_print.c >nul
if not errorlevel 1 (
    echo stdlib_print.c still emits __xl_std_print wrapper.
    exit /b 1
)
findstr /C:"__xl_std_println(" build\test\stdlib_print.c >nul
if not errorlevel 1 (
    echo stdlib_print.c still emits __xl_std_println wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_print.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_print.exe
if errorlevel 1 exit /b 1
build\test\stdlib_print.exe > build\test\stdlib_print.out.txt
if errorlevel 1 exit /b 1
findstr /C:"null" build\test\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing null output.
    exit /b 1
)
findstr /C:"true false 123 -45" build\test\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing scalar output.
    exit /b 1
)
findstr /C:"raw-text" build\test\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing write/writeln output.
    exit /b 1
)
findstr /C:"[1,2,3]" build\test\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing array JSON output.
    exit /b 1
)
findstr /C:"name" build\test\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing dict JSON output.
    exit /b 1
)

release\xl.exe check test\cases\stdlib_error_paths.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_error_paths.xl --emit build\test\stdlib_error_paths.c
if errorlevel 1 exit /b 1
findstr /C:"char* xrtFileReadAll" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtFileReadAll extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirScanEx" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtDirScanEx runtime prototype.
    exit /b 1
)
findstr /C:"xvalue xrtParseJSON" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtParseJSON extern prototype.
    exit /b 1
)
findstr /C:"xvalue xrtParseXSON" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtParseXSON extern prototype.
    exit /b 1
)
findstr /C:"xregex* xrtRegexCreate" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtRegexCreate runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtFormUrlEncodedDecodeTo" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtFormUrlEncodedDecodeTo runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtUrlResolveTo" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtUrlResolveTo runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtProcessRun" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtProcessRun runtime prototype.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -Encoding UTF8 'build\test\stdlib_error_paths.c'; if ($text -notmatch 'xrtFormUrlEncodedDecodeTo\([^\)]*\) \) \{\s*xrtFree\(outText\);\s*return xl6_query_empty_string\(\);') { Write-Host 'stdlib_error_paths.c missing query decode failure cleanup.'; exit 1 }; if ($text -notmatch 'if \( !xl6_url_parse_view\(base, baseSize, &baseUrl\) \) \{ return xl6_url_empty_string\(\); \}') { Write-Host 'stdlib_error_paths.c missing url resolve invalid base fallback.'; exit 1 }; if ($text -notmatch 'if \( result == 0 \) \{ return xl6_process_empty_string\(\); \}\s*outText = xl6_process_copy_bytes\(result->pStdout, result->iStdoutSize\);\s*xl6_xrt_process_result_destroy\(result\);\s*return outText;') { Write-Host 'stdlib_error_paths.c missing process exec result cleanup.'; exit 1 }; if ($text -notmatch 'bool ok = result != 0 && xrtProcessResultSuccess\(result\);\s*xl6_xrt_process_result_destroy\(result\);\s*return ok;') { Write-Host 'stdlib_error_paths.c missing process success result cleanup.'; exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_error_paths.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_error_paths.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "[System.IO.File]::WriteAllBytes('build\test\file_read_text_binary.bin', [byte[]](0x41,0x00,0x42,0x43))"
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_file_read_text_binary_reject.xl
if errorlevel 1 exit /b 1
build\test\stdlib_error_paths.exe > build\test\stdlib_error_paths.out.txt 2> build\test\stdlib_error_paths.err.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$out=[System.IO.File]::ReadAllBytes('build\test\stdlib_error_paths.out.txt'); $err=[System.IO.File]::ReadAllBytes('build\test\stdlib_error_paths.err.txt'); if($out.Length -ne 0){ Write-Host 'stdlib_error_paths.out.txt should be empty.'; exit 1 }; if($err.Length -ne 0){ Write-Host 'stdlib_error_paths.err.txt should be empty.'; exit 2 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_codec.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_codec.xl --emit build\test\stdlib_codec.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtHexEncode" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtHexEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtBase64Encode" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtBase64Encode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPercentEncode" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtPercentEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtFormUrlEncode" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtFormUrlEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtBufferFromHex" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtBufferFromHex extern prototype.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Buffer" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing Buffer class bridge for bytes decode.
    exit /b 1
)
findstr /C:"const char* __xl_std_codec_base64_decode" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing codec.base64Decode helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_codec.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_codec.exe
if errorlevel 1 exit /b 1
build\test\stdlib_codec.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_codec.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_codec_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_codec_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_codec_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_codec_c.exe > build\test\stdlib_codec_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_codec_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_codec_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib codec xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_encoding.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_encoding.xl --emit build\test\stdlib_encoding.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtConvCharset" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing xrtConvCharset runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_encoding_to_bytes" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing encoding.toBytes runtime helper.
    exit /b 1
)
findstr /C:"long long xl6_xrt_encoding_detect_text" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing encoding.detect runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_encoding_is_utf8_bytes" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing encoding.isUtf8Bytes runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_encoding_swap_utf16" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing UTF-16 endian runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_encoding_swap_utf32" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing UTF-32 endian runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_buffer_wrap_handle" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing Buffer handle wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_encoding.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_encoding.exe
if errorlevel 1 exit /b 1
build\test\stdlib_encoding.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_encoding.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_encoding_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_encoding_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_encoding_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_encoding_c.exe > build\test\stdlib_encoding_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_encoding_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_encoding_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib encoding xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_hash.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_hash.xl --emit build\test\stdlib_hash.c
if errorlevel 1 exit /b 1
findstr /C:"size_t strlen(const char* text);" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing strlen extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtHash32" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32 extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtHash32_WithSeed" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32_WithSeed extern prototype.
    exit /b 1
)
findstr /C:"unsigned long long xrtHash64" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64 extern prototype.
    exit /b 1
)
findstr /C:"unsigned long long xrtHash64_WithSeed" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64_WithSeed extern prototype.
    exit /b 1
)
findstr /C:"unsigned int __xl_std_hash_u32_with_seed" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing hash.u32WithSeed helper.
    exit /b 1
)
findstr /C:"unsigned long long __xl_std_hash_u64_with_seed" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing hash.u64WithSeed helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_hash.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_hash.exe
if errorlevel 1 exit /b 1
build\test\stdlib_hash.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_hash.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_hash_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_hash_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_hash_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_hash_c.exe > build\test\stdlib_hash_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_hash_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_hash_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib hash xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_crypto.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_crypto.xl --emit build\test\stdlib_crypto.c
if errorlevel 1 exit /b 1
findstr /C:"void xrtSHA256" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing xrtSHA256 runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_sha512_hex" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing sha512 runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_sha256_stream_create" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing sha256 stream runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_sha256_bytes" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing binary sha256 runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_aes256_gcm_decrypt_bytes" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing binary AEAD runtime helper.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_ed25519_keypair" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing ed25519 keypair runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_crypto.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_crypto.exe
if errorlevel 1 exit /b 1
build\test\stdlib_crypto.exe
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_crypto.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_crypto_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_crypto_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_crypto_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_crypto_c.exe > build\test\stdlib_crypto_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_crypto_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_crypto_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib crypto xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_id_net.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_id_net.xl --emit build\test\stdlib_id_net.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtMakeXIDS" build\test\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing xrtMakeXIDS extern prototype.
    exit /b 1
)
findstr /C:"void* xrtGetLocalName" build\test\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing xrtGetLocalName extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_id_net.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_id_net.exe
if errorlevel 1 exit /b 1
build\test\stdlib_id_net.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_id_net.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_id_net_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_id_net_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_id_net_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_id_net_c.exe > build\test\stdlib_id_net_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_id_net_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_id_net_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib id_net xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_url.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_url.xl --emit build\test\stdlib_url.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtUrlParseViewN" build\test\stdlib_url.c >nul
if errorlevel 1 (
    echo stdlib_url.c missing xrtUrlParseViewN runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_url_resolve" build\test\stdlib_url.c >nul
if errorlevel 1 (
    echo stdlib_url.c missing url resolve runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_url.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_url.exe
if errorlevel 1 exit /b 1
build\test\stdlib_url.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_url.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_url_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_url_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_url_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_url_c.exe > build\test\stdlib_url_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_url_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_url_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib url xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_query.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_query.xl --emit build\test\stdlib_query.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtPercentEncodeTo" build\test\stdlib_query.c >nul
if errorlevel 1 (
    echo stdlib_query.c missing xrtPercentEncodeTo runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_query_append" build\test\stdlib_query.c >nul
if errorlevel 1 (
    echo stdlib_query.c missing query append runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_query.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_query.exe
if errorlevel 1 exit /b 1
build\test\stdlib_query.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_query.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_query_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_query_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_query_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_query_c.exe > build\test\stdlib_query_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_query_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_query_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib query xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_log.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_log.xl --emit build\test\stdlib_log.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Logger" build\test\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing Logger class bridge.
    exit /b 1
)
findstr /C:"xlogger* xlogCreate" build\test\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing xlogCreate runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_log_create" build\test\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing log create runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_log_wrap_handle" build\test\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing log handle wrap helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_log_default_add_file" build\test\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing default addFile helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_log_add_rolling_file" build\test\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing rolling file helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_log.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_log.exe
if errorlevel 1 exit /b 1
build\test\stdlib_log.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_log_object.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_log_object.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_log_object.xl -o build\test\stdlib_log_object.exe
if errorlevel 1 exit /b 1
build\test\stdlib_log_object.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_log.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_log_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_log_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_log_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_log_c.exe > build\test\stdlib_log_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_log_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_log_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib log xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_template.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_template.xl --emit build\test\stdlib_template.c
if errorlevel 1 exit /b 1
findstr /C:"xtetemplate xteParse" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing xteParse runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_template_parse" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing template parse helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_template_render" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing template render helper.
    exit /b 1
)
findstr /C:"unsigned int xl6_xrt_template_node_count" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing template stats helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_template.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_template.exe
if errorlevel 1 exit /b 1
build\test\stdlib_template.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_template_engine.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_template_engine.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_template_engine.xl -o build\test\stdlib_template_engine.exe
if errorlevel 1 exit /b 1
build\test\stdlib_template_engine.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_template.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_template_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_template_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_template_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_template_c.exe > build\test\stdlib_template_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_template_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_template_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib template xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_regex.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_regex.xl --emit build\test\stdlib_regex.c
if errorlevel 1 exit /b 1
findstr /C:"xregex* xrtRegexCreate" build\test\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing xrtRegexCreate runtime prototype.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_xrt_regex_find" build\test\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing regex find runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_regex_match(" build\test\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing regex match runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_regex_match_ok" build\test\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing RegexMatch ok helper.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_regex_set_matches" build\test\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing RegexSet matches helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_regex.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_regex.exe
if errorlevel 1 exit /b 1
build\test\stdlib_regex.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_regex_extended.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_regex_extended.xl --emit build\test\stdlib_regex_extended.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_xrt_regex_match_capture_start" build\test\stdlib_regex_extended.c >nul
if errorlevel 1 (
    echo stdlib_regex_extended.c missing capture range helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_regex_error_message" build\test\stdlib_regex_extended.c >nul
if errorlevel 1 (
    echo stdlib_regex_extended.c missing compile diagnostic helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_regex_extended.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_regex_extended.exe
if errorlevel 1 exit /b 1
build\test\stdlib_regex_extended.exe
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_regex.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_regex_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_regex_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_regex_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_regex_c.exe > build\test\stdlib_regex_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_regex_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_regex_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib regex xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_os.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_os.xl --emit build\test\stdlib_os.c
if errorlevel 1 exit /b 1
findstr /C:"void* xl6_xrt_os_env" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing OS env runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_os_platform" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing OS platform runtime helper.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_os_args" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing OS args runtime helper.
    exit /b 1
)
findstr /C:"xl6_xrt_os_set_args(arg)" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing xlang_main OS args setup.
    exit /b 1
)
findstr /C:"xl6_xrt_os_clear_args()" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing xlang_main OS args cleanup.
    exit /b 1
)
findstr /C:"int xrtChain" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing xrtChain prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_process_exec" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing process exec helper dependency.
    exit /b 1
)
findstr /C:"void* xl6_xrt_process_spawn_shell" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing process shell start helper dependency.
    exit /b 1
)
findstr /C:"void* xl6_xrt_process_spawn_file" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing process file start helper dependency.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_os.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_os.exe
if errorlevel 1 exit /b 1
build\test\stdlib_os.exe > build\test\stdlib_os.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib_os=ok" build\test\stdlib_os.out.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_os.xl > build\test\stdlib_os_example.out.txt
if errorlevel 1 exit /b 1
findstr /C:"missing_env=fallback" build\test\stdlib_os_example.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_process.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_process.xl --emit build\test\stdlib_process.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtProcessRun" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing xrtProcessRun runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtProcessPipelineRun" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing xrtProcessPipelineRun runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_process_exec" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exec runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_process_exit_code" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exitCode runtime helper.
    exit /b 1
)
findstr /C:"xrtProcessReadEventsSince" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing incremental process event API.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_process.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_process.exe
if errorlevel 1 exit /b 1
build\test\stdlib_process.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_process_extended.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_process_extended.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_process_shell_quotes.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_process_shell_quotes.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_process.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_process_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_process_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_process_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_process_c.exe > build\test\stdlib_process_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_process_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_process_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib process xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_extended_config.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_net_extended_config.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_net_extended_config.xl -o build\test\stdlib_net_extended_config.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_extended_config.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_extended_io.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_net_extended_io.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_net_extended_io.xl -o build\test\stdlib_net_extended_io.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_extended_io.exe
if errorlevel 1 exit /b 1

if not "%XL6_ENABLE_DEFERRED_HTTP%"=="1" (
    echo Skipping deferred HTTP client/server checks. Set XL6_ENABLE_DEFERRED_HTTP=1 to run them.
    goto after_deferred_http
)

release\xl.exe check test\cases\stdlib_http_client.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_http_client.xl --emit build\test\stdlib_http_client.c
if errorlevel 1 exit /b 1
findstr /C:"void* xl6_xrt_http_client_request_create_url" build\test\stdlib_http_client.c >nul
if errorlevel 1 (
    echo stdlib_http_client.c missing http client request helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_client_response_body_text" build\test\stdlib_http_client.c >nul
if errorlevel 1 (
    echo stdlib_http_client.c missing http client response helper.
    exit /b 1
)
findstr /C:"static void xl6_http_client_free_response_value" build\test\stdlib_http_client.c >nul
if errorlevel 1 (
    echo stdlib_http_client.c missing typed future response release helper.
    exit /b 1
)
findstr /C:"xvoCreateHandle(xl6_type_desc_XL_Class_HttpClientResponse()" build\test\stdlib_http_client.c >nul
if errorlevel 1 (
    echo stdlib_http_client.c does not publish async responses as typed class values.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_get_request" build\test\stdlib_http_client.c >nul
if errorlevel 1 (
    echo stdlib_http_client.c missing http server request route helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\stdlib_http_client.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_http_client.exe
if errorlevel 1 exit /b 1
build\test\stdlib_http_client.exe > build\test\stdlib_http_client.out.txt
if errorlevel 1 exit /b 1
findstr /C:"http_get=ok" build\test\stdlib_http_client.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client.out.txt missing http_get verification.
    exit /b 1
)
findstr /C:"http_request_object=ok" build\test\stdlib_http_client.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client.out.txt missing http_request_object verification.
    exit /b 1
)
findstr /C:"http_post=ok" build\test\stdlib_http_client.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client.out.txt missing http_post verification.
    exit /b 1
)
findstr /C:"http_request_method=ok" build\test\stdlib_http_client.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client.out.txt missing http_request_method verification.
    exit /b 1
)
for %%L in (
    "http_request_bytes=ok"
    "http_request_proxy=ok"
    "http_response_metadata=ok"
    "http_diagnostics=ok"
    "http_async=ok"
    "http_stream=ok"
    "http_stream_abort=ok"
    "http_async_cancel=ok"
    "http_explicit_engine=ok"
) do (
    findstr /C:%%L build\test\stdlib_http_client.out.txt >nul
    if errorlevel 1 (
        echo stdlib_http_client.out.txt missing %%L verification.
        exit /b 1
    )
)
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_http_client_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_http_client_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_http_client_c.exe > build\test\stdlib_http_client_c.out.txt
if errorlevel 1 exit /b 1
findstr /C:"http_get=ok" build\test\stdlib_http_client_c.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client_c.out.txt missing http_get verification.
    exit /b 1
)
findstr /C:"http_request_object=ok" build\test\stdlib_http_client_c.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client_c.out.txt missing http_request_object verification.
    exit /b 1
)
findstr /C:"http_post=ok" build\test\stdlib_http_client_c.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client_c.out.txt missing http_post verification.
    exit /b 1
)
findstr /C:"http_request_method=ok" build\test\stdlib_http_client_c.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client_c.out.txt missing http_request_method verification.
    exit /b 1
)
for %%L in (
    "http_response_metadata=ok"
    "http_diagnostics=ok"
    "http_async=ok"
    "http_stream=ok"
    "http_stream_abort=ok"
    "http_async_cancel=ok"
) do (
    findstr /C:%%L build\test\stdlib_http_client_c.out.txt >nul
    if errorlevel 1 (
        echo stdlib_http_client_c.out.txt missing %%L verification.
        exit /b 1
    )
)
release\xl.exe run release\examples\stdlib_http_client_c.c > build\test\stdlib_http_client_c_xl_run.out.txt
if errorlevel 1 exit /b 1
findstr /C:"http_diagnostics=ok" build\test\stdlib_http_client_c_xl_run.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_client_c_xl_run.out.txt missing HTTP diagnostics verification.
    exit /b 1
)

release\xl.exe check test\cases\stdlib_http_server.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_http_server.xl --emit build\test\stdlib_http_server.c
if errorlevel 1 exit /b 1
findstr /C:"#define XL6_WEBSOCKET_RUNTIME" build\test\stdlib_http_server.c >nul
if not errorlevel 1 (
    echo stdlib_http_server.c unexpectedly contains WebSocket runtime.
    exit /b 1
)
findstr /C:"xwebserver* xrtWebServerCreate" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing xrtWebServerCreate runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_server_create" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_post" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server route helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_get" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server get helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_any" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server any helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_get_request" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server request route helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_request_param" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http request object helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_request_cookie" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http request cookie helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_request_body_data" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http request body data helper.
    exit /b 1
)
findstr /C:"long long xl6_xrt_http_request_body_size" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http request body size helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_request_connection_open" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing raw connection state helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_request_close_connection" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing raw connection close helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_text" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response object helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_add_header" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response addHeader helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_json_text" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response json helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_body" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response body helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_append_text" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response append text helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_file" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response file helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_file_range" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response file range helper.
    exit /b 1
)
findstr /C:"long long xl6_xrt_http_response_status_code" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response metadata helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_response_body_buffer" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response body view helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_redirect" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response redirect helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_static" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server static helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_app_create" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http app create helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_reload_app" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server reload app helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_host" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server host helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_post_stream_request" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server stream request helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_app_post_stream_request" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http app stream request helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_body_chunk_text" build\test\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http body chunk text helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\stdlib_http_server.c test\cases\stdlib_http_server_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_http_server.exe
if errorlevel 1 exit /b 1
build\test\stdlib_http_server.exe
if errorlevel 1 exit /b 1

"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_http_server_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_http_server_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_http_server_c.exe > build\test\stdlib_http_server_c.out.txt
if errorlevel 1 exit /b 1
findstr /C:"callback_count=ok" build\test\stdlib_http_server_c.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_server_c.exe missing callback_count verification.
    exit /b 1
)
release\xl.exe run release\examples\stdlib_http_server.xl > build\test\stdlib_http_server_example.out.txt
if errorlevel 1 exit /b 1
cmd /c release\projects\xs_static\test_smoke.bat
if errorlevel 1 exit /b 1
cmd /c release\projects\xs\test_smoke.bat
if errorlevel 1 exit /b 1

:after_deferred_http

release\xl.exe run test\cases\stdlib_http_semantics.xl > build\test\stdlib_http_semantics.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib http semantics ok" build\test\stdlib_http_semantics.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_websocket.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_websocket.xl --emit build\test\stdlib_websocket.c
if errorlevel 1 exit /b 1
findstr /C:"#define XL6_WEBSOCKET_RUNTIME" build\test\stdlib_websocket.c >nul
if errorlevel 1 (
    echo stdlib_websocket.c missing WebSocket runtime.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_websocket.xl > build\test\stdlib_websocket.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib websocket ok" build\test\stdlib_websocket.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_websocket_http_upgrade.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_websocket_http_upgrade.xl --emit build\test\stdlib_websocket_http_upgrade.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_http_response_upgrade_websocket" build\test\stdlib_websocket_http_upgrade.c >nul
if errorlevel 1 (
    echo stdlib_websocket_http_upgrade.c missing shared HTTP Upgrade bridge.
    exit /b 1
)
findstr /C:"xrtWebServerCreateHostFullEx((xnetengine*)engine" build\test\stdlib_websocket_http_upgrade.c >nul
if errorlevel 1 (
    echo stdlib_websocket_http_upgrade.c missing shared NetEngine HTTP server path.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_websocket_http_upgrade.xl > build\test\stdlib_websocket_http_upgrade.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib websocket http upgrade ok" build\test\stdlib_websocket_http_upgrade.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_websocket_tls.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_websocket_tls.xl --emit build\test\stdlib_websocket_tls.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_tls_lease_create" build\test\stdlib_websocket_tls.c >nul
if errorlevel 1 (
    echo stdlib_websocket_tls.c missing TLS lease runtime.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_websocket_tls.xl > build\test\stdlib_websocket_tls.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib websocket tls ok" build\test\stdlib_websocket_tls.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_tcc_import_only.xl > build\test\stdlib_tcc_import_only.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib tcc import only ok" build\test\stdlib_tcc_import_only.out.txt >nul
if errorlevel 1 (
    echo stdlib_tcc_import_only.out.txt missing ok output.
    exit /b 1
)

release\xl.exe run test\cases\stdlib_tcc_context_only.xl > build\test\stdlib_tcc_context_only.out.txt
if errorlevel 1 exit /b 1
findstr /C:"after tcc context" build\test\stdlib_tcc_context_only.out.txt >nul
if errorlevel 1 (
    echo stdlib_tcc_context_only.out.txt missing context output.
    exit /b 1
)

release\xl.exe cgen test\cases\stdlib_tcc_embed.xl --emit build\test\stdlib_tcc_embed.c
if errorlevel 1 exit /b 1
findstr /C:"void* xl6_tcc_context_create(void)" build\test\stdlib_tcc_embed.c >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.c missing TCC context helper.
    exit /b 1
)
findstr /C:"int tcc_relocate(void* s)" build\test\stdlib_tcc_embed.c >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.c missing TCC relocate prototype.
    exit /b 1
)
findstr /C:"bool xrtWebResponseAppendText" build\test\stdlib_tcc_embed.c >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.c missing XRT response append symbol prototype.
    exit /b 1
)
findstr /C:"bool xrtWebServerStreamRouteAsync" build\test\stdlib_tcc_embed.c >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.c missing XRT async stream route symbol prototype.
    exit /b 1
)
findstr /C:"XL6_TCC_ADD_SYMBOL(xrtWebResponseAppendText)" build\test\stdlib_tcc_embed.c >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.c missing XRT response append symbol registration.
    exit /b 1
)
findstr /C:"XL6_TCC_ADD_SYMBOL(xrtWebServerStreamRouteAsync)" build\test\stdlib_tcc_embed.c >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.c missing XRT async stream route symbol registration.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_tcc_embed.xl > build\test\stdlib_tcc_embed.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib tcc embed ok" build\test\stdlib_tcc_embed.out.txt >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.out.txt missing xlang ok output.
    exit /b 1
)
findstr /C:"c from xlang tcc.run" build\test\stdlib_tcc_embed.out.txt >nul
if errorlevel 1 (
    echo stdlib_tcc_embed.out.txt missing nested C output.
    exit /b 1
)

release\xl.exe run release\examples\script_embed_tcc.xl > build\test\script_embed_tcc.out.txt
if errorlevel 1 exit /b 1
findstr /C:"script_embed_tcc=ok" build\test\script_embed_tcc.out.txt >nul
if errorlevel 1 (
    echo script_embed_tcc.out.txt missing ok output.
    exit /b 1
)

release\xl.exe cgen test\cases\stdlib_xlang_embed.xl --emit build\test\stdlib_xlang_embed.c
if errorlevel 1 exit /b 1
findstr /C:"void* xl6_xlang_context_create(void)" build\test\stdlib_xlang_embed.c >nul
if errorlevel 1 (
    echo stdlib_xlang_embed.c missing xlang context helper.
    exit /b 1
)
findstr /C:"int xl6_xlang_run_source" build\test\stdlib_xlang_embed.c >nul
if errorlevel 1 (
    echo stdlib_xlang_embed.c missing xlang run helper.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_xlang_embed.xl > build\test\stdlib_xlang_embed.out.txt
if errorlevel 1 exit /b 1
findstr /C:"nested xlang shortcut ok" build\test\stdlib_xlang_embed.out.txt >nul
if errorlevel 1 (
    echo stdlib_xlang_embed.out.txt missing shortcut output.
    exit /b 1
)
findstr /C:"nested xlang file ok" build\test\stdlib_xlang_embed.out.txt >nul
if errorlevel 1 (
    echo stdlib_xlang_embed.out.txt missing nested file output.
    exit /b 1
)
findstr /C:"stdlib xlang embed ok" build\test\stdlib_xlang_embed.out.txt >nul
if errorlevel 1 (
    echo stdlib_xlang_embed.out.txt missing xlang embed ok output.
    exit /b 1
)

release\xl.exe run release\examples\script_embed_xlang.xl > build\test\script_embed_xlang.out.txt
if errorlevel 1 exit /b 1
findstr /C:"script_embed_xlang=ok" build\test\script_embed_xlang.out.txt >nul
if errorlevel 1 (
    echo script_embed_xlang.out.txt missing ok output.
    exit /b 1
)

release\xl.exe run test\cases\xlang_nested_child.xl > build\test\xlang_nested_child.out.txt
if errorlevel 1 exit /b 1
findstr /C:"nested xlang file ok" build\test\xlang_nested_child.out.txt >nul
if errorlevel 1 (
    echo xlang_nested_child.out.txt missing nested file output.
    exit /b 1
)

release\xl.exe run test\cases\stdlib_special_collections.xl > build\test\stdlib_special_collections.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib special collections ok" build\test\stdlib_special_collections.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_special_collections_lifecycle.xl > build\test\stdlib_special_collections_lifecycle.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib special collection lifecycle ok" build\test\stdlib_special_collections_lifecycle.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_special_collections_concurrent.xl > build\test\stdlib_special_collections_concurrent.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib special collection concurrent ok" build\test\stdlib_special_collections_concurrent.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_special_collections_async.xl > build\test\stdlib_special_collections_async.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib special collection async ok" build\test\stdlib_special_collections_async.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_special_collection_temporary_receiver.xl >nul 2> build\test\sem_special_collection_temporary_receiver.err.txt
if not errorlevel 1 (
    echo sem_special_collection_temporary_receiver.xl should fail.
    exit /b 1
)
findstr /C:"[SC-TEMP-RECEIVER]" build\test\sem_special_collection_temporary_receiver.err.txt >nul
if errorlevel 1 (
    echo sem_special_collection_temporary_receiver.xl missing temporary receiver diagnostic.
    exit /b 1
)

release\xl.exe run release\examples\stdlib_special_collections.xl > build\test\stdlib_special_collections_example.out.txt
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_special_collections_c.c > build\test\stdlib_special_collections_c_example.out.txt
if errorlevel 1 exit /b 1
fc build\test\stdlib_special_collections_example.out.txt build\test\stdlib_special_collections_c_example.out.txt >nul
if errorlevel 1 (
    echo special collection C/xlang example output mismatch.
    exit /b 1
)

release\xl.exe run test\cases\stdlib_tls_session.xl > build\test\stdlib_tls_session.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session=ok" build\test\stdlib_tls_session.out.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_tls_session.xl --emit build\test\stdlib_tls_session.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_tls_session_create" build\test\stdlib_tls_session.c >nul
if errorlevel 1 (
    echo stdlib_tls_session.c missing raw TLS wrapper.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_tls_session.xl -o build\test\stdlib_tls_session.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_session.exe > build\test\stdlib_tls_session_static.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session=ok" build\test\stdlib_tls_session_static.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_tls_session_pair.xl > build\test\stdlib_tls_session_pair.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session_pair=ok" build\test\stdlib_tls_session_pair.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_tls_session_pair.xl > build\test\stdlib_tls_session_pair_example.out.txt
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_tls_session_pair_c.c > build\test\stdlib_tls_session_pair_c_example.out.txt
if errorlevel 1 exit /b 1
fc build\test\stdlib_tls_session_pair_example.out.txt build\test\stdlib_tls_session_pair_c_example.out.txt >nul
if errorlevel 1 (
    echo raw TLS C/xlang example output mismatch.
    exit /b 1
)

release\xl.exe run test\cases\stdlib_url_query_extended.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_xid_object.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_url.xl > build\test\stdlib_url_extended_xlang.out.txt
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_url_c.c > build\test\stdlib_url_extended_c.out.txt
if errorlevel 1 exit /b 1
fc build\test\stdlib_url_extended_xlang.out.txt build\test\stdlib_url_extended_c.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_query.xl > build\test\stdlib_query_extended_xlang.out.txt
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_query_c.c > build\test\stdlib_query_extended_c.out.txt
if errorlevel 1 exit /b 1
fc build\test\stdlib_query_extended_xlang.out.txt build\test\stdlib_query_extended_c.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_id_net.xl > build\test\stdlib_id_extended_xlang.out.txt
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_id_net_c.c > build\test\stdlib_id_extended_c.out.txt
if errorlevel 1 exit /b 1
fc build\test\stdlib_id_extended_xlang.out.txt build\test\stdlib_id_extended_c.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_signal.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_signal.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_signal.xl > build\test\stdlib_signal_xlang.out.txt
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_os.xl > build\test\stdlib_os_xlang.out.txt
if errorlevel 1 exit /b 1
findstr /C:"chain_exit=0" build\test\stdlib_os_xlang.out.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_os_c.c > build\test\stdlib_os_c.out.txt
if errorlevel 1 exit /b 1
for %%L in ("args=ok" "platform=ok" "cwd=ok" "env=ok" "run=ok" "startFile=ok" "chain=ok") do (
    findstr /C:%%L build\test\stdlib_os_c.out.txt >nul
    if errorlevel 1 exit /b 1
)

echo DEMO6 suite ok: extended standard library
exit /b 0

