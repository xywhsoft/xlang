# xlang 编译脚本 - Windows PowerShell

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

# 创建输出目录
if (!(Test-Path release)) {
	New-Item -ItemType Directory -Path release | Out-Null
}

# 编译 xlang
Write-Host "Compiling xlang..."
$srcFiles = @(
	"main.c",
	"lib\xrt\xrt.c",
	"lib\tcc\libtcc.c"
)
$includes = @("-Ilib\tcc")
$libs = @("-lshlwapi", "-lgdi32", "-lws2_32", "-lIPHLPAPI")
$opts = @("-O2", "-s", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections")

$gccArgs = $srcFiles + $includes + $libs + $opts + @("-o", "release\xlang.exe")
& gcc $gccArgs 2>&1

if (Test-Path "release\xlang.exe") {
	Write-Host "Build succeeded: release\xlang.exe" -ForegroundColor Green
	$size = (Get-Item "release\xlang.exe").Length / 1KB
	Write-Host "File size: $([math]::Round($size, 2)) KB"
} else {
	Write-Host "Build failed!" -ForegroundColor Red
}
