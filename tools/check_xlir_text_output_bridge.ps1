param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Captured([string]$File, [string]$Arguments, [int]$Expected, [AllowNull()][object]$Output, [string]$Marker = "") {
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
            ($Marker -ne "" -and !$diagnostic.Contains($Marker)) -or
            ($Expected -eq 0 -and $stderr.Result -ne "")) {
            throw "Text output command failed: $File $Arguments exit=$($process.ExitCode) $diagnostic"
        }
        if ($null -ne $Output -and $stdout.Result.Replace("`r`n", "`n") -cne $Output) {
            throw "Text output bytes/newlines differ: $File $Arguments expected=[$Output] actual=[$($stdout.Result)]"
        }
    } finally { $process.Dispose() }
}

$unicodeText = ([string][char]0x4E2D) + [char]0x6587
$fixtures = @(
    @{ Name = "xlir_text_output_write_only"; Output = "text:$unicodeText|$unicodeText"; Bridges = @("text-write") },
    @{ Name = "xlir_text_output_writeln_only"; Output = "`n$unicodeText`n`n"; Bridges = @("text-writeln") },
    @{ Name = "xlir_text_output_bridge"; Output = "xlang|xlang`nxlang-temporary|xlang-temporary`nxlang-closure`nxlang-again`n"; Bridges = @("text-write", "text-writeln") },
    @{ Name = "xlir_text_output_continuation"; Output = "async:ok`ngen:done`n"; Bridges = @("text-write", "text-writeln") }
)
Push-Location $Root
try {
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $source = Join-Path $Root "test\cases\$name.xl"
        $ir = Join-Path $outputDir "$name.lowered.json"
        $cPath = Join-Path $outputDir "$name.c"
        $repeat = Join-Path $outputDir "$name.repeat.c"
        Invoke-Captured $xl ('ir "{0}" --lowered --format json --emit "{1}"' -f $source,$ir) 0 $null
        $module = [System.IO.File]::ReadAllText($ir) | ConvertFrom-Json
        $types = @{}
        foreach ($type in $module.types) { $types[$type.id] = $type }
        $targets = @($module.callTargets | Where-Object {
            $_.reachability -ne "unreachable" -and $_.cName -in @("xl6_xrt_write", "xl6_xrt_writeln")
        })
        if ((($targets.runtimeBridgeName | Sort-Object) -join ',') -cne (($fixture.Bridges | Sort-Object) -join ',')) {
            throw "$name lost operation-level text output bridge selection"
        }
        foreach ($target in $targets) {
            if ($target.kind -ne 2 -or $target.parameterCount -ne 1 -or
                $types[$target.parameterTypes[0]].name -ne "string" -or
                $target.parameterAbis[0] -ne 2 -or $target.parameterPhysicalAbis[0] -ne "direct" -or
                $target.parameterConstraintAbis[0] -ne "none" -or $target.parameterPhysicalFieldDeclarations[0] -ne 0 -or
                $types[$target.returnType].name -ne "c_bool" -or $target.resultPhysicalAbiName -ne "direct" -or
                $target.resultPhysicalDestroySymbol -ne 0 -or $target.resultPhysicalFieldDeclaration -ne 0 -or
                $target.errorAbiName -ne "no-raise" -or $target.errorPhysicalAbiName -ne "none" -or
                $target.variadicAbiName -ne "none") { throw "$name lost exact borrowed string/status/no-raise ABI" }
        }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath) 0 $null
        Invoke-Captured $xl ('cgen "{0}" --backend xlir --emit "{1}"' -f $source,$repeat) 0 $null
        $c = [System.IO.File]::ReadAllText($cPath)
        if ($c -cne [System.IO.File]::ReadAllText($repeat)) { throw "$name is not deterministic" }
        if ($c.Contains('xl6_output_write_text') -or
            $c.Contains("xrtConsoleFlush(XCONSOLE_STDOUT)") -or
            !$c.Contains('#include <__xl_runtime/xruntime_decl.h>') -or
            $c -match '#include\s*[<\"][^>\"]*(?<!_decl)\b(?:xrt|xruntime)\.h[>\"]') {
            throw "$name loses the declarations-only XRT console boundary"
        }
        foreach ($operation in @("write", "writeln")) {
            $expected = $fixture.Bridges -contains "text-$operation"
            if (([regex]::Matches($c, ('bool xl6_xrt_' + $operation + '\(')).Count -eq 1) -ne $expected) {
                throw "$name includes an unused/missing public text output operation: $operation"
            }
        }
        if ($name -like "*_only" -and $c -match 'xrtValue|xlir_xvalue|xl6_xrt_print|xlir_print_one') {
            throw "$name unexpectedly depends on value boxing or the print formatter"
        }
        $strictExe = Join-Path $outputDir "$name.strict.exe"
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"{0}\" test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}"' -f $cPath.Replace('\', '/'), $strictExe
        & cmd.exe /d /v:on /c $command
        if ($LASTEXITCODE -ne 0) { throw "$name strict standalone C failed" }
        Invoke-Captured $strictExe "" 0 $fixture.Output
        foreach ($backend in @("xlir")) {
            Invoke-Captured $xl ('run "{0}" --backend {1}' -f $source,$backend) 0 $fixture.Output
            $exe = Join-Path $outputDir "$name.$backend.exe"
            Invoke-Captured $xl ('build "{0}" --backend {1} -o "{2}"' -f $source,$backend,$exe) 0 $null
            Invoke-Captured $exe "" 0 $fixture.Output
        }
        Write-Output "${name}: exact IR, isolated deterministic strict C and canonical output verified"
    }
    foreach ($suffix in @("wrong_input", "wrong_result", "missing_input", "extra_input")) {
        $source = Join-Path $Root "test\cases\bad_xlir_text_output_$suffix.xl"
        $badC = Join-Path $outputDir "bad_xlir_text_output_$suffix.c"
        if (Test-Path -LiteralPath $badC) { Remove-Item -LiteralPath $badC }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$badC) 1 $null "XLIR direct runtime bridge has no exact declared ABI"
        if (Test-Path -LiteralPath $badC) { throw "Malformed text output ABI emitted C: $suffix" }
    }
} finally { Pop-Location }
Write-Output "XLIR text output bridge ok: exact borrowed string/status/no-raise ABI, language IOError on failure, explicit flushing, declarations-only XRT console boundary, operation isolation, no value-boxing dependency for text-only output, UTF-8/empty/newline/temporary/capture/continuation output parity, four ABI negatives, deterministic strict C and canonical run/build."
exit 0
