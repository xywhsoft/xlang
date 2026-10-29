param([string]$Directory = '')
$ErrorActionPreference = 'Stop'
$rootPath = Split-Path -Parent $PSScriptRoot
if (!$Directory) { $Directory = 'build/test/logging-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
$outputPath = if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) }
    else { [IO.Path]::GetFullPath((Join-Path $rootPath $Directory)) }
if (Test-Path -LiteralPath $outputPath) { throw 'Choose a fresh logging evidence directory' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$temporaryPath = Join-Path $outputPath 'tmp'
New-Item -ItemType Directory -Path $temporaryPath | Out-Null
$xl = Join-Path $rootPath 'release/xl.exe'
$compiler = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$rows = [Collections.Generic.List[object]]::new()

function Invoke-Checked([string]$Name, [string]$File, [string]$Arguments, [int]$Expected = 0,
    [string]$Marker = '', [string]$ExpectedOutput = '', [switch]$CheckOutput) {
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = [Diagnostics.ProcessStartInfo]::new()
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.WorkingDirectory = $rootPath
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(45000)) { $process.Kill(); $process.WaitForExit(); throw "Timeout: $Name" }
        $text = $stdout.Result; $errors = $stderr.Result
        [IO.File]::WriteAllText("$outputPath/$Name.out.log", $text, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText("$outputPath/$Name.err.log", $errors, [Text.UTF8Encoding]::new($false))
        if ($process.ExitCode -ne $Expected -or ($Marker -and !($text + $errors).Contains($Marker)) -or
            ($CheckOutput -and $text.Replace("`r`n", "`n") -cne $ExpectedOutput)) {
            throw "Failed: $Name exit=$($process.ExitCode) $text $errors"
        }
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$process.ExitCode;Expected=$Expected})
    } finally { $process.Dispose() }
}

