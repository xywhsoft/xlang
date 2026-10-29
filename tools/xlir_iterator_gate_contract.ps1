# Projection origins are the exact for-target nodes, not the enclosing loop.
# Include only that header's origins: never another loop or its body operations.
function Get-XlirIteratorOrigins($Typed, $Loop) {
    $targets = @($Typed.nodes | Where-Object {
        $_.parent -eq $Loop.id -and $_.sourceKindName -eq 'for_targets'
    })
    if ($targets.Count -ne 1) { throw 'Iterator has no unique target header' }
    return @($Loop.id) + @($Typed.nodes | Where-Object {
        $_.parent -eq $targets[0].id
    } | ForEach-Object id)
}

function Assert-XlirCheckedIteratorAdvance($Lowered, $Loop) {
    $instructions = @($Lowered.instructions)
    $advances = @($instructions | Where-Object {
        $_.origin -eq $Loop.id -and $_.kindName -eq 'runtime-call' -and
        $_.runtimeOpName -eq 'xvalue-iterator-advance'
    })
    if ($advances.Count -ne 1) { throw 'Iterator has no unique checked advance' }
    $advance = $advances[0]
    $index = [array]::IndexOf($instructions, $advance)
    if ($index -lt 0 -or $index+1 -ge $instructions.Count) { throw 'Missing advance outcome' }
    $test = $instructions[$index+1]
    if ($test.kindName -ne 'error-test' -or @($test.operands).Count -ne 1 -or
        $test.operands[0] -ne $advance.result) { throw 'Advance lacks immediate checked status edge' }
}

function Get-XlirIteratorCursorStore($Lowered, $Loop) {
    Assert-XlirCheckedIteratorAdvance $Lowered $Loop
    $advance = @($Lowered.instructions | Where-Object {
        $_.origin -eq $Loop.id -and $_.runtimeOpName -eq 'xvalue-iterator-advance'
    })[0]
    $load = @($Lowered.instructions | Where-Object {
        $_.result -eq $advance.operands[0] -and $_.kindName -eq 'load' -and
        $_.type -eq $Loop.loopResourceType
    })
    if ($load.Count -ne 1) { throw 'Advance is not bound to its unique cursor owner' }
    # Allocating constructors may first own an evaluation temporary, then take
    # it into this loop owner. That temporary is not a second live loop cursor.
    $stores = @($Lowered.instructions | Where-Object {
        $_.kindName -eq 'store' -and $_.targetSymbol -eq $load[0].targetSymbol -and
        $_.type -eq $Loop.loopResourceType
    })
    if ($stores.Count -ne 1) { throw 'Loop cursor publication is not unique' }
    return $stores[0]
}

function Get-XlirIteratorTargetStores($Typed, $Lowered, $Loop, $Type) {
    $origins = @(Get-XlirIteratorOrigins $Typed $Loop)
    foreach ($target in @($Typed.nodes | Where-Object {
        $_.id -in $origins -and $_.symbol -ne 0 -and $_.storageType -eq $Type
    })) {
        $stores = @($Lowered.instructions | Where-Object {
            $_.origin -eq $Loop.id -and $_.kindName -eq 'store' -and
            $_.targetSymbol -eq $target.symbol -and $_.type -eq $Type
        })
        if ($stores.Count -ne 1) { throw 'Iterator target has no unique initialization' }
        $stores[0]
    }
}

function Get-XlirRenderedIteratorReleases([string]$Generated, $Symbol, [string]$Type, [string]$Drop) {
    $typePattern = [regex]::Escape($Type)
    $dropPattern = [regex]::Escape($Drop)
    $apply = [regex]::Escape('(xl6SlotRuntimeV1Get()->Apply)(xlir_writes, sizeof(xlir_writes)/sizeof(*xlir_writes));')
    $pattern = "\{\s*$typePattern xlir_released;\s*$typePattern const xlir_empty = \{0\};\s*" +
        "\{ const XL6_SlotWrite xlir_writes\[\] = \{\s*" +
        "\{ &\(xlir_released\), &\(s$Symbol\), sizeof\(xlir_released\) \},\s*" +
        "\{ &\(s$Symbol\), &\(xlir_empty\), sizeof\(s$Symbol\) \},\s*\}; $apply \}\s*" +
        "$dropPattern\(xlir_released\); \}"
    return [regex]::Matches($Generated, $pattern).Count
}
