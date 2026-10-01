param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Test([string]$File, [string]$Arguments, [byte[]]$InputBytes = @(), [int]$Expected = 0, [string]$Marker = "") {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.WorkingDirectory = $Root
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardInput = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if ($InputBytes.Length -gt 0) { $process.StandardInput.BaseStream.Write($InputBytes, 0, $InputBytes.Length) }
        $process.StandardInput.Close()
        if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); throw "Timed out: $File $Arguments" }
        $diagnostic = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne $Expected -or ($Marker -ne "" -and !$diagnostic.Contains($Marker))) {
            throw "IO/console test failed: $File $Arguments exit=$($process.ExitCode) $diagnostic"
        }
    } finally { $process.Dispose() }
}
function Compile-Strict([string]$Source, [string]$Executable) {
    $command = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"{0}\" test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}"' -f $Source.Replace('\','/'),$Executable
    & cmd.exe /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw "Strict generated C failed: $Source" }
}

$compilerEnvironmentBefore = @{}
Push-Location $Root
try {
    # Prepare native test artifacts once per batch, not once per fixture.
    # Capture only the test compiler variables, never the entire environment.
    $toolchainOutput = & cmd.exe /d /v:on /c 'call test\suites\common.bat && set XL6_CGEN'
    if ($LASTEXITCODE -ne 0) { throw "IO/console toolchain preparation failed" }
    foreach ($line in $toolchainOutput) {
        if ($line -match '^(XL6_CGEN_[A-Z_]+)=(.*)$') {
            $key = $Matches[1]
            $compilerEnvironmentBefore[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            [Environment]::SetEnvironmentVariable($key, $Matches[2], 'Process')
        } else { Write-Output $line }
    }
    foreach ($key in @('XL6_CGEN_CC', 'XL6_CGEN_FLAGS', 'XL6_CGEN_INCLUDES', 'XL6_CGEN_LINKS')) {
        if (![Environment]::GetEnvironmentVariable($key, 'Process')) { throw "Missing test compiler variable: $key" }
    }
    # Compile against the actual declaration and execute the enum/int bridge.
    # The redeclaration catches incompatible CRT registration in native C.
    $flushExe = Join-Path $outputDir 'crt_console_flush_abi.exe'
    & cmd.exe /d /v:on /c ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\fixtures\crt_console_pending\flush_abi.c !XL6_CGEN_LINKS! -o "{0}"' -f $flushExe)
    if ($LASTEXITCODE -ne 0) { throw 'CRT console flush enum/int ABI compilation failed' }
    Invoke-Test $flushExe ''
    $fixtures = @(
        "stdlib_io_files", "stdlib_io_reader_only", "stdlib_io_writer_only", "stdlib_io_streams", "stdlib_io_console_constants",
        "stdlib_io_buffer", "stdlib_io_protocol", "stdlib_io_allocation", "stdlib_console_basic", "stdlib_console_print", "stdlib_console_capabilities",
        "stdlib_console_screen_only", "stdlib_console_input", "stdlib_io_stdin", "stdlib_console_invalid_input"
    )
    foreach ($name in $fixtures) {
        $source = Join-Path $Root "test\cases\$name.xl"
        $cPath = Join-Path $outputDir "$name.c"
        $repeat = Join-Path $outputDir "$name.repeat.c"
        $inputBytes = if ($name -eq "stdlib_console_input") { [System.Text.Encoding]::UTF8.GetBytes("`na`0b`r`n" + [char]0x4f60 + [char]::ConvertFromUtf32(0x1f600)) }
            elseif ($name -eq "stdlib_io_stdin") { [System.Text.Encoding]::UTF8.GetBytes("abc") }
            elseif ($name -eq "stdlib_console_invalid_input") { [byte[]]@(0xc0,0xaf) }
            else { [byte[]]@() }
        Invoke-Test $xl ('check "{0}"' -f $source)
        Invoke-Test $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath)
        Invoke-Test $xl ('cgen "{0}" --emit "{1}"' -f $source,$repeat)
        $c = [System.IO.File]::ReadAllText($cPath)
        if ($c -cne [System.IO.File]::ReadAllText($repeat)) { throw "$name is not deterministic" }
        if ($c -match '#include\s*[<\"][^>\"]*(?<!_decl)\b(?:xrt|xruntime)\.h[>\"]') { throw "$name imports an implementation header" }
        if ($name -eq "stdlib_console_screen_only" -and $c -match 'XRT_MODULE_(?:CONSOLE_TERMINAL|IO_STANDARD)|xl6_xrt_console_session_') { throw "Screen-only program retains sessions" }
        if ($name -eq "stdlib_io_reader_only" -and $c -match 'xl6_xrt_io_writer_|XRT_MODULE_CONSOLE_TERMINAL') { throw "Reader-only program retains writers" }
        $strict = Join-Path $outputDir "$name.strict.exe"
        Compile-Strict $cPath $strict
        Invoke-Test $strict "" $inputBytes
        Invoke-Test $xl ('run "{0}"' -f $source) $inputBytes
        $exe = Join-Path $outputDir "$name.aot.exe"
        Invoke-Test $xl ('build "{0}" -o "{1}"' -f $source,$exe)
        Invoke-Test $exe "" $inputBytes
        Write-Output "${name}: strict C, JIT/AOT, deterministic output and dependency isolation passed"
    }
    foreach ($name in @("bad_xlir_io_wrong_input", "bad_xlir_io_wrong_result", "bad_xlir_console_wrong_input", "bad_xlir_console_wrong_result")) {
        $cPath = Join-Path $outputDir "$name.c"
        if (Test-Path -LiteralPath $cPath) { Remove-Item -LiteralPath $cPath }
        Invoke-Test $xl ('cgen "test\cases\{0}.xl" --emit "{1}"' -f $name,$cPath) @() 1 "XLIR IO/console runtime bridge has no exact declared ABI"
        if (Test-Path -LiteralPath $cPath) { throw "Invalid ABI emitted C: $name" }
    }
    foreach ($name in @("stdlib_io", "stdlib_console")) {
        $source = Join-Path $Root "release\examples\$name.xl"
        $exe = Join-Path $outputDir "$name.example.exe"
        Invoke-Test $xl ('run "{0}"' -f $source)
        Invoke-Test $xl ('build "{0}" -o "{1}"' -f $source,$exe)
        Invoke-Test $exe ""
    }
    # The advanced file namespace remains independent of the new byte IO API.
    foreach ($name in @('stdlib_file_bytes_only', 'stdlib_buffer_file_runtime')) {
        $source = Join-Path $Root "test\cases\$name.xl"
        $exe = Join-Path $outputDir "$name.io_regression.exe"
        Invoke-Test $xl ('run "{0}"' -f $source)
        Invoke-Test $xl ('build "{0}" -o "{1}"' -f $source,$exe)
        Invoke-Test $exe ""
        Write-Output "${name}: existing advanced file namespace JIT/AOT regression passed"
    }
    $hostExe = Join-Path $outputDir "stdlib_console_windows_host.exe"
    & cmd.exe /d /v:on /c ('!XL6_CGEN_CC! -std=c11 -Wall -Wextra -Werror -municode test\cases\stdlib_console_windows_host.c -o "{0}"' -f $hostExe)
    if ($LASTEXITCODE -ne 0) { throw "Hidden console host failed to compile" }
    foreach ($name in @("stdlib_console_terminal", "stdlib_console_return_cleanup", "stdlib_console_finally_cleanup", "stdlib_console_error_cleanup")) {
        $source = Join-Path $Root "test\cases\$name.xl"
        $exe = Join-Path $outputDir "$name.aot.exe"
        $expected = if ($name -eq "stdlib_console_return_cleanup") { 17 } elseif ($name -eq "stdlib_console_error_cleanup") { 1 } else { 0 }
        Invoke-Test $xl ('check "{0}"' -f $source)
        Invoke-Test $xl ('build "{0}" -o "{1}"' -f $source,$exe)
        Invoke-Test $hostExe ('{0} "{1}"' -f $expected,$exe)
        Invoke-Test $hostExe ('{0} "{1}" run "{2}"' -f $expected,$xl,$source)
        Write-Output "${name}: hidden real terminal JIT/AOT and exact mode/cursor restoration passed"
    }
} finally {
    foreach ($key in $compilerEnvironmentBefore.Keys) {
        [Environment]::SetEnvironmentVariable($key, $compilerEnvironmentBefore[$key], 'Process')
    }
    Pop-Location
}
Write-Output "IO/console gate passed: binary/NUL/clamping, streams and protocols, independent lines, limits, EOF/UTF-8, strict deterministic C, JIT/AOT, four ABI negatives, native terminal events and finally/return cleanup."
