param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Captured([string]$File, [string]$Arguments, [int]$Expected, [string]$Marker = "") {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $diagnostic = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne $Expected -or
            ($Marker -ne "" -and !$diagnostic.Contains($Marker)) -or
            ($Expected -eq 0 -and $stderr.Result -ne "")) {
            throw "Serialization callback gate failed: $File $Arguments exit=$($process.ExitCode), expected=$Expected $diagnostic"
        }
    } finally { $process.Dispose() }
}

$fixtures = @(
    @{ Name = "xlir_serialization_callback_error"; Exit = 0; Marker = "" },
    @{ Name = "xlir_serialization_callback_async_error"; Exit = 0; Marker = "" },
    @{ Name = "xlir_serialization_callback_generator_error"; Exit = 0; Marker = "" },
    @{ Name = "xlir_serialization_callback_lifetime"; Exit = 0; Marker = "" },
    @{ Name = "xlir_serialization_callback_json_uncaught"; Exit = 1; Marker = "json-callback-uncaught-sentinel" },
    @{ Name = "xlir_serialization_callback_xson_uncaught"; Exit = 1; Marker = "xson-callback-uncaught-sentinel" }
)
Push-Location $Root
try {
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $source = Join-Path $Root "test\cases\$name.xl"
        $ir = Join-Path $outputDir "$name.lowered.json"
        $cPath = Join-Path $outputDir "$name.c"
        $repeat = Join-Path $outputDir "$name.repeat.c"
        Invoke-Captured $xl ('ir "{0}" --lowered --format json --emit "{1}"' -f $source,$ir) 0
        $module = [System.IO.File]::ReadAllText($ir) | ConvertFrom-Json
        $targets = @($module.callTargets | Where-Object {
            $_.reachability -ne "unreachable" -and $_.runtimeBridgeName -in @("json-visit", "xson-visit")
        })
        $expectedTargets = if ($fixture.Exit -eq 1) { 1 } else { 2 }
        if ($targets.Count -ne $expectedTargets) { throw "$name lost its visitor bridge coverage" }
        foreach ($target in $targets) {
            if ($target.errorAbiName -ne "may-raise" -or $target.errorPhysicalAbiName -ne "result-carrier") {
                throw "$name reduced callback exceptions to native parser status/TLS diagnostics"
            }
        }
        if ($name -in @("xlir_serialization_callback_async_error", "xlir_serialization_callback_generator_error")) {
            $machineKind = if ($name -eq "xlir_serialization_callback_async_error") { "async-worker" } else { "generator" }
            $machineOps = @($module.functions | Where-Object { $_.kindName -like "$machineKind*" } | ForEach-Object {
                $function = $_
                $module.instructions | Select-Object -Skip $function.firstInstruction -First $function.instructionCount |
                    ForEach-Object { $_.runtimeOpName }
            })
            foreach ($operation in @("message-copy", "file-copy", "domain-copy", "operation-copy", "data-copy",
                "kind-copy", "code", "line", "column", "system-code", "cause-ref", "kind-is")) {
                if ($machineOps -notcontains "error-$operation") { throw "$name omits a machine error projection: $operation" }
            }
            foreach ($operation in @("string-equal", "string-order-lt", "string-order-le", "string-order-gt", "string-order-ge")) {
                if ($machineOps -notcontains $operation) { throw "$name omits a shared string comparison: $operation" }
            }
            $targetKind = if ($machineKind -eq "async-worker") { 4 } else { 5 }
            $workers = @($module.callTargets | Where-Object { $_.kind -eq $targetKind -and $_.reachability -ne "unreachable" })
            if (@($workers | Where-Object { $_.workerErrorAbiName -eq "may-raise" }).Count -eq 0) {
                throw "$name lost transitive worker error effects"
            }
            if ($targetKind -eq 5 -and @($workers | Where-Object {
                # Generator construction is lazy with respect to the body, but
                # activation/frame allocation is an independently fallible
                # synchronous operation.  The launcher therefore owns a result
                # carrier while body exceptions remain on workerErrorAbi/NEXT.
                $_.errorAbiName -ne "may-raise" -or $_.errorPhysicalAbiName -ne "result-carrier"
            }).Count -ne 0) { throw "$name conflates the fallible launcher with generator body execution" }
        }
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath) 0
        Invoke-Captured $xl ('cgen "{0}" --backend xlir --emit "{1}"' -f $source,$repeat) 0
        $c = [System.IO.File]::ReadAllText($cPath)
        if ($c -cne [System.IO.File]::ReadAllText($repeat)) { throw "$name C emission is not deterministic" }
        if ([regex]::Matches($c, 'static int xl6_serialization_callback_call\(').Count -ne 1 -or
            !$c.Contains("typedef xlir_result_c_int XL6_Serialization_Visit_Result;") -or
            !$c.Contains("xrtSetErrorTake(context->CallbackError);") -or
            $c.Contains("xl6_function_call_var") -or
            $c -match '<[^>]*(?:xrt|xruntime|xl_runtime)\.h>') {
            throw "$name lost its single declaration-only callback boundary/result carrier"
        }
        foreach ($target in $targets) {
            if ($c -notmatch ('xlir_result_c_int r\d+ = ' + [regex]::Escape($target.cName) + '\(')) {
                throw "$name does not consume the verified visitor result carrier"
            }
        }
        $strictExe = Join-Path $outputDir "$name.strict.exe"
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"{0}\" test\cases\xlir_generated_module_host.c test\cases\xlir_json_stream_lifetime_host.c !XL6_CGEN_LINKS! -o "{1}"' -f $cPath.Replace('\', '/'), $strictExe
        & cmd.exe /d /v:on /c $command
        if ($LASTEXITCODE -ne 0) { throw "$name strict standalone C failed" }
        Invoke-Captured $strictExe "" $fixture.Exit
        $backends = @("xlir")
        foreach ($backend in $backends) {
            Invoke-Captured $xl ('run "{0}" --backend {1} --link test/cases/xlir_json_stream_lifetime_host.c' -f $source,$backend) $fixture.Exit $fixture.Marker
            $exe = Join-Path $outputDir "$name.$backend.exe"
            Invoke-Captured $xl ('build "{0}" --backend {1} --link test/cases/xlir_json_stream_lifetime_host.c -o "{2}"' -f $source,$backend,$exe) 0
            Invoke-Captured $exe "" $fixture.Exit $fixture.Marker
        }
        Write-Output "${name}: callback error/result ABI, strict C and runtime verified"
    }
} finally { Pop-Location }
Write-Output "XLIR serialization callback errors ok: parser status and language exceptions remain independent; nested, checked conversion, sync/async/generator catch and uncaught outcomes, transitive worker effects, event/capture lifetime, one shared callback component, deterministic strict C and runtime, async/generator callback propagation, and temporary capture reclamation."
exit 0
