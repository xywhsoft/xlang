param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_callable_reachability.xl"
$hostSource = Join-Path $Root "test\cases\xlir_callable_reachability_host.c"
$classSource = Join-Path $Root "test\cases\xlir_value_initializer_class_default.xl"
$virtualSource = Join-Path $Root "test\cases\xlir_class_virtual_dispatch.xl"
$protocolSource = Join-Path $Root "test\cases\protocol_dynamic_dispatch.xl"
$asyncSource = Join-Path $Root "test\cases\xlir_async_ready_differential.xl"
$outputDir = Join-Path $Root "build\test"
$jsonPath = Join-Path $outputDir "xlir_callable_reachability.lowered.json"
$cPath = Join-Path $outputDir "xlir_callable_reachability_generated.c"
$classJsonPath = Join-Path $outputDir "xlir_callable_reachability_class.lowered.json"
$classCPath = Join-Path $outputDir "xlir_callable_reachability_class.c"
$virtualJsonPath = Join-Path $outputDir "xlir_callable_reachability_virtual.lowered.json"
$virtualCPath = Join-Path $outputDir "xlir_callable_reachability_virtual.c"
$protocolJsonPath = Join-Path $outputDir "xlir_callable_reachability_protocol.lowered.json"
$protocolCPath = Join-Path $outputDir "xlir_callable_reachability_protocol.c"
$asyncJsonPath = Join-Path $outputDir "xlir_callable_reachability_async.lowered.json"
$asyncCPath = Join-Path $outputDir "xlir_callable_reachability_async.c"
$exePath = Join-Path $outputDir "xlir_callable_reachability_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $classSource, $virtualSource, $protocolSource, $asyncSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR reachability gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Read-Json([string]$Path) {
    return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Get-InstructionOwner($Module, $Instruction) {
    $index = [int64]$Instruction.id - 1
    return @($Module.functions | Where-Object {
        $index -ge [int64]$_.firstInstruction -and
        $index -lt [int64]$_.firstInstruction + [int64]$_.instructionCount
    })
}

function Assert-DispatchClosure($Module, [string]$InstructionKind, [string]$ImplProperty) {
    $calls = @($Module.instructions | Where-Object { $_.kindName -eq $InstructionKind })
    if ( $calls.Count -eq 0 ) {
        throw "XLIR reachability fixture has no $InstructionKind instruction"
    }
    $impls = if ( $ImplProperty -eq 'classDispatchImpls' ) {
        @($Module.classDispatchImpls)
    } else {
        @($Module.protocolDispatchImpls)
    }
    foreach ( $call in $calls ) {
        $owner = @(Get-InstructionOwner $Module $call)
        if ( $owner.Count -ne 1 -or $owner[0].reachability -eq 'unreachable' ) {
            throw "XLIR reachable dispatch instruction has no reachable owner"
        }
        $rows = @($impls | Where-Object { [int64]$_.slot -eq [int64]$call.dispatchSlot - 1 })
        if ( $rows.Count -eq 0 ) {
            throw "XLIR reachable dispatch slot has no implementation rows"
        }
        foreach ( $row in $rows ) {
            $target = @($Module.callTargets | Where-Object {
                [int64]$_.symbol -eq [int64]$row.implementationSymbol
            })
            if ( $target.Count -ne 1 -or $target[0].reachability -eq 'unreachable' ) {
                throw "XLIR dispatch implementation escaped the transitive callable closure"
            }
        }
    }
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable reachability lowering failed" }
$module = Read-Json $jsonPath
$functions = @($module.functions)
$targets = @($module.callTargets)
$closures = @($module.closureTargets)
$moduleRoots = @($functions | Where-Object { $_.kind -eq 1 -and $_.reachability -eq 'root' })
$exported = @($module.exports)
if ( $moduleRoots.Count -ne 1 -or $exported.Count -ne 1 ) {
    throw "Lowered XLIR does not expose one module-init root and one export root"
}
$exportFunction = @($functions | Where-Object { $_.symbol -eq $exported[0].symbol })
if ( $exportFunction.Count -ne 1 -or $exportFunction[0].reachability -ne 'root' ) {
    throw "XLIR exported function is not a canonical callable root"
}

$liveTarget = @($targets | Where-Object { $_.cName -eq 'xlir_reachability_live_external' })
$deadTarget = @($targets | Where-Object { $_.cName -eq 'xlir_reachability_dead_external' })
if ( $liveTarget.Count -ne 1 -or $liveTarget[0].reachability -ne 'transitive' -or
     $deadTarget.Count -ne 1 -or $deadTarget[0].reachability -ne 'unreachable' ) {
    throw "XLIR extern target reachability does not match the root call closure"
}
$liveCall = @($module.instructions | Where-Object {
    $_.kindName -eq 'call' -and $_.targetSymbol -eq $liveTarget[0].symbol
})
$deadCall = @($module.instructions | Where-Object {
    $_.kindName -eq 'call' -and $_.targetSymbol -eq $deadTarget[0].symbol
})
$liveOwner = @(if ( $liveCall.Count -eq 1 ) { Get-InstructionOwner $module $liveCall[0] })
$deadOwner = @(if ( $deadCall.Count -eq 1 ) { Get-InstructionOwner $module $deadCall[0] })
if ( $liveOwner.Count -ne 1 -or $liveOwner[0].reachability -ne 'transitive' -or
     $deadOwner.Count -ne 1 -or $deadOwner[0].reachability -ne 'unreachable' ) {
    throw "XLIR function reachability does not distinguish live and dead call owners " +
          "(liveCalls=$($liveCall.Count), liveOwners=$($liveOwner.Count), live=$($liveOwner[0].reachability), " +
          "deadCalls=$($deadCall.Count), deadOwners=$($deadOwner.Count), dead=$($deadOwner[0].reachability))"
}
if ( $closures.Count -ne 1 -or $closures[0].reachability -ne 'transitive' ) {
    throw "XLIR closure construction did not mark its body transitively reachable"
}
$closureFunction = @($functions | Where-Object { $_.symbol -eq $closures[0].symbol })
if ( $closureFunction.Count -ne 1 -or $closureFunction[0].reachability -ne 'transitive' ) {
    throw "XLIR reachable ClosureTarget has no matching reachable Function"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable reachability Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    'xlir_reachability_live_external',
    ('xlir_fn_{0}(' -f $liveOwner[0].symbol),
    ('xlir_fn_{0}(' -f $closureFunction[0].symbol),
    'xlir_reachability_probe'
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR Cgen omitted reachable callable artifact: $required"
    }
}
foreach ( $forbidden in @(
    'xlir_reachability_dead_external',
    ('xlir_fn_{0}(' -f $deadOwner[0].symbol)
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "XLIR Cgen emitted unreachable callable artifact: $forbidden"
    }
}

