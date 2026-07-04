@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: extended standard library...

release\xl.exe check test\cases\stdlib_json.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_json.xl --emit release\tmp\stdlib_json.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xrtParseJSON" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtParseJSON extern prototype.
    exit /b 1
)
findstr /C:"bool xrtJsonValid" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtJsonValid extern prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyJSON" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtStringifyJSON extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_json_stringify" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing json.stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_json.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_json.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_json.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_json.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_json_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_json_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_json_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_json_c.exe > release\tmp\stdlib_json_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_json_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_json_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib json xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_xson.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_xson.xl --emit release\tmp\stdlib_xson.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xrtParseXSON" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtParseXSON extern prototype.
    exit /b 1
)
findstr /C:"bool xrtXsonValid" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtXsonValid extern prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyXSON" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtStringifyXSON extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_xson_stringify" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xson.stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_xson.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_xson.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_xson.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_xson.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_xson_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_xson_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_xson_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_xson_c.exe > release\tmp\stdlib_xson_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_xson_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_xson_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib xson xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_print.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_print.xl --emit release\tmp\stdlib_print.c
if errorlevel 1 exit /b 1
findstr /C:"void xl6_xrt_print(xvalue values, bool newline)" release\tmp\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing print runtime helper.
    exit /b 1
)
findstr /C:"void* xrtStringifyJSON" release\tmp\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing xrtStringifyJSON runtime prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyXSON" release\tmp\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing xrtStringifyXSON runtime prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_print.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_print.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_print.exe > release\tmp\stdlib_print.out.txt
if errorlevel 1 exit /b 1
findstr /C:"null" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing null output.
    exit /b 1
)
findstr /C:"true false 123 -45" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing scalar output.
    exit /b 1
)
findstr /C:"[1,2,3]" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing array JSON output.
    exit /b 1
)
findstr /C:"name" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing dict JSON output.
    exit /b 1
)

