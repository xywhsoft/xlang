param([string]$Root = "", [switch]$CheckIndex)
$ErrorActionPreference = "Stop"
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
& python (Join-Path $PSScriptRoot "sync_xrt_snapshot.py") --root $Root --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
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
    $upstreamBytes=Read-SnapshotIndexBlob 'lib/xrt/upstream.json'
    $upstreamSha=[Security.Cryptography.SHA256]::Create()
    try { $upstreamHash=[BitConverter]::ToString($upstreamSha.ComputeHash($upstreamBytes)).Replace('-','') }
    finally { $upstreamSha.Dispose() }
    foreach ($product in @(
        @{Metadata='lib/xrt/xrt.snapshot.json';Implementation='lib/xrt/xrt.h';Declarations='release/include/xrt_decl.h'},
        @{Metadata='lib/xruntime/xruntime.snapshot.json';Implementation='lib/xruntime/xruntime.h';Declarations='release/include/xruntime_decl.h';Features='lib/xruntime/xruntime_features.h'})) {
        $bytes=Read-SnapshotIndexBlob $product.Metadata
        $metadata=[Text.Encoding]::UTF8.GetString($bytes)|ConvertFrom-Json
        if ($metadata.upstreamSha256 -cne $upstreamHash) {
            throw "Staged upstream provenance disagrees with staged metadata: $($product.Metadata)"
        }
        foreach ($section in @('implementation','declarations','features') | Where-Object { $product.ContainsKey($_) }) {
            $bytes=Read-SnapshotIndexBlob $product[$section]
            $sha=[Security.Cryptography.SHA256]::Create()
            try { $hash=[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','') }
            finally { $sha.Dispose() }
            if ($hash -cne $metadata.$section.sha256 -or $bytes.Length -ne $metadata.$section.bytes) {
                throw "Staged snapshot bytes disagree with staged metadata: $($product[$section])"
            }
        }
    }
    Write-Host 'Staged snapshot bytes and metadata agree: 5 header assets'
}

exit 0
