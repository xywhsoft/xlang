@echo off
setlocal
cd /d %~dp0\..\..\..

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

set LINKS=--link release\examples\feature_suite\host.c %XL6_XL_DEFAULT_LINK_ARGS%

echo Checking feature suite...
for %%f in (release\examples\feature_suite\*.xl) do (
    echo CHECK %%~nxf
    release\xl.exe check "%%f"
    if errorlevel 1 exit /b 1
)

echo Running executable examples...
for %%f in (
    00_language_basics.xl
    01_expressions_control_flow.xl
    02_functions.xl
    03_types_and_values.xl
    04_containers.xl
    05_generics.xl
    06_function_values_closures.xl
    07_class_custom.xl
    08_c_interop_exports.xl
    10_errors_docs.xl
    11_concurrency.xl
    12_union.xl
) do (
    echo RUN %%f
    release\xl.exe run "release\examples\feature_suite\%%f" %LINKS%
    if errorlevel 1 exit /b 1
)

echo Re-running concurrency feature example...
for /L %%I in (1,1,2) do (
    release\xl.exe run "release\examples\feature_suite\11_concurrency.xl" %LINKS%
    if errorlevel 1 (
        echo 11_concurrency.xl repeat run %%I failed.
        exit /b 1
    )
)

echo Verifying C export ABI...
if not exist release\tmp\feature_exports mkdir release\tmp\feature_exports

release\xl.exe cgen release\examples\feature_suite\09_c_export_abi.xl --emit release\tmp\feature_exports\09_c_export_abi.c --emit-header release\tmp\feature_exports\09_c_export_abi.h
if errorlevel 1 exit /b 1

"%XL6_TCC%" -Irelease\tmp\feature_exports %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\feature_exports\09_c_export_abi.c release\examples\feature_suite\host_exports.c %XL6_DEFAULT_LINKS% -o release\tmp\feature_exports\host_exports.exe
if errorlevel 1 exit /b 1

release\tmp\feature_exports\host_exports.exe
if errorlevel 1 exit /b 1

echo Feature suite ok.
exit /b 0
