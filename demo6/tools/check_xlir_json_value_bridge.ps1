param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
$contracts = @(
    @{ Name = "xl6_xrt_json_parse"; Bridge = "json-parse"; Types = "string"; Abis = "2"; Result = "var"; Ownership = 2; Null = 2 },
    @{ Name = "xl6_xrt_json_valid"; Bridge = "json-valid"; Types = "string"; Abis = "2"; Result = "c_bool"; Ownership = 1; Null = 0 },
    @{ Name = "xl6_xrt_json_parse_file"; Bridge = "json-parse-file"; Types = "string"; Abis = "2"; Result = "var"; Ownership = 2; Null = 2 },
    @{ Name = "xl6_xrt_json_stringify"; Bridge = "json-stringify"; Types = "var,c_int"; Abis = "2,1"; Result = "point"; Ownership = 1; Null = 1 },
    @{ Name = "xl6_xrt_json_write_file"; Bridge = "json-write-file"; Types = "string,var,c_int"; Abis = "2,2,1"; Result = "c_bool"; Ownership = 1; Null = 0 }
)

function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw "JSON value bridge compiler command failed: $Arguments" }
}

Push-Location $Root
try {
    foreach ( $fixture in @("xlir_json_value_bridge", "xlir_json_valid_only") ) {
        $source = Join-Path $Root "test\cases\$fixture.xl"
        $jsonPath = Join-Path $outputDir "$fixture.lowered.json"
        $cPath = Join-Path $outputDir "$fixture.c"
        $repeatPath = Join-Path $outputDir "$fixture.repeat.c"
        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $jsonPath)
        $module = [System.IO.File]::ReadAllText($jsonPath) | ConvertFrom-Json
        $types = @{}
        foreach ( $type in $module.types ) { $types[$type.id] = $type }
        $expected = @($contracts | Where-Object {
            $fixture -eq "xlir_json_value_bridge" -or $_.Bridge -eq "json-valid"
        })
        $targets = @($module.callTargets | Where-Object {
            $_.cName -in $contracts.Name -and $_.reachability -ne "unreachable"
        })
        if ( $targets.Count -ne $expected.Count ) { throw "$fixture lost the exact JSON operation set" }
        foreach ( $contract in $expected ) {
            $found = @($targets | Where-Object { $_.cName -eq $contract.Name })
            if ( $found.Count -ne 1 ) { throw "$fixture missing/duplicate JSON target: $($contract.Name)" }
            $target = $found[0]
            $parameters = @($target.parameterTypes | ForEach-Object { $types[$_] })
            $result = $types[$target.returnType]
            if ( $target.kind -ne 2 -or $target.runtimeBridgeName -ne $contract.Bridge -or
                 $target.parameterCount -ne $contract.Types.Split(',').Count -or
                 ($parameters.name -join ',') -cne $contract.Types -or
                 ($target.parameterAbis -join ',') -ne $contract.Abis -or
                 @($target.parameterPhysicalAbis | Where-Object { $_ -ne "direct" }).Count -ne 0 -or
                 @($target.parameterConstraintAbis | Where-Object { $_ -ne "none" }).Count -ne 0 -or
                 @($target.parameterPhysicalFieldDeclarations | Where-Object { $_ -ne 0 }).Count -ne 0 -or
                 $target.resultPhysicalAbiName -ne "direct" -or $target.resultPhysicalFieldDeclaration -ne 0 -or
                 $target.resultPhysicalDestroySymbol -ne 0 -or $target.errorAbiName -ne "no-raise" -or
                 $target.variadicAbiName -ne "none" -or $result.name -cne $contract.Result -or
                 $result.kind -ne 1 -or $result.collectionAbiName -ne "none" -or
                 @($result.params).Count -ne 0 -or $result.ownershipAbi -ne $contract.Ownership -or
                 $result.nullAbi -ne $contract.Null ) {
                throw "$fixture lost exact borrowed-input/owned-result JSON ABI: $($contract.Name)"
            }
        }
        Invoke-Xl @("cgen", $source, "--emit", $cPath)
        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $repeatPath)
        $c = [System.IO.File]::ReadAllText($cPath)
        if ( $c -cne [System.IO.File]::ReadAllText($repeatPath) ) { throw "$fixture is not deterministic" }
        foreach ( $contract in $contracts ) {
            $implementations = [regex]::Matches($c, '(?m)^(?:xvalue\*|void\*|bool) ' + $contract.Name + '\(').Count
            $wanted = [int]($contract.Name -in $expected.Name)
            if ( $implementations -ne $wanted ) { throw "$fixture has wrong emitted operation closure: $($contract.Name)" }
        }
        foreach ( $forbidden in @("xrtJsonWriter", "xrtXson", "xl6_xrt_json_visit", "XL6_Function",
            "<__xl_runtime/xl_runtime.h>", "xrt.h", "xruntime.h") ) {
            if ( $c.Contains($forbidden) ) { throw "$fixture acquired an unrelated or legacy dependency: $forbidden" }
        }
        if ( $fixture -eq "xlir_json_valid_only" -and $c -match 'xrtJson(?:Parse|Stringify)' ) {
            throw "JSON validation-only program pulled in DOM/file serialization"
        }
        $strictExe = Join-Path $outputDir "$fixture.strict.exe"
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED=1 -include "{0}" test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}" && "{1}"' -f $cPath, $strictExe
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) { throw "$fixture strict standalone C/runtime failed" }
        foreach ( $backend in @("xlir", "legacy") ) {
            Invoke-Xl @("run", $source, "--backend", $backend)
            $exe = Join-Path $outputDir "$fixture.$backend.exe"
            Invoke-Xl @("build", $source, "--backend", $backend, "-o", $exe)
            & $exe
            if ( $LASTEXITCODE -ne 0 ) { throw "$fixture $backend executable failed" }
        }
    }
    foreach ( $suffix in @("wrong_input", "wrong_pretty", "wrong_result", "wrong_dom", "missing_pretty") ) {
        $bad = Join-Path $Root "test\cases\bad_xlir_json_value_$suffix.xl"
        $badOutput = Join-Path $outputDir "bad_xlir_json_value_$suffix.c"
        if ( Test-Path -LiteralPath $badOutput ) { Remove-Item -LiteralPath $badOutput }
        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
        $process.StartInfo.FileName = $xl
        $process.StartInfo.Arguments = 'cgen "{0}" --emit "{1}"' -f $bad, $badOutput
        $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true
        $process.StartInfo.RedirectStandardError = $true
        $process.StartInfo.RedirectStandardOutput = $true
        try {
            [void]$process.Start()
            $stdout = $process.StandardOutput.ReadToEndAsync()
            $stderr = $process.StandardError.ReadToEndAsync()
            $process.WaitForExit()
            $diagnostic = $stdout.Result + $stderr.Result
            if ( $process.ExitCode -ne 1 -or (Test-Path -LiteralPath $badOutput) -or
                 !$diagnostic.Contains("XLIR JSON value runtime bridge has no exact parameter/result ABI") ) {
                throw "Malformed JSON value bridge was not rejected precisely: $suffix $diagnostic"
            }
        } finally { $process.Dispose() }
    }
} finally { Pop-Location }

Write-Output "XLIR JSON value bridge ok: five exact reserved ABIs, operation-level shared emission, validation-only closure, five malformed-ABI rejections, deterministic strict standalone C, and XLIR/legacy run/build parity."
exit 0
