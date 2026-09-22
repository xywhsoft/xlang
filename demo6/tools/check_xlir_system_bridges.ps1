param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Captured([string]$File, [string]$Arguments, [int]$Expected = 0, [string]$Marker = "") {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $diagnostic = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne $Expected -or
            ($Expected -eq 0 -and $diagnostic -ne "") -or
            ($Marker -ne "" -and !$diagnostic.Contains($Marker))) {
            throw "System bridge command failed: $File $Arguments exit=$($process.ExitCode) $diagnostic"
        }
    } finally { $process.Dispose() }
}

$contracts = @{
    "dir-list" = @{ Symbol = "xl6_xrt_dir_list"; Result = "var"; Parameters = @("string", "c_bool", "c_int") }
    "dir-glob" = @{ Symbol = "xl6_xrt_dir_glob"; Result = "var"; Parameters = @("string", "string", "c_bool", "c_int") }
    "dir-entries" = @{ Symbol = "xl6_xrt_dir_entries"; Result = "var"; Parameters = @("string", "c_bool", "c_bool", "string", "c_int") }
    "dir-is-empty" = @{ Symbol = "xl6_xrt_dir_is_empty"; Result = "c_bool"; Parameters = @("string") }
    "dir-count" = @{ Symbol = "xl6_xrt_dir_count"; Result = "int"; Parameters = @("string", "c_bool", "c_int") }
    "dir-size" = @{ Symbol = "xl6_xrt_dir_size"; Result = "int"; Parameters = @("string", "c_bool") }
    "process-exec" = @{ Symbol = "xl6_xrt_process_exec"; Result = "point"; Parameters = @("string", "c_int") }
    "process-success" = @{ Symbol = "xl6_xrt_process_success"; Result = "c_bool"; Parameters = @("string", "c_int") }
    "process-exit-code" = @{ Symbol = "xl6_xrt_process_exit_code"; Result = "c_int"; Parameters = @("string", "c_int") }
}
$fixtures = @(
    @{ Name = "xlir_process_exec_bridge"; Bridges = @("process-exec"); Probe = "XLIR_TEST_PROCESS_EXEC" },
    @{ Name = "xlir_process_success_bridge"; Bridges = @("process-success"); Probe = "XLIR_TEST_PROCESS_SUCCESS" },
    @{ Name = "xlir_process_exit_code_bridge"; Bridges = @("process-exit-code"); Probe = "XLIR_TEST_PROCESS_EXIT_CODE" },
    @{ Name = "xlir_dir_list_bridge"; Bridges = @("dir-list") },
    @{ Name = "xlir_dir_glob_bridge"; Bridges = @("dir-glob") },
    @{ Name = "xlir_dir_entries_bridge"; Bridges = @("dir-entries") },
    @{ Name = "xlir_dir_empty_bridge"; Bridges = @("dir-is-empty"); LegacyMissing = "xl6_xrt_dir_is_empty" },
    @{ Name = "xlir_dir_count_bridge"; Bridges = @("dir-count"); LegacyMissing = "xl6_xrt_dir_count" },
    @{ Name = "xlir_dir_size_bridge"; Bridges = @("dir-size"); LegacyMissing = "xl6_xrt_dir_size" },
    @{ Name = "xlir_directory_bridge"; Bridges = @("dir-list", "dir-glob", "dir-entries", "dir-is-empty", "dir-count", "dir-size") },
    @{ Name = "xlir_system_bridge_continuation"; Bridges = @("dir-list", "dir-entries", "process-exec", "process-success", "process-exit-code") }
)
Push-Location $Root
try {
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $source = Join-Path $Root "test\cases\$name.xl"
        $irPath = Join-Path $outputDir "$name.lowered.json"
        $cPath = Join-Path $outputDir "$name.c"
        $repeat = Join-Path $outputDir "$name.repeat.c"
        Invoke-Captured $xl ('ir "{0}" --lowered --format json --emit "{1}"' -f $source,$irPath)
        $module = [System.IO.File]::ReadAllText($irPath) | ConvertFrom-Json
        $types = @{}
        foreach ($type in $module.types) { $types[$type.id] = $type }
        $targets = @($module.callTargets | Where-Object {
            $_.reachability -ne "unreachable" -and $_.runtimeBridgeName -in $contracts.Keys
        })
        if ((($targets.runtimeBridgeName | Sort-Object) -join ',') -cne (($fixture.Bridges | Sort-Object) -join ',')) {
            throw "$name lost operation-level directory/process selection"
        }
        foreach ($target in $targets) {
            $contract = $contracts[$target.runtimeBridgeName]
            if ($target.cName -ne $contract.Symbol -or $target.kind -ne 2 -or
                $target.parameterCount -ne $contract.Parameters.Count -or
                $types[$target.returnType].name -ne $contract.Result -or
                $target.resultPhysicalAbiName -ne "direct" -or
                $target.resultPhysicalFieldDeclaration -ne 0 -or $target.resultPhysicalDestroySymbol -ne 0 -or
                $target.errorAbiName -ne "no-raise" -or $target.errorPhysicalAbiName -ne "none" -or
                $target.variadicAbiName -ne "none") { throw "$name lost exact direct result/error ABI" }
            for ($i = 0; $i -lt $target.parameterCount; $i++) {
                $expectedOwnership = if ($contract.Parameters[$i] -eq "string") { 2 } else { 1 }
                if ($types[$target.parameterTypes[$i]].name -ne $contract.Parameters[$i] -or
                    $target.parameterAbis[$i] -ne $expectedOwnership -or
                    $target.parameterPhysicalAbis[$i] -ne "direct" -or
                    $target.parameterConstraintAbis[$i] -ne "none" -or
                    $target.parameterPhysicalFieldDeclarations[$i] -ne 0) { throw "$name lost borrowed string/scalar parameter ABI" }
            }
        }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath)
        Invoke-Captured $xl ('cgen "{0}" --backend xlir --emit "{1}"' -f $source,$repeat)
        $c = [System.IO.File]::ReadAllText($cPath)
        if ($c -cne [System.IO.File]::ReadAllText($repeat)) { throw "$name C is not deterministic" }
        if ($c -match '<[^>]*(?:xrt|xruntime|xl_runtime)\.h>' -or
            $c -match 'xl6_process_live|xl6_process_command|XL6_PROCESS_RUNTIME') {
            throw "$name includes private headers or the legacy live-process runtime"
        }
        foreach ($bridge in $contracts.Keys) {
            $pattern = '\b' + $contracts[$bridge].Symbol + '\([^;{}]*\)\s*\{'
            $expected = if ($bridge -in $fixture.Bridges) { 1 } else { 0 }
            if ([regex]::Matches($c, $pattern).Count -ne $expected) { throw "$name failed isolated definition emission: $bridge" }
        }
        $strictExe = Join-Path $outputDir "$name.strict.exe"
        $hostFile = "test\cases\xlir_generated_module_host.c"
        $probeFlag = ""
        if ($fixture.Probe) { $hostFile = "test\cases\xlir_process_cleanup_host.c"; $probeFlag = "-D" + $fixture.Probe }
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror {0} -DXLIR_GENERATED_MODULE=\"{1}\" {2} !XL6_CGEN_LINKS! -o "{3}"' -f $probeFlag,$cPath.Replace('\','/'),$hostFile,$strictExe
        & cmd.exe /d /v:on /c $command
        if ($LASTEXITCODE -ne 0) { throw "$name strict standalone C failed" }
        Invoke-Captured $strictExe ""
        Invoke-Captured $xl ('run "{0}" --backend xlir' -f $source)
        $exe = Join-Path $outputDir "$name.xlir.exe"
        Invoke-Captured $xl ('build "{0}" --backend xlir -o "{1}"' -f $source,$exe)
        Invoke-Captured $exe ""
        Write-Output "${name}: exact IR, operation isolation, deterministic strict C and canonical run/build passed"
    }
    foreach ($suffix in @("input", "result", "width", "boolean", "collection", "arity")) {
        $source = Join-Path $Root "test\cases\bad_xlir_direct_bridge_$suffix.xl"
        $badC = Join-Path $outputDir "bad_xlir_direct_bridge_$suffix.c"
        if (Test-Path -LiteralPath $badC) { Remove-Item -LiteralPath $badC }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$badC) 1 "XLIR direct runtime bridge has no exact declared ABI"
        if (Test-Path -LiteralPath $badC) { throw "Malformed direct bridge emitted C: $suffix" }
    }
} finally { Pop-Location }
Write-Output "XLIR system bridges ok: nine exact operations, six ABI negatives, operation isolation, owned process result cleanup including injected timeout/failure, directory metadata/filter/recursion and continuation lifetime, strict C, and canonical run/build contracts."
exit 0
