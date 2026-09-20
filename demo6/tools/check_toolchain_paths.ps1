param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$srcDir = Join-Path $Root "src"
$allowedSource = "src/backend/xl6_toolchain.h"
$allowedBatch = "tools/xl6_toolchain_env.bat"
$fixtureScanDirs = @(
    "release\examples",
    "test\cases",
    "test\fixtures",
    "release\extlibs"
)
$minToolchainSourceFiles = 157
$minToolchainFixtureFiles = 153
$minToolchainBatchFiles = 12
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Relative-From-Root($path)
{
    $rootPath = (Resolve-Path -LiteralPath $Root).Path
    return $path.Substring($rootPath.Length + 1).Replace("\", "/")
}

if ( ! (Test-Path -LiteralPath $srcDir -PathType Container) ) {
    Write-Host "src directory missing: $srcDir"
    exit 1
}

$sourceFiles = @(Get-ChildItem -LiteralPath $srcDir -Recurse -File -Include "*.c", "*.h" | Sort-Object FullName)
$fixtureFiles = @(
    foreach ( $relativeDir in $fixtureScanDirs ) {
        $dir = Join-Path $Root $relativeDir
        if ( Test-Path -LiteralPath $dir -PathType Container ) {
            Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object {
                $relative = Relative-From-Root $_.FullName
                ($_.Extension -eq ".c" -or $_.Extension -eq ".h")
            }
        }
    }
) | Sort-Object FullName

if ( $sourceFiles.Count -lt $minToolchainSourceFiles ) {
    Add-Failure "toolchain source scan baseline regressed: $($sourceFiles.Count) < $minToolchainSourceFiles"
}

if ( $fixtureFiles.Count -lt $minToolchainFixtureFiles ) {
    Add-Failure "toolchain fixture scan baseline regressed: $($fixtureFiles.Count) < $minToolchainFixtureFiles"
}

foreach ( $file in $sourceFiles ) {
    $relative = Relative-From-Root $file.FullName
    $text = Read-Utf8Text $file.FullName

    if ( $relative -eq $allowedSource ) {
        continue
    }

    if ( $text -match '(?i)demo5|[.][.][\\/]+demo5|[.][.][\\/]+[.][.][\\/]+xrt[\\/]release' ) {
        Add-Failure "source toolchain path/reference must stay in ${allowedSource}: ${relative}"
    }
}

foreach ( $file in $fixtureFiles ) {
    $relative = Relative-From-Root $file.FullName
    $text = Read-Utf8Text $file.FullName

    if ( $text -match '(?i)demo5|[.][.][\\/]+demo5|[.][.][\\/]+[.][.][\\/]+xrt[\\/]release' ) {
        Add-Failure "fixture toolchain path/reference must stay in ${allowedBatch}: ${relative}"
    }
}

$batchFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]
foreach ( $file in @(Get-ChildItem -LiteralPath $Root -File -Filter "*.bat") ) {
    $batchFiles.Add($file) | Out-Null
}

$batchScanDirs = @(
    (Join-Path $Root "tools"),
    (Join-Path $Root "release\examples")
)

foreach ( $dir in $batchScanDirs ) {
    if ( Test-Path -LiteralPath $dir -PathType Container ) {
        foreach ( $file in @(Get-ChildItem -LiteralPath $dir -Recurse -File -Filter "*.bat") ) {
            $batchFiles.Add($file) | Out-Null
        }
    }
}

$batchFiles = @($batchFiles | Sort-Object FullName -Unique)

if ( $batchFiles.Count -lt $minToolchainBatchFiles ) {
    Add-Failure "toolchain batch scan baseline regressed: $($batchFiles.Count) < $minToolchainBatchFiles"
}

foreach ( $file in $batchFiles ) {
    $relative = Relative-From-Root $file.FullName
    $text = Read-Utf8Text $file.FullName

    if ( $relative -eq $allowedBatch ) {
        continue
    }

    if ( $text -match '(?i)demo5|[.][.][\\/]+demo5|[.][.][\\/]+[.][.][\\/]+xrt[\\/]release' ) {
        Add-Failure "batch toolchain path/reference must stay in ${allowedBatch}: ${relative}"
    }
}

$apiPath = Join-Path $Root "src\api\xl_api.c"
$backendPath = Join-Path $Root "src\backend\xl6_build_backend.c"
$xrtBridgePath = Join-Path $Root "src\common\xl6_xrt.h"

foreach ( $path in @($apiPath, $backendPath, $xrtBridgePath) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "toolchain consumer source is missing: $(Relative-From-Root $path)"
    }
}

if ( (Test-Path -LiteralPath $apiPath -PathType Leaf) -and
     (Test-Path -LiteralPath $backendPath -PathType Leaf) -and
     (Test-Path -LiteralPath $xrtBridgePath -PathType Leaf) ) {
    $apiText = Read-Utf8Text $apiPath
    $backendText = Read-Utf8Text $backendPath
    $xrtBridgeText = Read-Utf8Text $xrtBridgePath

    if ( $backendText -notmatch [regex]::Escape("XL6_TCC") ) {
        Add-Failure "build backend must honor centralized toolchain environment variable: XL6_TCC"
    }
    if ( $backendText -notmatch 'xl6BuildBackendEnvPathOrDefault' ) {
        Add-Failure "build backend must keep centralized TCC executable default helper"
    }

    foreach ( $name in @(
        "XL6_TCC_INCLUDE",
        "XL6_TCC_WIN_INCLUDE",
        "XL6_TCC_WINAPI_INCLUDE",
        "XL6_TCC_LIB",
        "XL6_TCC_LIBTCC1"
    ) ) {
        if ( $apiText -notmatch [regex]::Escape($name) ) {
            Add-Failure "DLL API must honor centralized toolchain environment variable: $name"
        }
    }
    if ( $apiText -notmatch 'XRUNTIME_IMPLEMENTATION' -or
         $apiText -notmatch '#include \\"xruntime[.]h\\"' -or
         $apiText -notmatch 'src/common/xl6_xrt[.]h' -or
         $xrtBridgeText -notmatch 'lib/xruntime/xruntime[.]h' ) {
        Add-Failure "DLL compiler API must embed XRT/xruntime through the product single-header path"
    }

    foreach ( $name in @(
        "xlDefaultTccIncludePath",
        "xlDefaultTccWinIncludePath",
        "xlDefaultTccWinapiIncludePath",
        "xlDefaultTccLibPath",
        "xlDefaultTccLibtcc1Path"
    ) ) {
        if ( $apiText -notmatch ('(?<![A-Za-z0-9_])' + [regex]::Escape($name) + '(?![A-Za-z0-9_])') ) {
            Add-Failure "DLL API must keep centralized TCC default helper: $name"
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Toolchain path guard ok: $($sourceFiles.Count) source headers/files, $($fixtureFiles.Count) fixture C/H files, $($batchFiles.Count) batch files."
exit 0
