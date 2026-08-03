@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: bundled format extension libraries...

if not exist tmp mkdir tmp

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_tests.ps1
if errorlevel 1 exit /b 1

where python >nul 2>nul
if errorlevel 1 (
    echo Python 3 is required for the format interoperability tests.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_format_extlibs.ps1
if errorlevel 1 exit /b 1

for %%F in (
    extlib_gzip
    extlib_bz2
    extlib_zip
    extlib_tar
    extlib_tar_compressed
    extlib_csv
    extlib_ini
    extlib_toml
    extlib_netrc
    extlib_plist
    extlib_xml_sax
    extlib_xml_etree
    extlib_xml_dom
) do (
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package gzip -Source release\extlibs\gzip\test\test_gzip.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package bz2 -Source release\extlibs\bz2\test\test_bz2.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package zip -Source release\extlibs\zip\test\test_zip.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package tar -Source release\extlibs\tar\test\test_tar.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package csv -Source release\extlibs\csv\test\test_csv.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package ini -Source release\extlibs\ini\test\test_ini.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package toml -Source release\extlibs\toml\test\test_toml.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package netrc -Source release\extlibs\netrc\test\test_netrc.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package plist -Source release\extlibs\plist\test\test_plist.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package xml.sax -Source release\extlibs\xml.sax\test\test_xml_sax.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package xml.etree -Source release\extlibs\xml.etree\test\test_xml_etree.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package xml.dom -Source release\extlibs\xml.dom\test\test_xml_dom.c
if errorlevel 1 exit /b 1

for %%F in (
    gzip
    bz2
    zip
    tar
    csv
    ini
    toml
    netrc
    plist
    xml_sax
    xml_etree
    xml_dom
) do (
    release\xl.exe run release\examples\extlibs\%%F.xl
    if errorlevel 1 exit /b 1
)
release\xl.exe run release\examples\extlibs\tar_compressed.xl
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package gzip -Source release\examples\extlibs\gzip_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package bz2 -Source release\examples\extlibs\bz2_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package zip -Source release\examples\extlibs\zip_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package tar -Source release\examples\extlibs\tar_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package gzip,tar -Source release\examples\extlibs\tar_compressed_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package csv -Source release\examples\extlibs\csv_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package ini -Source release\examples\extlibs\ini_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package toml -Source release\examples\extlibs\toml_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package netrc -Source release\examples\extlibs\netrc_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package plist -Source release\examples\extlibs\plist_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package xml.sax -Source release\examples\extlibs\xml_sax_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package xml.etree -Source release\examples\extlibs\xml_etree_c.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_extlib_c.ps1 -Package xml.dom -Source release\examples\extlibs\xml_dom_c.c
if errorlevel 1 exit /b 1

for %%F in (compression csv ini toml netrc tar plist xml) do (
    release\xl.exe run test\interop\%%F_generate.xl
    if errorlevel 1 exit /b 1
    python test\interop\%%F_python.py
    if errorlevel 1 exit /b 1
    release\xl.exe run test\interop\%%F_verify.xl
    if errorlevel 1 exit /b 1
)

release\xl.exe build release\examples\hello.xl -o build\test\format_trim_hello.exe
if errorlevel 1 exit /b 1
release\xl.exe build release\examples\extlibs\gzip.xl -o build\test\format_trim_gzip.exe
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$plain=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes('build\test\format_trim_hello.exe')); $gzip=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes('build\test\format_trim_gzip.exe')); if($plain.Contains('__xl6_import_gzip_')){ Write-Host 'hello executable unexpectedly contains gzip package code'; exit 1 }; if(-not $gzip.Contains('__xl6_import_gzip_')){ Write-Host 'gzip executable is missing imported gzip package code'; exit 1 }; if((Get-Item 'build\test\format_trim_gzip.exe').Length -le (Get-Item 'build\test\format_trim_hello.exe').Length){ Write-Host 'format package trimming size guard is inconclusive'; exit 1 }"
if errorlevel 1 exit /b 1

del /q tmp\python_interop.* tmp\xlang_interop.* 2>nul
del /q tmp\python_gnu.tar tmp\python_pax.tar tmp\python_ustar.tar 2>nul

echo DEMO6 suite ok: bundled format extension libraries
exit /b 0
