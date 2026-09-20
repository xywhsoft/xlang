param(
    [string]$Root = "",
    [string]$OutputPath = "",
    [int]$Limit = 0
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path
if ( $OutputPath -eq "" ) {
    $OutputPath = Join-Path $Root "build\tmp\xlir_migration_gap_audit.json"
}

$xl = Join-Path $Root "release\xl.exe"
$xlc = Join-Path $Root "release\xlc.dll"
$caseDir = Join-Path $Root "test\cases"
$tmpDir = Join-Path $Root "build\tmp"
$legacyC = Join-Path $tmpDir "xlir_gap_legacy_probe.c"
$xlirC = Join-Path $tmpDir "xlir_gap_xlir_probe.c"
$expectedXlirRejections = @(
    "bad_xlir_direct_bridge_input",
    "bad_xlir_direct_bridge_result",
    "bad_xlir_direct_bridge_width",
    "bad_xlir_direct_bridge_boolean",
    "bad_xlir_direct_bridge_collection",
    "bad_xlir_direct_bridge_arity",
    "bad_xlir_text_output_wrong_input",
    "bad_xlir_text_output_wrong_result",
    "bad_xlir_text_output_missing_input",
    "bad_xlir_text_output_extra_input",
    "sem_task_all_error_payload_xlir_rejected",
    "sem_channel_recv_captured_target",
    "sem_channel_try_recv_captured_target",
    "cgen_xlir_set_i64_key_reject",
    "cgen_xlir_set_u64_iterator_reject",
    "bad_xlir_borrowed_managed_expression_discard",
    "bad_xlir_base64_text_missing_table",
    "bad_xlir_base64_text_wrong_input",
    "bad_xlir_base64_text_wrong_result",
    "bad_xlir_byte_codec_wrong_input",
    "bad_xlir_byte_codec_missing_table",
    "bad_xlir_byte_codec_wrong_result",
    "bad_xlir_json_value_wrong_input",
    "bad_xlir_json_value_wrong_pretty",
    "bad_xlir_json_value_wrong_result",
    "bad_xlir_json_value_wrong_dom",
    "bad_xlir_json_value_missing_pretty",
    "bad_xlir_json_stream_wrong_receiver",
    "bad_xlir_json_stream_wrong_factory",
    "bad_xlir_json_stream_wrong_writer_arg",
    "bad_xlir_json_stream_wrong_callback",
    "bad_xlir_json_stream_wrong_callback_result",
    "bad_xlir_xson_wrong_input",
    "bad_xlir_xson_wrong_option",
    "bad_xlir_xson_wrong_result",
    "bad_xlir_xson_wrong_dom",
    "bad_xlir_xson_missing_option",
    "bad_xlir_xson_wrong_receiver",
    "bad_xlir_xson_wrong_factory",
    "bad_xlir_xson_wrong_writer_arg",
    "bad_xlir_xson_wrong_callback",
    "bad_xlir_xson_wrong_callback_result",
    "xlir_string_bridge_bad_parameter",
    "xlir_string_bridge_bad_result"
)
foreach ( $path in @($xl, $xlc, $caseDir) ) {
    if ( ! (Test-Path -LiteralPath $path) ) {
        throw "XLIR migration audit input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
$compilerHashes = @((Get-FileHash -LiteralPath $xl -Algorithm SHA256).Hash,
    (Get-FileHash -LiteralPath $xlc -Algorithm SHA256).Hash)

function Invoke-XlCgen([string]$sourcePath, [string]$backend, [string]$emitPath)
{
    # Capture native streams directly.  Windows PowerShell otherwise wraps
    # stderr into display-width-dependent ErrorRecord text, corrupting the
    # canonical first-failure grouping used by this audit.
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $xl
    $process.StartInfo.WorkingDirectory = $Root
    $process.StartInfo.EnvironmentVariables["XLANG_PACKAGE_PATH"] =
        (Join-Path $Root "release\extlibs") + ";" + (Join-Path $Root "test\fixtures")
    $process.StartInfo.Arguments = 'cgen "{0}" --backend {1} --emit "{2}"' -f `
        $sourcePath.Replace('"', '\"'), $backend, $emitPath.Replace('"', '\"')
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    if ( Test-Path -LiteralPath $emitPath ) { Remove-Item -LiteralPath $emitPath }
    try {
        if ( ! $process.Start() ) { throw "failed to start XLIR migration audit compiler" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $text = ($stdout.Result + $stderr.Result).Trim()
        $code = $process.ExitCode
        if ( $code -notin @(0, 1) -or ($code -eq 1 -and $text -eq "") ) {
            throw "Migration audit compiler execution failed (exit=$code, backend=$backend): $sourcePath $text"
        }
        if ( $code -eq 0 -and (!(Test-Path -LiteralPath $emitPath) -or
                (Get-Item -LiteralPath $emitPath).Length -eq 0) ) {
            throw "Migration audit compiler succeeded without an artifact: $sourcePath"
        }
        return [pscustomobject]@{ exitCode = $code; output = $text }
    } finally {
        $process.Dispose()
    }
}

$sources = @(Get-ChildItem -LiteralPath $caseDir -File -Filter "*.xl" | Sort-Object Name)
if ( $Limit -gt 0 ) { $sources = @($sources | Select-Object -First $Limit) }
$results = New-Object System.Collections.Generic.List[object]
$index = 0
foreach ( $source in $sources ) {
    ++$index
    if ( ($index % 100) -eq 0 ) {
        Write-Output "XLIR migration audit progress: $index / $($sources.Count)"
    }
    $expectedXlirRejection = $source.BaseName -in $expectedXlirRejections
    $legacyRun = Invoke-XlCgen $source.FullName "legacy" $legacyC
    $legacyOutput = $legacyRun.output
    $legacyExit = $legacyRun.exitCode
    if ( $legacyExit -ne 0 ) {
        $results.Add([pscustomobject]@{
            name = $source.BaseName
            legacy = $false
            xlir = $false
            expectedXlirRejection = $expectedXlirRejection
            line = 0
            provenance = ""
            reason = "legacy backend rejected fixture"
            diagnostic = $legacyOutput
        })
        continue
    }

    $xlirRun = Invoke-XlCgen $source.FullName "xlir" $xlirC
    $xlirOutput = $xlirRun.output
    $xlirExit = $xlirRun.exitCode
    if ( $xlirExit -eq 0 ) {
        $results.Add([pscustomobject]@{
            name = $source.BaseName
            legacy = $true
            xlir = $true
            expectedXlirRejection = $expectedXlirRejection
            line = 0
            provenance = ""
            reason = ""
            diagnostic = ""
        })
        continue
    }

    $lineNumber = 0
    $provenance = ""
    $reason = $xlirOutput
    $match = [regex]::Match(
        $xlirOutput, '(?m)^(?:.*?):(?<line>[0-9]+):(?<column>[0-9]+):\s*\[(?<provenance>[^\]]+)\]\s*(?<reason>[^\r\n]+)')
    if ( $match.Success ) {
        $lineNumber = [int]$match.Groups['line'].Value
        $provenance = $match.Groups['provenance'].Value
        $reason = $match.Groups['reason'].Value.Trim()
    }
    $results.Add([pscustomobject]@{
        name = $source.BaseName
        legacy = $true
        xlir = $false
        expectedXlirRejection = $expectedXlirRejection
        line = $lineNumber
        provenance = $provenance
        reason = $reason
        diagnostic = $xlirOutput
    })
    if ( ($index % 100) -eq 0 ) {
        Write-Progress -Activity "Auditing XLIR migration" -Status "$index / $($sources.Count)" -PercentComplete (($index * 100) / $sources.Count)
    }
}
Write-Progress -Activity "Auditing XLIR migration" -Completed

$legacyOk = @($results | Where-Object { $_.legacy }).Count
$xlirOk = @($results | Where-Object { $_.legacy -and $_.xlir }).Count
$xlirFailed = @($results | Where-Object {
    $_.legacy -and ! $_.xlir -and ! $_.expectedXlirRejection
})
$expectedXlirRejected = @($results | Where-Object {
    $_.legacy -and ! $_.xlir -and $_.expectedXlirRejection
})
$groups = @($xlirFailed | Group-Object reason | Sort-Object Count -Descending | ForEach-Object {
    [pscustomobject]@{
        count = $_.Count
        reason = $_.Name
        examples = @($_.Group | Select-Object -First 8 | ForEach-Object { $_.name })
    }
})
$report = [ordered]@{
    generatedAt = [DateTime]::UtcNow.ToString("o")
    sourceCount = $sources.Count
    legacyAccepted = $legacyOk
    xlirAcceptedWithinLegacy = $xlirOk
    xlirRejectedWithinLegacy = $xlirFailed.Count
    expectedXlirRejected = $expectedXlirRejected.Count
    compilerStable = ((Get-FileHash -LiteralPath $xl -Algorithm SHA256).Hash -eq
        $compilerHashes[0] -and (Get-FileHash -LiteralPath $xlc -Algorithm SHA256).Hash -eq
        $compilerHashes[1])
    compilerSha256 = $compilerHashes[0]
    compilerDllSha256 = $compilerHashes[1]
    groups = $groups
    fixtures = @($results | ForEach-Object { $_ })
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
if ( ! $report.compilerStable ) {
    throw "Migration audit compiler changed during execution; report is invalid"
}

Write-Output "XLIR migration audit: sources=$($sources.Count) legacy=$legacyOk xlir=$xlirOk rejected=$($xlirFailed.Count) expected-rejected=$($expectedXlirRejected.Count)"
$groups | Select-Object -First 25 | ForEach-Object {
    Write-Output ("{0,4}  {1}" -f $_.count, $_.reason)
}
Write-Output "Audit JSON: $OutputPath"
exit 0
