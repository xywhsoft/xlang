param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_callsite_c_vararg.xl"
$hostSource = Join-Path $Root "test\cases\xlir_callsite_c_vararg_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_callsite_c_vararg.typed.xlir"
$loweredPath = Join-Path $outputDir "xlir_callsite_c_vararg.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_callsite_c_vararg.lowered.json"
$cPath = Join-Path $outputDir "xlir_callsite_c_vararg_generated.c"
$headerPath = Join-Path $outputDir "xlir_callsite_c_vararg_generated.h"
$exePath = Join-Path $outputDir "xlir_callsite_c_vararg_host.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR C variadic CallSite gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format text --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR C variadic CallSite typed IR failed" }
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($typed, '(?m)^  call-site ')).Count -ne 1 -or
     ! $typed.Contains('arguments=0+4 fixed=1 variadic=c') -or
     ([regex]::Matches($typed, 'source=c-vararg-item')).Count -ne 3 ) {
    throw "XLIR typed C variadic CallSite has no canonical fixed/tail binding ABI"
}

& $xl ir $source --lowered --format text --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR C variadic CallSite lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches(
        $lowered, 'call opcode=none runtime=none v[0-9]+ type=[0-9]+ target=1 .*operands=\[v[0-9]+,v[0-9]+,v[0-9]+,v[0-9]+\]')).Count -ne 1 ) {
    throw "XLIR C variadic lowering did not retain typed physical tail operands and releases"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR C variadic CallSite JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$site = $json.callSites[0]
$targets = @($json.callTargets | Where-Object { $_.symbol -eq $site.targetSymbol })
$slice = @($json.callArguments)
if ( $targets.Count -ne 1 ) {
    throw "XLIR JSON C variadic CallSite does not resolve to one CallTarget"
}
$target = $targets[0]
if ( $json.callTargetCount -ne @($json.callTargets).Count -or $json.callSiteCount -ne 1 -or
     $json.callArgumentCount -ne 4 -or
     $target.parameterCount -ne 1 -or $target.variadicAbiName -ne 'c' -or
     $site.fixedParameterCount -ne 1 -or $site.variadicAbiName -ne 'c' -or
     (($slice | ForEach-Object { $_.sourceName }) -join ',') -ne
        'explicit,c-vararg-item,c-vararg-item,c-vararg-item' -or
     (($slice | ForEach-Object { $_.parameter }) -join ',') -ne '0,1,1,1' ) {
    throw "XLIR JSON C variadic CallTarget/CallSite facts are incomplete"
}

# Foreign calls have distinct success/failure continuations. Each must close
# exactly the same two string owners in reverse acquisition order; counting
# all RELEASE instructions together cannot prove either path is balanced.
$calls = @($json.instructions | Where-Object { $_.kindName -eq 'call' -and $_.targetSymbol -eq $site.targetSymbol })
if ($calls.Count -ne 1) { throw 'C variadic contract requires exactly one selected call' }
$call = $calls[0]
$outcome = $json.instructions[$call.index + 1]
if ($outcome.kindName -ne 'error-test' -or $outcome.operands[0] -ne $call.result) {
    throw 'C variadic call must observe its foreign outcome before cleanup'
}
$owners = @()
foreach ($value in @($call.operands[3], $call.operands[0])) {
    $definition = @($json.instructions | Where-Object result -eq $value)
    if ($definition.Count -ne 1 -or $definition[0].kindName -ne 'load') {
        throw 'C variadic strings require explicit owner anchors'
    }
    $owners += $definition[0].targetSymbol
}
foreach ($blockId in @($outcome.trueTarget, $outcome.falseTarget)) {
    $block = @($json.blocks | Where-Object id -eq $blockId)[0]
    $releases = @($json.instructions | Where-Object {
        $_.index -ge $block.firstInstruction -and $_.index -lt $block.firstInstruction + $block.instructionCount -and
        $_.kindName -eq 'release'
    })
    if ($releases.Count -ne 2 -or ($releases.targetSymbol -join ',') -ne ($owners -join ',')) {
        throw 'C variadic continuation does not release both strings in reverse acquisition order'
    }
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR C variadic CallSite Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains('extern int xlir_c_vararg_sum(const char*, ...);') -or
     ! $header.Contains('int xlir_callsite_c_vararg_probe(void);') ) {
    throw "XLIR C variadic generated prototype or public export ABI is missing"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR C variadic CallSite generated-C runtime failed" }

$boundarySource = Join-Path $Root 'test/cases/xlir_c_vararg_text_boundary.xl'
$boundaryC = Join-Path $outputDir 'xlir_c_vararg_text_boundary_generated.c'
$boundaryExe = Join-Path $outputDir 'xlir_c_vararg_text_boundary.exe'
& $xl cgen $boundarySource --backend xlir --emit $boundaryC
if ($LASTEXITCODE -ne 0) { throw 'C variadic text boundary Cgen failed' }
$boundary = [IO.File]::ReadAllText($boundaryC)
if ([regex]::Matches($boundary, 'const char\* ct\d+_\d+ = xlir_string_cstr\(v\d+\);').Count -ne 4 -or
    [regex]::Matches($boundary, 'if \(ct\d+_\d+ != NULL && ct\d+_\d+ != NULL\)').Count -ne 2) {
    throw 'Every variadic text operand must be adapted and gate the native invocation'
}
$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_C_VARARG_TEXT_BOUNDARY $hostSource !XL6_CGEN_LINKS! -o $boundaryExe && $boundaryExe"
& $env:ComSpec /d /v:on /c $command
if ($LASTEXITCODE -ne 0) { throw 'C variadic text NUL/default-promotions/skip-callee runtime failed' }

Write-Output "XLIR C variadic CallSite gate ok: fixed/tail ABI, native operands, ownership, prototype, strict C, and va_list runtime."
exit 0
