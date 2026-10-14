param(
    [string]$Root = "",
    [switch]$CheckIndex
)

$ErrorActionPreference = "Stop"



function Get-XlSha256(
    [string]$Path
)
{
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return [System.BitConverter]::ToString(
                $sha256.ComputeHash($stream)
            ).Replace("-", "")
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}



if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$coreHeaderPath = Join-Path $Root "lib\xrt\xrt.h"
$coreDeclarationPath = Join-Path $Root "release\include\xrt_decl.h"
$coreMetadataPath = Join-Path $Root "lib\xrt\xrt.snapshot.json"
$runtimeHeaderPath = Join-Path $Root "lib\xruntime\xruntime.h"
$runtimeDeclarationPath = Join-Path $Root "release\include\xruntime_decl.h"
$runtimeMetadataPath = Join-Path $Root "lib\xruntime\xruntime.snapshot.json"



# Build validation uses only repository-local assets.
$assets = @(
    $coreHeaderPath,
    $coreDeclarationPath,
    $coreMetadataPath,
    $runtimeHeaderPath,
    $runtimeDeclarationPath,
    $runtimeMetadataPath
)
if ( @($assets | Where-Object { ! (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -ne 0 ) {
    Write-Error "XRT/xruntime snapshot assets are incomplete"
    exit 1
}

$coreMetadata = Get-Content -LiteralPath $coreMetadataPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
$runtimeMetadata = Get-Content -LiteralPath $runtimeMetadataPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
$coreFile = Get-Item -LiteralPath $coreHeaderPath
$coreDeclarationFile = Get-Item -LiteralPath $coreDeclarationPath
$runtimeFile = Get-Item -LiteralPath $runtimeHeaderPath
$runtimeDeclarationFile = Get-Item -LiteralPath $runtimeDeclarationPath
$coreHash = Get-XlSha256 $coreHeaderPath
$coreDeclarationHash = Get-XlSha256 $coreDeclarationPath
$runtimeHash = Get-XlSha256 $runtimeHeaderPath
$runtimeDeclarationHash = Get-XlSha256 $runtimeDeclarationPath

if ( $coreMetadata.schema -ne 3 -or
    $coreMetadata.product -cne "xrt" -or
    $coreMetadata.implementation.source -cne "xrt/single/xrt.h" -or
    [int64]$coreMetadata.implementation.bytes -ne $coreFile.Length -or
    $coreMetadata.implementation.sha256 -cne $coreHash -or
    $coreMetadata.declarations.source -cne "xrt/single/xrt_decl.h" -or
    [int64]$coreMetadata.declarations.bytes -ne $coreDeclarationFile.Length -or
    $coreMetadata.declarations.sha256 -cne $coreDeclarationHash -or
    [int64]$coreMetadata.publicApiCount -lt 1000 ) {
    Write-Error "XRT snapshot metadata is stale"
    exit 1
}

if ( $runtimeMetadata.schema -ne 3 -or
    $runtimeMetadata.product -cne "xruntime" -or
    $runtimeMetadata.implementation.source -cne "xrt/extlibs/xruntime/single/xruntime.h" -or
    [int64]$runtimeMetadata.implementation.bytes -ne $runtimeFile.Length -or
    $runtimeMetadata.implementation.sha256 -cne $runtimeHash -or
    $runtimeMetadata.declarations.source -cne "xrt/extlibs/xruntime/single/xruntime_decl.h" -or
    [int64]$runtimeMetadata.declarations.bytes -ne $runtimeDeclarationFile.Length -or
    $runtimeMetadata.declarations.sha256 -cne $runtimeDeclarationHash -or
    [int64]$runtimeMetadata.publicApiCount -lt [int64]$coreMetadata.publicApiCount -or
    [int64]$runtimeMetadata.ownedPublicApiCount -ne
        ([int64]$runtimeMetadata.publicApiCount - [int64]$coreMetadata.publicApiCount) ) {
    Write-Error "xruntime snapshot metadata is stale"
    exit 1
}



# The runtime symbol table must be derived from this exact snapshot.
if ($CheckIndex) {
    # Check the bytes Git will actually commit, not a text-decoded pipeline.
    # In particular, eol=lf may normalize a CRLF worktree declaration header.
    $gitCommand=(Get-Command git.exe -ErrorAction Stop).Source
    $prefix=[string](& $gitCommand -C $Root rev-parse --show-prefix)
    if ($LASTEXITCODE -ne 0) { throw 'Snapshot index check requires a Git workspace' }
    $prefix=$prefix.Trim()
    function Read-SnapshotIndexBlob([string]$relative) {
        $info=[Diagnostics.ProcessStartInfo]::new()
        $info.FileName=$gitCommand; $info.WorkingDirectory=$Root
        $info.Arguments='show "'+(':'+$prefix+$relative).Replace('"','\"')+'"'
        $info.UseShellExecute=$false; $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
        $process=[Diagnostics.Process]::new(); $process.StartInfo=$info
        $stream=[IO.MemoryStream]::new()
        try {
            if (!$process.Start()) { throw 'Cannot start Git snapshot reader' }
            $process.StandardOutput.BaseStream.CopyTo($stream)
            $errorText=$process.StandardError.ReadToEnd(); $process.WaitForExit()
            if ($process.ExitCode -ne 0) { throw "Cannot read staged snapshot $relative : $errorText" }
            return ,$stream.ToArray()
        } finally { $stream.Dispose(); $process.Dispose() }
    }
    foreach ($product in @(
        @{Metadata='lib/xrt/xrt.snapshot.json';Implementation='lib/xrt/xrt.h';Declarations='release/include/xrt_decl.h'},
        @{Metadata='lib/xruntime/xruntime.snapshot.json';Implementation='lib/xruntime/xruntime.h';Declarations='release/include/xruntime_decl.h'})) {
        $bytes=Read-SnapshotIndexBlob $product.Metadata
        $metadata=[Text.Encoding]::UTF8.GetString($bytes)|ConvertFrom-Json
        foreach ($section in @('implementation','declarations')) {
            $bytes=Read-SnapshotIndexBlob $product[$section]
            $sha=[Security.Cryptography.SHA256]::Create()
            try { $hash=[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','') }
            finally { $sha.Dispose() }
            if ($hash -cne $metadata.$section.sha256 -or $bytes.Length -ne $metadata.$section.bytes) {
                throw "Staged snapshot bytes disagree with staged metadata: $($product[$section])"
            }
        }
    }
    Write-Host 'Staged snapshot bytes and metadata agree: 4 header assets'
}

$symbolCheckPath = Join-Path $Root "tools\gen_xrt_symbol_table.ps1"
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File $symbolCheckPath -Root $Root -Check
if ( $LASTEXITCODE -ne 0 ) {
    exit $LASTEXITCODE
}

# Match the ordinary Linux build: generated capability evidence must agree
# with the same repository-local runtime and language sources, not just APIs.
& python (Join-Path $Root "tools\gen_xrt_xlang_matrix.py") --root $Root --check
if ( $LASTEXITCODE -ne 0 ) {
    exit $LASTEXITCODE
}

Write-Host "XRT/xruntime snapshots are current: xrt=$($coreMetadata.publicApiCount) APIs, xruntime=$($runtimeMetadata.publicApiCount) APIs ($($runtimeMetadata.ownedPublicApiCount) owned)"
exit 0
