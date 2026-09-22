# sync_xge.ps1 - 从 XGE 仓库同步 C 源码到 _xge_native 包
# 用法: .\sync_xge.ps1 [-XgeRoot "..\xge"]

param(
    [string]$XgeRoot = ""
)

$ErrorActionPreference = "Stop"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$PkgDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path "$PkgDir\xlang.package.json")) {
    # 如果从 tools/ 运行，PkgDir 应该是 _xge_native 根目录
    $PkgDir = Split-Path -Parent $PSScriptRoot
}
if ( $XgeRoot -eq "" ) {
    $XgeRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $PkgDir "..\..\..\..\..\xge"))
}

Write-Host "[sync_xge] XGE source: $XgeRoot"
Write-Host "[sync_xge] Package dir: $PkgDir"

if (-not (Test-Path "$XgeRoot\xge.h")) {
    Write-Host "[ERROR] xge.h not found in $XgeRoot" -ForegroundColor Red
    exit 1
}

# ---- 统计变更 ----
$addedFiles = @()
$updatedFiles = @()

function Copy-SyncFile($src, $dst) {
    $dstDir = Split-Path -Parent $dst
    if (-not (Test-Path $dstDir)) {
        New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
    }
    if (Test-Path $dst) {
        $srcHash = (Get-FileHash $src -Algorithm MD5).Hash
        $dstHash = (Get-FileHash $dst -Algorithm MD5).Hash
        if ($srcHash -ne $dstHash) {
            Copy-Item -Path $src -Destination $dst -Force
            $script:updatedFiles += $dst
        }
    } else {
        Copy-Item -Path $src -Destination $dst -Force
        $script:addedFiles += $dst
    }
}

