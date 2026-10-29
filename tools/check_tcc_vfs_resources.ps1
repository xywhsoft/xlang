param(
    [string]$Resource = "",

    [string]$Root = ""
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
            ).Replace("-", "").ToLowerInvariant()
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}



if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Get-Location).Path
}
$Root = [System.IO.Path]::GetFullPath($Root)
$resources = if ([string]::IsNullOrWhiteSpace($Resource)) {
    @(
        "build\tmp\tcc_builtin_resources_runtime.c",
        "build\tmp\tcc_builtin_resources.c"
    )
} else {
    @($Resource)
}

foreach ($resourceItem in $resources) {
    $resourcePath = if ([System.IO.Path]::IsPathRooted($resourceItem)) {
        [System.IO.Path]::GetFullPath($resourceItem)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $Root $resourceItem))
    }
    $stampPath = "$resourcePath.stamp.json"

    if (-not (Test-Path -LiteralPath $resourcePath -PathType Leaf)) {
        throw "missing TCC VFS resource source: $resourcePath"
    }
    if (-not (Test-Path -LiteralPath $stampPath -PathType Leaf)) {
        throw "missing TCC VFS resource stamp: $stampPath"
    }

    $stamp = Get-Content -LiteralPath $stampPath -Raw | ConvertFrom-Json
    if ($stamp.schema -ne 2 -or $null -eq $stamp.resources) {
        throw "invalid TCC VFS resource stamp: $stampPath"
    }

    $virtualPaths = New-Object System.Collections.Generic.HashSet[string]
    foreach ($entry in $stamp.resources) {
        if ([string]::IsNullOrWhiteSpace($entry.source) -or
            [string]::IsNullOrWhiteSpace($entry.sha256)) {
            throw "invalid TCC VFS resource entry in $stampPath"
        }
        if (-not (Test-Path -LiteralPath $entry.source -PathType Leaf)) {
            throw "TCC VFS source disappeared after packing: $($entry.source)"
        }
        $actual = Get-XlSha256 $entry.source
        if ($actual -ne $entry.sha256.ToLowerInvariant()) {
            throw "TCC VFS resource is stale: $($entry.virtual) <- $($entry.source)"
        }
        [void]$virtualPaths.Add([string]$entry.virtual)
    }

    if ([System.IO.Path]::GetFileName($resourcePath) -eq "tcc_builtin_resources.c" -and
        -not $virtualPaths.Contains("__xl_objects/static-compiler/src/api/xl_tcc_standalone_env.c.o")) {
        throw "compiler TCC VFS is missing standalone XRT environment precompiled object"
    }

    if ((Get-XlSha256 $resourcePath) -cne $stamp.generatedSha256) {
        throw "TCC VFS generated catalog is stale: $resourcePath"
    }
    if (@($stamp.resources | Where-Object { $_.virtual -match '/tcc_builtin_resources_runtime[.]c$' }).Count) {
        throw 'nested catalog source must not be stored a second time'
    }

    Write-Host "TCC VFS resource snapshot is current: $resourcePath"
}
