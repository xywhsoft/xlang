@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
if "%XL6_TRACE_TESTS%"=="1" echo on
echo Running DEMO6 suite: extended standard library...
set "PATH=%CD%\release;%PATH%"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_vfs.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$root=(Resolve-Path 'build\test').Path; for($i=0; $i -lt 20; $i++){ $left=@(Get-ChildItem -LiteralPath $root -Filter 'stdlib_*.exe' -File); if($left.Count -eq 0){ break }; foreach($f in $left){ Remove-Item -LiteralPath $f.FullName -Force -ErrorAction SilentlyContinue }; if($i -lt 19){ Start-Sleep -Milliseconds 250 } }; $left=@(Get-ChildItem -LiteralPath $root -Filter 'stdlib_*.exe' -File); if($left.Count -gt 0){ Write-Host 'stdlib test executables are still present before build.'; $left.FullName; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_buffer_file_runtime.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\stdlib_file_bytes_only.xl -o build\test\stdlib_file_bytes_only.exe
if errorlevel 1 exit /b 1
build\test\stdlib_file_bytes_only.exe
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_base64_text_codec_abi.ps1
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_codec_base64_text_only.xl -o build\test\stdlib_codec_base64_text_only.exe
if errorlevel 1 exit /b 1
build\test\stdlib_codec_base64_text_only.exe
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_byte_codec_abi.ps1
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_codec_hex_bytes_only.xl -o build\test\stdlib_codec_hex_bytes_only.exe
if errorlevel 1 exit /b 1
build\test\stdlib_codec_hex_bytes_only.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_dev_codec_crypto_buffer_build.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_dev_codec_crypto_buffer_build.xl -o build\test\stdlib_dev_codec_crypto_buffer_build.exe
if errorlevel 1 exit /b 1
build\test\stdlib_dev_codec_crypto_buffer_build.exe
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_json_value_bridge.ps1
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_json.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_json.xl --emit build\test\stdlib_json.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue* xl6_xrt_json_parse" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xl6_xrt_json_parse runtime prototype.
    exit /b 1
)
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_json.c >nul
if not errorlevel 1 (
    echo stdlib_json.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"void* xl6_xrt_json_stringify" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xl6_xrt_json_stringify runtime prototype.
    exit /b 1
)
findstr /C:"= xl6_xrt_json_stringify(" build\test\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing json.stringify resolved runtime call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_json.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_json.exe
if errorlevel 1 exit /b 1
build\test\stdlib_json.exe
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_json_stream_bridge.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_native_factory_declaration_abi.ps1
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_json_stream.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_json_stream.xl --emit build\test\stdlib_json_stream.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_json_stream.c >nul
if not errorlevel 1 (
    echo stdlib_json_stream.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xl6_xrt_json_writer_create" build\test\stdlib_json_stream.c >nul
if errorlevel 1 (
    echo stdlib_json_stream.c missing json writer runtime adapter.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_json_stream.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_json_stream.exe
if errorlevel 1 exit /b 1
build\test\stdlib_json_stream.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_json.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_json_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_json_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_json_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_json_c.exe > build\test\stdlib_json_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_json_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_json_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib json xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_xson_value_bridge.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_xson_stream_bridge.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_host_error_boundary.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_serialization_callback_error.ps1
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_xson.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_xson.xl --emit build\test\stdlib_xson.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue* xl6_xrt_xson_parse_options" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xl6_xrt_xson_parse_options runtime prototype.
    exit /b 1
)
findstr /C:"bool xl6_xrt_xson_valid_options" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xl6_xrt_xson_valid_options runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_xson_stringify_options" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xl6_xrt_xson_stringify_options runtime prototype.
    exit /b 1
)
findstr /C:"= xl6_xrt_xson_stringify_options(" build\test\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xson.stringify resolved runtime call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_xson.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_xson.exe
if errorlevel 1 exit /b 1
build\test\stdlib_xson.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_xson.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_xson_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_xson_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_xson_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_xson_c.exe > build\test\stdlib_xson_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_xson_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_xson_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib xson xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_text_output_bridge.ps1
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_print.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_print.xl --emit build\test\stdlib_print.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_print.c >nul
if not errorlevel 1 (
    echo stdlib_print.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"    xl6_xrt_print(" build\test\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing explicit xvalue print runtime call.
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
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_print.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_print.exe
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

release\xl.exe check test\cases\cgen_nested_typed_literal_trim.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_nested_typed_literal_trim.xl --emit build\test\cgen_nested_typed_literal_trim.c
if errorlevel 1 exit /b 1
findstr /C:"= xrtValueArray(" build\test\cgen_nested_typed_literal_trim.c >nul
if errorlevel 1 (
    echo cgen_nested_typed_literal_trim.c missing nested xvalue array constructor.
    exit /b 1
)
findstr /C:"= xrtValueSet(" build\test\cgen_nested_typed_literal_trim.c >nul
if errorlevel 1 (
    echo cgen_nested_typed_literal_trim.c missing nested xvalue set constructor.
    exit /b 1
)
release\xl.exe build test\cases\cgen_nested_typed_literal_trim.xl -o build\test\cgen_nested_typed_literal_trim.exe
if errorlevel 1 exit /b 1
build\test\cgen_nested_typed_literal_trim.exe > build\test\cgen_nested_typed_literal_trim.out.txt
if errorlevel 1 exit /b 1
findstr /C:"red" build\test\cgen_nested_typed_literal_trim.out.txt >nul
if errorlevel 1 (
    echo cgen_nested_typed_literal_trim.out.txt missing set output.
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_system_bridges.ps1
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_error_paths.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_error_paths.xl --emit build\test\stdlib_error_paths.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_error_paths.c >nul
if not errorlevel 1 (
    echo stdlib_error_paths.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xvalue* xl6_xrt_dir_list(const char* path, bool recursive, int mode)" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing the shared directory-list implementation with its exact ABI.
    exit /b 1
)
findstr /C:"xvalue* xl6_xrt_json_parse" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xl6_xrt_json_parse runtime prototype.
    exit /b 1
)
findstr /C:"xvalue* xl6_xrt_xson_parse_options" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xl6_xrt_xson_parse_options runtime prototype.
    exit /b 1
)
findstr /C:"xl6ProcessRunV1" build\test\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing process runtime adapter.
    exit /b 1
)
rem check_xlir_system_bridges above executes the shared process component with
rem native destructor instrumentation, including partial-output failure/timeout.
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_error_paths.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_error_paths.exe
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
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_codec.c >nul
if not errorlevel 1 (
    echo stdlib_codec.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xl6CodecBase64TextV1" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing base64 runtime adapter.
    exit /b 1
)
findstr /C:"xl6CodecPercentTextV1" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing percent runtime adapter.
    exit /b 1
)
findstr /C:"xl6CodecHexDecodeV1" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing resident byte-result hex adapter.
    exit /b 1
)
findstr /C:"= xl6CodecBase64DecodeV1(" build\test\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing resolved base64 decode call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_codec.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_codec.exe
if errorlevel 1 exit /b 1
build\test\stdlib_codec.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_codec.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_codec_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_codec_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_codec_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_codec_c.exe > build\test\stdlib_codec_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_codec_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_codec_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib codec xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_encoding.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_encoding.xl --emit build\test\stdlib_encoding.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_encoding.c >nul
if not errorlevel 1 (
    echo stdlib_encoding.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xl6EncodingEncodeV1(" /C:"xl6EncodingDecodeV1(" /C:"xl6EncodingTranscodeV1(" build\test\stdlib_encoding.c >nul
if errorlevel 1 (
    echo stdlib_encoding.c missing declared Unicode codec calls.
    exit /b 1
)
findstr /C:"xl6_xrt_encoding_" /C:"xl6_xrt_take_cstr" /C:"xl6_xrt_buffer_wrap_handle" build\test\stdlib_encoding.c >nul
if not errorlevel 1 (
    echo stdlib_encoding.c retains obsolete encoding or Buffer helpers.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_encoding.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_encoding.exe
if errorlevel 1 exit /b 1
build\test\stdlib_encoding.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_encoding.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_encoding_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_encoding_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_encoding_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_encoding_c.exe > build\test\stdlib_encoding_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_encoding_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_encoding_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib encoding xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_hash.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_hash.xl --emit build\test\stdlib_hash.c
if errorlevel 1 exit /b 1
findstr /C:"#include <string.h>" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing standard string header.
    exit /b 1
)
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_hash.c >nul
if not errorlevel 1 (
    echo stdlib_hash.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xrtHash32Seed(" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32Seed use.
    exit /b 1
)
findstr /C:"xrtHash64(" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64 use.
    exit /b 1
)
findstr /C:"xrtHash64Seed(" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64Seed use.
    exit /b 1
)
findstr /C:"extern unsigned int xrtHash32Seed(const char*, size_t, unsigned int);" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing exact u32 seeded hash ABI.
    exit /b 1
)
findstr /C:"extern unsigned long long xrtHash64Seed(const char*, size_t, unsigned long long);" build\test\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing exact u64 seeded hash ABI.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\stdlib_hash.c %XL6_CGEN_LINKS% -o build\test\stdlib_hash.exe
if errorlevel 1 exit /b 1
build\test\stdlib_hash.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_hash.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_hash_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_hash_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_hash_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_hash_c.exe > build\test\stdlib_hash_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_hash_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_hash_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib hash xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_crypto.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_crypto.xl --emit build\test\stdlib_crypto.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_crypto.c >nul
if not errorlevel 1 (
    echo stdlib_crypto.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xl6CryptoSha512TextHexV1(" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing resident sha512 call.
    exit /b 1
)
findstr /C:"xl6CryptoSha256CreateV1(" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing resident sha256 stream call.
    exit /b 1
)
findstr /C:"xl6CryptoSha256BufferV1(" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing resident Buffer sha256 call.
    exit /b 1
)
findstr /C:"xl6CryptoAesGcmOpenV1(" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing resident AEAD open call.
    exit /b 1
)
findstr /C:"xl6CryptoEd25519GenerateV1(" build\test\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing managed ed25519 key factory.
    exit /b 1
)
findstr /C:"xl6_xrt_sha" /C:"xl6_xrt_aes" /C:"xl6_xrt_chacha20" /C:"xl6_xrt_ed25519" build\test\stdlib_crypto.c >nul
if not errorlevel 1 (
    echo stdlib_crypto.c still uses a retired crypto wrapper.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_crypto.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_crypto.exe
if errorlevel 1 exit /b 1
build\test\stdlib_crypto.exe
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_crypto.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_crypto_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_crypto_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_crypto_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_crypto_c.exe > build\test\stdlib_crypto_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_crypto_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_crypto_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib crypto xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_id_net.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_id_net.xl --emit build\test\stdlib_id_net.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_id_net.c >nul
if not errorlevel 1 (
    echo stdlib_id_net.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xl6NetHostNameV1(" build\test\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing resident host-name call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_id_net.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_id_net.exe
if errorlevel 1 exit /b 1
build\test\stdlib_id_net.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_id_net.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_id_net_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_id_net_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_id_net_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_id_net_c.exe > build\test\stdlib_id_net_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_id_net_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_id_net_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib id_net xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_logging.ps1
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_template.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_template.xl --emit build\test\stdlib_template.c
if errorlevel 1 exit /b 1
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_template.c >nul
if not errorlevel 1 (
    echo stdlib_template.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"xteParse" /C:"xteCreateEngine" /C:"xteMake" build\test\stdlib_template.c >nul
if not errorlevel 1 (
    echo stdlib_template.c still contains removed XTE compatibility symbols.
    exit /b 1
)
findstr /C:"xl6TemplateCreateV1" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing resident template constructor.
    exit /b 1
)
findstr /C:"xl6TemplateRenderV1" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing checked resident render declaration.
    exit /b 1
)
findstr /C:"xl6TemplateNodeCountV1" build\test\stdlib_template.c >nul
if errorlevel 1 (
    echo stdlib_template.c missing stable template node view helper.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_template.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_template.exe
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
for %%F in (stdlib_compress_values stdlib_template_values stdlib_template_privacy stdlib_template_engine_values stdlib_template_files stdlib_native_field_mixin) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)
release\xl.exe run release\examples\stdlib_template.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_template_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_template_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_template_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_template_c.exe > build\test\stdlib_template_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_template_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_template_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib template xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
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
findstr /C:"xvalue* xl6_xrt_os_args" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing OS args runtime helper.
    exit /b 1
)
findstr /C:"= xl6_xrt_os_args();" build\test\stdlib_os.c >nul
if errorlevel 1 (
    echo stdlib_os.c missing borrowed host argument snapshot call.
    exit /b 1
)
findstr /C:"xl6_xrt_os_clear_args()" build\test\stdlib_os.c >nul
if not errorlevel 1 (
    echo stdlib_os.c must leave host argument cleanup to the entry adapter.
    exit /b 1
)
findstr /C:"xrtChain" build\test\stdlib_os.c >nul
if not errorlevel 1 (
    echo stdlib_os.c still references removed xrtChain compatibility API.
    exit /b 1
)
findstr /C:"xl6Future" /C:"xl6TaskGroup" /C:"xl6Channel" build\test\stdlib_os.c >nul
if not errorlevel 1 (
    echo stdlib_os.c pulled async runtime into a synchronous Process program.
    exit /b 1
)
findstr /C:"xl6ProcessRunV1" build\test\stdlib_os.c >nul
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
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_os.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_os.exe
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
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\stdlib_process.c >nul
if not errorlevel 1 (
    echo stdlib_process.c unexpectedly imports the legacy private runtime header.
    exit /b 1
)
findstr /C:"= xl6_xrt_process_pipeline_run(" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing resolved process pipeline call.
    exit /b 1
)
findstr /C:"xrtProcessPipelineRun" build\test\stdlib_process.c >nul
if not errorlevel 1 (
    echo stdlib_process.c still contains removed xrtProcessPipelineRun API.
    exit /b 1
)
findstr /C:"xl6ProcessRunV1" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exec runtime helper.
    exit /b 1
)
findstr /C:"xl6ProcessResultExitCodeV1" build\test\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exitCode runtime helper.
    exit /b 1
)
findstr /C:"xl6_xrt_process_read_events" build\test\stdlib_process.c >nul
if not errorlevel 1 (
    echo stdlib_process.c contains an unreachable process-event bridge.
    exit /b 1
)
findstr /C:"xrtProcessReadEventsSince" build\test\stdlib_process.c >nul
if not errorlevel 1 (
    echo stdlib_process.c still contains removed xrtProcessReadEventsSince API.
    exit /b 1
)
findstr /C:"xrtProcessWaitAsync" build\test\stdlib_process.c >nul
if not errorlevel 1 (
    echo stdlib_process.c contains unused process Future runtime.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/stdlib_process.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\stdlib_process.exe
if errorlevel 1 exit /b 1
build\test\stdlib_process.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_process_extended.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_process_extended.xl --emit build\test\stdlib_process_extended.c
if errorlevel 1 exit /b 1
findstr /C:"= xl6_xrt_process_wait_future(" build\test\stdlib_process_extended.c >nul
if errorlevel 1 (
    echo stdlib_process_extended.c missing resolved process Future call.
    exit /b 1
)
findstr /C:"= xl6_xrt_process_read_events(" build\test\stdlib_process_extended.c >nul
if errorlevel 1 (
    echo stdlib_process_extended.c missing reachable incremental process-event call.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_process_extended.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\stdlib_process_shell_quotes.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_process_shell_quotes.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_process.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_process_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_process_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_process_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_process_c.exe > build\test\stdlib_process_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_process_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_process_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib process xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_engine_trim.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_engine_trim.xl --emit build\test\stdlib_net_engine_trim.c
if errorlevel 1 exit /b 1
findstr /C:"xl6NetEngineCreateWorkersV1(" build\test\stdlib_net_engine_trim.c >nul
if errorlevel 1 (
    echo stdlib_net_engine_trim.c missing resolved network-engine factory ABI.
    exit /b 1
)
for %%S in (xl6_net_resolver XRT_MODULE_NET_RESOLVER XRT_MODULE_NET_TCP_ XRT_MODULE_NET_UDP) do (
    findstr /C:"%%S" build\test\stdlib_net_engine_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_net_engine_trim.c unexpectedly keeps %%S.
        exit /b 1
    )
)
release\xl.exe run test\cases\stdlib_net_engine_trim.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_net_engine_trim.xl -o build\test\stdlib_net_engine_trim.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_engine_trim.exe
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

release\xl.exe check test\cases\stdlib_net_send_file.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_send_file.xl --emit build\test\stdlib_net_send_file.c
if errorlevel 1 exit /b 1
findstr /C:"xl6TcpStreamSendFileV1(" build\test\stdlib_net_send_file.c >nul
if errorlevel 1 (
    echo stdlib_net_send_file.c missing resident TCP file-transfer call.
    exit /b 1
)
findstr /C:"xl6_xrt_tcp_stream_send_file" build\test\stdlib_net_send_file.c >nul
if not errorlevel 1 (
    echo stdlib_net_send_file.c unexpectedly contains retired file-send glue.
    exit /b 1
)
release\xl.exe cgen test\cases\stdlib_tcp_managed_sync.xl --emit build\test\stdlib_net_extended_io_trim.c
if errorlevel 1 exit /b 1
findstr /C:"XRT_MODULE_NET_TCP_FILE" build\test\stdlib_net_extended_io_trim.c >nul
if not errorlevel 1 (
    echo ordinary TCP source unexpectedly keeps TCP file module.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_net_send_file.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_net_send_file.xl -o build\test\stdlib_net_send_file.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_send_file.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_interfaces.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_interfaces.xl --emit build\test\stdlib_net_interfaces.c
if errorlevel 1 exit /b 1
findstr /C:"xl6NetInterfacesCreateV1(" build\test\stdlib_net_interfaces.c >nul
if errorlevel 1 (
    echo stdlib_net_interfaces.c missing resolved network-interface factory ABI.
    exit /b 1
)
findstr /C:"XRT_MODULE_NET_INTERFACE_TEXT" build\test\stdlib_net_interfaces.c >nul
if not errorlevel 1 (
    echo raw interface enumeration unexpectedly keeps interface text helpers.
    exit /b 1
)
findstr /C:"xl6NetEngineCreate" build\test\stdlib_net_interfaces.c >nul
if not errorlevel 1 (
    echo raw interface enumeration unexpectedly creates a network engine.
    exit /b 1
)
findstr /C:"xl6NetResolverCreate" build\test\stdlib_net_interfaces.c >nul
if not errorlevel 1 (
    echo raw interface enumeration unexpectedly creates a DNS resolver.
    exit /b 1
)
release\xl.exe run test\cases\stdlib_net_interfaces.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_net_interfaces.xl -o build\test\stdlib_net_interfaces.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_interfaces.exe
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_net_interfaces.xl > build\test\stdlib_net_interfaces_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% release\examples\stdlib_net_interfaces_c.c %XL6_CGEN_LINKS% -o build\test\stdlib_net_interfaces_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_net_interfaces_c.exe > build\test\stdlib_net_interfaces_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_net_interfaces_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_net_interfaces_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib net interfaces xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_tls_stream_fast_path.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_tls_stream_fast_path.xl --emit build\test\stdlib_tls_stream_fast_path.c
if errorlevel 1 exit /b 1
findstr /C:"xl6TlsStreamSendBytesAsyncV1" build\test\stdlib_tls_stream_fast_path.c >nul
if errorlevel 1 (
    echo stdlib_tls_stream_fast_path.c missing managed TLS stream binary async declaration.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_tls_stream_fast_path.xl -o build\test\stdlib_tls_stream_fast_path.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_stream_fast_path.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_tls_server_fast_path.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_tls_server_fast_path.xl --emit build\test\stdlib_tls_server_fast_path.c
if errorlevel 1 exit /b 1
findstr /C:"xl6TlsListenerAcceptAsyncV1" build\test\stdlib_tls_server_fast_path.c >nul
if errorlevel 1 (
    echo stdlib_tls_server_fast_path.c missing managed TLS listener async declaration.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_tls_server_fast_path.xl -o build\test\stdlib_tls_server_fast_path.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_server_fast_path.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_tls_server_loopback.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_tls_server_loopback.xl -o build\test\stdlib_tls_server_loopback.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_server_loopback.exe
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
rem The DLL boundary gate checks both the exact registration declaration and
rem call in the shared component, as well as the centralized symbol table.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_dll_api_boundary.ps1
if errorlevel 1 exit /b 1
findstr /C:"XL6_TCC_ADD_SYMBOL" build\test\stdlib_tcc_embed.c >nul
if not errorlevel 1 (
    echo stdlib_tcc_embed.c still contains the removed private XRT symbol registry.
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

release\xl.exe run test\cases\stdlib_tcc_runtime_contract.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_tcc_runtime_contract.xl -o build\test\stdlib_tcc_runtime_contract.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tcc_runtime_contract.exe
if errorlevel 1 exit /b 1

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
release\xl.exe cgen test\cases\stdlib_special_collections.xl --emit build\test\stdlib_special_collections.c
if errorlevel 1 exit /b 1
findstr /C:"= xlir_special_queue_push_batch(" build\test\stdlib_special_collections.c >nul
if errorlevel 1 exit /b 1
findstr /C:"= xlir_special_queue_pop_batch_into(" build\test\stdlib_special_collections.c >nul
if errorlevel 1 exit /b 1
findstr /C:"XLIR_CHANNEL_PAYLOAD_STRING_COPY, sizeof(char*), xrtTypeString()" build\test\stdlib_special_collections.c >nul
if errorlevel 1 exit /b 1
findstr /C:"XLIR_CHANNEL_PAYLOAD_TRIVIAL_COPY, sizeof(int64_t), xrtTypeInt64()" build\test\stdlib_special_collections.c >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_special_collections.xl -o build\test\stdlib_special_collections.exe
if errorlevel 1 exit /b 1
build\test\stdlib_special_collections.exe > build\test\stdlib_special_collections_static.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib special collections ok" build\test\stdlib_special_collections_static.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\stdlib_special_collections_lifecycle.xl --emit build\test\stdlib_special_collections_lifecycle.c
if errorlevel 1 exit /b 1
findstr /C:"static void xlir_stack_handle_drop(void* opaque, void* user)" build\test\stdlib_special_collections_lifecycle.c >nul
if errorlevel 1 (
    echo stdlib_special_collections_lifecycle.c missing managed stack payload lifetime ABI.
    exit /b 1
)
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

release\xl.exe check test\cases\sem_special_collection_contract.xl >nul 2> build\test\sem_special_collection_contract.err.txt
if not errorlevel 1 (
    echo sem_special_collection_contract.xl should fail.
    exit /b 1
)
findstr /C:"[SC-ARG-COUNT]" build\test\sem_special_collection_contract.err.txt >nul
if errorlevel 1 (
    echo sem_special_collection_contract.xl missing argument-count diagnostic.
    exit /b 1
)
findstr /C:"[SC-WAIT-RESET]" build\test\sem_special_collection_contract.err.txt >nul
if errorlevel 1 (
    echo sem_special_collection_contract.xl missing WaitQueue reset diagnostic.
    exit /b 1
)
findstr /C:"[SC-WAIT-BATCH]" build\test\sem_special_collection_contract.err.txt >nul
if errorlevel 1 (
    echo sem_special_collection_contract.xl missing WaitQueue batch diagnostic.
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
findstr /C:"xl6TlsSessionClientCreateV1" build\test\stdlib_tls_session.c >nul
if errorlevel 1 (
    echo stdlib_tls_session.c missing managed TLS Session declaration.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_tls_session.xl -o build\test\stdlib_tls_session.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_session.exe > build\test\stdlib_tls_session_static.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session=ok" build\test\stdlib_tls_session_static.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_tls_session_trim.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_tls_session_trim.xl --emit build\test\stdlib_tls_session_trim.c
if errorlevel 1 exit /b 1
findstr /C:"xl6TlsSessionClientCreateV1" build\test\stdlib_tls_session_trim.c >nul
if errorlevel 1 (
    echo stdlib_tls_session_trim.c missing managed TLS Session declaration.
    exit /b 1
)
for %%S in (xl6_xrt_net_engine_create xl6_xrt_tcp_stream_create xl6_xrt_tcp_client_create xl6_xrt_tcp_server_create xl6_xrt_udp_create xl6_xrt_tls_stream_create xl6_xrt_tls_server_create) do (
    findstr /C:"%%S" build\test\stdlib_tls_session_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_tls_session_trim.c unexpectedly contains %%S.
        exit /b 1
    )
)
release\xl.exe build test\cases\stdlib_tls_session_trim.xl -o build\test\stdlib_tls_session_trim.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_session_trim.exe > build\test\stdlib_tls_session_trim.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls-session-trim-ok" build\test\stdlib_tls_session_trim.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_tls_session_pair.xl > build\test\stdlib_tls_session_pair.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session_pair=ok" build\test\stdlib_tls_session_pair.out.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_tls_session_pair.xl -o build\test\stdlib_tls_session_pair.exe
if errorlevel 1 exit /b 1
build\test\stdlib_tls_session_pair.exe > build\test\stdlib_tls_session_pair_static.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session_pair=ok" build\test\stdlib_tls_session_pair_static.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_tls_session_pair.xl > build\test\stdlib_tls_session_pair_example.out.txt
if errorlevel 1 exit /b 1
findstr /C:"tls_session_pair=ok" build\test\stdlib_tls_session_pair_example.out.txt >nul
if errorlevel 1 (
    echo managed TLS Session example did not complete.
    exit /b 1
)

release\xl.exe run test\cases\stdlib_xid_object.xl
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

release\xl.exe run test\cases\xrt_secure_random.c > build\test\xrt_secure_random.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xrt_secure_random=ok" build\test\xrt_secure_random.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_http1_core.xl > build\test\stdlib_http1_core.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib http1 core ok" build\test\stdlib_http1_core.out.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_http1_core.xl --emit build\test\stdlib_http1_core.c
if errorlevel 1 exit /b 1
for %%S in (XRT_MODULE_HTTP1_BODY XRT_MODULE_HTTP_DECODE xl6_xrt_http1_request_head xl6_xrt_http_decoder_create) do (
    findstr /C:"%%S" build\test\stdlib_http1_core.c >nul
    if errorlevel 1 (
        echo stdlib_http1_core.c missing %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_HTTP_CLIENT XRT_MODULE_HTTP_SERVER XRT_MODULE_HTTP_COOKIES XRT_MODULE_HTTP_FORM) do (
    findstr /C:"%%S" build\test\stdlib_http1_core.c >nul
    if not errorlevel 1 (
        echo stdlib_http1_core.c unexpectedly contains %%S.
        exit /b 1
    )
)
release\xl.exe build test\cases\stdlib_http1_core.xl -o build\test\stdlib_http1_core.exe
if errorlevel 1 exit /b 1
build\test\stdlib_http1_core.exe > build\test\stdlib_http1_core_static.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib http1 core ok" build\test\stdlib_http1_core_static.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_websocket_core.xl > build\test\stdlib_websocket_core.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib websocket core ok" build\test\stdlib_websocket_core.out.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_websocket_core.xl --emit build\test\stdlib_websocket_core.c
if errorlevel 1 exit /b 1
for %%S in (XRT_MODULE_WEBSOCKET_FRAME XRT_MODULE_WEBSOCKET_CLOSE XRT_MODULE_WEBSOCKET_KEYGEN XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE xl6_xrt_ws_frame_parse xl6_xrt_ws_upgrade_server) do (
    findstr /C:"%%S" build\test\stdlib_websocket_core.c >nul
    if errorlevel 1 (
        echo stdlib_websocket_core.c missing %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_HTTP_CLIENT XRT_MODULE_HTTP_SERVER XRT_MODULE_WEBSOCKET_STREAM XRT_MODULE_WEBSOCKET_STREAM_TLS XRT_MODULE_TLS_STREAM XRT_MODULE_NET_ENGINE) do (
    findstr /C:"%%S" build\test\stdlib_websocket_core.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_core.c unexpectedly contains %%S.
        exit /b 1
    )
)
release\xl.exe build test\cases\stdlib_websocket_core.xl -o build\test\stdlib_websocket_core.exe
if errorlevel 1 exit /b 1
build\test\stdlib_websocket_core.exe > build\test\stdlib_websocket_core_static.out.txt
if errorlevel 1 exit /b 1
findstr /C:"stdlib websocket core ok" build\test\stdlib_websocket_core_static.out.txt >nul
if errorlevel 1 exit /b 1

for %%T in (
    frame
    handshake
    upgrade
) do (
    release\xl.exe cgen test\cases\stdlib_websocket_%%T_trim.xl --emit build\test\stdlib_websocket_%%T_trim.c
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\stdlib_websocket_%%T_trim.xl -o build\test\stdlib_websocket_%%T_trim.exe
    if errorlevel 1 exit /b 1
    build\test\stdlib_websocket_%%T_trim.exe > build\test\stdlib_websocket_%%T_trim.out.txt
    if errorlevel 1 exit /b 1
)
for %%S in (XRT_MODULE_WEBSOCKET_KEYGEN XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE XRT_MODULE_WEBSOCKET_STREAM XRT_MODULE_NET_ENGINE XRT_MODULE_TLS_STREAM) do (
    findstr /C:"%%S" build\test\stdlib_websocket_frame_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_frame_trim.c unexpectedly contains %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_WEBSOCKET_FRAME XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE XRT_MODULE_WEBSOCKET_STREAM XRT_MODULE_NET_ENGINE XRT_MODULE_TLS_STREAM) do (
    findstr /C:"%%S" build\test\stdlib_websocket_handshake_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_handshake_trim.c unexpectedly contains %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_WEBSOCKET_FRAME XRT_MODULE_WEBSOCKET_STREAM XRT_MODULE_NET_ENGINE XRT_MODULE_TLS_STREAM) do (
    findstr /C:"%%S" build\test\stdlib_websocket_upgrade_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_upgrade_trim.c unexpectedly contains %%S.
        exit /b 1
    )
)

for %%T in (
    stream_config
    stream
    stream_tls
) do (
    release\xl.exe cgen test\cases\stdlib_websocket_%%T_trim.xl --emit build\test\stdlib_websocket_%%T_trim.c
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\stdlib_websocket_%%T_trim.xl -o build\test\stdlib_websocket_%%T_trim.exe
    if errorlevel 1 exit /b 1
    build\test\stdlib_websocket_%%T_trim.exe > build\test\stdlib_websocket_%%T_trim.out.txt
    if errorlevel 1 exit /b 1
)
for %%S in (XRT_MODULE_NET_ XRT_MODULE_WEBSOCKET_ XRT_MODULE_TLS_ XRT_MODULE_EVENT) do (
    findstr /C:"%%S" build\test\stdlib_websocket_stream_config_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_stream_config_trim.c unexpectedly contains %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_HTTP1_BODY XRT_MODULE_WEBSOCKET_KEYGEN XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE XRT_MODULE_WEBSOCKET_STREAM_REF XRT_MODULE_WEBSOCKET_STREAM_DEFLATE XRT_MODULE_EVENT xl6_xrt_tcp_stream_peek_buffer xl6_xrt_tcp_stream_consume_exact xl6_xrt_ws_stream_attach_tcp xl6_xrt_ws_stream_send_text) do (
    findstr /C:"%%S" build\test\stdlib_websocket_stream_trim.c >nul
    if errorlevel 1 (
        echo stdlib_websocket_stream_trim.c missing %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_WEBSOCKET_STREAM_TLS XRT_MODULE_HTTP_CLIENT XRT_MODULE_HTTP_SERVER xl6_xrt_ws_stream_attach_tls) do (
    findstr /C:"%%S" build\test\stdlib_websocket_stream_trim.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_stream_trim.c unexpectedly contains %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_WEBSOCKET_STREAM_TLS xl6_xrt_tls_stream_peek_buffer xl6_xrt_tls_stream_consume_exact xl6_xrt_ws_stream_attach_tls xrtWsStreamAttachTls) do (
    findstr /C:"%%S" build\test\stdlib_websocket_stream_tls_trim.c >nul
    if errorlevel 1 (
        echo stdlib_websocket_stream_tls_trim.c missing %%S.
        exit /b 1
    )
)

for %%T in (
    upgrade_loopback
    upgrade_tls_loopback
) do (
    release\xl.exe check test\cases\stdlib_websocket_%%T.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\stdlib_websocket_%%T.xl > build\test\stdlib_websocket_%%T.out.txt
    if errorlevel 1 exit /b 1
    findstr /C:"loopback-ok" build\test\stdlib_websocket_%%T.out.txt >nul
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\stdlib_websocket_%%T.xl --emit build\test\stdlib_websocket_%%T.c
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\stdlib_websocket_%%T.xl -o build\test\stdlib_websocket_%%T.exe
    if errorlevel 1 exit /b 1
    build\test\stdlib_websocket_%%T.exe > build\test\stdlib_websocket_%%T.static.out.txt
    if errorlevel 1 exit /b 1
    findstr /C:"loopback-ok" build\test\stdlib_websocket_%%T.static.out.txt >nul
    if errorlevel 1 exit /b 1
)
for %%S in (XRT_MODULE_HTTP1_BODY XRT_MODULE_WEBSOCKET_STREAM_REF XRT_MODULE_WEBSOCKET_STREAM_DEFLATE XRT_MODULE_EVENT xl6_xrt_tcp_stream_peek_buffer xl6_xrt_ws_stream_attach_tcp) do (
    findstr /C:"%%S" build\test\stdlib_websocket_upgrade_loopback.c >nul
    if errorlevel 1 (
        echo stdlib_websocket_upgrade_loopback.c missing %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_WEBSOCKET_STREAM_TLS XRT_MODULE_TLS_STREAM_FUTURE xl6_xrt_ws_stream_attach_tls) do (
    findstr /C:"%%S" build\test\stdlib_websocket_upgrade_loopback.c >nul
    if not errorlevel 1 (
        echo stdlib_websocket_upgrade_loopback.c unexpectedly contains %%S.
        exit /b 1
    )
)
for %%S in (XRT_MODULE_WEBSOCKET_STREAM_TLS XRT_MODULE_TLS_STREAM_FUTURE xl6_xrt_tls_stream_peek_buffer xl6_xrt_ws_stream_attach_tls) do (
    findstr /C:"%%S" build\test\stdlib_websocket_upgrade_tls_loopback.c >nul
    if errorlevel 1 (
        echo stdlib_websocket_upgrade_tls_loopback.c missing %%S.
        exit /b 1
    )
)

rem Fresh immutable compiler inputs shared by the three time acceptance gates.
set "XL6_TIME_CONTRACT_DIR=build\test\stdlib_time_%RANDOM%_%RANDOM%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build_xlir_public_source_driver.ps1 -Directory "%XL6_TIME_CONTRACT_DIR%\compiler" -Optimization O2
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_time_core.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\core"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_time_calendar.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\calendar"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_time_text.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\text"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_environment.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\environment"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_signal_core.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\signal_core"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_signal_queue.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\signal_queue"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_process_run.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\process_run"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_process_future_module.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\process_future_module"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_process_future_module.ps1 -PublicSourceDirectory "%XL6_TIME_CONTRACT_DIR%\compiler" -Directory "%XL6_TIME_CONTRACT_DIR%\process_future_module_globals" -GlobalNativeHandles
if errorlevel 1 exit /b 1
python tools\check_tls_retirement_source.py --root . --build "%XL6_TIME_CONTRACT_DIR%\compiler\build.json" --out "%XL6_TIME_CONTRACT_DIR%\tls_retirement"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: extended standard library
exit /b 0

