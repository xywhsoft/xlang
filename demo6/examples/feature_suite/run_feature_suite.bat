@echo off
setlocal
cd /d %~dp0\..\..

set LINKS=--link examples\feature_suite\host.c --link ..\..\xrt\release\x64\xrt.o --link ..\demo5\release\tcc\lib\libtcc1-64.a --link Ws2_32 --link IPHLPAPI --link Shell32

echo Checking feature suite...
for %%f in (examples\feature_suite\*.xl) do (
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
    release\xl.exe run "examples\feature_suite\%%f" %LINKS%
    if errorlevel 1 exit /b 1
)

echo Verifying C export ABI...
if not exist release\tmp\feature_exports mkdir release\tmp\feature_exports

release\xl.exe cgen examples\feature_suite\09_c_export_abi.xl --emit release\tmp\feature_exports\09_c_export_abi.c --emit-header release\tmp\feature_exports\09_c_export_abi.h
if errorlevel 1 exit /b 1

..\demo5\lib\tcc\tcc.exe -Irelease\tmp\feature_exports -I..\demo5\lib\tcc\include -I..\demo5\release\tcc\include_win -L..\demo5\release\tcc\lib release\tmp\feature_exports\09_c_export_abi.c examples\feature_suite\host_exports.c ..\..\xrt\release\x64\xrt.o ..\demo5\release\tcc\lib\libtcc1-64.a -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\feature_exports\host_exports.exe
if errorlevel 1 exit /b 1

release\tmp\feature_exports\host_exports.exe
if errorlevel 1 exit /b 1

echo Feature suite ok.
exit /b 0