release\xl.exe check test\cases\stdlib_error_paths.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_error_paths.xl --emit release\tmp\stdlib_error_paths.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtFileReadAll" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtFileReadAll extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirScanEx" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtDirScanEx runtime prototype.
    exit /b 1
)
findstr /C:"xvalue xrtParseJSON" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtParseJSON extern prototype.
    exit /b 1
)
findstr /C:"xvalue xrtParseXSON" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtParseXSON extern prototype.
    exit /b 1
)
findstr /C:"xregex* xrtRegexCreate" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtRegexCreate runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtFormUrlEncodedDecodeTo" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtFormUrlEncodedDecodeTo runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtUrlResolveTo" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtUrlResolveTo runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtProcessRun" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtProcessRun runtime prototype.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -Encoding UTF8 'release\tmp\stdlib_error_paths.c'; if ($text -notmatch 'xrtFormUrlEncodedDecodeTo\([^\)]*\) \) \{\s*xrtFree\(outText\);\s*return xl6_query_empty_string\(\);') { Write-Host 'stdlib_error_paths.c missing query decode failure cleanup.'; exit 1 }; if ($text -notmatch 'if \( !xl6_url_parse_view\(base, baseSize, &baseUrl\) \) \{ return xl6_url_empty_string\(\); \}') { Write-Host 'stdlib_error_paths.c missing url resolve invalid base fallback.'; exit 1 }; if ($text -notmatch 'if \( result == 0 \) \{ return xl6_process_empty_string\(\); \}\s*outText = xl6_process_copy_bytes\(result->pStdout, result->iStdoutSize\);\s*xl6_xrt_process_result_destroy\(result\);\s*return outText;') { Write-Host 'stdlib_error_paths.c missing process exec result cleanup.'; exit 1 }; if ($text -notmatch 'bool ok = result != 0 && xrtProcessResultSuccess\(result\);\s*xl6_xrt_process_result_destroy\(result\);\s*return ok;') { Write-Host 'stdlib_error_paths.c missing process success result cleanup.'; exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_error_paths.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_error_paths.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_error_paths.exe > release\tmp\stdlib_error_paths.out.txt 2> release\tmp\stdlib_error_paths.err.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$out=[System.IO.File]::ReadAllBytes('release\tmp\stdlib_error_paths.out.txt'); $err=[System.IO.File]::ReadAllBytes('release\tmp\stdlib_error_paths.err.txt'); if($out.Length -ne 0){ Write-Host 'stdlib_error_paths.out.txt should be empty.'; exit 1 }; if($err.Length -ne 0){ Write-Host 'stdlib_error_paths.err.txt should be empty.'; exit 2 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_codec.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_codec.xl --emit release\tmp\stdlib_codec.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtHexEncode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtHexEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtBase64Encode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtBase64Encode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPercentEncode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtPercentEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtFormUrlEncode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtFormUrlEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtBufferFromHex" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtBufferFromHex extern prototype.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Buffer" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing Buffer class bridge for bytes decode.
    exit /b 1
)
findstr /C:"const char* __xl_std_codec_base64_decode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing codec.base64Decode helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_codec.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_codec.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_codec.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_codec.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_codec_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_codec_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_codec_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_codec_c.exe > release\tmp\stdlib_codec_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_codec_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_codec_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib codec xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_encoding.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_encoding.xl --emit release\tmp\stdlib_encoding.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtConvCharset" release\tmp\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing xrtConvCharset runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_encoding_to_bytes" release\tmp\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing encoding.toBytes runtime helper.
    exit /b 1
)
findstr /C:"long long xl6_xrt_encoding_detect_text" release\tmp\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing encoding.detect runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_encoding_is_utf8_bytes" release\tmp\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing encoding.isUtf8Bytes runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_buffer_wrap_handle" release\tmp\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing Buffer handle wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_encoding.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_encoding.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_encoding.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_encoding.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_encoding_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_encoding_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_encoding_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_encoding_c.exe > release\tmp\stdlib_encoding_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_encoding_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_encoding_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib encoding xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_hash.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_hash.xl --emit release\tmp\stdlib_hash.c
if errorlevel 1 exit /b 1
findstr /C:"size_t strlen(const char* text);" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing strlen extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtHash32" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32 extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtHash32_WithSeed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32_WithSeed extern prototype.
    exit /b 1
)
findstr /C:"unsigned long long xrtHash64" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64 extern prototype.
    exit /b 1
)
findstr /C:"unsigned long long xrtHash64_WithSeed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64_WithSeed extern prototype.
    exit /b 1
)
findstr /C:"unsigned int __xl_std_hash_u32_with_seed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing hash.u32WithSeed helper.
    exit /b 1
)
findstr /C:"unsigned long long __xl_std_hash_u64_with_seed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing hash.u64WithSeed helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_hash.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_hash.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_hash.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_hash.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_hash_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_hash_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_hash_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_hash_c.exe > release\tmp\stdlib_hash_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_hash_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_hash_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib hash xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_crypto.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_crypto.xl --emit release\tmp\stdlib_crypto.c
if errorlevel 1 exit /b 1
findstr /C:"void xrtSHA256" release\tmp\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing xrtSHA256 runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_sha512_hex" release\tmp\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing sha512 runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_sha256_stream_create" release\tmp\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing sha256 stream runtime helper.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_ed25519_keypair" release\tmp\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing ed25519 keypair runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_crypto.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_crypto.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_crypto.exe
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_crypto.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_crypto_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_crypto_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_crypto_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_crypto_c.exe > release\tmp\stdlib_crypto_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_crypto_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_crypto_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib crypto xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_id_net.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_id_net.xl --emit release\tmp\stdlib_id_net.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtMakeXIDS" release\tmp\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing xrtMakeXIDS extern prototype.
    exit /b 1
)
findstr /C:"void* xrtGetLocalName" release\tmp\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing xrtGetLocalName extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_id_net.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_id_net.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_id_net.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_id_net.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_id_net_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_id_net_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_id_net_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_id_net_c.exe > release\tmp\stdlib_id_net_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_id_net_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_id_net_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib id_net xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_url.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_url.xl --emit release\tmp\stdlib_url.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtUrlParseViewN" release\tmp\stdlib_url.c >nul
if errorlevel 1 (
    echo stdlib_url.c missing xrtUrlParseViewN runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_url_resolve" release\tmp\stdlib_url.c >nul
if errorlevel 1 (
    echo stdlib_url.c missing url resolve runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_url.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_url.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_url.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_url.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_url_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_url_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_url_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_url_c.exe > release\tmp\stdlib_url_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_url_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_url_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib url xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_query.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_query.xl --emit release\tmp\stdlib_query.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtPercentEncodeTo" release\tmp\stdlib_query.c >nul
if errorlevel 1 (
    echo stdlib_query.c missing xrtPercentEncodeTo runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_query_append" release\tmp\stdlib_query.c >nul
if errorlevel 1 (
    echo stdlib_query.c missing query append runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_query.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_query.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_query.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_query.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_query_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_query_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_query_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_query_c.exe > release\tmp\stdlib_query_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_query_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_query_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib query xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_log.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_log.xl --emit release\tmp\stdlib_log.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Logger" release\tmp\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing Logger class bridge.
    exit /b 1
)
findstr /C:"xlogger* xlogCreate" release\tmp\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing xlogCreate runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_log_create" release\tmp\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing log create runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_log_wrap_handle" release\tmp\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing log handle wrap helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_log_default_add_file" release\tmp\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing default addFile helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_log_add_rolling_file" release\tmp\stdlib_log.c >nul
if errorlevel 1 (
    echo stdlib_log.c missing rolling file helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_log.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_log.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_log.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_log.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_log_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_log_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_log_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_log_c.exe > release\tmp\stdlib_log_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_log_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_log_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib log xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_template.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_template.xl --emit release\tmp\stdlib_template.c
if errorlevel 1 exit /b 1
findstr /C:"xtetemplate xteParse" release\tmp\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing xteParse runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_template_parse" release\tmp\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing template parse helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_template_render" release\tmp\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing template render helper.
    exit /b 1
)
findstr /C:"unsigned int xl6_xrt_template_node_count" release\tmp\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing template stats helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_template.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_template.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_template.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_template.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_template_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_template_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_template_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_template_c.exe > release\tmp\stdlib_template_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_template_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_template_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib template xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_regex.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_regex.xl --emit release\tmp\stdlib_regex.c
if errorlevel 1 exit /b 1
findstr /C:"xregex* xrtRegexCreate" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing xrtRegexCreate runtime prototype.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_xrt_regex_find" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing regex find runtime helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_regex_match(" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing regex match runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_regex_match_ok" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing RegexMatch ok helper.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_regex_set_matches" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing RegexSet matches helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_regex.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_regex.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_regex.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_regex.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_regex_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_regex_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_regex_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_regex_c.exe > release\tmp\stdlib_regex_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_regex_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_regex_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib regex xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_process.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_process.xl --emit release\tmp\stdlib_process.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtProcessRun" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing xrtProcessRun runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtProcessPipelineRun" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing xrtProcessPipelineRun runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_process_exec" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exec runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_process_exit_code" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exitCode runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_process.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_process.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_process.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_process.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_process_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_process_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_process_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_process_c.exe > release\tmp\stdlib_process_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_process_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_process_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib process xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_http_server.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_http_server.xl --emit release\tmp\stdlib_http_server.c
if errorlevel 1 exit /b 1
findstr /C:"xwebserver* xrtWebServerCreate" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing xrtWebServerCreate runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_server_create" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_post" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server route helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_get" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server get helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_any" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server any helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_get_request" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server request route helper.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_request_param" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http request object helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_http_response_text" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http response object helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_http_server_static" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server static helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_http_server.c test\cases\stdlib_http_server_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_http_server.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_http_server.exe
if errorlevel 1 exit /b 1

"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_http_server_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_http_server_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_http_server_c.exe > release\tmp\stdlib_http_server_c.out.txt
if errorlevel 1 exit /b 1
findstr /C:"callback_count=ok" release\tmp\stdlib_http_server_c.out.txt >nul
if errorlevel 1 (
    echo stdlib_http_server_c.exe missing callback_count verification.
    exit /b 1
)


echo DEMO6 suite ok: extended standard library
exit /b 0

