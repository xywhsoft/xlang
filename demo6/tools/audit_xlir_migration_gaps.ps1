param(
    [string]$Root = '',
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$OutputPath = '',
    [int]$Limit = 0,
    # Fail closed by default. Pass -1 explicitly only for a discovery run that
    # is not being used as production admission evidence.
    [ValidateRange(-1, 2147483647)][int]$MaximumUnexpectedRejections = 0,
    [ValidateRange(60, 600)][int]$CompilerTimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_public_cgen.ps1')
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$evidencePath = if ([IO.Path]::IsPathRooted($Directory)) {
    [IO.Path]::GetFullPath($Directory)
} else {
    [IO.Path]::GetFullPath((Join-Path $Root $Directory))
}
if (Test-Path -LiteralPath $evidencePath) {
    throw "Refusing to reuse XLIR migration-audit evidence: $evidencePath"
}
[IO.Directory]::CreateDirectory($evidencePath) | Out-Null
if (!$OutputPath) { $OutputPath = Join-Path $evidencePath 'results.json' }
elseif (![IO.Path]::IsPathRooted($OutputPath)) { $OutputPath = Join-Path $Root $OutputPath }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$null = Get-XlirContainedRelativePath -Base $evidencePath -Path $OutputPath
if (Test-Path -LiteralPath $OutputPath) {
    throw "Refusing to overwrite XLIR migration-audit report: $OutputPath"
}

$session = Open-XlirPublicCgen -Root $Root -PublicDirectory $PublicDirectory
$caseDirectory = Join-Path $Root 'test\cases'
if (!(Test-Path -LiteralPath $caseDirectory -PathType Container)) {
    throw "XLIR migration-audit case directory is missing: $caseDirectory"
}
$expectedXlirRejections = @(
    'bad_xlir_direct_bridge_input',
    'bad_xlir_direct_bridge_result',
    'bad_xlir_direct_bridge_width',
    'bad_xlir_direct_bridge_boolean',
    'bad_xlir_direct_bridge_collection',
    'bad_xlir_direct_bridge_arity',
    'bad_xlir_text_output_wrong_input',
    'bad_xlir_text_output_wrong_result',
    'bad_xlir_text_output_missing_input',
    'bad_xlir_text_output_extra_input',
    'sem_task_all_error_payload_xlir_rejected',
    'sem_channel_recv_captured_target',
    'sem_channel_try_recv_captured_target',
    'cgen_xlir_set_i64_key_reject',
    'cgen_xlir_set_u64_iterator_reject',
    'bad_xlir_borrowed_managed_expression_discard',
    'bad_xlir_base64_text_missing_table',
    'bad_xlir_base64_text_wrong_input',
    'bad_xlir_base64_text_wrong_result',
    'bad_xlir_byte_codec_wrong_input',
    'bad_xlir_byte_codec_missing_table',
    'bad_xlir_byte_codec_wrong_result',
    'bad_xlir_json_value_wrong_input',
    'bad_xlir_json_value_wrong_pretty',
    'bad_xlir_json_value_wrong_result',
    'bad_xlir_json_value_wrong_dom',
    'bad_xlir_json_value_missing_pretty',
    'bad_xlir_json_stream_wrong_receiver',
    'bad_xlir_json_stream_wrong_factory',
    'bad_xlir_json_stream_wrong_writer_arg',
    'bad_xlir_json_stream_wrong_callback',
    'bad_xlir_json_stream_wrong_callback_result',
    'bad_xlir_xson_wrong_input',
    'bad_xlir_xson_wrong_option',
    'bad_xlir_xson_wrong_result',
    'bad_xlir_xson_wrong_dom',
    'bad_xlir_xson_missing_option',
    'bad_xlir_xson_wrong_receiver',
    'bad_xlir_xson_wrong_factory',
    'bad_xlir_xson_wrong_writer_arg',
    'bad_xlir_xson_wrong_callback',
    'bad_xlir_xson_wrong_callback_result',
    'xlir_string_bridge_bad_parameter',
    'xlir_string_bridge_bad_result',
    'xlir_code_owner_reserved_export',
    'xlir_code_owner_reserved_import',
    'xlir_string_repeat_bad_parameter',
    'xlir_string_repeat_bad_result'
)

$allSources = @(Get-ChildItem -LiteralPath $caseDirectory -File -Filter '*.xl' | Sort-Object Name)
$missingExpectedFixtures = @($expectedXlirRejections | Where-Object {
    $_ -notin @($allSources | ForEach-Object { $_.BaseName })
})
if ($missingExpectedFixtures.Count) {
    throw "Expected XLIR rejection fixtures are missing: $($missingExpectedFixtures -join ', ')"
}
$sources = @($allSources)
if ($Limit -gt 0) { $sources = @($sources | Select-Object -First $Limit) }
$sourceSnapshot = @(New-XlirPublicCgenSnapshot -Paths @($sources | ForEach-Object { $_.FullName }))
$results = [System.Collections.Generic.List[object]]::new()
$executionFailures = [System.Collections.Generic.List[object]]::new()
$index = 0
$sourceBatchSize = 8
for ($offset = 0; $offset -lt $sources.Count; $offset += $sourceBatchSize) {
    $sourceBatch = @($sources | Select-Object -Skip $offset -First $sourceBatchSize)
    $requests = [System.Collections.Generic.List[object]]::new()
    foreach ($source in $sourceBatch) {
        $slot = [int]($requests.Count / 2)
        $requests.Add([pscustomobject]@{ Source = $source.FullName; Backend = 'legacy'; Output = (Join-Path $evidencePath "slot$slot-legacy.c") })
        $requests.Add([pscustomobject]@{ Source = $source.FullName; Backend = 'xlir'; Output = (Join-Path $evidencePath "slot$slot-xlir.c") })
    }
    $batch = @(Invoke-XlirPublicCgenBatch -Session $session -Requests $requests.ToArray() `
        -TimeoutSeconds $CompilerTimeoutSeconds)
    if ($batch.Count -ne $sourceBatch.Count * 2) {
        throw 'XLIR migration-audit batch returned an incomplete result set'
    }
    for ($sourceIndex = 0; $sourceIndex -lt $sourceBatch.Count; ++$sourceIndex) {
        ++$index
        $source = $sourceBatch[$sourceIndex]
        $expectedXlirRejection = $source.BaseName -in $expectedXlirRejections
        $legacy = $batch[$sourceIndex * 2]
        $xlir = $batch[$sourceIndex * 2 + 1]
    $sourceHashesMatch = $legacy.SourceSha256 -eq $xlir.SourceSha256
    if (!$sourceHashesMatch) {
        $executionFailures.Add([pscustomobject]@{
            Name = $source.BaseName
            Backend = 'batch'
            Failure = 'legacy and XLIR did not consume the same source revision'
            Diagnostic = ''
        })
    }
    foreach ($invocation in @($legacy, $xlir)) {
        if ($invocation.ExecutionFailure) {
            $executionFailures.Add([pscustomobject]@{
                Name = $source.BaseName
                Backend = $invocation.Backend
                Failure = $invocation.ExecutionFailure
                Diagnostic = $invocation.Output
            })
        }
    }
    $lineNumber = 0
    $provenance = ''
    $reason = ''
    if ($legacy.ExitCode -ne 0) {
        $reason = 'legacy backend rejected fixture'
    } elseif ($xlir.ExitCode -ne 0) {
        $reason = $xlir.Output
        $match = [regex]::Match($xlir.Output,
            '(?m)^(?:.*?):(?<line>[0-9]+):(?<column>[0-9]+):\s*\[(?<provenance>[^\]]+)\]\s*(?<reason>[^\r\n]+)')
        if ($match.Success) {
            $lineNumber = [int]$match.Groups['line'].Value
            $provenance = $match.Groups['provenance'].Value
            $reason = $match.Groups['reason'].Value.Trim()
        }
    }
    $results.Add([pscustomobject]@{
        Name = $source.BaseName
        SourceSha256 = $xlir.SourceSha256
        SourceHashesMatch = $sourceHashesMatch
        Legacy = !$legacy.ExecutionFailure -and $legacy.ExitCode -eq 0
        Xlir = !$xlir.ExecutionFailure -and $xlir.ExitCode -eq 0
        ExpectedXlirRejection = $expectedXlirRejection
        Line = $lineNumber
        Provenance = $provenance
        Reason = $reason
        Diagnostic = if ($xlir.ExitCode -ne 0) { $xlir.Output } else { '' }
        LegacyMilliseconds = $legacy.DurationMilliseconds
        XlirMilliseconds = $xlir.DurationMilliseconds
    })
    if (($index % 100) -eq 0) {
        Write-Output "XLIR current-source migration audit progress: $index / $($sources.Count)"
    }
    }
}

$inputsUnchanged = $true
$inputFailure = ''
try { Assert-XlirPublicCgenUnchanged $session }
catch { $inputsUnchanged = $false; $inputFailure = $_.Exception.Message }
if ($inputsUnchanged) {
    try {
        if ($Limit -le 0) {
            Assert-XlirPublicCgenDirectoryInventoryUnchanged -Directory $caseDirectory `
                -Filter '*.xl' -Snapshot $sourceSnapshot
        } else {
            Assert-XlirPublicCgenSnapshotUnchanged -Snapshot $sourceSnapshot
        }
    } catch {
        $inputsUnchanged = $false
        $inputFailure = $_.Exception.Message
    }
}
$legacyAccepted = @($results | Where-Object Legacy).Count
$xlirAcceptedWithinLegacy = @($results | Where-Object { $_.Legacy -and $_.Xlir }).Count
$unexpectedRejections = @($results | Where-Object {
    $_.Legacy -and !$_.Xlir -and !$_.ExpectedXlirRejection
})
$expectedRejected = @($results | Where-Object {
    $_.Legacy -and !$_.Xlir -and $_.ExpectedXlirRejection
})
$expectedAccepted = @($results | Where-Object {
    $_.Legacy -and $_.Xlir -and $_.ExpectedXlirRejection
})
$expectedContractViolations = @($results | Where-Object {
    $_.ExpectedXlirRejection -and (!$_.Legacy -or $_.Xlir)
})
$groups = @($unexpectedRejections | Group-Object Reason | Sort-Object Count -Descending | ForEach-Object {
    [pscustomobject]@{
        Count = $_.Count
        Reason = $_.Name
        Examples = @($_.Group | Select-Object -First 8 | ForEach-Object Name)
    }
})
$report = [ordered]@{
    SchemaVersion = 3
    GeneratedAt = [DateTime]::UtcNow.ToString('o')
    SourceCount = $sources.Count
    LegacyAccepted = $legacyAccepted
    XlirAcceptedWithinLegacy = $xlirAcceptedWithinLegacy
    UnexpectedXlirRejections = $unexpectedRejections.Count
    ExpectedXlirRejected = $expectedRejected.Count
    ExpectedXlirAccepted = $expectedAccepted.Count
    ExpectedContractViolationCount = $expectedContractViolations.Count
    ExecutionFailureCount = $executionFailures.Count
    CompilerTimeoutSeconds = $CompilerTimeoutSeconds
    InputsUnchanged = $inputsUnchanged
    InputFailure = $inputFailure
    CompilerManifest = $session.ManifestPath
    CompilerManifestSha256 = $session.ManifestSha256
    Driver = $session.Driver
    DriverSha256 = $session.DriverSha256
    CompilerScope = $session.Manifest.Scope
    SourceInputs = @($sourceSnapshot | ForEach-Object {
        [ordered]@{ Path = Get-XlirContainedRelativePath -Base $Root -Path $_.Path; Sha256 = $_.Sha256 }
    })
    Groups = $groups
    ExpectedAcceptedFixtures = @($expectedAccepted | ForEach-Object Name)
    ExpectedContractViolations = @($expectedContractViolations | ForEach-Object {
        [ordered]@{ Name = $_.Name; Legacy = $_.Legacy; Xlir = $_.Xlir; Reason = $_.Reason }
    })
    ExecutionFailures = @($executionFailures.ToArray())
    Fixtures = @($results.ToArray())
}
[IO.Directory]::CreateDirectory((Split-Path -Parent $OutputPath)) | Out-Null
[IO.File]::WriteAllText($OutputPath, ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
Write-Output "XLIR current-source migration audit: sources=$($sources.Count) legacy=$legacyAccepted xlir=$xlirAcceptedWithinLegacy rejected=$($unexpectedRejections.Count) expected-rejected=$($expectedRejected.Count)"
$groups | Select-Object -First 25 | ForEach-Object {
    Write-Output ('{0,4}  {1}' -f $_.Count, $_.Reason)
}
Write-Output "Audit JSON: $OutputPath"

if (!$inputsUnchanged) { throw "XLIR migration-audit evidence was invalidated: $inputFailure" }
if ($executionFailures.Count) { throw "XLIR migration audit encountered $($executionFailures.Count) compiler execution failures" }
if ($expectedContractViolations.Count) {
    throw "XLIR migration audit found $($expectedContractViolations.Count) expected-rejection contract violations"
}
if ($MaximumUnexpectedRejections -ge 0 -and $unexpectedRejections.Count -gt $MaximumUnexpectedRejections) {
    throw "XLIR migration audit found $($unexpectedRejections.Count) unexpected rejections; maximum is $MaximumUnexpectedRejections"
}
exit 0