# The original regression: a class with an unused string member wrapper must
# not introduce the wrapper's native symbol into the generated translation unit.
& $xl ir $classSource --lowered --format json --emit $classJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class reachability lowering failed" }
$classModule = Read-Json $classJsonPath
$unusedStringCount = @($classModule.callTargets | Where-Object { $_.cName -eq 'xl6_xrt_string_count' })
if ( $unusedStringCount.Count -ne 1 -or $unusedStringCount[0].reachability -ne 'unreachable' ) {
    throw "XLIR unused standard-library wrapper is not explicitly unreachable"
}
& $xl cgen $classSource --backend xlir --emit $classCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class reachability Cgen failed" }
$classGenerated = [System.IO.File]::ReadAllText($classCPath, [System.Text.Encoding]::UTF8)
if ( $classGenerated.Contains('xl6_xrt_string_count') ) {
    throw "XLIR Cgen retained an unreachable standard-library wrapper dependency"
}

& $xl ir $virtualSource --lowered --format json --emit $virtualJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR virtual reachability lowering failed" }
$virtualModule = Read-Json $virtualJsonPath
Assert-DispatchClosure $virtualModule 'virtual-call' 'classDispatchImpls'
& $xl cgen $virtualSource --backend xlir --emit $virtualCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR virtual reachability Cgen failed" }
if ( ! [System.IO.File]::ReadAllText($virtualCPath, [System.Text.Encoding]::UTF8).Contains('xlir_vdispatch_') ) {
    throw "XLIR Cgen omitted a reachable virtual dispatcher"
}

& $xl ir $protocolSource --lowered --format json --emit $protocolJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR protocol reachability lowering failed" }
$protocolModule = Read-Json $protocolJsonPath
Assert-DispatchClosure $protocolModule 'protocol-call' 'protocolDispatchImpls'
& $xl cgen $protocolSource --backend xlir --emit $protocolCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR protocol reachability Cgen failed" }
if ( ! [System.IO.File]::ReadAllText($protocolCPath, [System.Text.Encoding]::UTF8).Contains('xlir_pdispatch_') ) {
    throw "XLIR Cgen omitted a reachable protocol dispatcher"
}

& $xl ir $asyncSource --lowered --format json --emit $asyncJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async reachability lowering failed" }
$asyncModule = Read-Json $asyncJsonPath
$asyncFunctions = @($asyncModule.functions | Where-Object { $_.kind -eq 3 })
if ( $asyncFunctions.Count -eq 0 -or
     @($asyncFunctions | Where-Object { $_.reachability -eq 'unreachable' }).Count -ne 0 ) {
    throw "XLIR async start did not retain its worker in the transitive closure"
}
foreach ( $function in $asyncFunctions ) {
    $target = @($asyncModule.callTargets | Where-Object { $_.symbol -eq $function.symbol })
    if ( $target.Count -ne 1 -or $target[0].reachability -eq 'unreachable' ) {
        throw "XLIR reachable async worker has no reachable CallTarget"
    }
}
& $xl cgen $asyncSource --backend xlir --emit $asyncCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async reachability Cgen failed" }
if ( ! [System.IO.File]::ReadAllText($asyncCPath, [System.Text.Encoding]::UTF8).Contains('xlir_async_start_') ) {
    throw "XLIR Cgen omitted a reachable async launcher"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable reachability generated-C runtime failed" }

Write-Output "XLIR callable reachability gate ok: roots, direct/extern/closure/virtual/protocol/async edges, dead standard-library pruning, strict C, and runtime."
exit 0
