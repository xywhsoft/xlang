param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw "Byte codec compiler command failed: $Arguments" }
}

Push-Location $Root
try {
    foreach ( $family in @("hex", "base64") ) {
        $source = Join-Path $Root "test\cases\stdlib_codec_${family}_bytes_only.xl"
        $prefix = Join-Path $outputDir "xlir_${family}_bytes_codec"
        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", "$prefix.json")
        $module = [System.IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
        $types = @{}
        foreach ( $type in $module.types ) { $types[[int]$type.id] = $type }
        $targets = @($module.callTargets | Where-Object {
            $_.runtimeBridgeName -eq "$family-bytes-codec" -and $_.reachability -ne "unreachable"
        })
        if ( $targets.Count -ne 2 ) { throw "$family byte codec lost its exact encode/decode pair" }
        foreach ( $target in $targets ) {
            $encode = $target.cName -eq "xl6_xrt_${family}_encode_bytes"
            $decode = $target.cName -eq "xl6_xrt_buffer_from_$family"
            $parameterTypes = @($target.parameterTypes | ForEach-Object { $types[[int]$_].name })
            $expectedTypes = if ( $encode ) { "point,c_size" } else { "string,c_size" }
            $expectedAbis = if ( $encode ) { "1,1" } else { "2,1" }
            if ( $family -eq "base64" ) { $expectedTypes += ",point"; $expectedAbis += ",1" }
            $result = $types[[int]$target.returnType]
            if ( !($encode -or $decode) -or $target.kind -ne 2 -or
                 ($parameterTypes -join ",") -ne $expectedTypes -or
                 ($target.parameterAbis -join ",") -ne $expectedAbis -or
                 @($target.parameterPhysicalAbis | Where-Object { $_ -ne "direct" }).Count -ne 0 -or
                 $target.resultPhysicalAbiName -ne "direct" -or $result.name -ne "point" -or
                 $result.ownershipAbi -ne 1 -or $result.nullAbi -ne 1 ) {
                throw "$family byte codec has no exact raw-data/borrowed-text, size, alphabet and owned-result ABI"
            }
        }
        Invoke-Xl @("cgen", $source, "--emit", "$prefix.c")
        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", "$prefix.repeat.c")
        $c = [System.IO.File]::ReadAllText("$prefix.c")
        if ( $c -cne [System.IO.File]::ReadAllText("$prefix.repeat.c") ) {
            throw "$family byte codec default/explicit C is not deterministic"
        }
        foreach ( $name in @("xl6_xrt_${family}_encode_bytes", "xl6_xrt_buffer_from_$family") ) {
            if ( [regex]::Matches($c, '(?m)^void\* ' + $name + '\(').Count -ne 1 ) {
                throw "$family byte codec must have one shared implementation: $name"
            }
        }
        $other = if ( $family -eq "hex" ) { "base64" } else { "hex" }
        foreach ( $forbidden in @("xl6_xrt_${other}_encode", "xl6_xrt_${family}_encode_text",
            "xl6_xrt_percent_encode_text", "<__xl_runtime/xl_runtime.h>", '"xrt.h"', '"xruntime.h"') ) {
            if ( $c.Contains($forbidden) ) { throw "$family bytes acquired unrelated runtime: $forbidden" }
        }
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "{0}.c" test\cases\xlir_generated_module_host.c -DXLIR_GENERATED_MODULE_PREINCLUDED !XL6_CGEN_LINKS! -o "{0}.strict.exe" && "{0}.strict.exe"' -f $prefix
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) { throw "$family bytes strict C/runtime failed" }
        foreach ( $backend in @("xlir", "legacy") ) {
            Invoke-Xl @("run", $source, "--backend", $backend)
            Invoke-Xl @("build", $source, "--backend", $backend, "-o", "$prefix.$backend.exe")
            & "$prefix.$backend.exe"
            if ( $LASTEXITCODE -ne 0 ) { throw "$family bytes $backend executable failed" }
        }
    }
    foreach ( $suffix in @("wrong_input", "missing_table", "wrong_result") ) {
        $bad = Join-Path $Root "test\cases\bad_xlir_byte_codec_$suffix.xl"
        $badOutput = Join-Path $outputDir "bad_xlir_byte_codec_$suffix.c"
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
                 !$diagnostic.Contains("XLIR byte codec runtime bridge has no exact ABI") ) {
                throw "Malformed byte codec was not rejected precisely: $suffix $diagnostic"
            }
        } finally { $process.Dispose() }
    }
} finally { Pop-Location }
Write-Output "XLIR byte codec ABI ok: two exact bridge families, shared adapters, binary/empty payloads, three malformed-ABI rejections, deterministic strict C and run/build parity."
exit 0
