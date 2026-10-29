@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1

echo Running DEMO6 suite: featured extension libraries RC...
python -B tools\test_tcc_vfs_output_directory.py
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_file_runtime.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_aho.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_blake2.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_blake3.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_xxhash.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_crc32.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_md5.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_murmur3.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_sha3.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_siphash128.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_brotli.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_bz2.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_lz4.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_argon2_bloom.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_featured_extlibs_rc.ps1
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: featured extension libraries RC
exit /b 0
