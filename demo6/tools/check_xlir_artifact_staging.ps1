param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicDirectory).Path
    $out=[IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $out) { throw 'Preserve staging evidence; use a fresh directory' }
    New-Item -ItemType Directory -Path $out | Out-Null
    $frozen=@($PSCommandPath,'tools/stage_xlir_artifact_runtime_sources.ps1',
        'tools/tcc_vfs_manifest_win64_xlc.txt',"$public/build.json") | ForEach-Object {
        $file=Get-Item -LiteralPath $_
        [pscustomobject]@{Path=$file.FullName;Sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
    }
    $package=Join-Path $out 'package'
    New-Item -ItemType Directory -Path $package | Out-Null
    Copy-Item -LiteralPath "$public/build.json" -Destination "$package/build.json"
    $shell=(Get-Command powershell.exe).Source
    function Parallel-Stage([string]$name) {
        $children=@()
        try {
            foreach ($n in 0..3) {
                $child=Start-Process -FilePath $shell -WindowStyle Hidden -PassThru -WorkingDirectory $root -ArgumentList @(
                    '-NoProfile','-ExecutionPolicy','Bypass','-File','tools/stage_xlir_artifact_runtime_sources.ps1',
                    '-Directory',('"'+$package+'"')) -RedirectStandardOutput "$out/$name.$n.out.log" -RedirectStandardError "$out/$name.$n.err.log"
                $null=$child.Handle
                $children+=$child
            }
            foreach ($child in $children) {
                $null=$child.Handle
                if (!$child.WaitForExit(60000)) { throw 'Concurrent staging timeout' }
                $child.Refresh()
                if ($child.ExitCode -ne 0) { throw 'Concurrent staging failed' }
            }
        } finally {
            foreach ($child in $children) { if (!$child.HasExited) { $child.Kill(); $child.WaitForExit() }; $child.Dispose() }
        }
    }
    Parallel-Stage 'initial'
    $manifest="$package/artifact_sources.json"
    $staged=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $paths=@($staged.Files.Path)+@($manifest)
    $before=@($paths | ForEach-Object {
        $file=Get-Item -LiteralPath $_
        [pscustomobject]@{Path=$file.FullName;Sha256=(Get-FileHash -LiteralPath $_).Hash;WriteTicks=$file.LastWriteTimeUtc.Ticks}
    })
    Parallel-Stage 'reuse'
    foreach ($record in $before) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256 -or
            (Get-Item -LiteralPath $record.Path).LastWriteTimeUtc.Ticks -ne $record.WriteTicks) { throw 'Reuse rewrote a published file' }
    }
    # Deliberately corrupt only this new isolated package, never source/runtime.
    $corrupt=[IO.Path]::GetFullPath($staged.Files[0].Path)
    if (!$corrupt.StartsWith($package+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Corruption target escaped evidence package' }
    [IO.File]::AppendAllText($corrupt,'/* deliberate staging corruption */',[Text.UTF8Encoding]::new($false))
    $corruptHash=(Get-FileHash -LiteralPath $corrupt).Hash
    $manifestHash=(Get-FileHash -LiteralPath $manifest).Hash
    $priorErrorAction=$ErrorActionPreference
    try {
        $ErrorActionPreference='Continue'
        & $shell -NoProfile -ExecutionPolicy Bypass -File tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $package *> "$out/reject.log"
        $rejected=$LASTEXITCODE
    } finally { $ErrorActionPreference=$priorErrorAction }
    if ($rejected -eq 0 -or !(Select-String -LiteralPath "$out/reject.log" -SimpleMatch 'Published staged file changed' -Quiet) -or
        (Get-FileHash -LiteralPath $corrupt).Hash -ne $corruptHash -or (Get-FileHash -LiteralPath $manifest).Hash -ne $manifestHash) { throw 'Corrupted publication was replaced instead of refused' }
    $unchanged=!@($frozen | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    if (!$unchanged) { throw 'Staging test inputs changed' }
    $report=[pscustomobject]@{Complete=$true;InputsUnchanged=$true;Inputs=$frozen;Files=$before.Count;
        ConcurrentInitialWriters=4;ConcurrentReadOnlyReuses=4;CorruptionRejected=$true;
        Scope='Isolated package only: atomic initial publication, exact hash/mtime read-only reuse, corrupted target refused without overwriting target or manifest.'}
    [IO.File]::WriteAllText("$out/results.json",($report | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
    Write-Output "Staging: 4 concurrent initial writers, 4 immutable reuses, $($before.Count) unchanged files, corruption refused"
} finally { Pop-Location }
