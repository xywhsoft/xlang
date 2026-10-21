[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SdkRoot
)

$ErrorActionPreference = 'Stop'

function Get-Sha256Hex {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read
    )
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $sha256.ComputeHash($stream)
        return ([System.BitConverter]::ToString($bytes)).Replace('-', '')
    }
    finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

try {
    $resolvedRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
    $manifestPath = Join-Path $resolvedRoot 'sdk_manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "SDK manifest not found: $manifestPath"
    }

    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.schemaVersion -notin @(1, 2)) {
        throw "Unsupported SDK manifest version: $($manifest.schemaVersion)"
    }
    if ($manifest.policy.runtimeOwner -ne 'xge.dll' -or
        $manifest.policy.xrtLinkage -ne 'shared' -or
        -not $manifest.policy.replaceAsMatchedSetOnly) {
        throw 'The SDK must provide one shared XRT runtime owned by xge.dll.'
    }

    foreach ($file in $manifest.files) {
        $relativePath = [string]$file.path
        if ([System.IO.Path]::IsPathRooted($relativePath) -or
            $relativePath -match '(^|[\\/])\.\.([\\/]|$)') {
            throw "Invalid SDK manifest path: $relativePath"
        }
        $fullPath = Join-Path $resolvedRoot ($relativePath -replace '/', '\')
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "SDK file not found: $relativePath"
        }

        $actualHash = Get-Sha256Hex -Path $fullPath
        $expectedHash = ([string]$file.sha256).ToUpperInvariant()
        if ($actualHash -ne $expectedHash) {
            throw "SDK file hash mismatch: $relativePath`nExpected: $expectedHash`nActual:   $actualHash"
        }
    }

    $xrtDirectory = if ($manifest.xrtDirectory) { [string]$manifest.xrtDirectory } else { 'lib/xrt' }
    if ([IO.Path]::IsPathRooted($xrtDirectory) -or $xrtDirectory -match '(^|[\\/])\.\.([\\/]|$)') { throw 'Invalid XRT include directory.' }
    $configPath = Join-Path $resolvedRoot ($xrtDirectory + '/xrt_config.h')
    $configText = Get-Content -LiteralPath $configPath -Raw
    if ($manifest.schemaVersion -eq 2) {
        if (@($manifest.buildDefines).Count -ne 1 -or
            $manifest.buildDefines[0] -ne 'XGE_XRT_PROFILE_IDE') {
            throw 'Expected the mainline XGE IDE build profile.'
        }
        $gcc = Get-Command 'gcc' -ErrorAction Stop
        $headerPath = Join-Path $resolvedRoot ($xrtDirectory + '/xrt.h')
        $profileFlags = @('-I' + $resolvedRoot)
        if ($manifest.configurationHeader) {
            $configurationHeader = [string]$manifest.configurationHeader
            if ([IO.Path]::IsPathRooted($configurationHeader) -or $configurationHeader -match '(^|[\\/])\.\.([\\/]|$)') { throw 'Invalid SDK configuration header.' }
            $profileFlags += @('-include', (Join-Path $resolvedRoot $configurationHeader))
        }
        # Inspect the effective profile, not text in an inactive #if block.
        $configText = ('' | & $gcc.Source @profileFlags -dM -E -x c -DXGE_DLL -DXUI_DLL `
            -DXGE_XRT_PROFILE_IDE= -include $configPath -include $headerPath - 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to preprocess the XGE IDE profile: $configText"
        }
        if ($configText -notmatch '(?m)^#define XRT_USE_SHARED(?:\s|$)' -or
            $configText -match '(?m)^#define XRT_BUILD_SHARED(?:\s|$)') {
            throw 'SDK consumers must import, not implement, the shared XRT runtime.'
        }
        $upstream = Get-Content -LiteralPath (Join-Path $resolvedRoot ($xrtDirectory + '/upstream.json')) -Raw | ConvertFrom-Json
        if ((Get-Sha256Hex -Path $headerPath) -ne $upstream.declaration_header_sha256) {
            throw 'SDK XRT declarations do not match the recorded generated source snapshot.'
        }
    }
    foreach ($module in $manifest.requiredXrtModules) {
        $escapedModule = [Regex]::Escape([string]$module)
        if ($configText -notmatch "(?m)^\s*#define\s+$escapedModule(?:\s|$)") {
            throw "Required SDK-profile XRT module is disabled: $module"
        }
    }

    $objdump = Get-Command 'objdump' -ErrorAction SilentlyContinue
    if ($null -eq $objdump) {
        throw "objdump is required to verify the protected XGE ABI exports."
    }
    $dllPath = Join-Path $resolvedRoot 'xge.dll'
    $exportText = (& $objdump.Source -p $dllPath 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to inspect XGE exports with objdump."
    }
    foreach ($export in $manifest.requiredExports) {
        $escapedExport = [Regex]::Escape([string]$export)
        if ($exportText -notmatch "(?m)^\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?$escapedExport\s*$") {
            throw "Required XGE export is missing: $export"
        }
    }

    Write-Host "[XGE SDK] Verified protected profile: $($manifest.profile)"
    Write-Host "[XGE SDK] XRT implementation owner: $($manifest.policy.runtimeOwner)"
    exit 0
}
catch {
    [Console]::Error.WriteLine("[XGE SDK] Verification failed: $($_.Exception.Message)")
    exit 1
}
