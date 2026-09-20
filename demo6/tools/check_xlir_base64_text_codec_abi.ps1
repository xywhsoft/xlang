param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\stdlib_codec_base64_text_only.xl"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
$jsonPath = Join-Path $outputDir "xlir_base64_text_codec.lowered.json"
$cPath = Join-Path $outputDir "xlir_base64_text_codec.c"
$repeatPath = Join-Path $outputDir "xlir_base64_text_codec.repeat.c"

function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw "Base64 text codec compiler command failed: $Arguments" }
}

Push-Location $Root
try {
    Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $jsonPath)
    $module = [System.IO.File]::ReadAllText($jsonPath) | ConvertFrom-Json
    $targets = @($module.callTargets | Where-Object {
        $_.runtimeBridgeName -eq "base64-text-codec" -and $_.reachability -ne "unreachable"
    })
    if ( $targets.Count -ne 2 ) { throw "Base64 text codec has no exact encode/decode bridge pair" }
    foreach ( $target in $targets ) {
        $types = @($target.parameterTypes | ForEach-Object {
            $id = $_
            $module.types | Where-Object { $_.id -eq $id }
        })
        $result = @($module.types | Where-Object { $_.id -eq $target.returnType })
        if ( $target.kind -ne 2 -or $target.parameterCount -ne 3 -or
             ($types.name -join ",") -ne "string,c_size,point" -or
             ($target.parameterAbis -join ",") -ne "2,1,1" -or
             ($target.parameterPhysicalAbis -join ",") -ne "direct,direct,direct" -or
             $target.resultPhysicalAbiName -ne "direct" -or $result.Count -ne 1 -or
             $result[0].name -ne "point" -or $result[0].ownershipAbi -ne 1 -or
             $result[0].nullAbi -ne 1 ) {
            throw "Base64 text codec lost its borrowed-text/size/alphabet -> raw owned-result bridge ABI"
        }
    }
    Invoke-Xl @("cgen", $source, "--emit", $cPath)
    Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $repeatPath)
    $c = [System.IO.File]::ReadAllText($cPath)
    if ( $c -cne [System.IO.File]::ReadAllText($repeatPath) ) {
        throw "Base64 text codec Cgen is not deterministic"
    }
    foreach ( $name in @("xl6_xrt_base64_encode_text", "xl6_xrt_base64_decode_text") ) {
        if ( [regex]::Matches($c, '(?m)^void\* ' + $name + '\(').Count -ne 1 ) {
            throw "Base64 text codec must emit exactly one implementation: $name"
        }
    }
    foreach ( $forbidden in @("XL6_XBufferLayout", "xl6_xrt_buffer_create",
        "xl6_xrt_hex_encode_text", "xl6_xrt_percent_encode_text",
        "<__xl_runtime/xl_runtime.h>", "xrt.h", "xruntime.h") ) {
        if ( $c.Contains($forbidden) ) { throw "Text-only Base64 acquired an unrelated dependency: $forbidden" }
    }
    foreach ( $suffix in @("missing_table", "wrong_input", "wrong_result") ) {
        $bad = Join-Path $Root "test\cases\bad_xlir_base64_text_$suffix.xl"
        $badOutput = Join-Path $outputDir "bad_xlir_base64_text_$suffix.c"
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
            if ( $process.ExitCode -ne 1 -or
                 (Test-Path -LiteralPath $badOutput) -or
                 ! $diagnostic.Contains("XLIR text codec runtime bridge has no exact ABI") ) {
                throw "Malformed reserved Base64 bridge was not rejected precisely: $suffix $diagnostic"
            }
        } finally { $process.Dispose() }
    }
    $strictExe = Join-Path $outputDir "xlir_base64_text_codec.strict.exe"
    $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "{0}" test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o "{1}" && "{1}"' -f $cPath, $strictExe
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) { throw "Base64 text-only strict C/runtime failed" }
    foreach ( $backend in @("xlir", "legacy") ) {
        Invoke-Xl @("run", $source, "--backend", $backend)
        $exe = Join-Path $outputDir "xlir_base64_text_codec.$backend.exe"
        Invoke-Xl @("build", $source, "--backend", $backend, "-o", $exe)
        & $exe
        if ( $LASTEXITCODE -ne 0 ) { throw "Base64 text-only $backend executable failed" }
    }
} finally { Pop-Location }

Write-Output "XLIR Base64 text codec ABI ok: exact reserved bridge, shared adapter, no Buffer/hex/percent dependency, three malformed-ABI rejections, deterministic strict C, and XLIR/legacy run/build parity."
exit 0
