param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
$contracts = @(
    @{ Name = "xl6_xrt_xson_parse_options"; Bridge = "xson-parse"; Types = "string,bool,bool,bool"; Abis = "2,1,1,1"; Result = "var"; Ownership = 2; Null = 2 },
    @{ Name = "xl6_xrt_xson_valid_options"; Bridge = "xson-valid"; Types = "string,bool,bool,bool"; Abis = "2,1,1,1"; Result = "bool"; Ownership = 1; Null = 0 },
    @{ Name = "xl6_xrt_xson_read_options"; Bridge = "xson-read"; Types = "string,bool,bool,bool"; Abis = "2,1,1,1"; Result = "var"; Ownership = 2; Null = 2 },
    @{ Name = "xl6_xrt_xson_stringify_options"; Bridge = "xson-stringify"; Types = "var,bool,bool,bool"; Abis = "2,1,1,1"; Result = "point"; Ownership = 1; Null = 1 },
    @{ Name = "xl6_xrt_xson_write_options"; Bridge = "xson-write"; Types = "string,var,bool,bool,bool"; Abis = "2,2,1,1,1"; Result = "bool"; Ownership = 1; Null = 0 }
)

function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw "XSON value bridge compiler command failed: $Arguments" }
}

Push-Location $Root
try {
    foreach ( $fixture in @("xlir_xson_value_bridge", "xlir_xson_valid_only") ) {
        $source = Join-Path $Root "test\cases\$fixture.xl"
        $jsonPath = Join-Path $outputDir "$fixture.lowered.json"
        $cPath = Join-Path $outputDir "$fixture.c"
        $repeatPath = Join-Path $outputDir "$fixture.repeat.c"
        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $jsonPath)
        $module = [System.IO.File]::ReadAllText($jsonPath) | ConvertFrom-Json
        $types = @{}
        foreach ( $type in $module.types ) { $types[$type.id] = $type }
        $expected = @($contracts | Where-Object {
            $fixture -eq "xlir_xson_value_bridge" -or $_.Bridge -eq "xson-valid"
        })
        $targets = @($module.callTargets | Where-Object {
            $_.cName -in $contracts.Name -and $_.reachability -ne "unreachable"
        })
        if ( $targets.Count -ne $expected.Count ) { throw "$fixture lost the exact XSON operation set" }
        foreach ( $contract in $expected ) {
            $found = @($targets | Where-Object { $_.cName -eq $contract.Name })
            if ( $found.Count -ne 1 ) { throw "$fixture missing/duplicate XSON target: $($contract.Name)" }
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
                throw "$fixture lost exact borrowed-input/owned-result XSON ABI: $($contract.Name)"
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
        foreach ( $forbidden in @("xl6_xrt_json_", "xl6_xrt_xson_writer_", "xl6_xrt_xson_visit_options", "XL6_Function",
            "<__xl_runtime/xl_runtime.h>", "xrt.h", "xruntime.h") ) {
            if ( $c.Contains($forbidden) ) { throw "$fixture acquired an unrelated or legacy dependency: $forbidden" }
        }
        if ( $fixture -eq "xlir_xson_valid_only" -and $c -match 'xl6_xrt_xson_(?:parse|stringify|read|write)_options|xl6_xson_write_config' ) {
            throw "XSON validation-only program pulled in DOM/file serialization"
        }
        $strictExe = Join-Path $outputDir "$fixture.strict.exe"
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED=1 -include "{0}" test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}" && "{1}"' -f $cPath, $strictExe
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) { throw "$fixture strict standalone C/runtime failed" }
        foreach ( $backend in @("xlir") ) {
            Invoke-Xl @("run", $source, "--backend", $backend)
            $exe = Join-Path $outputDir "$fixture.$backend.exe"
            Invoke-Xl @("build", $source, "--backend", $backend, "-o", $exe)
            & $exe
            if ( $LASTEXITCODE -ne 0 ) { throw "$fixture $backend executable failed" }
        }
    }
    foreach ( $suffix in @("wrong_input", "wrong_option", "wrong_result", "wrong_dom", "missing_option") ) {
        $bad = Join-Path $Root "test\cases\bad_xlir_xson_$suffix.xl"
        $badOutput = Join-Path $outputDir "bad_xlir_xson_$suffix.c"
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
                 !$diagnostic.Contains("XLIR XSON runtime bridge has no exact parameter/result/callback/handle ABI") ) {
                throw "Malformed XSON value bridge was not rejected precisely: $suffix $diagnostic"
            }
        } finally { $process.Dispose() }
    }
} finally { Pop-Location }

Write-Output "XLIR XSON value bridge ok: five exact reserved ABIs, operation-level shared emission, validation-only closure, five malformed-ABI rejections, deterministic strict standalone C, and canonical run/build parity."
exit 0
