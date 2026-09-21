param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\tmp"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Read-Case([string]$Name) {
    $source = Join-Path $Root "test\cases\$Name.xl"
    $jsonPath = Join-Path $outputDir "xlir_core_contract_$Name.json"
    $cPath = Join-Path $outputDir "xlir_core_contract_$Name.c"
    $repeatPath = Join-Path $outputDir "xlir_core_contract_$Name.repeat.c"
    & $xl ir $source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) { throw "Core contract lowering failed: $Name" }
    & $xl cgen $source --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "Core contract default Cgen failed: $Name" }
    & $xl cgen $source --backend xlir --emit $repeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "Core contract explicit Cgen failed: $Name" }
    $c = [System.IO.File]::ReadAllText($cPath)
    if ( $c -cne [System.IO.File]::ReadAllText($repeatPath) ) {
        throw "Core contract default/explicit XLIR C is not deterministic: $Name"
    }
    if ( $c.Contains("__xl6_user_global_") -or
         $c.Contains("xl6_temp_managed_release") -or
         $c.Contains("<__xl_runtime/xl_runtime.h>") ) {
        throw "Core contract leaked a legacy emitter representation: $Name"
    }
    return [pscustomobject]@{
        Ir = ([System.IO.File]::ReadAllText($jsonPath) | ConvertFrom-Json)
        C = $c
    }
}