$savedEnvironment = @{}
Push-Location $rootPath
try {
    foreach ($key in @('TMP','TEMP')) {
        $savedEnvironment[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key,$temporaryPath,'Process')
    }
    # Freeze the toolchain and the project-owned source inputs before one batch.
    $files = @(@(Get-ChildItem src -Recurse -File) + @(Get-ChildItem test/cases/stdlib_log* -File) +
        @(Get-ChildItem test/cases/bad_log_printf* -File) + @(Get-Item release/xl.exe,release/xl.dll,release/xlc.dll,
        test/cases/xlir_log_adapter_contract.c,test/cases/bad_xlir_log_input.xl,test/cases/bad_xlir_log_result.xl,
        release/examples/stdlib_logging.xl,release/examples/stdlib_log.xl,release/examples/stdlib_log_formats.xl,release/examples/stdlib_log_c.c,
        test/support/log_selected_native.c,lib/xruntime/xruntime.h,tools/check_stdlib_coverage.ps1,$compiler,$PSCommandPath)) | Sort-Object FullName -Unique
    $inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    # This gate links the actual DLL ABI, so it needs declarations, not the two
    # full-product XRT objects prepared by the general suite bootstrap. Actual
    # standalone source/implementation closure is proved by the artifact gate.
    $sdk = "$outputPath/include/__xl_runtime"
    New-Item -ItemType Directory -Path $sdk -Force | Out-Null
    Get-ChildItem src/runtime/xl6_*.h -File | Copy-Item -Destination $sdk
    Copy-Item release/include/xl_runtime.h,release/include/xl_extlib_abi.h,release/include/xrt_decl.h,release/include/xruntime_decl.h -Destination $sdk
    $configuration = @{
        XL6_CGEN_CC='"' + $compiler + '"';
        XL6_CGEN_FLAGS='-std=c11 -Wall -Wextra -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers';
        XL6_CGEN_INCLUDES='-I"' + "$outputPath/include" + '" -Irelease -Irelease/include -Ilib/xrt -Ilib/xruntime -I.';
        XL6_CGEN_LINKS='release/xl.dll -lWs2_32 -lIPHLPAPI -lShell32'
    }
    foreach ($key in $configuration.Keys) {
        $savedEnvironment[$key] = [Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$configuration[$key],'Process')
    }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    Invoke-Checked 'native-build' cmd.exe ('/d /v:on /c !XL6_CGEN_CC! -std=c11 -Wall -Wextra -Werror !XL6_CGEN_INCLUDES! test\cases\stdlib_log_core_host.c !XL6_CGEN_LINKS! -o "{0}/native.exe"' -f $outputPath)
    Invoke-Checked 'native-run' "$outputPath/native.exe" ''
    Invoke-Checked 'remaining-native-build' cmd.exe ('/d /v:on /c !XL6_CGEN_CC! -std=c11 -Wall -Wextra -Werror !XL6_CGEN_INCLUDES! test\cases\stdlib_log_remaining_host.c !XL6_CGEN_LINKS! -o "{0}/remaining-native.exe"' -f $outputPath)
    Invoke-Checked 'remaining-native-run' "$outputPath/remaining-native.exe" ''
    Invoke-Checked 'oom-build' cmd.exe ('/d /v:on /c !XL6_CGEN_CC! -std=c11 -Wall -Wextra -Werror -I. -Irelease -Irelease/include -DXL6_LOG_FAULT_ALLOCATOR test\cases\stdlib_log_remaining_host.c test\support\log_selected_native.c src\runtime\xl6_log.c src\runtime\xl6_log_record.c src\runtime\xl6_log_text.c src\runtime\xl6_log_json.c src\runtime\xl6_log_printf.c -o "{0}/oom.exe"' -f $outputPath)
    Invoke-Checked 'oom-run' "$outputPath/oom.exe" '' 0 'logical allocations balanced'

    # Canonical ABI mutation tests use the shared-source object cache.
    & "$PSScriptRoot/build_xlir_validation_driver.ps1" -BatchDrivers test/cases/xlir_log_adapter_contract.c -BatchDirectory "$outputPath/contracts"
    if (!$?) { throw 'Logging contract driver failed to build' }
    $contract = "$outputPath/contracts/xlir_log_adapter_contract.exe"
    foreach ($name in @('stdlib_log_core','stdlib_log_console','stdlib_log_global_only','stdlib_log_records',
        'stdlib_log_callbacks','stdlib_log_queues','stdlib_log_writers','stdlib_log_printf','stdlib_log_files',
        'stdlib_log_vfs','stdlib_log_default_cleanup','stdlib_log_exit','stdlib_log','stdlib_log_object','stdlib_log_async_failure')) {
        $source = "test/cases/$name.xl"
        $cPath = "$outputPath/$name.c"
        $expected = if ($name -eq 'stdlib_log_console') { 'a\x00b' + "`n" + '你\nnext' + "`n" }
            elseif ($name -eq 'stdlib_log_queues') { "async`nring`n" } else { '' }
        Invoke-Checked "$name.check" $xl "check $source"
        Invoke-Checked "$name.cgen" $xl ('cgen {0} --emit "{1}"' -f $source,$cPath)
        Invoke-Checked "$name.repeat" $xl ('cgen {0} --emit "{1}/{2}.repeat.c"' -f $source,$outputPath,$name)
        Invoke-Checked "$name.contract" $contract ('{0} "{1}/{2}.contract.c"' -f $source,$outputPath,$name)
        $c = [IO.File]::ReadAllText($cPath)
        if ($c -cne [IO.File]::ReadAllText("$outputPath/$name.repeat.c") -or
            $c -cne [IO.File]::ReadAllText("$outputPath/$name.contract.c")) { throw "Nondeterministic or stale compiler output: $name" }
        if ($c -notmatch '#include <__xl_runtime/xl6_log.h>' -or $c -match 'xl6_xrt_log_|strlen\(.*(?:message|logger)' -or
            $c -match '(?m)^\s*(?:static\s+)?(?:void\*|bool|int64_t|xstrview|uint64_t)\s+xl6Log\w+\([^;]*\)\s*\{') {
            throw "Generated logging implementation or legacy ABI: $name"
        }
        if ($name -in @('stdlib_log_core','stdlib_log_global_only') -and $c -match 'xl6LogConsoleV1\(') { throw 'Core-only output retained console calls' }
        if ($name -eq 'stdlib_log_global_only' -and $c -match 'xl6Log(?:Create|Take|Name|Handle|Stats|Sink)\w*V1\(') {
            throw 'Global write allocated or accessed a language Logger wrapper'
        }
        $entry = if ($name -eq 'stdlib_log_exit') { '-DXLIR_LOG_USER_ENTRY' } else { '' }
        Invoke-Checked "$name.strict-build" cmd.exe ('/d /v:on /c !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "{0}" {3} test\cases\stdlib_log_generated_host.c !XL6_CGEN_LINKS! -o "{1}/{2}.strict.exe"' -f $cPath,$outputPath,$name,$entry)
        Invoke-Checked "$name.strict-run" "$outputPath/$name.strict.exe" '' -ExpectedOutput $expected -CheckOutput
        Invoke-Checked "$name.jit" $xl "run $source" -ExpectedOutput $expected -CheckOutput
        Invoke-Checked "$name.aot-build" $xl ('build {0} -o "{1}/{2}.aot.exe"' -f $source,$outputPath,$name)
        Invoke-Checked "$name.aot-run" "$outputPath/$name.aot.exe" '' -ExpectedOutput $expected -CheckOutput
        if ($name -eq 'stdlib_log_exit' -and [IO.File]::ReadAllText("$rootPath/build/test/log-complete-exit.txt").Replace("`r`n","`n") -cne "before exit`nfinally`n") {
            throw 'Default asynchronous sink did not drain after main/finally and module cleanup'
        }
        Write-Output "${name}: exact output, canonical ABI, strict C, JIT/AOT and determinism passed"
    }
    foreach ($name in @('bad_log_printf_count','bad_log_printf_type','bad_log_printf_dynamic',
        'bad_log_printf_n','bad_log_printf_star','bad_log_printf_flags','bad_log_printf_nul')) {
        $cPath = "$outputPath/$name.c"
        Invoke-Checked $name $xl ('cgen test/cases/{0}.xl --emit "{1}"' -f $name,$cPath) 1 'log printf requires a literal format'
        if (Test-Path -LiteralPath $cPath) { throw "Unsafe printf emitted C: $name" }
    }
    foreach ($name in @('bad_xlir_log_input','bad_xlir_log_result')) {
        $cPath = "$outputPath/$name.c"
        $marker = if ($name -eq 'bad_xlir_log_input') { 'XLIR logging resource parameter has no exact native field borrow' }
            else { 'XLIR logging adapter has no exact physical/ownership ABI' }
        Invoke-Checked $name $xl ('cgen test/cases/{0}.xl --emit "{1}"' -f $name,$cPath) 1 $marker
        if (Test-Path -LiteralPath $cPath) { throw "Invalid ABI emitted C: $name" }
    }
    $failure = 'test/cases/stdlib_log_cleanup_failure.xl'
    $failureC = "$outputPath/cleanup-failure.c"
    Invoke-Checked 'cleanup-failure.cgen' $xl ('cgen {0} --emit "{1}"' -f $failure,$failureC)
    Invoke-Checked 'cleanup-failure.strict-build' cmd.exe ('/d /v:on /c !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "{0}" test\cases\stdlib_log_cleanup_failure_host.c !XL6_CGEN_LINKS! -o "{1}/cleanup-failure.strict.exe"' -f $failureC,$outputPath)
    Invoke-Checked 'cleanup-failure.strict-run' "$outputPath/cleanup-failure.strict.exe" ''
    Invoke-Checked 'cleanup-failure.jit' $xl "run $failure" 1 'final flush failure'
    Invoke-Checked 'cleanup-failure.aot-build' $xl ('build {0} -o "{1}/cleanup-failure.aot.exe"' -f $failure,$outputPath)
    Invoke-Checked 'cleanup-failure.aot-run' "$outputPath/cleanup-failure.aot.exe" '' 1 'final flush failure'
    foreach ($name in @('stdlib_io_writer_only','stdlib_fs_names','stdlib_crt_math')) {
        Invoke-Checked "$name.regression" $xl "run test/cases/$name.xl"
    }
    $example = 'release/examples/stdlib_logging.xl'
    Invoke-Checked 'example.jit' $xl "run $example"
    Invoke-Checked 'example.aot-build' $xl ('build {0} -o "{1}/example.exe"' -f $example,$outputPath)
    Invoke-Checked 'example.aot-run' "$outputPath/example.exe" ''
    $parity = "level=ok`nfile=ok`nrolling=ok`ncustom=ok`n"
    Invoke-Checked 'legacy-example.jit' $xl 'run release/examples/stdlib_log.xl' -ExpectedOutput $parity -CheckOutput
    Invoke-Checked 'legacy-example.aot-build' $xl ('build release/examples/stdlib_log.xl -o "{0}/legacy-example.exe"' -f $outputPath)
    Invoke-Checked 'legacy-example.aot-run' "$outputPath/legacy-example.exe" '' -ExpectedOutput $parity -CheckOutput
    Invoke-Checked 'parity-native-build' cmd.exe ('/d /v:on /c !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! release\examples\stdlib_log_c.c !XL6_CGEN_LINKS! -o "{0}/parity-native.exe"' -f $outputPath)
    Invoke-Checked 'parity-native-run' "$outputPath/parity-native.exe" '' -ExpectedOutput $parity -CheckOutput
    Invoke-Checked 'formats-example.jit' $xl 'run release/examples/stdlib_log_formats.xl'
    Invoke-Checked 'formats-example.aot-build' $xl ('build release/examples/stdlib_log_formats.xl -o "{0}/formats-example.exe"' -f $outputPath)
    Invoke-Checked 'formats-example.aot-run' "$outputPath/formats-example.exe" ''
    & "$PSScriptRoot/check_stdlib_coverage.ps1"
    if (!$?) { throw 'Standard library coverage failed' }
    foreach ($input in $inputs) {
        if ((Get-FileHash -LiteralPath $input.Path).Hash -ne $input.Sha256) { throw "Input changed during gate: $($input.Path)" }
    }
    [IO.File]::WriteAllText("$outputPath/results.json", ([pscustomobject]@{
        Scope='Windows 12-module logging integration: records, formatters, Buffer/Writer/native/VFS files, checked printf, custom/async/ring, ownership, failures and module cleanup; not cross-platform or whole-repository acceptance';
        Inputs=$inputs;Results=@($rows.ToArray());Complete=$true
    } | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
} finally {
    foreach ($key in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process') }
    Pop-Location
}
Write-Output "Complete logging integration gate passed ($($rows.Count) checks): $outputPath"
