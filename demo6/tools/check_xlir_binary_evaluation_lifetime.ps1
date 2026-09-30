param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_binary_operand_lifetime.xl"
$prefix = Join-Path $Root "build\test\xlir_binary_operand_lifetime.contract"
foreach ($suffix in @("", ".repeat")) {
    & $xl ir $source --lowered --format json --emit "$prefix$suffix.json"
    if ($LASTEXITCODE -ne 0) { throw "Managed binary evaluation lowering failed" }
    & $xl cgen $source --backend xlir --emit "$prefix$suffix.c"
    if ($LASTEXITCODE -ne 0) { throw "Managed binary evaluation Cgen failed" }
}
foreach ($extension in @("json", "c")) {
    if ([IO.File]::ReadAllText("$prefix.$extension") -cne
        [IO.File]::ReadAllText("$prefix.repeat.$extension")) {
        throw "Managed binary evaluation is not deterministic: $extension"
    }
}
if ([IO.File]::ReadAllText("$prefix.c") -match '__xl6_user_global_|<__xl_runtime/xl_runtime\.h>|\(\{') {
    throw "Managed binary evaluation regressed to legacy tracing or statement expressions"
}
$module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
$definitions = @{}; $nodes = @{}; $storages = @{}; $blocks = @{}
foreach ($instruction in $module.instructions) {
    if ($instruction.result -ne 0) { $definitions[$instruction.result] = $instruction }
}
foreach ($node in $module.nodes) { $nodes[$node.id] = $node }
foreach ($storage in $module.storages) { $storages[$storage.symbol] = $storage }
foreach ($block in $module.blocks) { $blocks[$block.id] = $block }
$required = @(
    "string-concat", "string-equal", "string-order-lt", "string-approx",
    "class-derived-equal", "xvalue-equal", "xvalue-order-lt", "xvalue-order-le",
    "xvalue-order-gt", "xvalue-order-ge", "xvalue-approx",
    "xvalue-binary-add", "xvalue-binary-sub", "xvalue-binary-mul",
    "xvalue-binary-div", "xvalue-binary-mod", "xvalue-binary-bit-and",
    "xvalue-binary-bit-or", "xvalue-binary-bit-xor", "xvalue-binary-shl", "xvalue-binary-shr"
)
$operations = @($module.instructions | Where-Object {
    $_.runtimeOpName -in $required -and $nodes[$_.origin].kindName -eq "binary"
})
foreach ($name in $required) {
    if (@($operations | Where-Object { $_.runtimeOpName -eq $name }).Count -eq 0) {
        throw "Managed binary coverage lost $name"
    }
}
function Assert-Anchor($load) {
    if ($null -eq $load -or $load.kindName -ne "load" -or
        $storages[$load.targetSymbol].kindName -ne "temporary") {
        throw "Managed binary operand has no explicit owned anchor"
    }
    $store = $module.instructions[$load.index - 1]
    if ($store.kindName -ne "store" -or $store.targetSymbol -ne $load.targetSymbol -or
        $store.type -ne $load.type) { throw "Managed binary anchor lost its acquisition" }
}
function Assert-FailureCleanup($test, $owners) {
    $block = $blocks[$test.falseTarget]
    $releases = @($module.instructions | Where-Object {
        $_.index -ge $block.firstInstruction -and
        $_.index -lt $block.firstInstruction + $block.instructionCount -and
        $_.kindName -eq "release" -and $_.targetSymbol -in @($owners.targetSymbol)
    })
    if ($releases.Count -ne $owners.Count) { throw "Binary failure lost or duplicated an operand owner" }
    for ($i = 0; $i -lt $owners.Count; ++$i) {
        $owner = $owners[$owners.Count - 1 - $i]
        if ($releases[$i].targetSymbol -ne $owner.targetSymbol -or $releases[$i].type -ne $owner.type) {
            throw "Binary failure must release exact owners in reverse order"
        }
    }
}
$protectedFailures = 0; $checkedOperations = 0; $convertedInputs = 0
foreach ($operation in $operations) {
    if ($operation.operandCount -ne 2) { throw "Managed binary arity changed" }
    $left = $definitions[$operation.operands[0]]
    $right = $definitions[$operation.operands[1]]
    $owners = @()
    foreach ($load in @($left, $right)) {
        Assert-Anchor $load
        $producer = $definitions[$module.instructions[$load.index - 1].operands[0]]
        if ($producer.runtimeOpName -eq "xvalue-to-string") {
            $inputOwner = $definitions[$producer.operands[0]]
            Assert-Anchor $inputOwner
            $owners += $inputOwner
            $convertedInputs += 1
        }
        $owners += $load
    }
    if ($left.index -ge $right.index) { throw "Binary source evaluation order changed" }
    $cursor = $operation.index + 1
    $next = $module.instructions[$cursor]
    if ($next.kindName -eq "error-test") {
        if ($next.operands[0] -ne $operation.result -or
            $nodes[$operation.origin].operatorErrorAbiName -ne "may-raise") {
            throw "Checked binary operation lost its typed error outcome"
        }
        Assert-FailureCleanup $next $owners
        $cursor = $blocks[$next.trueTarget].firstInstruction
        $checkedOperations += 1
    }
    $resultAnchor = $null
    $candidate = $module.instructions[$cursor]
    if ($candidate.kindName -eq "store" -and $candidate.operands[0] -eq $operation.result -and
        $candidate.type -eq $operation.type -and
        $storages[$candidate.targetSymbol].kindName -eq "temporary") {
        $resultAnchor = $candidate
        $cursor += 1
    }
    $next = $module.instructions[$cursor]
    if ($next.kindName -eq "cleanup-begin") {
        # A fallible owner release is represented as an explicit cleanup
        # outcome.  The operation's value is kept while releases run, then
        # the cleanup-end result is checked before the value is consumed.
        $cleanupStart = $next.index + 1
        $cleanupEnd = $module.instructions[$cleanupStart + $owners.Count]
        $cleanupTest = $module.instructions[$cleanupStart + $owners.Count + 1]
        if ($cleanupEnd.kindName -ne "cleanup-end" -or
            $cleanupTest.kindName -ne "error-test" -or
            $cleanupTest.operands[0] -ne $cleanupEnd.result) {
            throw "Binary cleanup lost its verified error outcome"
        }
        if ($null -ne $resultAnchor) {
            $failure = $blocks[$cleanupTest.falseTarget]
            $resultReleases = @($module.instructions | Where-Object {
                $_.index -ge $failure.firstInstruction -and
                $_.index -lt $failure.firstInstruction + $failure.instructionCount -and
                $_.kindName -eq "release" -and
                $_.targetSymbol -eq $resultAnchor.targetSymbol
            })
            if ($resultReleases.Count -ne 1 -or
                $resultReleases[0].type -ne $resultAnchor.type) {
                throw "Binary cleanup failure lost the owned result anchor"
            }
        }
    } else { $cleanupStart = $cursor }
    for ($i = 0; $i -lt $owners.Count; ++$i) {
        $release = $module.instructions[$cleanupStart + $i]
        $owner = $owners[$owners.Count - 1 - $i]
        if ($release.kindName -ne "release" -or $release.targetSymbol -ne $owner.targetSymbol -or
            $release.type -ne $owner.type) {
            throw "Binary success lost reverse exact-owner cleanup at $($operation.runtimeOpName) instruction $($operation.index): expected release $($owner.targetSymbol)/$($owner.type), got $($release.kindName) $($release.targetSymbol)/$($release.type)"
        }
    }
    foreach ($test in @($module.instructions | Where-Object {
        $_.index -gt $left.index -and $_.index -lt $right.index -and $_.kindName -eq "error-test"
    })) {
        $liveOwners = @($owners | Where-Object { $_.index -lt $test.index })
        Assert-FailureCleanup $test $liveOwners
        $protectedFailures += 1
    }
}
if ($protectedFailures -lt 14 -or $checkedOperations -lt 6 -or $convertedInputs -lt 4) {
    throw "Managed binary partial-failure, checked-operation or coercion coverage was removed"
}
$boxedOwners = 0
foreach ($box in @($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-string-copy" })) {
    $inputOwner = $definitions[$box.operands[0]]
    Assert-Anchor $inputOwner
    $next = $module.instructions[$box.index + 1]
    if ($next.kindName -eq "error-test") {
        if ($next.operands[0] -ne $box.result) {
            throw "String boxing lost its checked allocation result"
        }
        Assert-FailureCleanup $next @($inputOwner)
        $release = $module.instructions[$blocks[$next.trueTarget].firstInstruction]
    } else {
        $release = $next
    }
    if ($release.kindName -ne "release" -or $release.type -ne $inputOwner.type -or
        $release.targetSymbol -ne $inputOwner.targetSymbol) {
        throw "String boxing copied the bytes but leaked its source owner"
    }
    $boxedOwners += 1
}
if ($boxedOwners -lt 4) { throw "Owned string boxing regression coverage was removed" }
Push-Location $Root
try {
    & $xl cgen $source --backend xlir --emit build\test\xlir_binary_operand_lifetime.c
    if ($LASTEXITCODE -ne 0) { throw "Allocation probe emission failed" }
    $command = 'call test\suites\common.bat && "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_ALLOCATION_BINARY -DXLIR_MIN_ALLOCATIONS=22000 test\cases\xlir_named_callable_owner_lifetime_host.c !XL6_CGEN_LINKS! -o build\test\xlir_binary_operand_lifetime.alloc.exe && build\test\xlir_binary_operand_lifetime.alloc.exe'
    & $env:ComSpec /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw "Managed binary strict-C allocation accounting failed" }
    & $xl run $source
    if ($LASTEXITCODE -ne 0) { throw "Managed binary JIT runtime failed" }
    & $xl build $source -o build\test\xlir_binary_operand_lifetime.exe
    if ($LASTEXITCODE -ne 0) { throw "Managed binary AOT build failed" }
    & .\build\test\xlir_binary_operand_lifetime.exe
    if ($LASTEXITCODE -ne 0) { throw "Managed binary AOT runtime failed" }
} finally { Pop-Location }
Write-Output "XLIR managed binary contract passed: $($operations.Count) operations, $protectedFailures later-operand failure edges, $checkedOperations checked-operation errors, $convertedInputs coercion anchors, $boxedOwners source-string releases; deterministic IR/C, strict-C allocation accounting, JIT/AOT and 3000 balanced language objects."
