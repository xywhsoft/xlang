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
$matrixName = 'XLIR' + ([char]0x8FC1) + ([char]0x79FB) +
    ([char]0x9A8C) + ([char]0x6536) + ([char]0x77E9) + ([char]0x9635) + '.md'
$matrix = Join-Path $Root (Join-Path 'docs' $matrixName)
foreach ($path in @($shell, $api, $header, $source, $matrix)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "XLIR default-backend boundary input is missing: $path"
    }
}
$boundarySnapshot = @(New-XlirPublicCgenSnapshot -Paths @($shell, $api, $header, $source, $matrix))

$shellText = [IO.File]::ReadAllText($shell, [Text.Encoding]::UTF8)
$apiText = [IO.File]::ReadAllText($api, [Text.Encoding]::UTF8)
$headerText = [IO.File]::ReadAllText($header, [Text.Encoding]::UTF8)
if (!$headerText.Contains('XLC_BACKEND_XLIR = 0') -or
    !$headerText.Contains('XLC_BACKEND_LEGACY = 1')) {
    throw 'Public compiler API must expose XLIR as the zero/default backend and legacy explicitly'
}
if (!$apiText.Contains('ctx->backend = XLC_BACKEND_XLIR;') -or
    !$apiText.Contains('ctx->backend == XLC_BACKEND_LEGACY') -or
    !$apiText.Contains('xlEmitUnitXlirCToFile(')) {
    throw 'Compiler context default or selected-backend C emission routing regressed'
}
if (!$shellText.Contains('xlcSetBackend(ctx, XLC_BACKEND_XLIR)') -or
    !$shellText.Contains('xlcSetBackend(ctx, XLC_BACKEND_LEGACY)') -or
    !$shellText.Contains('[--backend legacy|xlir]')) {
    throw 'CLI must expose and apply both explicit backend selections'
}
$matrixText = [IO.File]::ReadAllText($matrix, [Text.Encoding]::UTF8)
if (!$matrixText.Contains('XLC_BACKEND_XLIR') -or
    !$matrixText.Contains('--backend legacy') -or
    !$matrixText.Contains('legacy-vs-XLIR')) {
    throw 'XLIR default-backend contract is undocumented'
}

$session = Open-XlirPublicCgen -Root $Root -PublicDirectory $PublicDirectory
$defaultOutput = Join-Path $outputPath 'default.c'
$xlirOutput = Join-Path $outputPath 'explicit-xlir.c'
$legacyOutput = Join-Path $outputPath 'explicit-legacy.c'
$batch = @(Invoke-XlirPublicCgenBatch -Session $session -Requests @(
    [pscustomobject]@{ Source = $source; Backend = 'default'; Output = $defaultOutput },
    [pscustomobject]@{ Source = $source; Backend = 'xlir'; Output = $xlirOutput },
    [pscustomobject]@{ Source = $source; Backend = 'legacy'; Output = $legacyOutput }
))
$default, $xlir, $legacy = $batch
foreach ($invocation in @($default, $xlir, $legacy)) {
    if ($invocation.ExecutionFailure) {
        throw "XLIR default-backend $($invocation.Backend) execution failed: $($invocation.ExecutionFailure)"
    }
    if ($invocation.ExitCode -ne 0) {
        throw "XLIR default-backend $($invocation.Backend) Cgen failed: $($invocation.Output)"
    }
}
if ($default.SourceSha256 -ne $xlir.SourceSha256 -or
    $default.SourceSha256 -ne $legacy.SourceSha256) {
    throw 'Default-backend differential invocations did not consume the same source revision'
}

if ($default.ArtifactSha256 -cne $xlir.ArtifactSha256) {
    throw 'Cgen without a backend override must be byte-identical to explicit XLIR'
}
if ($default.ArtifactSha256 -ceq $legacy.ArtifactSha256) {
    throw 'Explicit legacy Cgen must remain a distinct differential baseline'
}
$defaultText = [IO.File]::ReadAllText($defaultOutput, [Text.Encoding]::UTF8)
$legacyText = [IO.File]::ReadAllText($legacyOutput, [Text.Encoding]::UTF8)
if ($legacyText.Contains('xlir_module_init')) {
    throw 'Explicit legacy Cgen emitted the XLIR module boundary'
}
if (!$defaultText.Contains('xlir_module_init') -or !$defaultText.Contains('xlang_main')) {
    throw 'Default Cgen did not emit the verified XLIR module and language entry'
}
if ([Text.RegularExpressions.Regex]::IsMatch($defaultText, '(?m)^\s*int\s+main\s*\(')) {
    throw 'Cgen module artifacts must not own the native process main entry'
}
Assert-XlirPublicCgenUnchanged $session
Assert-XlirPublicCgenSnapshotUnchanged $boundarySnapshot

$report = [ordered]@{
    SchemaVersion = 3
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
    ExplicitLegacy = $legacy
    DefaultEqualsExplicitXlir = $true
    LegacyIsDistinct = $true
    InputsUnchanged = $true
}
[IO.File]::WriteAllText((Join-Path $outputPath 'results.json'),
    ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output 'XLIR current-source default backend boundary ok: default equals explicit XLIR, legacy remains explicit, and Cgen emits an embeddable module.'
exit 0
