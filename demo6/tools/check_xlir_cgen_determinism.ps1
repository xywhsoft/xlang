param(
    [string]$Root = "",
    [string]$Cc = "gcc",
    [switch]$SkipFixtureBatch
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$compileFlags = @(
    "-std=c11", "-Wall", "-Wextra", "-Werror",
    "-I$outputDir",
    "-Wno-unused-function", "-Wno-unused-label", "-Wno-unused-variable",
    "-Wno-unused-parameter", "-Wno-unused-const-variable",
    "-Wno-missing-field-initializers"
)

function Assert-GeneratedCellExchangeRelease(
    [string]$text, [string]$fixture, [string]$failureMessage) {
    $exchange = [regex]::Match(
        $text, '\(xl6CellRuntimeV1Get\(\)->Exchange\)\(s([0-9]+),')
    if ( ! $exchange.Success ) {
        throw "generated C $fixture has no resident cell replacement"
    }
    $storage = [regex]::Escape("s$($exchange.Groups[1].Value)")
    $tail = $text.Substring($exchange.Index)
    $release = [regex]::Match($tail,
        "(?s)\{\s*xlir_cell\*\s+xlir_released;.*?\{\s*&\(xlir_released\),\s*&\($storage\).*?xlir_cell_release\(xlir_released\)")
    if ( ! $release.Success ) { throw $failureMessage }
}
$expectedLoweringFailures = [ordered]@{
    "xlir_string_bridge_bad_parameter" =
        "XLIR string runtime bridge has no exact parameter/result ABI"
    "xlir_string_bridge_bad_result" =
        "XLIR string runtime bridge has no exact parameter/result ABI"
    "xlir_string_repeat_bad_parameter" =
        "XLIR string runtime bridge has no exact parameter/result ABI"
    "xlir_string_repeat_bad_result" =
        "XLIR string runtime bridge has no exact parameter/result ABI"
    "xlir_code_owner_reserved_export" =
        "XLIR module code owner binding symbol is reserved for the artifact boundary"
    "xlir_code_owner_reserved_import" =
        "XLIR module code owner binding symbol is reserved for the artifact boundary"
}
$explicitProbeFixtures = @(
    "test/cases/cgen_closure_auto_copy_probe.xl",
    "test/cases/cgen_closure_return_capture_probe.xl",
    "test/cases/cgen_closure_return_simple_probe.xl"
)
$fixtures = @(
    @(Get-ChildItem -LiteralPath (Join-Path $Root "test\cases") -File -Filter "xlir_*.xl" |
        Sort-Object Name |
        Where-Object { $_.BaseName -notin $expectedLoweringFailures.Keys } |
        ForEach-Object { $_.BaseName }) + @(
            "cgen_generic_function", "cgen_generic_dedupe", "cgen_runtime_type_baseline",
            "cgen_typed_container_index_assign_objects_functions",
            "cgen_container_ops",
            "cgen_function_direct_owned_args", "cgen_function_default", "cgen_copy_clone",
            "cgen_generic_class_copy_clone", "cgen_class_copy",
            "cgen_class_custom_copy_clone", "cgen_generic_class_custom_copy_clone",
            "cgen_extern_function_value", "cgen_protocol_ops",
            "cgen_protocol_complex_receiver", "cgen_dynamic_container_methods"
        ) + @($explicitProbeFixtures | ForEach-Object {
            [System.IO.Path]::GetFileNameWithoutExtension($_)
        })
)
if ( $fixtures.Count -eq 0 ) {
    throw "XLIR determinism check found no XLIR fixtures"
}

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "XLIR determinism check requires $xl"
}
if ( $null -eq (Get-Command $Cc -ErrorAction SilentlyContinue) ) {
    throw "XLIR determinism check requires C compiler: $Cc"
}
if ( ! (Test-Path -LiteralPath $outputDir -PathType Container) ) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

