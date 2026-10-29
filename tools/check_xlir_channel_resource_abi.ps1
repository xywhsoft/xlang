param([string]$Root = '', [string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = [IO.Path]::GetFullPath($Root)
$python = (Get-Command python -CommandType Application | Select-Object -First 1).Source
Push-Location $Root
try {
    if (!$Directory) { $Directory = 'build/test/channel-owned-' + [guid]::NewGuid().ToString('N') }
    $out = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $out) { throw 'Preserve prior Channel evidence; choose a fresh directory' }
    if (!$PublicDirectory) {
        $PublicDirectory = $out + '-source'
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source Channel compiler build failed' }
    }
    # The shared cross-platform gate checks actual typed/lowered owning results,
    # closure writes, async residency, exact native calls, C/JIT/standalone AOT,
    # negative diagnostics and strict graph reads through all allocation prefixes.
    # No obsolete Into storage-address or generated Channel implementation match.
    & $python "$PSScriptRoot/check_stdlib_channel_regressions.py" --root $Root --public-directory $PublicDirectory --out $out
    if ($LASTEXITCODE -ne 0) { throw 'Channel owned source/global graph gate failed' }
    $report = Get-Content -LiteralPath "$out/results.json" -Raw | ConvertFrom-Json
    if (!$report.Complete -or !$report.InputsUnchanged) { throw 'Incomplete Channel evidence' }
    # Export only the freshly validated diagnostic bytes for the central
    # suite collector; preserve the unique batch report as their receipt.
    $diagnosticDirectory = Join-Path $Root 'build/test'
    [IO.Directory]::CreateDirectory($diagnosticDirectory) | Out-Null
    foreach ($entry in $report.NegativeFixtures.PSObject.Properties) {
        $row = @($report.Rows | Where-Object Name -eq ('negative-' + $entry.Name))
        if ($row.Count -ne 1 -or $row[0].Exit -eq 0 -or
            (Get-FileHash -LiteralPath $row[0].Log -Algorithm SHA256).Hash -ne $row[0].LogSha256) {
            throw 'Negative Channel diagnostic evidence changed'
        }
        $bytes = [IO.File]::ReadAllBytes($row[0].Log)
        [void]([Text.UTF8Encoding]::new($false,$true)).GetString($bytes)
        [IO.File]::WriteAllBytes((Join-Path $diagnosticDirectory ('sem_channel_' + $entry.Name + '.err.txt')), $bytes)
    }
    Write-Output "Channel owned ABI passed: $($report.Rows.Count) stages; WaitQueue/full53/shipping acceptance remain separate."
} finally { Pop-Location }
