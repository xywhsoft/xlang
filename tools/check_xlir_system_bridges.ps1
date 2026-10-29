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
    "fs-DIR_LIST" = @{ Symbol = "xl6_xrt_fs_dir_list"; Result = "array"; Parameters = @("string", "bool", "int", "int") }
    "fs-DIR_GLOB" = @{ Symbol = "xl6_xrt_fs_dir_glob"; Result = "array"; Parameters = @("string", "string", "bool", "int") }
    "fs-DIR_ENTRIES" = @{ Symbol = "xl6_xrt_fs_dir_entries"; Result = "array"; Parameters = @("string", "string", "bool", "bool", "int") }
    "fs-DIR_EMPTY" = @{ Symbol = "xl6_xrt_fs_dir_empty"; Result = "bool"; Parameters = @("string") }
    "fs-DIR_STATS" = @{ Symbol = "xl6_xrt_fs_dir_stats"; Result = "WalkStats"; Native = $true; Parameters = @("string", "bool") }
    "fs-DIR_SIZE" = @{ Symbol = "xl6_xrt_fs_dir_size"; Result = "uint64"; Parameters = @("string", "bool") }
}
$fixtures = @(
    @{ Name = "xlir_process_exec_bridge"; Bridges = @(); ManagedProcess = $true },
    @{ Name = "xlir_process_success_bridge"; Bridges = @(); ManagedProcess = $true },
    @{ Name = "xlir_process_exit_code_bridge"; Bridges = @(); ManagedProcess = $true },
    @{ Name = "xlir_dir_list_bridge"; Bridges = @("fs-DIR_LIST") },
    @{ Name = "xlir_dir_glob_bridge"; Bridges = @("fs-DIR_GLOB") },
    @{ Name = "xlir_dir_entries_bridge"; Bridges = @("fs-DIR_ENTRIES") },
    @{ Name = "xlir_dir_empty_bridge"; Bridges = @("fs-DIR_EMPTY") },
    @{ Name = "xlir_dir_count_bridge"; Bridges = @("fs-DIR_STATS") },
    @{ Name = "xlir_dir_size_bridge"; Bridges = @("fs-DIR_SIZE") },
    @{ Name = "xlir_directory_bridge"; Bridges = @("fs-DIR_LIST", "fs-DIR_GLOB", "fs-DIR_ENTRIES", "fs-DIR_EMPTY", "fs-DIR_STATS", "fs-DIR_SIZE") },
    @{ Name = "xlir_system_bridge_continuation"; Bridges = @("fs-DIR_LIST", "fs-DIR_ENTRIES"); ManagedProcess = $true }
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
        if ((($targets.runtimeBridgeName | Sort-Object -Unique) -join ',') -cne (($fixture.Bridges | Sort-Object -Unique) -join ',')) {
            throw "$name lost operation-level directory/process selection"
        }
        foreach ($target in $targets) {
            $contract = $contracts[$target.runtimeBridgeName]
            $kind=if($contract.Native){3}else{2}
            if ($target.cName -ne $contract.Symbol -or $target.kind -ne $kind -or
                $target.parameterCount -ne $contract.Parameters.Count -or
                $types[$target.returnType].name -ne $contract.Result -or
                $target.errorAbiName -ne "may-raise" -or $target.errorPhysicalAbiName -ne "xrt-thread-error" -or
                $target.variadicAbiName -ne "none") { throw "$name lost exact direct result/error ABI" }
            if($contract.Native){
                if($target.resultPhysicalAbiName-ne'native-class-field-take'-or
                    !$target.resultPhysicalFieldDeclaration-or!$target.resultPhysicalDestroySymbol){throw "$name lost owned WalkStats"}
            }elseif($target.resultPhysicalAbiName-ne'direct'-or$target.resultPhysicalFieldDeclaration-or$target.resultPhysicalDestroySymbol){
                throw "$name lost direct result ABI"
            }
            for ($i = 0; $i -lt $target.parameterCount; $i++) {
                $expectedOwnership = if ($contract.Parameters[$i] -eq "string") { 2 } else { 1 }
                if ($types[$target.parameterTypes[$i]].name -ne $contract.Parameters[$i] -or
                    $target.parameterAbis[$i] -ne $expectedOwnership -or
                    $target.parameterPhysicalAbis[$i] -ne "direct" -or
                    $target.parameterConstraintAbis[$i] -ne "none" -or
                    $target.parameterPhysicalFieldDeclarations[$i] -ne 0) { throw "$name lost borrowed string/scalar parameter ABI" }
            }
        }
        if ($fixture.ManagedProcess) {
            $managed = @($module.callTargets | Where-Object {
                $_.reachability -ne "unreachable" -and $_.cName -like "xl6Process*"
            })
            foreach ($symbol in @("xl6ProcessRunV1", "xl6ProcessCommandShellV1", "xl6ProcessRunOptionsCreateV1",
                "xl6ProcessResultDestroyV1", "xl6ProcessCommandDestroyV1", "xl6ProcessRunOptionsDestroyV1")) {
                if ($symbol -notin $managed.cName) { throw "$name lost resident process ownership: $symbol" }
            }
            foreach ($target in $managed) {
                $destroy = $target.cName -match 'DestroyV1$'
                if ($target.runtimeBridgeName -ne "text-value" -or $target.variadicAbiName -ne "none" -or
                    ($destroy -and $target.errorAbiName -ne "no-raise") -or
                    (!$destroy -and ($target.errorAbiName -ne "may-raise" -or $target.errorPhysicalAbiName -ne "xrt-thread-error"))) {
                    throw "$name lost declared checked process ABI"
                }
                if ($target.kind -eq 3 -and ($target.resultPhysicalAbiName -ne "native-class-field-take" -or
                    !$target.resultPhysicalFieldDeclaration -or !$target.resultPhysicalDestroySymbol)) {
                    throw "$name lost native result owner"
                }
            }
        }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath)
        Invoke-Captured $xl ('cgen "{0}" --backend xlir --emit "{1}"' -f $source,$repeat)
        $c = [System.IO.File]::ReadAllText($cPath)
        if ($c -cne [System.IO.File]::ReadAllText($repeat)) { throw "$name C is not deterministic" }
        if ($c -match '<[^>]*(?:xrt|xruntime|xl_runtime)\.h>' -or
            $c -match 'xl6_process_live|xl6_process_command|XL6_PROCESS_RUNTIME|xl6_xrt_process_|\bxrtProcess[A-Za-z]+\(') {
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
        if ($fixture.ManagedProcess) { $hostFile = "test\cases\xlir_process_cleanup_host.c" }
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
        $rejection = switch ($suffix) {
            'result' { 'has no exact compiler-runtime bridge ABI' }
            { $_ -in @('boolean', 'collection', 'arity') } { 'XLIR VFS runtime bridge has no exact resource-owner ABI' }
            default { 'XLIR direct runtime bridge has no exact declared ABI' }
        }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$badC) 1 $rejection
        if (Test-Path -LiteralPath $badC) { throw "Malformed direct bridge emitted C: $suffix" }
    }
} finally { Pop-Location }
Write-Output "XLIR system bridges ok: six directory operations and checked resident process declarations, six ABI negatives, operation isolation, real process module-owned lifetime ledger, directory metadata/filter/recursion and continuation lifetime, strict C, and canonical run/build contracts."
exit 0