function Read-Utf8Text($path) {
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Write-Utf8Text($path, $content) {
    [System.IO.File]::WriteAllText($path, $content, $Utf8NoBom)
}

# ---- 1. 同步 src/*.c、src/*.h 和 src/*.inc ----
Write-Host "[sync_xge] Syncing src/ ..."
$srcFiles = Get-ChildItem -Path "$XgeRoot\src" -File | Where-Object { $_.Extension -in '.c','.h','.inc' }
foreach ($f in $srcFiles) {
    Copy-SyncFile $f.FullName "$PkgDir\src\$($f.Name)"
}
Write-Host "  src/ files: $($srcFiles.Count)"

# ---- 2. 同步公开头文件 xge.h, xui.h ----
Write-Host "[sync_xge] Syncing headers ..."
Copy-SyncFile "$XgeRoot\xge.h" "$PkgDir\xge.h"
Copy-SyncFile "$XgeRoot\xui.h" "$PkgDir\xui.h"

# ---- 3. 同步第三方库 (排除 xrt) ----
# xlayout: XUI 布局引擎，被 xui_layout.c / xui_dock_panel.c / xui_rich_edit.c 引用
# zstd: 内建 emoji / 资源压缩解压，xge_impl.c 内联编译 lib/zstd/zstddeclib.c
Write-Host "[sync_xge] Syncing lib/ (excluding xrt) ..."
$libDirs = @("sokol", "stb", "miniaudio", "xlayout", "zstd")
foreach ($libName in $libDirs) {
    $libSrc = "$XgeRoot\lib\$libName"
    if (Test-Path $libSrc) {
        # 排除 test 子目录和二进制产物（.exe/.o/.obj/.lib/.pdb）
        $libFiles = Get-ChildItem -Path $libSrc -File -Recurse | Where-Object {
            $rel = $_.FullName.Substring($libSrc.Length).TrimStart('\','/')
            ($rel -notmatch '^(test|tests|examples)\\') -and
            ($_.Extension -notin '.exe','.o','.obj','.lib','.pdb','.exp')
        }
        foreach ($f in $libFiles) {
            $relPath = $f.FullName.Substring($libSrc.Length).TrimStart('\','/')
            Copy-SyncFile $f.FullName "$PkgDir\lib\$libName\$relPath"
        }
        Write-Host "  lib/${libName}: $($libFiles.Count) files"
    } else {
        Write-Host "  lib/${libName}: NOT FOUND (skipped)" -ForegroundColor Yellow
    }
}

# ---- 4. 自动 patch ----
Write-Host "[sync_xge] Applying patches ..."
$patchCount = 0

# 4a. XRT include patch: #include "lib/xrt/xrt.h" -> #include "xrt.h"
$patchTargets = @("$PkgDir\xge.h", "$PkgDir\xui.h", "$PkgDir\src\xge_impl.c")
foreach ($target in $patchTargets) {
    if (Test-Path $target) {
        $content = Read-Utf8Text $target
        if ($content -match '#include\s+"lib/xrt/xrt\.h"') {
            $content = $content -replace '#include\s+"lib/xrt/xrt\.h"', '#include "xrt.h"'
            Write-Utf8Text $target $content
            $patchCount++
            Write-Host "  [xrt] Patched: $target"
        }
    }
}

# 4b. xge_impl.c 相对路径 patch（使其从 src/ 目录解析）
$implFile = "$PkgDir\src\xge_impl.c"
if (Test-Path $implFile) {
    $content = Read-Utf8Text $implFile
    $changed = $false
    # "lib/xxx" -> "../lib/xxx"
    if ($content -match '#include\s+"lib/') {
        $content = $content -replace '#include\s+"lib/', '#include "../lib/'
        $changed = $true
    }
    # "src/xge_gl.h" -> "xge_gl.h" (same directory)
    if ($content -match '#include\s+"src/xge_gl\.h"') {
        $content = $content -replace '#include\s+"src/xge_gl\.h"', '#include "xge_gl.h"'
        $changed = $true
    }
    if ($changed) {
        Write-Utf8Text $implFile $content
        $patchCount++
        Write-Host "  [paths] Patched: $implFile"
    }
}
Write-Host "  Total patched: $patchCount files"

# ---- 5. 记录版本信息 ----
Write-Host "[sync_xge] Recording version ..."
$xgeCommit = "unknown"
try {
    $xgeCommit = (git -C $XgeRoot rev-parse --short HEAD 2>$null)
    if (-not $xgeCommit) { $xgeCommit = "unknown" }
} catch {}

# 从 xge.h 提取版本号
$versionMajor = 0; $versionMinor = 0; $versionPatch = 0
$xgeHeader = Read-Utf8Text "$PkgDir\xge.h"
if ($xgeHeader -match '#define XGE_VERSION_MAJOR\s+(\d+)') { $versionMajor = $Matches[1] }
if ($xgeHeader -match '#define XGE_VERSION_MINOR\s+(\d+)') { $versionMinor = $Matches[1] }
if ($xgeHeader -match '#define XGE_VERSION_PATCH\s+(\d+)') { $versionPatch = $Matches[1] }
$xgeVersion = "$versionMajor.$versionMinor.$versionPatch"

$versionInfo = @{
    xge_version = $xgeVersion
    xge_commit = $xgeCommit
    sync_date = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    source = "xge"
    src_files = $srcFiles.Count
} | ConvertTo-Json

Write-Utf8Text "$PkgDir\xge_version.json" $versionInfo
Write-Host "  XGE version: $xgeVersion (commit: $xgeCommit)"

# ---- 6. 输出变更摘要 ----
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "[sync_xge] Sync complete!" -ForegroundColor Green
Write-Host "  Added files:   $($addedFiles.Count)"
Write-Host "  Updated files: $($updatedFiles.Count)"
if ($addedFiles.Count -gt 0) {
    Write-Host "  New files:" -ForegroundColor Yellow
    foreach ($f in $addedFiles) { Write-Host "    + $f" }
}
Write-Host "========================================" -ForegroundColor Cyan
