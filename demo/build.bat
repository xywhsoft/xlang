@echo off
REM xlang 编译脚本 - Windows
cd /d %~dp0

REM 创建输出目录
if not exist release mkdir release

REM 编译 xlang (基础版本)
echo Compiling xlang...
gcc main.c lib\xrt\xrt.c lib\tcc\libtcc.c -Ilib\tcc -lshlwapi -lgdi32 -lws2_32 -lIPHLPAPI -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release\xlang.exe

if exist release\xlang.exe (
	echo Build succeeded: release\xlang.exe
) else (
	echo Build failed!
)
pause
