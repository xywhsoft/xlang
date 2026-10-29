param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$fixture = "test\cases\xlir_core_call_value_lifetime.xl"
$prefix = "build\test\xlir_core_call_value_lifetime"

function Invoke-Xl([string[]]$Arguments) {
    & $xl @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Evaluation lifetime command failed: $Arguments" }
}
Push-Location $Root
try {
    Invoke-Xl @("ir", $fixture, "--lowered", "--format", "json", "--emit", "$prefix.json")
    $module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $types = @{}; $nodes = @{}; $definitions = @{}; $blocks = @{}
    foreach ($item in $module.types) { $types[$item.id] = $item }
    foreach ($item in $module.nodes) { $nodes[$item.id] = $item }
    foreach ($item in $module.blocks) { $blocks[$item.id] = $item }
    foreach ($item in $module.instructions) {
        if ($item.result -ne 0) { $definitions[$item.result] = $item }
    }
    # A local destructuring binding is physical field storage, not a fictitious
    # owner for the producer's aggregate. Every managed field must be released.
    $fields = @($module.storages | Where-Object {
        $_.kindName -eq "local" -and $nodes[$_.declaration].kindName -eq "name" -and
        $types[$_.type].ownershipAbi -eq 2
    })
    if ($fields.Count -lt 6) { throw "Local product binding coverage was removed" }
    foreach ($field in $fields) {
        if (@($module.instructions | Where-Object {
            $_.kindName -eq "release" -and $_.targetSymbol -eq $field.symbol -and
            $_.type -eq $field.type
        }).Count -eq 0) { throw "Managed product field has no physical cleanup: $($field.symbol)" }
    }
    # STORE / LOAD protects an already acquired owner. A successful consumer
    # receives an explicit MOVE; failure releases the same exact temporary.
    $protected = @($module.storages | Where-Object {
        $_.kindName -eq "temporary" -and $types[$_.type].ownershipAbi -eq 2
    } | Where-Object {
        $symbol = $_.symbol
        @($module.instructions | Where-Object {
            $_.kindName -eq "move" -and $_.targetSymbol -eq $symbol
        }).Count -gt 0 -and @($module.instructions | Where-Object {
            $_.kindName -eq "release" -and $_.targetSymbol -eq $symbol
        }).Count -gt 0
    })
    if ($protected.Count -lt 1) { throw "Partial product owners lack move and failure cleanup" }
    $borrowFailures = 0
    foreach ($slot in $protected) {
        foreach ($move in @($module.instructions | Where-Object {
            $_.kindName -eq "move" -and $_.targetSymbol -eq $slot.symbol
        })) {
            $load = $definitions[$move.operands[0]]
            if ($move.operandCount -ne 1 -or $load.kindName -ne "load" -or
                $load.targetSymbol -ne $slot.symbol -or $load.type -ne $slot.type) {
                throw "Evaluation owner transfer has no exact storage LOAD"
            }
        }
    }
    # Consumer failure is not argument-evaluation failure: ownership must have
    # moved before invocation, so the caller cannot release it a second time.
    foreach ($test in @($module.instructions | Where-Object { $_.kindName -eq "error-test" })) {
        $producer = $definitions[$test.operands[0]]
        if ($producer.kindName -ne "call") { continue }
        $failure = $blocks[$test.falseTarget]
        if ($null -eq $failure) { throw "Missing explicit error block" }
        $released = @($module.instructions | Where-Object {
            $_.index -ge $failure.firstInstruction -and
            $_.index -lt ($failure.firstInstruction + $failure.instructionCount) -and
            $_.kindName -eq "release"
        })
        foreach ($operand in $producer.operands) {
            $load = $definitions[$operand]
            if ($load.kindName -eq "load" -and $load.targetSymbol -in $protected.symbol -and
                $load.targetSymbol -in $released.targetSymbol) {
                throw "Call failure releases an owner already transferred to the callee"
            }
        }
        foreach ($release in $released) {
            $slot = @($module.storages | Where-Object {
                $_.symbol -eq $release.targetSymbol -and $_.kindName -eq "temporary"
            })
            if ($slot.Count -ne 1) { continue }
            foreach ($laterCall in @($module.instructions | Where-Object {
                $_.kindName -eq "call" -and $_.index -gt $test.index
            })) {
                $target = @($module.callTargets | Where-Object { $_.symbol -eq $laterCall.targetSymbol })
                if ($target.Count -ne 1) { continue }
                for ($p = 0; $p -lt $laterCall.operandCount; ++$p) {
                    $load = $definitions[$laterCall.operands[$p]]
                    if ($p -lt $target[0].parameterAbis.Count -and
                        $target[0].parameterAbis[$p] -eq 2 -and $load.kindName -eq "load" -and
                        $load.targetSymbol -eq $release.targetSymbol) { $borrowFailures++ }
                }
            }
        }
    }
    if ($borrowFailures -lt 1) { throw "Earlier borrowed argument has no cleanup when later evaluation fails" }
    Invoke-Xl @("cgen", $fixture, "--backend", "xlir", "--emit", "$prefix.c")
    Invoke-Xl @("cgen", $fixture, "--emit", "$prefix.repeat.c")
    if ([IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw "Evaluation lifetime emission is nondeterministic"
    }
    Invoke-Xl @("run", $fixture)
    Invoke-Xl @("build", $fixture, "-o", "$prefix.exe")
    & ".\$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw "Evaluation lifetime built executable failed" }
    $strict = '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\test\xlir_core_call_value_lifetime.c -DXLIR_GENERATED_MODULE_PREINCLUDED test\cases\xlir_generated_module_host.c -x none !XL6_CGEN_LINKS! -o build\test\xlir_core_call_value_lifetime.strict.exe && build\test\xlir_core_call_value_lifetime.strict.exe'
    & $env:ComSpec /d /v:on /c "call test\suites\common.bat && $strict"
    if ($LASTEXITCODE -ne 0) { throw "Evaluation lifetime strict C execution failed" }
} finally { Pop-Location }
Write-Output "XLIR evaluation lifetime passed: product binding cleanup, partial evaluation unwind, consuming call transfer, exact destructor counts, deterministic strict C and CLI run/build."
