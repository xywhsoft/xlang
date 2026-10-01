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
    $files = @(@(Get-ChildItem src -Recurse -File) + @(Get-Item release/xl.exe,release/xl.dll,release/xlc.dll,
        test/cases/stdlib_log_core.xl,test/cases/stdlib_log_console.xl,test/cases/stdlib_log_global_only.xl,test/cases/stdlib_log_core_host.c,
        test/cases/xlir_log_adapter_contract.c,test/cases/bad_xlir_log_input.xl,test/cases/bad_xlir_log_result.xl,
        release/examples/stdlib_logging.xl,tools/check_stdlib_coverage.ps1,$PSCommandPath)) | Sort-Object FullName -Unique
    $inputs = @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    # Prepare private runtime declarations and the native compiler once.
    $preparation = & cmd.exe /d /v:on /c 'call test\suites\common.bat && set XL6_CGEN'
    if ($LASTEXITCODE -ne 0) { throw 'Logging compiler preparation failed' }
    foreach ($line in $preparation) {
        if ($line -match '^(XL6_CGEN_[A-Z_]+)=(.*)$') {
            $key = $Matches[1]
            $savedEnvironment[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            [Environment]::SetEnvironmentVariable($key, $Matches[2], 'Process')
        } else { Write-Output $line }
    }
    foreach ($key in @('XL6_CGEN_CC','XL6_CGEN_FLAGS','XL6_CGEN_INCLUDES','XL6_CGEN_LINKS')) {
        if (![Environment]::GetEnvironmentVariable($key, 'Process')) { throw "Missing compiler variable $key" }
    }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    Invoke-Checked 'native-build' cmd.exe ('/d /v:on /c !XL6_CGEN_CC! -std=c11 -Wall -Wextra -Werror !XL6_CGEN_INCLUDES! test\cases\stdlib_log_core_host.c !XL6_CGEN_LINKS! -o "{0}/native.exe"' -f $outputPath)
    Invoke-Checked 'native-run' "$outputPath/native.exe" ''

    # Canonical ABI mutation tests use the shared-source object cache.
    & "$PSScriptRoot/build_xlir_validation_driver.ps1" -BatchDrivers test/cases/xlir_log_adapter_contract.c -BatchDirectory "$outputPath/contracts"
    if (!$?) { throw 'Logging contract driver failed to build' }
    $contract = "$outputPath/contracts/xlir_log_adapter_contract.exe"
    foreach ($name in @('stdlib_log_core','stdlib_log_console','stdlib_log_global_only')) {
        $source = "test/cases/$name.xl"
        $cPath = "$outputPath/$name.c"
        $expected = if ($name -eq 'stdlib_log_console') { 'a\x00b' + "`n" + '你\nnext' + "`n" } else { '' }
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
        if ($name -ne 'stdlib_log_console' -and $c -match 'xl6LogConsoleV1\(') { throw 'Core-only output retained console calls' }
        if ($name -eq 'stdlib_log_global_only' -and $c -match 'xl6Log(?:Create|Take|Name|Handle|Stats|Sink)\w*V1\(') {
            throw 'Global write allocated or accessed a language Logger wrapper'
        }
        Invoke-Checked "$name.strict-build" cmd.exe ('/d /v:on /c !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "{0}" -DXLIR_GENERATED_MODULE_PREINCLUDED test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}/{2}.strict.exe"' -f $cPath,$outputPath,$name)
        Invoke-Checked "$name.strict-run" "$outputPath/$name.strict.exe" '' -ExpectedOutput $expected -CheckOutput
        Invoke-Checked "$name.jit" $xl "run $source" -ExpectedOutput $expected -CheckOutput
        Invoke-Checked "$name.aot-build" $xl ('build {0} -o "{1}/{2}.aot.exe"' -f $source,$outputPath,$name)
        Invoke-Checked "$name.aot-run" "$outputPath/$name.aot.exe" '' -ExpectedOutput $expected -CheckOutput
        Write-Output "${name}: exact output, canonical ABI, strict C, JIT/AOT and determinism passed"
    }
    foreach ($name in @('bad_xlir_log_input','bad_xlir_log_result')) {
        $cPath = "$outputPath/$name.c"
        $marker = if ($name -eq 'bad_xlir_log_input') { 'XLIR logging resource parameter has no exact native field borrow' }
            else { 'XLIR logging adapter has no exact physical/ownership ABI' }
        Invoke-Checked $name $xl ('cgen test/cases/{0}.xl --emit "{1}"' -f $name,$cPath) 1 $marker
        if (Test-Path -LiteralPath $cPath) { throw "Invalid ABI emitted C: $name" }
    }
    foreach ($name in @('stdlib_io_writer_only','stdlib_fs_names','stdlib_crt_math')) {
        Invoke-Checked "$name.regression" $xl "run test/cases/$name.xl"
    }
    $example = 'release/examples/stdlib_logging.xl'
    Invoke-Checked 'example.jit' $xl "run $example"
    Invoke-Checked 'example.aot-build' $xl ('build {0} -o "{1}/example.exe"' -f $example,$outputPath)
    Invoke-Checked 'example.aot-run' "$outputPath/example.exe" ''
    & "$PSScriptRoot/check_stdlib_coverage.ps1"
    if (!$?) { throw 'Standard library coverage failed' }
    foreach ($input in $inputs) {
        if ((Get-FileHash -LiteralPath $input.Path).Hash -ne $input.Sha256) { throw "Input changed during gate: $($input.Path)" }
    }
    [IO.File]::WriteAllText("$outputPath/results.json", ([pscustomobject]@{
        Scope='Windows logging foundation and console; not full 12-module or cross-platform acceptance';
        Inputs=$inputs;Results=@($rows.ToArray());Complete=$true
    } | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
} finally {
    foreach ($key in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process') }
    Pop-Location
}
Write-Output "Logging foundation/console gate passed: $outputPath"
