param([string]$Root = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path

function Read-XlirAdmissionText([string]$RelativePath) {
    $path = Join-Path $Root $RelativePath
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "XLIR current-source admission input is missing: $RelativePath"
    }
    return [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
}

function Get-XlirAdmissionMandatoryParameters([string]$RelativePath) {
    $path = Join-Path $Root $RelativePath
    $tokens = $null
    $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile(
        $path, [ref]$tokens, [ref]$errors)
    if ($errors.Count) { throw "Cannot parse XLIR current-source admission input: $RelativePath" }
    $required = @()
    foreach ($parameter in $ast.ParamBlock.Parameters) {
        foreach ($attribute in $parameter.Attributes) {
            foreach ($argument in $attribute.NamedArguments) {
                if ($argument.ArgumentName -eq 'Mandatory' -and
                    ($null -eq $argument.Argument -or $argument.Argument.Extent.Text -match '^\$true$')) {
                    $required += $parameter.Name.VariablePath.UserPath
                }
            }
        }
    }
    return @($required | Sort-Object -Unique)
}

$driver = Read-XlirAdmissionText 'test\cases\xlir_public_source_driver.c'
foreach ($required in @('cgen-default', 'cgen-xlir', 'cgen-legacy',
    'xlcSetBackend(compiler, XLC_BACKEND_XLIR)',
    'xlcSetBackend(compiler, XLC_BACKEND_LEGACY)',
    'xlcEmitC(compiler, argv[3])',
    'rootLength >= sizeof(root)', 'moduleLength >= sizeof(headers)')) {
    if (!$driver.Contains($required)) {
        throw "Public source driver lost an exact backend Cgen contract: $required"
    }
}

$output = Read-XlirAdmissionText 'src\common\xl6_output.c'
$api = Read-XlirAdmissionText 'src\api\xl_api.c'
if (!$output.Contains('xrtFileWriteAtomic(out->path') -or
    !$output.Contains('void xl6OutputDiscard(XL6_Output* out)') -or
    @([regex]::Matches($api, 'xl6OutputDiscard\(&out\)')).Count -lt 4) {
    throw 'Public compiler file emission lost atomic commit or failed-generation discard semantics'
}

$helper = Read-XlirAdmissionText 'tools\xlir_public_cgen.ps1'
foreach ($required in @('SourceXrt', 'RuntimeOnly', 'EnableExeCompiler',
    'DriverSha256', 'ManifestSha256', 'Assert-XlirPublicCgenUnchanged',
    'New-XlirPublicCgenSnapshot', 'Assert-XlirPublicCgenSnapshotUnchanged',
    'Assert-XlirPublicCgenDirectoryInventoryUnchanged', 'SourceSha256', 'SourceUnchanged',
    'Invoke-XlirPublicCgenBatch', 'XLIR Cgen batch outputs must be unique',
    'XLIR Cgen output aliases its source input',
    'compiler rejected input but left a generated C artifact',
    'XLIR Cgen output aliases a frozen compiler input', 'Stop-XlirProcessTree')) {
    if (!$helper.Contains($required)) {
        throw "Current-source Cgen helper lost provenance, stability or bounded execution: $required"
    }
}

$admission = Read-XlirAdmissionText 'tools\check_xlir_default_admission.ps1'
$boundary = Read-XlirAdmissionText 'tools\check_xlir_default_backend_boundary.ps1'
$audit = Read-XlirAdmissionText 'tools\audit_xlir_migration_gaps.ps1'
foreach ($contract in @(
    [pscustomobject]@{ Path = 'tools\check_xlir_default_admission.ps1'; Text = $admission },
    [pscustomobject]@{ Path = 'tools\check_xlir_default_backend_boundary.ps1'; Text = $boundary },
    [pscustomobject]@{ Path = 'tools\audit_xlir_migration_gaps.ps1'; Text = $audit })) {
    $mandatory = @(Get-XlirAdmissionMandatoryParameters $contract.Path)
    foreach ($parameter in @('PublicDirectory', 'Directory')) {
        if ($parameter -notin $mandatory) {
            throw "$($contract.Path) must require an immutable current-source build and evidence directory"
        }
    }
    if ($contract.Text.Contains('release\xl.exe') -or
        !$contract.Text.Contains('Open-XlirPublicCgen') -or
        !$contract.Text.Contains('Assert-XlirPublicCgenUnchanged')) {
        throw "$($contract.Path) must consume and freeze the reviewed current-source driver"
    }
}
if (!$admission.Contains('Get-XlirContainedRelativePath') -or
    !$audit.Contains('Get-XlirContainedRelativePath')) {
    throw 'Configurable current-source reports must remain inside fresh evidence directories'
}
foreach ($required in @('Invoke-XlirPublicCgenBatch', 'DefaultMatchesXlir',
    'DefaultMismatchCount', 'ArtifactSha256', 'Get-AdmissionDiagnostic',
    'CompilerTimeoutSeconds', 'SuiteInputs', 'SourceInputs', 'SourceHashesMatch')) {
    if (!$admission.Contains($required)) {
        throw "Default admission lost three-way behavior and byte-identity evidence: $required"
    }
}
foreach ($required in @('DefaultEqualsExplicitXlir', 'LegacyIsDistinct',
    'xlir_module_init', 'xlang_main', 'BoundaryInputs')) {
    if (!$boundary.Contains($required)) {
        throw "Default-backend boundary lost a required current-source assertion: $required"
    }
}
foreach ($required in @('MaximumUnexpectedRejections', 'ExpectedXlirRejection',
    'MaximumUnexpectedRejections = 0', 'ExpectedContractViolationCount',
    'ExecutionFailureCount', 'CompilerManifestSha256', 'CompilerTimeoutSeconds',
    'SourceInputs', 'SourceHashesMatch')) {
    if (!$audit.Contains($required)) {
        throw "Migration audit lost classification or provenance evidence: $required"
    }
}

$runner = Read-XlirAdmissionText 'tools\run_xlir_parameterized_guards.ps1'
foreach ($required in @('check_xlir_default_admission.ps1',
    'check_xlir_default_backend_boundary.ps1')) {
    if (!$runner.Contains($required)) {
        throw "Parameterized scenario runner does not own current-source admission: $required"
    }
}

Write-Output 'XLIR current-source admission contract ok: exact backend modes, frozen provenance, batched differential evidence and scenario ownership are enforced.'
exit 0
