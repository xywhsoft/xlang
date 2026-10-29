param(
    [string]$Root = "",
    [string]$RuntimeDirectory = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$releaseDir = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$testDir = Join-Path ([System.IO.Path]::GetTempPath()) ("xlang_demo6_standalone_compiler_vfs_" + [Guid]::NewGuid().ToString('N'))
$xlExe = Join-Path $releaseDir "xl.exe"
$xlcDll = Join-Path $releaseDir "xlc.dll"
$source = Join-Path $Root "release\examples\hello.xl"
$completed = $false

foreach ( $path in @($xlExe, $xlcDll, $source) ) {
    if ( !(Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "standalone compiler VFS input missing: $path"
    }
}

if ( Test-Path -LiteralPath $testDir ) {
    throw "standalone compiler VFS work directory already exists: $testDir"
}
New-Item -ItemType Directory -Path $testDir | Out-Null

Copy-Item -LiteralPath $xlExe -Destination (Join-Path $testDir "xl.exe")
Copy-Item -LiteralPath $xlcDll -Destination (Join-Path $testDir "xlc.dll")
Copy-Item -LiteralPath $source -Destination (Join-Path $testDir "hello.xl")

$publicHeaderSource = @'
#include "xl.h"
#include "xlc.h"
#include <stdio.h>

int main(void)
{
    printf("public_vfs_headers=%u:%u\n", XL_API_VERSION, XLC_API_VERSION);
    return 0;
}
'@
[System.IO.File]::WriteAllText(
    (Join-Path $testDir "public_vfs_headers.c"),
    $publicHeaderSource,
    [System.Text.Encoding]::ASCII)

$embeddedXlangSource = @'
import xlang

func main(...) as int
    if not xlang.run("println(\"standalone nested xlang\")\n") then
        return 2
    end
    println("standalone_xlang_embed=ok")
    return 0
end
'@
[System.IO.File]::WriteAllText(
    (Join-Path $testDir "embedded_xlang.xl"),
    $embeddedXlangSource,
    [System.Text.UTF8Encoding]::new($false))

$embeddedCompilerSource = @'
import xlang.compiler

func main(...) as int
    println("standalone_xlang_compiler=ok")
    return 0
end
'@
[System.IO.File]::WriteAllText(
    (Join-Path $testDir "embedded_xlang_compiler.xl"),
    $embeddedCompilerSource,
    [System.Text.UTF8Encoding]::new($false))

$savedLocation = Get-Location
try {
    Set-Location -LiteralPath $testDir
    $jitOutput = & ".\xl.exe" run "hello.xl" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone xlang JIT failed: $([string]::Join([Environment]::NewLine, @($jitOutput)))"
    }
    $missingOutput = & ".\xl.exe" run "missing.xl" 2>&1
    if ( $LASTEXITCODE -eq 0 ) {
        throw "standalone missing input unexpectedly succeeded"
    }
    $buildOutput = & ".\xl.exe" build "hello.xl" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone xl build failed: $([string]::Join([Environment]::NewLine, @($buildOutput)))"
    }
    if ( !(Test-Path -LiteralPath ".\hello.exe" -PathType Leaf) ) {
        throw "standalone xl build did not create hello.exe"
    }
    $runOutput = & ".\hello.exe" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone hello.exe failed: $([string]::Join([Environment]::NewLine, @($runOutput)))"
    }
    $headerOutput = & ".\xl.exe" run "public_vfs_headers.c" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone public VFS header run failed: $([string]::Join([Environment]::NewLine, @($headerOutput)))"
    }
    if ( [string]::Join("`n", @($headerOutput)).IndexOf("public_vfs_headers=1:1") -lt 0 ) {
        throw "standalone public VFS header output mismatch: $([string]::Join([Environment]::NewLine, @($headerOutput)))"
    }
    if ( Test-Path -LiteralPath (Join-Path $testDir 'tmp') ) {
        throw 'ordinary JIT/build must not create even an empty tmp directory'
    }
    $embedBuildOutput = & ".\xl.exe" build "embedded_xlang.xl" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone embedded xlang build failed: $([string]::Join([Environment]::NewLine, @($embedBuildOutput)))"
    }
    if ( Test-Path -LiteralPath ".\embedded_xlang.def" ) {
        throw "standalone embedded xlang EXE unexpectedly emitted embedded_xlang.def"
    }
    $embedRunOutput = & ".\embedded_xlang.exe" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone embedded xlang exe failed: $([string]::Join([Environment]::NewLine, @($embedRunOutput)))"
    }
    if ( [string]::Join([Environment]::NewLine, @($embedRunOutput)).IndexOf("standalone_xlang_embed=ok") -lt 0 ) {
        throw "standalone embedded xlang output mismatch: $([string]::Join([Environment]::NewLine, @($embedRunOutput)))"
    }
    $compilerBuildOutput = & ".\xl.exe" build "embedded_xlang_compiler.xl" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone embedded xlang compiler build failed: $([string]::Join([Environment]::NewLine, @($compilerBuildOutput)))"
    }
    if ( Test-Path -LiteralPath ".\embedded_xlang_compiler.def" ) {
        throw "standalone embedded xlang compiler EXE unexpectedly emitted embedded_xlang_compiler.def"
    }
    $compilerRunOutput = & ".\embedded_xlang_compiler.exe" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone embedded xlang compiler exe failed: $([string]::Join([Environment]::NewLine, @($compilerRunOutput)))"
    }
    if ( [string]::Join([Environment]::NewLine, @($compilerRunOutput)).IndexOf("standalone_xlang_compiler=ok") -lt 0 ) {
        throw "standalone embedded xlang compiler output mismatch: $([string]::Join([Environment]::NewLine, @($compilerRunOutput)))"
    }
    $completed = $true
} finally {
    Set-Location -LiteralPath $savedLocation
}

if ( $completed ) {
    # 仅清理本次新建且未被替换为目录链接的临时目录。
    $directory = Get-Item -LiteralPath $testDir -Force
    if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "standalone compiler VFS cleanup refused: $testDir"
    }
    $resolvedTestDir = (Resolve-Path -LiteralPath $testDir).Path
    if (![string]::Equals($resolvedTestDir, [IO.Path]::GetFullPath($testDir), [StringComparison]::OrdinalIgnoreCase)) {
        throw "standalone compiler VFS cleanup refused: $resolvedTestDir"
    }
    Remove-Item -LiteralPath $resolvedTestDir -Recurse -Force
}

Write-Host "Standalone compiler VFS ok: public headers and embedded xlang runtime/compiler work without repository files."
