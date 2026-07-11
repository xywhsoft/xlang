@echo off
setlocal
cd /d %~dp0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist build\test\tutorial mkdir build\test\tutorial
if not exist build\test\__xl_runtime mkdir build\test\__xl_runtime
copy /Y release\include\xl_runtime.h build\test\__xl_runtime\xl_runtime.h >nul
if errorlevel 1 exit /b 1

rem Generated C uses the private runtime mirror and the Windows TLS headers.
set "XL6_TCC_INCLUDES=-Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"

echo Checking tutorial examples...
for %%f in (release\examples\tutorial\*.xl) do (
    echo CHECK %%~nxf
    release\xl.exe check "%%f"
    if errorlevel 1 exit /b 1
)

echo Running pure tutorial examples...
for %%f in (
    01_entry_and_layout.xl
    02_variables_and_literals.xl
    03_operators.xl
    04_control_flow.xl
    05_functions.xl
    06_function_values_and_closures.xl
    07_containers.xl
    08_generics.xl
    12_errors_and_docs.xl
) do (
    echo RUN %%f
    release\xl.exe run "release\examples\tutorial\%%f"
    if errorlevel 1 exit /b 1
)

echo Running tutorial examples with C host...
for %%f in (
    09_class_lifecycle.xl
    10_c_interop_and_native_class.xl
) do (
    echo RUN %%f
    release\xl.exe run "release\examples\tutorial\%%f" --link release\examples\tutorial\host.c
    if errorlevel 1 exit /b 1
)

echo Verifying tutorial C export ABI...
release\xl.exe cgen release\examples\tutorial\11_export_to_c.xl --emit build\test\tutorial\11_export_to_c.c --emit-header build\test\tutorial\11_export_to_c.h
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$path='build\test\tutorial\host_export.exe'; for($i=0; $i -lt 20 -and (Test-Path -LiteralPath $path -PathType Leaf); $i++){ Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue; if(Test-Path -LiteralPath $path -PathType Leaf){ Start-Sleep -Milliseconds 250 } }; if(Test-Path -LiteralPath $path -PathType Leaf){ Write-Host 'tutorial host_export.exe is still present before build.'; exit 1 }"
if errorlevel 1 exit /b 1

"%XL6_TCC%" -DXL6_EXAMPLE_LINK_EXPORTS -Ibuild\test\tutorial %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\tutorial\11_export_to_c.c release\examples\tutorial\host_export.c %XL6_DEFAULT_LINKS% -o build\test\tutorial\host_export.exe
if errorlevel 1 exit /b 1

build\test\tutorial\host_export.exe
if errorlevel 1 exit /b 1

echo Tutorial examples ok.
exit /b 0
