param([string]$Root = "")
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root 'release\xl.exe'
$source = 'test\cases\xlir_closure_argument_conversion.xl'
$prefix = 'build\test\xlir_closure_argument_conversion'
function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Closure argument conversion command failed: $Arguments" }
}
Push-Location $Root
try {
    Invoke-Xl @('ir', $source, '--lowered', '--format', 'json', '--emit', "$prefix.json")
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $types = @{}
    $values = @{}
    foreach ($type in $ir.types) { $types[[int]$type.id] = $type }
    foreach ($instruction in $ir.instructions) {
        if ($instruction.result -ne 0) { $values[[int]$instruction.result] = $instruction }
    }
    $calls = @($ir.instructions | Where-Object { $_.kindName -eq 'closure-call' })
    if ($calls.Count -ne 9) { throw "Expected nine first-class calls, found $($calls.Count)" }
    $boundaryKinds = @{}
    foreach ($call in $calls) {
        $signature = $types[[int]$call.calleeType]
        if ($null -eq $signature -or $call.operandCount -ne @($signature.params).Count + 1) {
            throw 'Closure call lost its exact canonical signature'
        }
        for ($p = 0; $p -lt @($signature.params).Count; ++$p) {
            $argument = $values[[int]$call.operands[$p + 1]]
            if ($null -eq $argument -or $argument.type -ne $signature.params[$p]) {
                throw 'Closure argument still relies on an implicit backend conversion'
            }
            $boundaryKinds[$argument.kindName] = $true
            if ($argument.kindName -eq 'scalar-convert' -and
                $argument.scalarConversionAbiName -eq 'invalid') {
                throw 'Closure scalar conversion has no semantic family'
            }
        }
    }
    foreach ($required in @('scalar-convert', 'nullable-wrap', 'null')) {
        if (!$boundaryKinds.ContainsKey($required)) {
            throw "Closure boundary lost explicit $required coverage"
        }
    }
    foreach ($name in @('c_int', 'int', 'c_double')) {
        $conversions = @($ir.instructions | Where-Object {
            $_.kindName -eq 'scalar-convert' -and $types[[int]$_.type].name -eq $name
        })
        if ($conversions.Count -eq 0) { throw "Missing explicit conversion to $name" }
    }
    Invoke-Xl @('cgen', $source, '--backend', 'xlir', '--emit', "$prefix.c")
    Invoke-Xl @('cgen', $source, '--emit', "$prefix.repeat.c")
    if ([IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw 'Closure argument conversion C is not deterministic'
    }
    Invoke-Xl @('cgen', $source, '--backend', 'legacy', '--emit', "$prefix.legacy.c")
    # Source order is deliberately stronger than legacy host-C argument order.
    # Keep its assertions mandatory in a separate XLIR-only contract fixture.
    $orderSource = 'test\cases\xlir_closure_argument_evaluation_order.xl'
    Invoke-Xl @('cgen', $orderSource, '--backend', 'xlir', '--emit', "$prefix.order.c")
    # A variable int -> c_int still needs an explicit language conversion.
    # Keep both direct and first-class negative checks; fixing lowering must
    # not broaden semantic admission.
    $negative = '"' + $xl + '" check test\cases\sem_closure_argument_cint_variable_invalid.xl > ' + $prefix + '.negative.log 2>&1'
    & $env:ComSpec /d /c $negative
    if ($LASTEXITCODE -ne 1 -or
        ([regex]::Matches([IO.File]::ReadAllText("$prefix.negative.log"), 'target c_int, value int')).Count -ne 2) {
        throw 'Direct and closure calls must reject implicit int-variable narrowing equally'
    }
    $command = 'call test\suites\common.bat' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include ' + $prefix + '.c test\cases\xlir_closure_argument_conversion_host.c !XL6_CGEN_LINKS! -o ' + $prefix + '.exe' +
        ' && ' + $prefix + '.exe' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include ' + $prefix + '.order.c test\cases\xlir_closure_argument_conversion_host.c !XL6_CGEN_LINKS! -o ' + $prefix + '.order.exe' +
        ' && ' + $prefix + '.order.exe' +
        ' && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror ' + $prefix + '.legacy.c !XL6_CGEN_XLANG_LINKS! -o ' + $prefix + '.legacy.exe' +
        ' && ' + $prefix + '.legacy.exe'
    & $env:ComSpec /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Closure argument conversion strict C/runtime differential failed' }
} finally { Pop-Location }
Write-Output 'XLIR closure argument boundary passed: nine exact SSA call signatures, explicit scalar/nullable/boxed values, unchanged negative semantics, deterministic strict C and legacy runtime differential; source-order contract passed independently on XLIR.'
exit 0
