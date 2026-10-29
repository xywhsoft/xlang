param(
    [string]$Root = '',
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_public_cgen.ps1')
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$outputPath = if ([IO.Path]::IsPathRooted($Directory)) {
    [IO.Path]::GetFullPath($Directory)
} else {
    [IO.Path]::GetFullPath((Join-Path $Root $Directory))
}
if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to reuse XLIR default-backend evidence: $outputPath"
}
[IO.Directory]::CreateDirectory($outputPath) | Out-Null

$shell = Join-Path $Root 'src\driver\xl6_xlc_shell.c'
$api = Join-Path $Root 'src\api\xl_api.c'
$header = Join-Path $Root 'release\include\xlc.h'
$source = Join-Path $Root 'test\cases\xlir_checked_integer_arithmetic_abi.xl'
foreach ($path in @($shell, $api, $header, $source)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "XLIR default-backend boundary input is missing: $path"
    }
}
$boundarySnapshot = @(New-XlirPublicCgenSnapshot -Paths @($shell, $api, $header, $source))

$shellText = [IO.File]::ReadAllText($shell, [Text.Encoding]::UTF8)
$apiText = [IO.File]::ReadAllText($api, [Text.Encoding]::UTF8)
$headerText = [IO.File]::ReadAllText($header, [Text.Encoding]::UTF8)
if (!$headerText.Contains('XLC_BACKEND_XLIR = 0') -or
    $headerText.Contains('XLC_BACKEND_LEGACY')) {
    throw 'Public compiler API must expose only XLIR as the zero/default backend'
}
if (!$apiText.Contains('backend != XLC_BACKEND_XLIR') -or
    !$apiText.Contains('only the XLIR compiler backend is available') -or
    !$apiText.Contains('xlEmitUnitCToFile(') -or
    $apiText.Contains('xlEmitUnitXlirCToFile(') -or
    $apiText.Contains('ctx->backend')) {
    throw 'Compiler API must reject retired backend values and route C emission directly through XLIR'
}
if (!$shellText.Contains('xlcSetBackend(ctx, XLC_BACKEND_XLIR)') -or
    !$shellText.Contains('[--backend xlir]') -or
    !$shellText.Contains('only the xlir backend is available') -or
    $shellText.Contains('--backend legacy')) {
    throw 'CLI must expose only the canonical XLIR backend selector'
}

$session = Open-XlirPublicCgen -Root $Root -PublicDirectory $PublicDirectory
$defaultOutput = Join-Path $outputPath 'default.c'
$xlirOutput = Join-Path $outputPath 'explicit-xlir.c'
$batch = @(Invoke-XlirPublicCgenBatch -Session $session -Requests @(
    [pscustomobject]@{ Source = $source; Backend = 'default'; Output = $defaultOutput },
    [pscustomobject]@{ Source = $source; Backend = 'xlir'; Output = $xlirOutput }
))
$default, $xlir = $batch
foreach ($invocation in @($default, $xlir)) {
    if ($invocation.ExecutionFailure) {
        throw "XLIR default-backend $($invocation.Backend) execution failed: $($invocation.ExecutionFailure)"
    }
    if ($invocation.ExitCode -ne 0) {
        throw "XLIR default-backend $($invocation.Backend) Cgen failed: $($invocation.Output)"
    }
}
if ($default.SourceSha256 -ne $xlir.SourceSha256) {
    throw 'Default-backend differential invocations did not consume the same source revision'
}

if ($default.ArtifactSha256 -cne $xlir.ArtifactSha256) {
    throw 'Cgen without a backend override must be byte-identical to explicit XLIR'
}
$defaultText = [IO.File]::ReadAllText($defaultOutput, [Text.Encoding]::UTF8)
if (!$defaultText.Contains('xlir_module_init') -or !$defaultText.Contains('xlang_main')) {
    throw 'Default Cgen did not emit the verified XLIR module and language entry'
}
if ([Text.RegularExpressions.Regex]::IsMatch($defaultText, '(?m)^\s*int\s+main\s*\(')) {
    throw 'Cgen module artifacts must not own the native process main entry'
}

$recoveryPrefix = Join-Path $outputPath 'retired-backend-recovery'
$recoveryStdout = Join-Path $outputPath 'retired-backend-recovery.log'
$recoveryStderr = Join-Path $outputPath 'retired-backend-recovery.err.log'
$savedErrorActionPreference = $ErrorActionPreference
try {
    # Recovery deliberately runs a failing assertion. Windows PowerShell must
    # not promote that expected native stderr stream into a terminating error.
    $ErrorActionPreference = 'Continue'
    & ([string]$session.Driver) $source 'recovery' $recoveryPrefix `
        1> $recoveryStdout 2> $recoveryStderr
    $recoveryExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $savedErrorActionPreference
}
$recoveryText = if (Test-Path -LiteralPath $recoveryStdout) {
    [IO.File]::ReadAllText($recoveryStdout, [Text.Encoding]::UTF8)
} else { '' }
if ($recoveryExitCode -ne 0 -or
    !$recoveryText.Contains('retired-backend rejection and successful reuse')) {
    throw 'Public compiler recovery did not reject a retired backend value and remain reusable'
}
Assert-XlirPublicCgenUnchanged $session
Assert-XlirPublicCgenSnapshotUnchanged $boundarySnapshot

$report = [ordered]@{
    SchemaVersion = 4
    RecordedAt = [DateTime]::UtcNow.ToString('o')
    Source = $source
    CompilerManifest = $session.ManifestPath
    CompilerManifestSha256 = $session.ManifestSha256
    Driver = $session.Driver
    DriverSha256 = $session.DriverSha256
    BoundaryInputs = @($boundarySnapshot | ForEach-Object {
        [ordered]@{ Path = Get-XlirContainedRelativePath -Base $Root -Path $_.Path; Sha256 = $_.Sha256 }
    })
    Default = $default
    ExplicitXlir = $xlir
    DefaultEqualsExplicitXlir = $true
    RetiredBackendRejected = $true
    ContextReusableAfterRejectedBackend = $true
    RecoveryExitCode = $recoveryExitCode
    InputsUnchanged = $true
}
[IO.File]::WriteAllText((Join-Path $outputPath 'results.json'),
    ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output 'XLIR current-source backend boundary ok: default equals explicit XLIR, retired backend values are rejected, and Cgen emits an embeddable module.'
exit 0
