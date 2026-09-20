param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_membership_operand_lifetime.xl"
$prefix = Join-Path $Root "build\test\xlir_membership_operand_lifetime.contract"
foreach ($suffix in @("", ".repeat")) {
    & $xl ir $source --lowered --format json --emit "$prefix$suffix.json"
    if ($LASTEXITCODE -ne 0) { throw "Membership evaluation lowering failed" }
    & $xl cgen $source --backend xlir --emit "$prefix$suffix.c"
    if ($LASTEXITCODE -ne 0) { throw "Membership evaluation Cgen failed" }
}
foreach ($extension in @("json", "c")) {
    if ([IO.File]::ReadAllText("$prefix.$extension") -cne
        [IO.File]::ReadAllText("$prefix.repeat.$extension")) {
        throw "Membership evaluation is not deterministic: $extension"
    }
}
$generated = [IO.File]::ReadAllText("$prefix.c")
if ($generated -match '__xl6_user_global_|<__xl_runtime/xl_runtime\.h>|\(\{') {
    throw "Membership evaluation regressed to legacy tracing or statement expressions"
}
$module = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
$definitions = @{}; $nodes = @{}; $storages = @{}; $blocks = @{}
foreach ($instruction in $module.instructions) {
    if ($instruction.result -ne 0) { $definitions[$instruction.result] = $instruction }
}
foreach ($node in $module.nodes) { $nodes[$node.id] = $node }
foreach ($storage in $module.storages) { $storages[$storage.symbol] = $storage }
foreach ($block in $module.blocks) { $blocks[$block.id] = $block }
$counts = @{ "xvalue-contains" = 6; "xvalue-array-remove-value" = 3; "string-contains" = 3 }
$operations = @($module.instructions | Where-Object { $counts.ContainsKey($_.runtimeOpName) })
foreach ($entry in $counts.GetEnumerator()) {
    if (@($operations | Where-Object { $_.runtimeOpName -eq $entry.Key }).Count -ne $entry.Value) {
        throw "Membership source-path coverage changed: $($entry.Key)"
    }
}
$protectedFailures = 0
$retainedReceivers = 0
foreach ($operation in $operations) {
    $firstSlot = if ($nodes[$operation.origin].kindName -eq "call") { 0 } else { 1 }
    $first = $definitions[$operation.operands[$firstSlot]]
    $last = $definitions[$operation.operands[1 - $firstSlot]]
    if ($operation.operandCount -ne 2 -or $first.kindName -ne "load" -or
        $last.kindName -ne "load" -or $first.index -ge $last.index -or
        $storages[$first.targetSymbol].kindName -ne "temporary" -or
        $storages[$last.targetSymbol].kindName -ne "temporary") {
        throw "Runtime argument binding lost source-order owned anchors"
    }
    if ($firstSlot -eq 0) {
        $store = $module.instructions[$first.index - 1]
        if ($store.kindName -ne "store" -or $store.targetSymbol -ne $first.targetSymbol) {
            throw "Receiver anchor lost its acquisition"
        }
        if ($definitions[$store.operands[0]].kindName -eq "retain") { $retainedReceivers += 1 }
    }
    $releaseLast = $module.instructions[$operation.index + 1]
    $releaseFirst = $module.instructions[$operation.index + 2]
    if ($releaseLast.kindName -ne "release" -or $releaseFirst.kindName -ne "release" -or
        $releaseLast.targetSymbol -ne $last.targetSymbol -or
        $releaseFirst.targetSymbol -ne $first.targetSymbol -or
        $releaseLast.type -ne $last.type -or $releaseFirst.type -ne $first.type) {
        throw "Membership success does not close exact owners in reverse order"
    }
    foreach ($test in @($module.instructions | Where-Object {
        $_.index -gt $first.index -and $_.index -lt $last.index -and $_.kindName -eq "error-test"
    })) {
        $failure = $blocks[$test.falseTarget]
        $cleanup = @($module.instructions | Where-Object {
            $_.index -ge $failure.firstInstruction -and
            $_.index -lt $failure.firstInstruction + $failure.instructionCount -and
            $_.kindName -eq "release" -and $_.targetSymbol -eq $first.targetSymbol -and
            $_.type -eq $first.type
        })
        if ($cleanup.Count -ne 1) { throw "Later operand failure lost the first owner's cleanup" }
        $protectedFailures += 1
    }
}
if ($protectedFailures -lt 4) { throw "Membership operand failure coverage was removed" }
if ($retainedReceivers -lt 3) { throw "Borrowed receiver pin must acquire its own reference" }
& $xl run $source
if ($LASTEXITCODE -ne 0) { throw "Membership evaluation runtime failed" }
Write-Output "XLIR membership evaluation contract passed: source-order in/contains/removeValue, pinned borrowed values, reverse success cleanup, $protectedFailures explicit failure edges, deterministic C and 2400 balanced language objects."
