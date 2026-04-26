@echo off
REM xlang 测试脚本 - 批量运行所有示例
cd /d %~dp0

echo =====================================
echo xlang 测试套件
echo =====================================
echo.

set PASS=0
set FAIL=0

REM 测试基础语法
echo [测试基础语法]
for %%f in (examples\basics\0*.xl) do (
	echo 运行: %%f
	xlang.exe "%%f" > nul 2>&1
	if errorlevel 1 (
		echo   [失败]
		set /a FAIL+=1
	) else (
		echo   [通过]
		set /a PASS+=1
	)
)

echo.
echo =====================================
echo 测试结果: %PASS% 通过, %FAIL% 失败
echo =====================================

pause
