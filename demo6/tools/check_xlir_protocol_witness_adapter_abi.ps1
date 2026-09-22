param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$nativeSupport = Join-Path $Root "test\cases\protocol_native_backed_host.c"
$outputDir = Join-Path $Root "build\tmp"

$cases = @(
    [pscustomobject]@{
        Name = "protocol_inherited_dispatch"
        Source = Join-Path $Root "test\cases\protocol_inherited_dispatch.xl"
        Support = ""
        Expected = "protocol inherited dispatch ok"
    },
    [pscustomobject]@{
        Name = "protocol_native_backed_dispatch"
        Source = Join-Path $Root "test\cases\protocol_native_backed_dispatch.xl"
        Support = $nativeSupport
        Expected = "native-backed protocol dispatch ok"
    }
)

foreach ( $path in @($xl, $genericHost, $nativeSupport) + @($cases.Source) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR protocol witness adapter gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Read-Json([string]$Path) {
    return [System.IO.File]::ReadAllText(
        $Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Get-TypeByName($Module, [string]$Name) {
    $items = @($Module.types | Where-Object { $_.name -eq $Name })
    if ( $items.Count -ne 1 ) {
        throw "XLIR protocol witness adapter type is unresolved: $Name"
    }
    return $items[0]
}

function Invoke-GeneratedRuntime($Case, [string]$Generated) {
    $exe = Join-Path $outputDir "$($Case.Name).xlir.gate.exe"
    $generatedForward = ([System.IO.Path]::GetFullPath($Generated)) -replace '\\', '/'
    $define = '-DXLIR_GENERATED_MODULE=\"' + $generatedForward + '\"'
    $command = 'call test\suites\common.bat && ' +
        '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror ' +
        $define + ' "' + $genericHost + '"'
    if ( $Case.Support -ne "" ) { $command += ' "' + $Case.Support + '"' }
    $command += ' !XL6_CGEN_LINKS! -o "' + $exe + '"'
    $compileOutput = @(& $env:ComSpec /d /v:on /c $command 2>&1) -join "`n"
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR protocol witness strict generated-C compile failed: $($Case.Name): $compileOutput"
    }
    $output = @(& $exe 2>&1) -join "`n"
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR protocol witness generated-C runtime mismatch: $($Case.Name): $output"
    }
    return "ok"
}

$inherited = $cases[0]
$inheritedTypedPath = Join-Path $outputDir "$($inherited.Name).witness.typed.json"
$inheritedLoweredPath = Join-Path $outputDir "$($inherited.Name).witness.lowered.json"
& $xl ir $inherited.Source --typed --format json --emit $inheritedTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR inherited protocol witness Typed IR failed" }
& $xl ir $inherited.Source --lowered --format json --emit $inheritedLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR inherited protocol witness lowering failed" }
$inheritedTyped = Read-Json $inheritedTypedPath
$inheritedLowered = Read-Json $inheritedLoweredPath
$entity = Get-TypeByName $inheritedTyped "Entity"
$player = Get-TypeByName $inheritedTyped "Player"
$named = Get-TypeByName $inheritedTyped "Named"
$entityLayout = @($inheritedTyped.classLayouts | Where-Object { $_.type -eq $entity.id })[0]
$playerLayout = @($inheritedTyped.classLayouts | Where-Object { $_.type -eq $player.id })[0]
$namedSlot = @($inheritedTyped.protocolDispatchSlots | Where-Object {
    $_.protocolType -eq $named.id
})
$namedImpls = @($inheritedTyped.protocolDispatchImpls | Where-Object {
    $_.slot -eq $namedSlot[0].id
})
$entityImpl = @($namedImpls | Where-Object { $_.ownerType -eq $entity.id })
$playerImpl = @($namedImpls | Where-Object { $_.ownerType -eq $player.id })
$namedSet = @($inheritedTyped.nominalIdentitySets | Where-Object {
    $_.targetType -eq $named.id
})
if ( $null -eq $entityLayout -or $null -eq $playerLayout -or
     $playerLayout.baseType -ne $entity.id -or $namedSlot.Count -ne 1 -or
     $entityImpl.Count -ne 1 -or $playerImpl.Count -ne 1 -or
     $entityImpl[0].implementationDeclaration -ne
         $playerImpl[0].implementationDeclaration -or
     $entityImpl[0].implementationSymbol -ne $playerImpl[0].implementationSymbol -or
     $namedSet.Count -ne 1 -or $namedSet[0].kind -ne 3 -or
     @($namedSet[0].identities).Count -ne 2 -or
     @($namedSet[0].identities) -notcontains $entityLayout.runtimeTypeTag -or
     @($namedSet[0].identities) -notcontains $playerLayout.runtimeTypeTag ) {
    throw "Typed XLIR does not own exact inherited protocol witness rows and nominal closure"
}
$inheritedTarget = @($inheritedLowered.callTargets | Where-Object {
    $_.symbol -eq $playerImpl[0].implementationSymbol
})
if ( $inheritedTarget.Count -ne 1 -or $inheritedTarget[0].kind -ne 1 -or
     @($inheritedTarget[0].parameterTypes).Count -lt 1 -or
     @($inheritedTarget[0].parameterTypes)[0] -ne $entity.id ) {
    throw "Lowered XLIR inherited witness does not preserve its base-declared callable ABI"
}

$native = $cases[1]
$nativeTypedPath = Join-Path $outputDir "$($native.Name).witness.typed.json"
$nativeLoweredPath = Join-Path $outputDir "$($native.Name).witness.lowered.json"
& $xl ir $native.Source --typed --format json --emit $nativeTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native protocol witness Typed IR failed" }
& $xl ir $native.Source --lowered --format json --emit $nativeLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native protocol witness lowering failed" }
$nativeTyped = Read-Json $nativeTypedPath
$nativeLowered = Read-Json $nativeLoweredPath
$readable = Get-TypeByName $nativeTyped "Readable"
$counter = Get-TypeByName $nativeTyped "NativeCounter"
$counterLayout = @($nativeTyped.classLayouts | Where-Object { $_.type -eq $counter.id })[0]
$counterField = @($nativeTyped.classFields | Where-Object { $_.ownerType -eq $counter.id })
$readableSlot = @($nativeTyped.protocolDispatchSlots | Where-Object {
    $_.protocolType -eq $readable.id
})
$readableImpl = @($nativeTyped.protocolDispatchImpls | Where-Object {
    $_.slot -eq $readableSlot[0].id -and $_.ownerType -eq $counter.id
})
$readableSet = @($nativeTyped.nominalIdentitySets | Where-Object {
    $_.targetType -eq $readable.id
})
if ( $null -eq $counterLayout -or $counterField.Count -ne 1 -or
     $readableSlot.Count -ne 1 -or $readableImpl.Count -ne 1 -or
     $readableSet.Count -ne 1 -or $readableSet[0].kind -ne 3 -or
     @($readableSet[0].identities).Count -ne 1 -or
     @($readableSet[0].identities)[0] -ne $counterLayout.runtimeTypeTag ) {
    throw "Typed XLIR does not own the native-backed protocol witness and nominal identity"
}
$nativeTarget = @($nativeLowered.callTargets | Where-Object {
    $_.symbol -eq $readableImpl[0].implementationSymbol
})
if ( $nativeTarget.Count -ne 1 -or $nativeTarget[0].kind -ne 2 -or
     $nativeTarget[0].cName -ne "protocol_native_counter_value" -or
     $nativeTarget[0].resultPhysicalAbiName -ne "direct" -or
     $nativeTarget[0].errorAbiName -ne "no-raise" -or
     $nativeTarget[0].parameterCount -ne 1 -or
     @($nativeTarget[0].parameterTypes)[0] -ne $counter.id -or
     @($nativeTarget[0].parameterAbis)[0] -ne 2 -or
     @($nativeTarget[0].parameterPhysicalAbis)[0] -ne
         "native-class-handle-borrow" -or
     @($nativeTarget[0].parameterPhysicalFieldDeclarations)[0] -ne
         $counterField[0].declaration -or
     @($nativeLowered.instructions | Where-Object {
         $_.kindName -eq "protocol-call" -and
         $_.dispatchSlot -eq ($readableSlot[0].id + 1)
     }).Count -ne 1 ) {
    throw "Lowered XLIR native protocol witness has no exact extern field-borrow adapter ABI"
}

$generatedOutputs = @{}
foreach ( $case in $cases ) {
    $generated = Join-Path $outputDir "$($case.Name).witness.c"
    $generatedSecond = Join-Path $outputDir "$($case.Name).witness.second.c"
    & $xl cgen $case.Source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR protocol witness C generation failed: $($case.Name)"
    }
    & $xl cgen $case.Source --backend xlir --emit $generatedSecond
    if ( $LASTEXITCODE -ne 0 -or
         ! [System.Linq.Enumerable]::SequenceEqual(
             [byte[]][System.IO.File]::ReadAllBytes($generated),
             [byte[]][System.IO.File]::ReadAllBytes($generatedSecond)) ) {
        throw "XLIR protocol witness generated C is not byte-for-byte deterministic: $($case.Name)"
    }
    $generatedOutputs[$case.Name] = [System.IO.File]::ReadAllText(
        $generated, [System.Text.Encoding]::UTF8)
    [void](Invoke-GeneratedRuntime $case $generated)
}

$inheritedC = $generatedOutputs[$inherited.Name]
foreach ( $required in @(
    "case $($entityLayout.runtimeTypeTag): return xlir_fn_$($entityImpl[0].implementationSymbol)(receiver);",
    "case $($playerLayout.runtimeTypeTag): return xlir_fn_$($playerImpl[0].implementationSymbol)(receiver);"
) ) {
    if ( ! $inheritedC.Contains($required) ) {
        throw "XLIR inherited protocol dispatcher is not mechanical: $required"
    }
}
$nativeC = $generatedOutputs[$native.Name]
foreach ( $required in @(
    "extern int64_t protocol_native_counter_value(void*);",
    "case $($counterLayout.runtimeTypeTag): return protocol_native_counter_value(",
    "xlir_xvalue_object_get_literal_native_handle(receiver, xrtStrView(`"handle`"))"
) ) {
    if ( ! $nativeC.Contains($required) ) {
        throw "XLIR native protocol dispatcher is missing its verified adapter ABI: $required"
    }
}

Write-Output "XLIR protocol witness adapter gate passed: inherited concrete-owner rows, native-class-handle-borrow extern adapters, nominal identity closure, byte-for-byte deterministic C, and strict runtime."
