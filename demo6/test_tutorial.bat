@echo off
setlocal
cd /d %~dp0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist release\tmp\tutorial mkdir release\tmp\tutorial

echo Checking tutorial examples...
for %%f in (examples\tutorial\*.xl) do (
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
    release\xl.exe run "examples\tutorial\%%f"
    if errorlevel 1 exit /b 1
)

echo Running tutorial examples with C host...
for %%f in (
    09_class_lifecycle.xl
    10_c_interop_and_custom.xl
) do (
    echo RUN %%f
    release\xl.exe run "examples\tutorial\%%f" --link examples\tutorial\host.c
    if errorlevel 1 exit /b 1
)

echo Verifying tutorial C export ABI...
release\xl.exe cgen examples\tutorial\11_export_to_c.xl --emit release\tmp\tutorial\11_export_to_c.c --emit-header release\tmp\tutorial\11_export_to_c.h
if errorlevel 1 exit /b 1

"%XL6_TCC%" -Irelease\tmp\tutorial %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\tutorial\11_export_to_c.c examples\tutorial\host_export.c %XL6_DEFAULT_LINKS% -o release\tmp\tutorial\host_export.exe
if errorlevel 1 exit /b 1

release\tmp\tutorial\host_export.exe
if errorlevel 1 exit /b 1

echo Tutorial examples ok.
exit /b 0