if ( !$SkipFixtureBatch ) {
$previousPackagePath = $env:XLANG_PACKAGE_PATH
$env:XLANG_PACKAGE_PATH = Join-Path $Root "test\fixtures"
try {
# ABI negatives are verified, not silently excluded from the xlir_* inventory.
# They must fail precisely before producing any C artifact.
foreach ( $entry in $expectedLoweringFailures.GetEnumerator() ) {
    $fixture = $entry.Key
    $expectedDiagnostic = $entry.Value
    $source = Join-Path $Root "test\cases\$fixture.xl"
    $rejectedC = Join-Path $outputDir "$fixture.determinism-rejected.c"
    if ( Test-Path -LiteralPath $rejectedC ) { Remove-Item -LiteralPath $rejectedC }
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $xl
    $process.StartInfo.Arguments = 'cgen "{0}" --backend xlir --emit "{1}"' -f $source, $rejectedC
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        if ( $process.ExitCode -ne 1 -or (Test-Path -LiteralPath $rejectedC) -or
             !($stdout.Result + $stderr.Result).Contains($expectedDiagnostic) ) {
            throw "XLIR Cgen negative is not rejected precisely: $fixture"
        }
    } finally { $process.Dispose() }
}

$fixtureFailures = [System.Collections.Generic.List[string]]::new()
foreach ( $fixture in $fixtures ) {
    $source = Join-Path $Root "test\cases\$fixture.xl"
    $first = Join-Path $outputDir "$fixture.determinism-a.c"
    $second = Join-Path $outputDir "$fixture.determinism-b.c"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        $fixtureFailures.Add("missing fixture: $fixture")
        continue
    }
    Remove-Item -LiteralPath $first, $second -ErrorAction SilentlyContinue
    & $xl cgen $source --backend xlir --emit $first
    if ( $LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $first -PathType Leaf) ) {
        $fixtureFailures.Add("first emission failed: $fixture")
        continue
    }
    & $xl cgen $source --backend xlir --emit $second
    if ( $LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $second -PathType Leaf) ) {
        $fixtureFailures.Add("second emission failed: $fixture")
        continue
    }
    $left = [System.IO.File]::ReadAllBytes($first)
    $right = [System.IO.File]::ReadAllBytes($second)
    if ( $left.Length -ne $right.Length -or
         -not ([System.Linq.Enumerable]::SequenceEqual($left, $right)) ) {
        $fixtureFailures.Add("non-deterministic C: $fixture")
        continue
    }
    $object = Join-Path $outputDir "$fixture.determinism.o"
    Remove-Item -LiteralPath $object -ErrorAction SilentlyContinue
    & $Cc @compileFlags "-c" $first "-o" $object
    if ( $LASTEXITCODE -ne 0 ) {
        $fixtureFailures.Add("strict C compile failed: $fixture")
    }
}
if ( $fixtureFailures.Count -ne 0 ) {
    throw "XLIR Cgen batch found $($fixtureFailures.Count) fixture failures:`n$($fixtureFailures -join "`n")"
}
} finally {
    if ( $null -eq $previousPackagePath ) {
        Remove-Item Env:XLANG_PACKAGE_PATH -ErrorAction SilentlyContinue
    } else {
        $env:XLANG_PACKAGE_PATH = $previousPackagePath
    }
}
}

$loweredFixture = "xlir_xvalue_nested_collection"
$loweredSource = Join-Path $Root "test\cases\$loweredFixture.xl"
$loweredText = Join-Path $outputDir "$loweredFixture.lowered.xlir"
$loweredJson = Join-Path $outputDir "$loweredFixture.lowered.json"

& $xl ir $loweredSource --lowered --emit $loweredText
if ( $LASTEXITCODE -ne 0 ) { throw "verified lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($loweredText, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir stage=lowered", "runtime=xvalue-dynamic-field-get-literal",
    "runtime=xvalue-index-get-xvalue-key", "runtime=xvalue-i64"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR text dump is missing explicit operation ABI: $required"
    }
}

& $xl ir $loweredSource --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "verified lowered XLIR JSON emission failed" }
$json = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( $json.stage -ne "lowered" -or
     @($json.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-dynamic-field-get-literal"
     }).Count -ne 1 -or
     @($json.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-index-get-xvalue-key" -and
        $_.operandCount -eq 2
     }).Count -ne 1 -or
     @($json.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-i64"
     }).Count -lt 1 ) {
    throw "lowered XLIR JSON dump has no explicit runtime operation ABI"
}

$genericFixture = "cgen_generic_function"
$genericSource = Join-Path $Root "test\cases\$genericFixture.xl"
$genericText = Join-Path $outputDir "$genericFixture.lowered.xlir"
$genericJson = Join-Path $outputDir "$genericFixture.lowered.json"

& $xl ir $genericSource --lowered --emit $genericText
if ( $LASTEXITCODE -ne 0 ) { throw "verified generic lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($genericText, [System.Text.Encoding]::UTF8)
if ( ! $text.Contains("generic-instance source=") ) {
    throw "lowered XLIR text dump has no generic specialization instance ABI"
}

& $xl ir $genericSource --lowered --format json --emit $genericJson
if ( $LASTEXITCODE -ne 0 ) { throw "verified generic lowered XLIR JSON emission failed" }
$json = [System.IO.File]::ReadAllText($genericJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( $null -eq $json.genericSpecializations -or $json.genericSpecializations.Count -ne 2 -or
     $json.genericSpecializations[0].symbol -eq $json.genericSpecializations[1].symbol -or
     $json.genericSpecializations[0].declaration -eq $json.genericSpecializations[1].declaration ) {
    throw "lowered XLIR generic specialization table is incomplete or ambiguous"
}

$dedupeSource = Join-Path $Root "test\cases\cgen_generic_dedupe.xl"
$dedupeJson = Join-Path $outputDir "cgen_generic_dedupe.lowered.json"
& $xl ir $dedupeSource --lowered --format json --emit $dedupeJson
if ( $LASTEXITCODE -ne 0 ) { throw "verified generic dedupe lowered XLIR JSON emission failed" }
$json = [System.IO.File]::ReadAllText($dedupeJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( $null -eq $json.genericSpecializations -or $json.genericSpecializations.Count -ne 1 ) {
    throw "lowered XLIR generic specialization table did not dedupe explicit and inferred calls"
}

$closureFixture = "xlir_function_value"
$closureSource = Join-Path $Root "test\cases\$closureFixture.xl"
$closureText = Join-Path $outputDir "$closureFixture.lowered.xlir"
& $xl ir $closureSource --lowered --emit $closureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($closureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("closure-make", "closure-call", "xlir stage=lowered") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR closure dump is missing an explicit closure ABI: $required"
    }
}

$captureFixture = "xlir_closure_capture"
$captureSource = Join-Path $Root "test\cases\$captureFixture.xl"
$captureText = Join-Path $outputDir "$captureFixture.lowered.xlir"
$captureGenerated = Join-Path $outputDir "$captureFixture.determinism-a.c"
& $xl ir $captureSource --lowered --emit $captureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($captureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("closure-make", "closure-call", "kind=capture") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR capture closure dump is missing explicit environment ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($captureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_closure_env_", "environment)->c0",
    "XL6_CLOSURE_CAPTURE_TRIVIAL", "XL6_ClosureEnvironmentV1",
    "xlir_closure_env_drop(&xlir_closure_layout_"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C capture closure is missing its lowered environment ABI: $required"
    }
}

$localClosureFixture = "xlir_closure_local_capture"
$localClosureSource = Join-Path $Root "test\cases\$localClosureFixture.xl"
$localClosureText = Join-Path $outputDir "$localClosureFixture.lowered.xlir"
$localClosureGenerated = Join-Path $outputDir "$localClosureFixture.determinism-a.c"
& $xl ir $localClosureSource --lowered --emit $localClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified local capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($localClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("kind=capture", "closure-make", "closure-call") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR local capture closure dump is missing explicit environment ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($localClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_closure_env_", "->c0 = v", "environment)->c0") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C local capture closure is missing its isolated environment ABI: $required"
    }
}

$mutableClosureFixture = "xlir_closure_mutable_capture"
$mutableClosureSource = Join-Path $Root "test\cases\$mutableClosureFixture.xl"
$mutableClosureText = Join-Path $outputDir "$mutableClosureFixture.lowered.xlir"
$mutableClosureGenerated = Join-Path $outputDir "$mutableClosureFixture.determinism-a.c"
& $xl ir $mutableClosureSource --lowered --emit $mutableClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cell-new", "cell-load", "cell-store", "cell-retain", "cell-release") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR mutable capture closure is missing its explicit cell ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($mutableClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef XL6_Cell xlir_cell", "XL6_CLOSURE_CAPTURE_CELL",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_I64",
    "(xl6CellRuntimeV1Get()->Initialize)",
    "(xl6CellRuntimeV1Get()->Load)", "(xl6CellRuntimeV1Get()->Store)",
    "xlir_cell_retain(", "xlir_cell_release("
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C mutable capture closure is missing its explicit cell ABI: $required"
    }
}

$mutableParameterClosureFixture = "xlir_closure_mutable_parameter_capture"
$mutableParameterClosureSource = Join-Path $Root "test\cases\$mutableParameterClosureFixture.xl"
$mutableParameterClosureText = Join-Path $outputDir "$mutableParameterClosureFixture.lowered.xlir"
$mutableParameterClosureGenerated = Join-Path $outputDir "$mutableParameterClosureFixture.determinism-a.c"
& $xl ir $mutableParameterClosureSource --lowered --emit $mutableParameterClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable parameter capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableParameterClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cell-new", "cell-retain", "cell-store", "cell-release") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR mutable parameter capture is missing its promotion ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($mutableParameterClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef XL6_Cell xlir_cell", "XL6_CLOSURE_CAPTURE_CELL",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_I64",
    "(xl6CellRuntimeV1Get()->Initialize)",
    "xlir_cell_retain(s", "xlir_cell_release(xlir_released)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C mutable parameter capture is missing its promotion ABI: $required"
    }
}

$mutableStringClosureFixture = "xlir_closure_mutable_string_capture"
$mutableStringClosureSource = Join-Path $Root "test\cases\$mutableStringClosureFixture.xl"
$mutableStringClosureText = Join-Path $outputDir "$mutableStringClosureFixture.lowered.xlir"
$mutableStringClosureGenerated = Join-Path $outputDir "$mutableStringClosureFixture.determinism-a.c"
& $xl ir $mutableStringClosureSource --lowered --emit $mutableStringClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable string capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableStringClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cell-new", "cell-exchange", "cell-release", "runtime=string-copy") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR mutable string capture is missing its owned cell ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($mutableStringClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef XL6_Cell xlir_cell", "XL6_CLOSURE_CAPTURE_CELL",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_STRING",
    "(xl6CellRuntimeV1Get()->Initialize)",
    "(xl6CellRuntimeV1Get()->Exchange)", "xrtFree(xlir_released)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C mutable string capture is missing its element ownership ABI: $required"
    }
}

$mutableStringParameterClosureFixture = "xlir_closure_mutable_string_parameter_capture"
$mutableStringParameterClosureSource = Join-Path $Root "test\cases\$mutableStringParameterClosureFixture.xl"
$mutableStringParameterClosureText = Join-Path $outputDir "$mutableStringParameterClosureFixture.lowered.xlir"
$mutableStringParameterClosureGenerated = Join-Path $outputDir "$mutableStringParameterClosureFixture.determinism-a.c"
& $xl ir $mutableStringParameterClosureSource --lowered --emit $mutableStringParameterClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable string parameter capture lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableStringParameterClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cell-new", "runtime=string-copy", "cell-retain", "cell-release") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR mutable string parameter capture is missing its promotion ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($mutableStringParameterClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef XL6_Cell xlir_cell", "XL6_CLOSURE_CAPTURE_CELL",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_STRING",
    "(xl6CellRuntimeV1Get()->Initialize)",
    "xlir_cell_retain(s", "xlir_cell_release(xlir_released)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C mutable string parameter capture is missing its ownership ABI: $required"
    }
}

$mutableXValueClosureFixture = "xlir_closure_mutable_xvalue_capture"
$mutableXValueClosureSource = Join-Path $Root "test\cases\$mutableXValueClosureFixture.xl"
$mutableXValueClosureText = Join-Path $outputDir "$mutableXValueClosureFixture.lowered.xlir"
$mutableXValueClosureGenerated = Join-Path $outputDir "$mutableXValueClosureFixture.determinism-a.c"
& $xl ir $mutableXValueClosureSource --lowered --emit $mutableXValueClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable xvalue capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableXValueClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cell-new", "cell-exchange", "cell-load", "cell-release", "runtime=xvalue-retain") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR mutable xvalue capture is missing its owned cell ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($mutableXValueClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef XL6_Cell xlir_cell", "XL6_CLOSURE_CAPTURE_CELL",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_XVALUE",
    "(xl6CellRuntimeV1Get()->Initialize)",
    "(xl6CellRuntimeV1Get()->Exchange)", "xrtValueRelease(xlir_released)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C mutable xvalue capture is missing its element ownership ABI: $required"
    }
}

$mutableXValueParameterClosureFixture = "xlir_closure_mutable_xvalue_parameter_capture"
$mutableXValueParameterClosureSource = Join-Path $Root "test\cases\$mutableXValueParameterClosureFixture.xl"
$mutableXValueParameterClosureText = Join-Path $outputDir "$mutableXValueParameterClosureFixture.lowered.xlir"
$mutableXValueParameterClosureGenerated = Join-Path $outputDir "$mutableXValueParameterClosureFixture.determinism-a.c"
& $xl ir $mutableXValueParameterClosureSource --lowered --emit $mutableXValueParameterClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable xvalue parameter capture lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableXValueParameterClosureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("cell-new", "cell-retain", "cell-release", "runtime=xvalue-retain") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR mutable xvalue parameter capture is missing its promotion ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($mutableXValueParameterClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef XL6_Cell xlir_cell", "XL6_CLOSURE_CAPTURE_CELL",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_XVALUE",
    "(xl6CellRuntimeV1Get()->Initialize)",
    "xlir_cell_retain(s", "xlir_cell_release(xlir_released)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C mutable xvalue parameter capture is missing its ownership ABI: $required"
    }
}

$asyncXValueFixture = "xlir_async_future_xvalue_differential"
$asyncXValueGenerated = Join-Path $outputDir "$asyncXValueFixture.determinism-a.c"
$text = [System.IO.File]::ReadAllText($asyncXValueGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_async_machine_frame_", "xrtValueRetain(a1)", "xlir_xvalue_binary(", "xlir_async_machine_resume_") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async dynamic continuation is missing its verified frame/runtime ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async dynamic continuation fell back to task-pool dispatch"
}

$asyncExternForwardFixture = "xlir_async_future_direct_call"
$asyncExternForwardSource = Join-Path $Root "test\cases\$asyncExternForwardFixture.xl"
$asyncExternForwardText = Join-Path $outputDir "$asyncExternForwardFixture.lowered.xlir"
$asyncExternForwardGenerated = Join-Path $outputDir "$asyncExternForwardFixture.determinism-a.c"
& $xl ir $asyncExternForwardSource --lowered --emit $asyncExternForwardText
if ( $LASTEXITCODE -ne 0 ) { throw "verified async extern-forward lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncExternForwardText, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    'async-machine m0 status=analyzed[^\r\n]*target=2 function=fn1',
    'machine-frame f0 source=storage residence=frame-storage[^\r\n]*destroy=destroy-future',
    'suspension q0[^\r\n]*result=v[0-9]+/t3/copy-i64',
    'i[0-9]+ call opcode=none runtime=none v[0-9]+ type=4 target=1'
) ) {
    if ( $text -notmatch $required ) {
        throw "lowered XLIR async extern-forward is missing its canonical call/machine provenance: $required"
    }
}
$text = [System.IO.File]::ReadAllText($asyncExternForwardGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xlir_result_future xlir_async_start_2(void)",
    "static xfuture* xlir_async_start_raw_2(void)",
    "typedef struct xlir_async_machine_frame_2",
    "static void xlir_async_machine_resume_2_0",
    "xl6ContinuationWatchRegister(&frame->ticket, &frame->watch, source)",
    "xlir_async_machine_frame_free_2"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async extern-forward is missing its verified machine ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") -or
     $text.Contains("{ return xlir_make_i64_future(); }") ) {
    throw "generated C async extern-forward bypassed its verified machine program"
}

$asyncExternCatchFixture = "xlir_async_future_direct_call_catch"
$asyncExternCatchSource = Join-Path $Root "test\cases\$asyncExternCatchFixture.xl"
$asyncExternCatchText = Join-Path $outputDir "$asyncExternCatchFixture.lowered.xlir"
$asyncExternCatchGenerated = Join-Path $outputDir "$asyncExternCatchFixture.determinism-a.c"
& $xl ir $asyncExternCatchSource --lowered --emit $asyncExternCatchText
if ( $LASTEXITCODE -ne 0 ) { throw "verified async extern catch-default lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncExternCatchText, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "async-machine m0 status=analyzed",
    "completion=retire-before-publish", "source-lease=1 target=2 function=fn1",
    "suspension q0", "copy-i64", "error-take", "immediate=7LL"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR async extern catch-default is missing its canonical recovery CFG: $required"
    }
}
$text = [System.IO.File]::ReadAllText($asyncExternCatchGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_async_machine_resume_2_0",
    "xl6PayloadObserveOutcome(input->State, input->Error)",
    "xrtTakeError()",
    "xl6ContinuationWatchRegister(&frame->ticket, &frame->watch, source)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async extern catch-default is missing its verified all-state ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async extern catch-default fell back to task-pool dispatch"
}

$asyncCarriedFixture = "xlir_async_future_three_awaits"
$asyncCarriedSource = Join-Path $Root "test\cases\$asyncCarriedFixture.xl"
$asyncCarriedText = Join-Path $outputDir "$asyncCarriedFixture.lowered.xlir"
& $xl ir $asyncCarriedSource --lowered --emit $asyncCarriedText
if ( $LASTEXITCODE -ne 0 ) { throw "verified async carried-resume lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncCarriedText, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "async-machine m0 status=analyzed",
    "suspensions=0..3",
    "machine-frame f0 source=storage residence=launch-parameter",
    "machine-frame f1 source=storage residence=launch-parameter",
    "transfer=retain-future", "destroy=destroy-future",
    "machine-frame f2 source=value residence=ssa-spill",
    "machine-frame f3 source=value residence=ssa-spill",
    "suspension q0 state=0", "suspension q1 state=1", "suspension q2 state=2"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR async carried-resume graph is missing its canonical machine/liveness ABI: $required"
    }
}
if ( $text.Contains("binary-carried-scalar") -or $text.Contains("binary-carried-string") ) {
    throw "lowered XLIR async carried-resume graph regressed to a C-ABI-specific state kind"
}

$asyncExternCaptureFixture = "xlir_async_future_direct_call_capture"
$asyncExternCaptureSource = Join-Path $Root "test\cases\$asyncExternCaptureFixture.xl"
$asyncExternCaptureText = Join-Path $outputDir "$asyncExternCaptureFixture.lowered.xlir"
$asyncExternCaptureGenerated = Join-Path $outputDir "$asyncExternCaptureFixture.determinism-a.c"
& $xl ir $asyncExternCaptureSource --lowered --emit $asyncExternCaptureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified async extern-capture lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncExternCaptureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "async-machine m0 status=analyzed",
    "machine-frame f0 source=storage residence=frame-storage",
    "transfer=none", "destroy=none",
    "machine-frame f1 source=storage residence=frame-storage",
    "destroy=destroy-future", "suspension q0", "binary opcode=add"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR async extern-capture is missing its canonical frame/program ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($asyncExternCaptureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_async_machine_frame_2", "xlir_make_i64_future()", "xlir_async_machine_resume_2_0") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async extern-capture is missing its direct continuation ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async extern-capture fell back to task-pool dispatch"
}

$asyncExternStringCaptureFixture = "xlir_async_future_direct_string_capture"
$asyncExternStringCaptureSource = Join-Path $Root "test\cases\$asyncExternStringCaptureFixture.xl"
$asyncExternStringCaptureText = Join-Path $outputDir "$asyncExternStringCaptureFixture.lowered.xlir"
$asyncExternStringCaptureGenerated = Join-Path $outputDir "$asyncExternStringCaptureFixture.determinism-a.c"
& $xl ir $asyncExternStringCaptureSource --lowered --emit $asyncExternStringCaptureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified async extern-string-capture lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncExternStringCaptureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("async-machine m0 status=analyzed", "machine-frame f0 source=storage residence=frame-storage", "runtime=string-copy", "suspension q0", "destroy=free-string") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR async extern-string-capture is missing its canonical owned machine ABI: $required"
    }
}
$asyncExternStringCaptureJson = Join-Path $outputDir "$asyncExternStringCaptureFixture.machine-contract.json"
& $xl ir $asyncExternStringCaptureSource --lowered --format json --emit $asyncExternStringCaptureJson
if ( $LASTEXITCODE -ne 0 ) { throw "Async extern-string-capture JSON emission failed" }
$captureModule = [IO.File]::ReadAllText($asyncExternStringCaptureJson) | ConvertFrom-Json
if ( @($captureModule.asyncSuspensions).Count -ne 1 ) {
    throw "Async extern-string-capture must preserve its single suspension"
}
$suspension = $captureModule.asyncSuspensions[0]
$await = $captureModule.instructions[$suspension.awaitInstruction]
$resultType = @($captureModule.types | Where-Object { $_.id -eq $suspension.resultType })
if ( $await.kindName -ne "await" -or $suspension.resultValue -eq 0 -or
     $await.result -ne $suspension.resultValue -or $await.type -ne $suspension.resultType -or
     $await.asyncResultAbiName -ne "owned-string" -or $suspension.resultAbiName -ne "owned-string" -or
     $resultType.Count -ne 1 -or $resultType[0].name -ne "string" ) {
    throw "Async suspension must bind the exact owned string result of its await instruction"
}
$text = [System.IO.File]::ReadAllText($asyncExternStringCaptureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_async_machine_frame_2", "xrtStrDup(v", "xlir_make_string_future()", "xlir_async_machine_resume_2_0") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async extern-string-capture is missing its direct continuation ABI: $required"
    }
}
if ( $text -notmatch '(?s)v(\d+)\s*=\s*xrtStrDup\(v\d+\);.{0,800}?char\* xlir_committed\s*=\s*v\1;.{0,500}?\{\s*&\(frame->f0\),\s*&\(xlir_committed\)' ) {
    throw "generated C async extern-string-capture did not materialize the borrowed string before frame ownership"
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async extern-string-capture fell back to task-pool dispatch"
}

$asyncExternXValueCaptureFixture = "xlir_async_future_direct_xvalue_differential"
$asyncExternXValueCaptureSource = Join-Path $Root "test\cases\$asyncExternXValueCaptureFixture.xl"
$asyncExternXValueCaptureText = Join-Path $outputDir "$asyncExternXValueCaptureFixture.lowered.xlir"
$asyncExternXValueCaptureGenerated = Join-Path $outputDir "$asyncExternXValueCaptureFixture.determinism-a.c"
& $xl ir $asyncExternXValueCaptureSource --lowered --emit $asyncExternXValueCaptureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified async extern-dynamic-capture lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncExternXValueCaptureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("async-machine m0 status=analyzed", "machine-frame f0 source=storage residence=frame-storage", "runtime=xvalue-retain", "suspension q0", "retained-xvalue") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR async extern-dynamic-capture is missing its canonical owned machine ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($asyncExternXValueCaptureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_async_machine_frame_", "xrtValueRetain(v", "xlir_make_xvalue_future()", "xlir_async_machine_resume_") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async extern-dynamic-capture is missing its direct continuation ABI: $required"
    }
}
if ( $text -notmatch '(?s)v(\d+)\s*=\s*xrtValueRetain\(v\d+\);.{0,500}?xvalue\* xlir_committed\s*=\s*v\1;.{0,500}?\{\s*&\(frame->f0\),\s*&\(xlir_committed\)' ) {
    throw "generated C async extern-dynamic-capture did not retain the borrowed xvalue before frame ownership"
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async extern-dynamic-capture fell back to task-pool dispatch"
}

$asyncReadyFixture = "xlir_async_task_scalar_params"
$asyncReadyGenerated = Join-Path $outputDir "$asyncReadyFixture.determinism-a.c"
$text = [System.IO.File]::ReadAllText($asyncReadyGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtPromiseCreate(&future, NULL)",
    "*value = xlir_fn_1(a0, a1);",
    "xl6PayloadResolveOwned(promise, value, XL6_FUTURE_PAYLOAD_VALUE)",
    "xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE)(value, NULL)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async ready-Future is missing its verified direct-launch ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") -or $text.Contains("xlir_async_frame_") ) {
    throw "generated C async ready-Future retained a task-pool frame"
}

$asyncReadyErrorFixture = "xlir_async_task_worker_error"
$asyncReadyErrorGenerated = Join-Path $outputDir "$asyncReadyErrorFixture.determinism-a.c"
$text = [System.IO.File]::ReadAllText($asyncReadyErrorGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_result_i64 worker = xlir_fn_1();", "xrtTakeError()", "xrtPromiseReject(promise, error)") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async immediate-error Future is missing its verified result-carrier ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") -or $text.Contains("xlir_async_frame_") ) {
    throw "generated C async immediate-error Future retained a task-pool frame"
}

$asyncXValueLocalParameterFixture = "xlir_async_future_xvalue_local_parameter_differential"
$asyncXValueLocalParameterSource = Join-Path $Root "test\cases\$asyncXValueLocalParameterFixture.xl"
$asyncXValueLocalParameterText = Join-Path $outputDir "$asyncXValueLocalParameterFixture.lowered.xlir"
$asyncXValueLocalParameterGenerated = Join-Path $outputDir "$asyncXValueLocalParameterFixture.determinism-a.c"
& $xl ir $asyncXValueLocalParameterSource --lowered --emit $asyncXValueLocalParameterText
if ( $LASTEXITCODE -ne 0 ) { throw "verified dynamic local-parameter lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($asyncXValueLocalParameterText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("async-machine m0 status=analyzed", "machine-frame f0 source=storage residence=frame-storage", "runtime=xvalue-retain", "suspension q0", "retained-xvalue") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR dynamic local-parameter continuation is missing its canonical retained machine ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($asyncXValueLocalParameterGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_async_machine_frame_", "xrtValueRetain(v",
    "xl6ContinuationWatchRegister(&frame->ticket, &frame->watch, source)"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async dynamic local-parameter continuation is missing its verified state propagation ABI: $required"
    }
}
if ( $text -notmatch '(?s)v(\d+)\s*=\s*xrtValueRetain\(v\d+\);.{0,500}?xvalue\* xlir_committed\s*=\s*v\1;.{0,500}?\{\s*&\(frame->f0\),\s*&\(xlir_committed\)' ) {
    throw "generated C async dynamic local-parameter continuation did not retain its local before frame ownership"
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async dynamic local-parameter continuation fell back to task-pool dispatch"
}

$asyncXValueCaptureChainFixture = "xlir_async_future_xvalue_capture_chain"
$asyncXValueCaptureChainSource = Join-Path $Root "test\cases\$asyncXValueCaptureChainFixture.xl"
$asyncXValueCaptureChainText = Join-Path $outputDir "$asyncXValueCaptureChainFixture.lowered.xlir"
$asyncXValueCaptureChainTextSecond = Join-Path $outputDir "$asyncXValueCaptureChainFixture.lowered-second.xlir"
$asyncXValueCaptureChainJson = Join-Path $outputDir "$asyncXValueCaptureChainFixture.lowered.json"
$asyncXValueCaptureChainJsonSecond = Join-Path $outputDir "$asyncXValueCaptureChainFixture.lowered-second.json"
$asyncXValueCaptureChainGenerated = Join-Path $outputDir "$asyncXValueCaptureChainFixture.determinism-a.c"
& $xl ir $asyncXValueCaptureChainSource --lowered --emit $asyncXValueCaptureChainText
if ( $LASTEXITCODE -ne 0 ) { throw "verified dynamic capture-chain lowered XLIR text emission failed" }
& $xl ir $asyncXValueCaptureChainSource --lowered --emit $asyncXValueCaptureChainTextSecond
if ( $LASTEXITCODE -ne 0 ) { throw "second dynamic capture-chain lowered XLIR text emission failed" }
if ( -not ([System.Linq.Enumerable]::SequenceEqual(
        [System.IO.File]::ReadAllBytes($asyncXValueCaptureChainText),
        [System.IO.File]::ReadAllBytes($asyncXValueCaptureChainTextSecond))) ) {
    throw "lowered XLIR dynamic capture-chain text is not byte-for-byte deterministic"
}
$text = [System.IO.File]::ReadAllText($asyncXValueCaptureChainText, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "async-machine m0 status=analyzed",
    "machine-frame f0 source=storage residence=launch-parameter",
    "machine-frame f1 source=storage residence=launch-parameter",
    "transfer=retain-xvalue", "destroy=release-xvalue",
    "suspension q0", "runtime=xvalue-binary-add"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR dynamic capture chain is missing canonical machine/frame metadata: $required"
    }
}
& $xl ir $asyncXValueCaptureChainSource --lowered --json --emit $asyncXValueCaptureChainJson
if ( $LASTEXITCODE -ne 0 ) { throw "verified dynamic capture-chain lowered XLIR JSON emission failed" }
& $xl ir $asyncXValueCaptureChainSource --lowered --json --emit $asyncXValueCaptureChainJsonSecond
if ( $LASTEXITCODE -ne 0 ) { throw "second dynamic capture-chain lowered XLIR JSON emission failed" }
if ( -not ([System.Linq.Enumerable]::SequenceEqual(
        [System.IO.File]::ReadAllBytes($asyncXValueCaptureChainJson),
        [System.IO.File]::ReadAllBytes($asyncXValueCaptureChainJsonSecond))) ) {
    throw "lowered XLIR dynamic capture-chain JSON is not byte-for-byte deterministic"
}
$json = [System.IO.File]::ReadAllText($asyncXValueCaptureChainJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( $json.asyncMachineCount -ne 1 -or $json.asyncMachineFrameSlotCount -ne 2 -or
     $json.asyncSuspensionCount -ne 1 -or
     $json.asyncMachines[0].statusName -ne "analyzed" -or
     @($json.asyncMachineFrameSlots | Where-Object { $_.transferAbiName -eq "retain-xvalue" -and $_.destroyAbiName -eq "release-xvalue" }).Count -ne 2 -or
     $json.asyncSuspensions[0].resultAbiName -ne "retained-xvalue" ) {
    throw "lowered XLIR JSON dynamic capture chain is missing its canonical owned machine ABI"
}
$text = [System.IO.File]::ReadAllText($asyncXValueCaptureChainGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_async_machine_frame_", "xrtValueRetain(a1)", "xrtValueRetain(a2)", "xlir_xvalue_binary(", "xlir_async_machine_resume_") ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C async dynamic capture chain is missing its ordered retained frame ABI: $required"
    }
}
if ( $text.Contains("xrtTaskSubmit") ) {
    throw "generated C async dynamic capture chain fell back to task-pool dispatch"
}

$mutableXValueFinallyFixture = "xlir_closure_mutable_xvalue_finally_return"
$mutableXValueFinallySource = Join-Path $Root "test\cases\$mutableXValueFinallyFixture.xl"
$mutableXValueFinallyText = Join-Path $outputDir "$mutableXValueFinallyFixture.lowered.xlir"
$mutableXValueFinallyGenerated = Join-Path $outputDir "$mutableXValueFinallyFixture.determinism-a.c"
& $xl ir $mutableXValueFinallySource --lowered --emit $mutableXValueFinallyText
if ( $LASTEXITCODE -ne 0 ) { throw "verified mutable xvalue finally return lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($mutableXValueFinallyText, [System.Text.Encoding]::UTF8)
$storeAt = $text.IndexOf("cell-exchange")
if ( $storeAt -lt 0 ) {
    throw "lowered XLIR return-finally has no cell replacement"
}
$releaseAt = $text.IndexOf("cell-release", $storeAt)
if ( $releaseAt -le $storeAt ) {
    throw "lowered XLIR return-finally did not release the cell after the finally replacement"
}
$text = [System.IO.File]::ReadAllText($mutableXValueFinallyGenerated, [System.Text.Encoding]::UTF8)
Assert-GeneratedCellExchangeRelease $text $mutableXValueFinallyFixture `
    "generated C return-finally did not preserve the delayed cell cleanup order"

foreach ( $fixture in @(
    "xlir_closure_mutable_xvalue_catch_finally_return",
    "xlir_closure_mutable_xvalue_multi_catch_finally_return"
) ) {
    $source = Join-Path $Root "test\cases\$fixture.xl"
    $lowered = Join-Path $outputDir "$fixture.lowered.xlir"
    $generated = Join-Path $outputDir "$fixture.determinism-a.c"
    & $xl ir $source --lowered --emit $lowered
    if ( $LASTEXITCODE -ne 0 ) { throw "verified deferred cell cleanup lowering failed: $fixture" }
    $text = [System.IO.File]::ReadAllText($lowered, [System.Text.Encoding]::UTF8)
    $storeAt = $text.IndexOf("cell-exchange")
    if ( $storeAt -lt 0 ) {
        throw "lowered XLIR $fixture has no cell replacement"
    }
    $releaseAt = $text.IndexOf("cell-release", $storeAt)
    if ( $releaseAt -le $storeAt ) {
        throw "lowered XLIR $fixture did not delay its cell cleanup through finally"
    }
    $text = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    Assert-GeneratedCellExchangeRelease $text $fixture `
        "generated C $fixture did not preserve the delayed cell cleanup order"
}

$closureCaptureFixture = "xlir_closure_closure_capture"
$closureCaptureSource = Join-Path $Root "test\cases\$closureCaptureFixture.xl"
$closureCaptureText = Join-Path $outputDir "$closureCaptureFixture.lowered.xlir"
$closureCaptureGenerated = Join-Path $outputDir "$closureCaptureFixture.determinism-a.c"
& $xl ir $closureCaptureSource --lowered --emit $closureCaptureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified closure-valued capture lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($closureCaptureText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("kind=capture", "closure-make", "closure-call") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR closure-valued capture dump is missing explicit environment ABI: $required"
    }
}
$text = [System.IO.File]::ReadAllText($closureCaptureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_closure_retain(v", "environment)->c0",
    "XL6_CLOSURE_CAPTURE_CLOSURE", "XL6_ClosureEnvironmentV1",
    "xlir_closure_env_drop(&xlir_closure_layout_"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C closure-valued capture is missing its shared ownership ABI: $required"
    }
}

$closureReturnFixture = "xlir_closure_return"
$closureReturnSource = Join-Path $Root "test\cases\$closureReturnFixture.xl"
$closureReturnText = Join-Path $outputDir "$closureReturnFixture.lowered.xlir"
& $xl ir $closureReturnSource --lowered --emit $closureReturnText
if ( $LASTEXITCODE -ne 0 ) { throw "verified closure return lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($closureReturnText, [System.Text.Encoding]::UTF8)
foreach ( $required in @("closure-make", "closure-call", "retain opcode=none") ) {
    if ( ! $text.Contains($required) ) {
        throw "lowered XLIR closure return/copy dump is missing an explicit ownership ABI: $required"
    }
}

$nestedClosureFixture = "xlir_closure_nested_capture"
$nestedClosureSource = Join-Path $Root "test\cases\$nestedClosureFixture.xl"
$nestedClosureText = Join-Path $outputDir "$nestedClosureFixture.lowered.xlir"
$nestedClosureGenerated = Join-Path $outputDir "$nestedClosureFixture.determinism-a.c"
& $xl ir $nestedClosureSource --lowered --emit $nestedClosureText
if ( $LASTEXITCODE -ne 0 ) { throw "verified nested capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($nestedClosureText, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($text, "closure-make").Count -lt 2) -or
     ([regex]::Matches($text, "kind=capture").Count -lt 2) ) {
    throw "lowered XLIR nested closure dump is missing transitive capture frame ABI"
}
$text = [System.IO.File]::ReadAllText($nestedClosureGenerated, [System.Text.Encoding]::UTF8)
if ( [regex]::Matches($text, "typedef struct xlir_closure_env_").Count -lt 2 ) {
    throw "generated C nested closure is missing independent capture environments"
}

$stringClosureFixture = "xlir_closure_string_capture"
$stringClosureSource = Join-Path $Root "test\cases\$stringClosureFixture.xl"
$stringClosureLowered = Join-Path $outputDir "$stringClosureFixture.lowered.xlir"
$stringClosureGenerated = Join-Path $outputDir "$stringClosureFixture.determinism-a.c"
& $xl ir $stringClosureSource --lowered --emit $stringClosureLowered
if ( $LASTEXITCODE -ne 0 ) { throw "verified string capture closure lowered XLIR text emission failed" }
$text = [System.IO.File]::ReadAllText($stringClosureLowered, [System.Text.Encoding]::UTF8)
$borrowedArgument = [regex]::Match($text,
    '(?s)\bstore\b[^\r\n]*\btarget=(?<storage>\d+)\b[^\r\n]*\boperands=\[[^\]]+\][^\r\n]*\r?\n\s+i\d+ load\b[^\r\n]*\bv(?<value>\d+)\b[^\r\n]*\btarget=\k<storage>\b[^\r\n]*\r?\n\s+i\d+ closure-call\b[^\r\n]*\boperands=\[[^\]]*v\k<value>(?:,|\])')
if ( ! $borrowedArgument.Success ) {
    throw "lowered XLIR string closure call has no explicit borrowed-argument storage"
}
$borrowedArgumentReleasePattern = '\brelease\b[^\r\n]*\btarget=' +
    [regex]::Escape($borrowedArgument.Groups['storage'].Value) + '\b'
if ( [regex]::Matches($text, $borrowedArgumentReleasePattern).Count -lt 2 ) {
    throw "lowered XLIR string closure call did not release its materialized borrowed argument"
}
$text = [System.IO.File]::ReadAllText($stringClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtStrDup(v", "environment)->c0",
    "XL6_CLOSURE_CAPTURE_STRING", "XL6_ClosureEnvironmentV1",
    "xlir_closure_env_drop(&xlir_closure_layout_"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C string capture closure is missing its owned environment ABI: $required"
    }
}

$xvalueClosureFixture = "xlir_closure_xvalue_capture"
$xvalueClosureGenerated = Join-Path $outputDir "$xvalueClosureFixture.determinism-a.c"
$text = [System.IO.File]::ReadAllText($xvalueClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueRetain(v", "environment)->c0",
    "XL6_CLOSURE_CAPTURE_XVALUE", "XL6_ClosureEnvironmentV1",
    "xlir_closure_env_drop(&xlir_closure_layout_"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C xvalue capture closure is missing its retained environment ABI: $required"
    }
}

$futureClosureFixture = "xlir_closure_future_capture"
$futureClosureGenerated = Join-Path $outputDir "$futureClosureFixture.determinism-a.c"
$text = [System.IO.File]::ReadAllText($futureClosureGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtFutureRef(v", "environment)->c0",
    "XL6_CLOSURE_CAPTURE_FUTURE", "XL6_ClosureEnvironmentV1",
    "xlir_closure_env_drop(&xlir_closure_layout_"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "generated C future capture closure is missing its retained environment ABI: $required"
    }
}

Write-Output "XLIR Cgen determinism and strict C compile ok: $($fixtures.Count) fixtures."
exit 0
