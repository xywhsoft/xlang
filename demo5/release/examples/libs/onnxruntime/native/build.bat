@echo off
setlocal
cd /d "%~dp0"

set "XLLM_ROOT=D:\git\xllm"
set "ORT_INC=..\include"
set "ORT_LIB=..\bin"
set "OUT=..\bin\onnxruntime_xlang.dll"

if not exist "%ORT_INC%\onnxruntime_c_api.h" (
  echo [error] missing %ORT_INC%\onnxruntime_c_api.h
  exit /b 1
)
if not exist "%ORT_LIB%\onnxruntime.lib" (
  echo [error] missing %ORT_LIB%\onnxruntime.lib
  exit /b 1
)

gcc -shared -O2 -Wall -Wextra -I"%ORT_INC%" -o "%OUT%" onnxruntime_xlang.c "%ORT_LIB%\onnxruntime.lib"
if errorlevel 1 exit /b 1

copy /Y "%XLLM_ROOT%\lib\onnxruntime\onnxruntime.dll" "..\bin\onnxruntime.dll" >nul
if exist "%XLLM_ROOT%\build\onnxruntime_nupkg\runtimes\win-x64\native\onnxruntime_providers_shared.dll" (
  copy /Y "%XLLM_ROOT%\build\onnxruntime_nupkg\runtimes\win-x64\native\onnxruntime_providers_shared.dll" "..\bin\onnxruntime_providers_shared.dll" >nul
)

echo built %OUT%
exit /b 0