function Function-Ops($Module, $Function) {
    return @($Module.instructions | Select-Object -Skip $Function.firstInstruction `
        -First $Function.instructionCount)
}

function Assert-OwnedChain($Module, $Operations, $First, $Second, [string]$Name) {
    $initialStores = @($Operations | Where-Object {
        $_.kindName -eq "store" -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $First.result
    })
    if ( $initialStores.Count -ne 1 ) { throw "$Name has no unique owned-result anchor" }
    $initialAnchor = $initialStores[0].targetSymbol
    $useLoads = @($Operations | Where-Object {
        $_.kindName -eq "load" -and $_.result -eq $Second.operands[0]
    })
    if ( $useLoads.Count -ne 1 ) { throw "$Name has no unique borrowed use" }
    $useAnchor = $useLoads[0].targetSymbol
    $storages = @($Module.storages | Where-Object {
        $_.symbol -eq $initialAnchor -or $_.symbol -eq $useAnchor
    })
    $expectedStorageCount = if ( $initialAnchor -eq $useAnchor ) { 1 } else { 2 }
    $invalidStorages = @($storages | Where-Object {
        $_.kindName -ne "temporary" -or $_.type -ne $First.type
    })
    $initialReleases = @($Operations | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $initialAnchor
    })
    $useReleases = @($Operations | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $useAnchor
    })
    $wrongTypeReleases = @(@($initialReleases) + @($useReleases) | Where-Object {
        $_.type -ne $First.type
    })
    $valid =
        $storages.Count -eq $expectedStorageCount -and $invalidStorages.Count -eq 0 -and
        $First.id -lt $initialStores[0].id -and
        $useLoads[0].id -lt $Second.id -and
        $useLoads[0].type -eq $First.type -and
        $wrongTypeReleases.Count -eq 0
    if ( $initialAnchor -eq $useAnchor ) {
        $valid = $valid -and $initialStores[0].id -lt $useLoads[0].id -and
            $initialReleases.Count -ge 1 -and
            @($initialReleases | Where-Object { $_.id -le $useLoads[0].id }).Count -eq 0
    } else {
        $takes = @($Operations | Where-Object {
            $_.kindName -eq "storage-take" -and $_.targetSymbol -eq $initialAnchor
        })
        $handoffStores = @()
        if ( $takes.Count -eq 1 ) {
            $takenValue = $takes[0].result
            $handoffStores = @($Operations | Where-Object {
                $_.kindName -eq "store" -and $_.targetSymbol -eq $useAnchor -and
                $_.operandCount -eq 1 -and $_.operands[0] -eq $takenValue
            })
        }
        $valid = $valid -and $takes.Count -eq 1 -and $handoffStores.Count -eq 1 -and
            $initialStores[0].id -lt $takes[0].id -and
            $takes[0].id -lt $handoffStores[0].id -and
            $handoffStores[0].id -lt $useLoads[0].id -and
            $initialReleases.Count -ge 1 -and $useReleases.Count -ge 1 -and
            @($useReleases | Where-Object { $_.id -le $useLoads[0].id }).Count -eq 0
    }
    if ( ! $valid ) {
        throw "$Name lost its exact call/store/borrow/use/release lifetime"
    }
}

$case = Read-Case "index_ownership_top_level_var_return"
foreach ( $line in @(12, 13) ) {
    $ops = @($case.Ir.instructions | Where-Object { $_.sourceLine -eq $line })
    $calls = @($ops | Where-Object { $_.kindName -eq "call" })
    $projections = @($ops | Where-Object {
        $_.runtimeOpName -eq "xvalue-project-scalar-copy"
    })
    if ( $calls.Count -ne 1 -or $projections.Count -ne 1 ) {
        throw "Compound index RHS must call and project exactly once at line $line"
    }
    Assert-OwnedChain $case.Ir $ops $calls[0] $projections[0] "Compound index RHS"
}

$case = Read-Case "hir_xvalue_scalar_bridge_matrix"
$projection = @($case.Ir.instructions | Where-Object {
    $_.sourceLine -eq 19 -and $_.runtimeOpName -eq "xvalue-project-string-copy"
})
if ( $projection.Count -ne 1 -or @($case.Ir.instructions | Where-Object {
        $_.sourceLine -eq 19 -and $_.runtimeOpName -in @("string-copy", "xvalue-to-string")
    }).Count -ne 0 -or @($case.Ir.instructions | Where-Object {
        $_.runtimeOpName -eq "integer-convert-checked" -and
        $_.integerSourceAbiName -eq "xvalue" -and $_.integerTargetName -eq "i64"
    }).Count -ne 7 ) {
    throw "Core scalar bridge lost its single string projection or checked integer ABI"
}

$case = Read-Case "cgen_global_identifier_collision"
$globals = @($case.Ir.storages | Where-Object { $_.kindName -eq "global" })
if ( $globals.Count -ne 8 ) { throw "Global collision fixture lost a storage" }
foreach ( $globalStorage in $globals ) {
    $pattern = '(?m)^static [^\r\n]+ s' + $globalStorage.symbol + ';\r?$'
    if ( [regex]::Matches($case.C, $pattern).Count -ne 1 ) {
        throw "Global storage has no unique symbol-ID-based C declaration: $($globalStorage.symbol)"
    }
}
$send = @($case.Ir.instructions | Where-Object { $_.runtimeOpName -eq "channel-send" })
if ( $send.Count -ne 1 ) { throw "Global channel receiver has no unique send" }
$receiverLoads = @($case.Ir.instructions | Where-Object {
    $_.kindName -eq "load" -and $_.result -eq $send[0].operands[0]
})
$receiverStorage = @()
$receiverStores = @()
$retains = @()
$globalLoads = @()
$receiverReleases = @()
if ( $receiverLoads.Count -eq 1 ) {
    $receiverStorage = @($case.Ir.storages | Where-Object {
        $_.symbol -eq $receiverLoads[0].targetSymbol
    })
    $receiverStores = @($case.Ir.instructions | Where-Object {
        $_.kindName -eq "store" -and
        $_.targetSymbol -eq $receiverLoads[0].targetSymbol -and
        $_.operandCount -eq 1
    })
    if ( $receiverStores.Count -eq 1 ) {
        $retains = @($case.Ir.instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-retain" -and
            $_.result -eq $receiverStores[0].operands[0] -and
            $_.operandCount -eq 1
        })
    }
    if ( $retains.Count -eq 1 ) {
        $globalLoads = @($case.Ir.instructions | Where-Object {
            $_.kindName -eq "load" -and $_.result -eq $retains[0].operands[0] -and
            $_.targetSymbol -in @($globals.symbol)
        })
    }
    $receiverReleases = @($case.Ir.instructions | Where-Object {
        $_.kindName -eq "release" -and
        $_.targetSymbol -eq $receiverLoads[0].targetSymbol
    })
}
$validReceiver = $receiverLoads.Count -eq 1 -and $receiverStorage.Count -eq 1 -and
    $receiverStorage[0].kindName -eq "temporary" -and
    $receiverStorage[0].type -eq $receiverLoads[0].type -and
    $receiverStores.Count -eq 1 -and $retains.Count -eq 1 -and
    $retains[0].resultOwnershipName -eq "move" -and
    $globalLoads.Count -eq 1 -and $globalLoads[0].resultOwnershipName -eq "borrow" -and
    $globalLoads[0].type -eq $receiverLoads[0].type -and
    $globalLoads[0].id -lt $retains[0].id -and
    $retains[0].id -lt $receiverStores[0].id -and
    $receiverStores[0].id -lt $receiverLoads[0].id -and
    $receiverLoads[0].id -lt $send[0].id -and
    $receiverReleases.Count -ge 1 -and
    @($receiverReleases | Where-Object {
        $_.id -le $send[0].id -or $_.type -ne $receiverLoads[0].type
    }).Count -eq 0
if ( ! $validReceiver ) {
    throw "Global channel call lost its retain/store/borrow/release receiver lifetime"
}

$case = Read-Case "cgen_function_direct_owned_args"
if ( @($case.Ir.types | Where-Object {
        @($_.parameterAbis) -contains "borrow"
    }).Count -eq 0 -or @($case.Ir.instructions | Where-Object {
        $_.kindName -eq "closure-call"
    }).Count -ne 1 -or ! $case.C.Contains("xrtValueRetain(") -or
     ! $case.C.Contains("xrtValueRelease(") -or
     ! $case.C.Contains("xlir_closure_release(") ) {
    throw "Repeated callable use lost its borrowed signature or explicit owner cleanup"
}

$case = Read-Case "cgen_implicit_string_methods"
$wrappers = @{}
foreach ( $name in @("trim", "lower") ) {
    $native = @($case.Ir.callTargets | Where-Object {
        $_.cName -eq "xl6_xrt_string_$name" -and $_.reachability -ne "unreachable"
    })
    if ( $native.Count -ne 1 -or @($native[0].parameterAbis | Where-Object {
            $_ -ne 2
        }).Count -ne 0 ) { throw "Implicit string $name has no exact borrowed native ABI" }
    $owners = @($case.Ir.functions | Where-Object {
        @(Function-Ops $case.Ir $_ | Where-Object {
            $_.kindName -eq "call" -and $_.targetSymbol -eq $native[0].symbol
        }).Count -eq 1
    })
    if ( $owners.Count -ne 1 ) { throw "Implicit string $name has no unique resolved wrapper" }
    $wrappers[$name] = $owners[0].symbol
}
$main = @($case.Ir.functions | Where-Object {
    $_.kindName -eq "user" -and $_.reachability -eq "root"
})
if ( $main.Count -ne 1 ) { throw "String method fixture has no unique language entry" }
$ops = @(Function-Ops $case.Ir $main[0] | Where-Object { $_.sourceLine -eq 11 })
$trim = @($ops | Where-Object { $_.kindName -eq "call" -and $_.targetSymbol -eq $wrappers.trim })
$lower = @($ops | Where-Object { $_.kindName -eq "call" -and $_.targetSymbol -eq $wrappers.lower })
if ( $trim.Count -ne 1 -or $lower.Count -ne 1 ) {
    throw "Chained string methods must each execute exactly once"
}
Assert-OwnedChain $case.Ir $ops $trim[0] $lower[0] "Chained string methods"

$case = Read-Case "cgen_generic_class_return_self"
$returns = @($case.Ir.instructions | Where-Object {
    $_.sourceLine -eq 13 -and $_.kindName -eq "return"
})
if ( $returns.Count -ne 2 ) { throw "Generic self return lost a concrete specialization" }
foreach ( $return in $returns ) {
    $resultLoad = @($case.Ir.instructions | Where-Object {
        $_.kindName -eq "load" -and $_.result -eq $return.operands[0]
    })
    $resultStorage = if ( $resultLoad.Count -eq 1 ) { $resultLoad[0].targetSymbol } else { 0 }
    $resultStore = @($case.Ir.instructions | Where-Object {
        $_.kindName -eq "store" -and $_.targetSymbol -eq $resultStorage
    })
    $resultMove = @($case.Ir.instructions | Where-Object {
        $_.kindName -eq "move" -and $_.targetSymbol -eq $resultStorage -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $return.operands[0]
    })
    $retain = @()
    if ( $resultStore.Count -eq 1 ) {
        $retainedValue = $resultStore[0].operands[0]
        $retain = @($case.Ir.instructions | Where-Object {
            $_.result -eq $retainedValue -and $_.runtimeOpName -eq "xvalue-retain"
        })
    }
    $receiverReleases = @()
    if ( $retain.Count -eq 1 ) {
        $retainedType = $retain[0].type
        $receiverReleases = @($case.Ir.instructions | Where-Object {
            $_.id -gt $resultStore[0].id -and $_.id -lt $resultLoad[0].id -and
            $_.kindName -eq "release" -and $_.type -eq $retainedType -and
            $_.targetSymbol -ne $resultStorage
        })
    }
    $storage = @($case.Ir.storages | Where-Object { $_.symbol -eq $resultStorage })
    $returnContractValid =
        $resultLoad.Count -eq 1 -and $resultStore.Count -eq 1 -and
        $resultMove.Count -eq 1 -and $retain.Count -eq 1 -and
        $receiverReleases.Count -eq 1 -and $storage.Count -eq 1 -and
        $storage[0].kindName -eq "temporary" -and
        $retain[0].id -lt $resultStore[0].id -and
        $resultStore[0].id -lt $receiverReleases[0].id -and
        $receiverReleases[0].id -lt $resultLoad[0].id -and
        $resultLoad[0].id -lt $resultMove[0].id -and
        $resultMove[0].id -lt $return.id
    if ( ! $returnContractValid ) {
        throw "Generic self return must acquire its result before releasing its receiver " +
            "(load=$($resultLoad.Count), store=$($resultStore.Count), move=$($resultMove.Count), " +
            "retain=$($retain.Count), release=$($receiverReleases.Count), storage=$($storage.Count))"
    }
}
$init = @($case.Ir.functions | Where-Object { $_.kindName -eq "module-init" })
foreach ( $line in @(22, 27) ) {
    $ops = @(Function-Ops $case.Ir $init[0] | Where-Object { $_.sourceLine -eq $line })
    $calls = @($ops | Where-Object { $_.kindName -eq "call" })
    if ( $calls.Count -ne 2 ) { throw "Generic method chain lost a call at line $line" }
    Assert-OwnedChain $case.Ir $ops $calls[0] $calls[1] "Generic method chain"
}

$case = Read-Case "cgen_typed_container_var_identity"
$init = @($case.Ir.functions | Where-Object { $_.kindName -eq "module-init" })
$initOps = @(Function-Ops $case.Ir $init[0])
foreach ( $line in @(4, 16, 21, 28) ) {
    $ops = @($initOps | Where-Object { $_.sourceLine -eq $line })
    $retain = @($ops | Where-Object { $_.runtimeOpName -eq "xvalue-retain" })
    $rebind = @($ops | Where-Object { $_.kindName -eq "value" })
    if ( $retain.Count -ne 1 -or $rebind.Count -ne 1 -or
         $rebind[0].operands[0] -ne $retain[0].result -or
         $retain[0].type -eq $rebind[0].type -or @($ops | Where-Object {
             $_.kindName -eq "runtime-call" -and $_.runtimeOpName -ne "xvalue-retain"
         }).Count -ne 0 ) {
        throw "Container erasure must preserve its handle through retain/value at line $line"
    }
}
if ( @($initOps | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-create-checked"
    }).Count -ne 1 ) { throw "Dynamic for-in lost its canonical checked snapshot iterator" }

Write-Output "XLIR current core contracts ok: seven fixtures, single evaluation, explicit owner anchors, checked projections, symbol-based globals, chained receiver lifetimes, handle-preserving erasure and deterministic default C."
exit 0
