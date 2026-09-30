param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
$writerParameters = @{
    create_default = ""; create_pretty = "bool"; destroy = "JsonWriter"
    begin_object = "JsonWriter"; begin_object_key = "JsonWriter,string"; end_object = "JsonWriter"
    begin_array = "JsonWriter"; begin_array_key = "JsonWriter,string"; end_array = "JsonWriter"
    null = "JsonWriter"; null_key = "JsonWriter,string"
    bool = "JsonWriter,bool"; bool_key = "JsonWriter,string,bool"
    int = "JsonWriter,int"; int_key = "JsonWriter,string,int"
    float = "JsonWriter,float"; float_key = "JsonWriter,string,float"
    string = "JsonWriter,string"; string_key = "JsonWriter,string,string"
    value = "JsonWriter,var"; value_key = "JsonWriter,string,var"; finish = "JsonWriter"
}
function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw "JSON stream compiler command failed: $Arguments" }
}

Push-Location $Root
try {
    foreach ( $fixture in @("xlir_json_writer_bridge", "xlir_json_visit_bridge", "xlir_json_visit_capture_lifetime") ) {
        $source = Join-Path $Root "test\cases\$fixture.xl"
        $jsonPath = Join-Path $outputDir "$fixture.lowered.json"
        $cPath = Join-Path $outputDir "$fixture.c"
        $repeatPath = Join-Path $outputDir "$fixture.repeat.c"
        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $jsonPath)
        $module = [System.IO.File]::ReadAllText($jsonPath) | ConvertFrom-Json
        $types = @{}
        foreach ( $type in $module.types ) { $types[$type.id] = $type }
        $targets = @($module.callTargets | Where-Object { $_.reachability -ne "unreachable" })
        $writers = @($targets | Where-Object { $_.cName -like "xl6_xrt_json_writer_*" })
        $visitors = @($targets | Where-Object { $_.cName -eq "xl6_xrt_json_visit" })
        if ( $fixture -eq "xlir_json_writer_bridge" ) {
            if ( $writers.Count -ne $writerParameters.Count -or $visitors.Count -ne 0 ) {
                throw "JSON Writer lost the complete isolated operation set"
            }
            foreach ( $target in $writers ) {
                $operation = $target.cName.Substring("xl6_xrt_json_writer_".Length)
                $parameters = @($target.parameterTypes | ForEach-Object { $types[$_] })
                $result = $types[$target.returnType]
                if ( !$writerParameters.ContainsKey($operation) -or
                     ($parameters.name -join ',') -cne $writerParameters[$operation] -or
                     $target.runtimeBridgeName -ne "json-writer" -or
                     $target.errorAbiName -ne "no-raise" -or $target.variadicAbiName -ne "none" -or
                     @($target.parameterConstraintAbis | Where-Object { $_ -ne "none" }).Count -ne 0 ) {
                    throw "JSON Writer has an inexact component contract: $operation"
                }
                $factory = $operation -in @("create_default", "create_pretty")
                $start = 0
                if ( $factory ) {
                    $destroy = @($targets | Where-Object { $_.symbol -eq $target.resultPhysicalDestroySymbol })
                    if ( $target.kind -ne 3 -or $result.name -ne "JsonWriter" -or
                         $result.cAbi -ne 26 -or $result.ownershipAbi -ne 2 -or $result.nullAbi -ne 2 -or
                         $target.resultPhysicalAbiName -ne "native-class-field-take" -or
                         $target.resultPhysicalFieldDeclaration -eq 0 -or $destroy.Count -ne 1 -or
                         $destroy[0].cName -ne "xl6_xrt_json_writer_destroy" -or
                         $destroy[0].parameterTypes[0] -ne $target.returnType -or
                         $destroy[0].parameterPhysicalFieldDeclarations[0] -ne $target.resultPhysicalFieldDeclaration ) {
                        throw "JSON factory lost its exact owned native field/destroy pair: $operation"
                    }
                } else {
                    $expectedResult = if ( $operation -eq "destroy" ) { "" } elseif ( $operation -eq "finish" ) { "point" } else { "bool" }
                    if ( $target.kind -ne 2 -or $target.resultPhysicalAbiName -ne "direct" -or
                         $target.resultPhysicalDestroySymbol -ne 0 -or $target.resultPhysicalFieldDeclaration -ne 0 -or
                         [string]$result.name -cne $expectedResult -or
                         $target.parameterAbis[0] -ne 2 -or $target.parameterPhysicalAbis[0] -ne "native-class-handle-borrow" -or
                         $target.parameterPhysicalFieldDeclarations[0] -eq 0 ) {
                        throw "JSON method lost borrowed native receiver/result ABI: $operation"
                    }
                    $start = 1
                }
                for ( $i = $start; $i -lt $target.parameterCount; $i++ ) {
                    $ownership = if ( $parameters[$i].name -in @("string", "var") ) { 2 } else { 1 }
                    if ( $target.parameterAbis[$i] -ne $ownership -or $target.parameterPhysicalAbis[$i] -ne "direct" -or
                         $target.parameterPhysicalFieldDeclarations[$i] -ne 0 ) {
                        throw "JSON Writer payload borrowing/physical ABI mismatch: $operation $i"
                    }
                }
            }
        } else {
            if ( $writers.Count -ne 0 -or $visitors.Count -ne 1 ) { throw "JSON visitor pulled in Writer or lost its target" }
            $target = $visitors[0]
            $signature = $types[$target.parameterTypes[1]]
            if ( $target.runtimeBridgeName -ne "json-visit" -or $target.kind -ne 2 -or $target.parameterCount -ne 2 -or
                 $target.errorAbiName -ne "may-raise" -or $target.errorPhysicalAbiName -ne "result-carrier" -or
                 ($target.parameterAbis -join ',') -ne "2,2" -or
                 ($target.parameterPhysicalAbis -join ',') -ne "direct,language-function-value" -or
                 $types[$target.returnType].name -ne "c_int" -or $signature.kind -ne 4 -or
                 @($signature.params).Count -ne 1 -or $types[$signature.params[0]].name -ne "var" -or
                 ($signature.parameterAbis -join ',') -ne "borrow" -or
                 $types[$signature.callableResultType].name -ne "bool" ) {
                throw "JSON visitor lost its exact borrowed var -> bool callback ABI"
            }
        }
        Invoke-Xl @("cgen", $source, "--emit", $cPath)
        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $repeatPath)
        $c = [System.IO.File]::ReadAllText($cPath)
        if ( $c -cne [System.IO.File]::ReadAllText($repeatPath) ) { throw "$fixture is not deterministic" }
        if ( !$c.Contains("<__xl_runtime/xruntime_decl.h>") -or
             $c -match 'xrtXlon|xl6_xrt_xlon|xl6_function_call_var|<__xl_runtime/xl_runtime.h>|<[^>]*(?:xrt|xruntime)\.h>' ) {
            throw "$fixture lost its declaration-only, backend-neutral runtime boundary"
        }
        if ( ($fixture -eq "xlir_json_writer_bridge" -and $c.Contains("xl6_xrt_json_visit")) -or
             ($fixture -ne "xlir_json_writer_bridge" -and $c.Contains("xl6_xrt_json_writer_")) ) {
            throw "$fixture lost Writer/visitor component isolation"
        }
        $strictExe = Join-Path $outputDir "$fixture.strict.exe"
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"{0}\" test\cases\xlir_generated_module_host.c test\cases\xlir_json_stream_lifetime_host.c !XL6_CGEN_LINKS! -o "{1}" && "{1}"' -f $cPath.Replace('\', '/'), $strictExe
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) { throw "$fixture strict standalone C/runtime failed" }
        # The weak-reference reclamation assertion belongs to the canonical
        # XLIR ownership contract and runs through both CLI execution modes.
        $backends = @("xlir")
        foreach ( $backend in $backends ) {
            Invoke-Xl @("run", $source, "--backend", $backend, "--link", "test/cases/xlir_json_stream_lifetime_host.c")
            $exe = Join-Path $outputDir "$fixture.$backend.exe"
            Invoke-Xl @("build", $source, "--backend", $backend, "--link", "test/cases/xlir_json_stream_lifetime_host.c", "-o", $exe)
            & $exe
            if ( $LASTEXITCODE -ne 0 ) { throw "$fixture $backend executable failed" }
        }
    }
    foreach ( $suffix in @("wrong_receiver", "wrong_factory", "wrong_writer_arg", "wrong_callback", "wrong_callback_result") ) {
        $source = Join-Path $Root "test\cases\bad_xlir_json_stream_$suffix.xl"
        $badC = Join-Path $outputDir "bad_xlir_json_stream_$suffix.c"
        if ( Test-Path -LiteralPath $badC ) { Remove-Item -LiteralPath $badC }
        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
        $process.StartInfo.FileName = $xl
        $process.StartInfo.Arguments = 'cgen "{0}" --emit "{1}"' -f $source, $badC
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
            if ( $process.ExitCode -ne 1 -or (Test-Path -LiteralPath $badC) -or
                 !$diagnostic.Contains("XLIR JSON stream runtime bridge has no exact callback/handle ABI") ) {
                throw "Malformed JSON stream bridge was not rejected precisely: $suffix $diagnostic"
            }
        } finally { $process.Dispose() }
    }
} finally { Pop-Location }
Write-Output "XLIR JSON stream bridge ok: exact Writer/callback contracts, isolated shared components, weak-reference event/native-wrapper lifetime probes, five ABI negatives, deterministic strict standalone C, canonical event/Writer run/build parity, and temporary capture reclamation."
exit 0
