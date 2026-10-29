param([string]$Root = "", [string]$Manifest = "", [string]$Output = "",
    [string]$TccDist = "", [string]$XrtObj = "", [string]$PackTool = "",
    [int]$PackLevel = 9, [switch]$NoCompress)
$ErrorActionPreference = "Stop"
if (!$Root) { $Root = (Get-Location).Path }
if (!$Manifest) { $Manifest = Join-Path $Root "tools/tcc_vfs_manifest_win64_xlc.txt" }
if (!$Output) { $Output = Join-Path $Root "build/tmp/tcc_builtin_resources.c" }
if (!$TccDist) { $TccDist = Join-Path $Root "res/tcc/win64/x86_64" }
if (!$XrtObj) { $XrtObj = Join-Path $Root "build/tmp/xrt.o" }
if (!$PackTool) { $PackTool = Join-Path $Root "build/tmp/tcc_vfs_lzma_pack.exe" }
$arguments = @((Join-Path $PSScriptRoot "gen_tcc_vfs_resources.py"), "--root", $Root,
    "--manifest", $Manifest, "--output", $Output, "--tcc-dist", $TccDist,
    "--xrt-obj", $XrtObj, "--pack-tool", $PackTool, "--pack-level", $PackLevel)
if ($NoCompress) { $arguments += "--no-compress" }
& python @arguments
exit $LASTEXITCODE
