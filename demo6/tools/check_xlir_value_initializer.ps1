param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$defaultSource = Join-Path $Root "test\cases\xlir_value_initializer_defaults.xl"
$classDefaultSource = Join-Path $Root "test\cases\xlir_value_initializer_class_default.xl"
$classNullSource = Join-Path $Root "test\cases\xlir_value_initializer_class_null.xl"
$ownershipSource = Join-Path $Root "test\cases\cgen_native_class_ownership.xl"
$nativeNullSource = Join-Path $Root "test\cases\cgen_native_class_null_destroy.xl"
$containerSource = Join-Path $Root "test\cases\xlir_value_initializer_containers.xl"
$packageExportSource = Join-Path $Root "test\cases\xlir_value_initializer_package_exports.xl"
$scalarSource = Join-Path $Root "test\cases\cgen_safe_widening.xl"
$callableSource = Join-Path $Root "test\cases\function_types.xl"
$hostSource = Join-Path $Root "test\cases\xlir_value_initializer_host.c"
$genericHostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\test"
$defaultTypedPath = Join-Path $outputDir "xlir_value_initializer_default.typed.json"
$defaultLoweredPath = Join-Path $outputDir "xlir_value_initializer_default.lowered.json"
$classDefaultTypedPath = Join-Path $outputDir "xlir_value_initializer_class_default.typed.json"
$classDefaultLoweredPath = Join-Path $outputDir "xlir_value_initializer_class_default.lowered.json"
$classNullTypedPath = Join-Path $outputDir "xlir_value_initializer_class_null.typed.json"
$classNullLoweredPath = Join-Path $outputDir "xlir_value_initializer_class_null.lowered.json"
$ownershipTypedPath = Join-Path $outputDir "xlir_value_initializer_native_ownership.typed.json"
$ownershipLoweredPath = Join-Path $outputDir "xlir_value_initializer_native_ownership.lowered.json"
$nativeNullTypedPath = Join-Path $outputDir "xlir_value_initializer_native_null.typed.json"
$nativeNullLoweredPath = Join-Path $outputDir "xlir_value_initializer_native_null.lowered.json"
$containerTypedPath = Join-Path $outputDir "xlir_value_initializer_containers.typed.json"
$containerLoweredPath = Join-Path $outputDir "xlir_value_initializer_containers.lowered.json"
$packageExportTypedPath = Join-Path $outputDir "xlir_value_initializer_package_exports.typed.json"
$packageExportLoweredPath = Join-Path $outputDir "xlir_value_initializer_package_exports.lowered.json"
$scalarTypedPath = Join-Path $outputDir "xlir_value_initializer_scalar.typed.json"
$scalarLoweredPath = Join-Path $outputDir "xlir_value_initializer_scalar.lowered.json"
$callableTypedPath = Join-Path $outputDir "xlir_value_initializer_callable.typed.json"
$callableLoweredPath = Join-Path $outputDir "xlir_value_initializer_callable.lowered.json"
$defaultCPath = Join-Path $outputDir "xlir_value_initializer_default_generated.c"
$classDefaultCPath = Join-Path $outputDir "xlir_value_initializer_class_default_generated.c"
$classNullCPath = Join-Path $outputDir "xlir_value_initializer_class_null_generated.c"
$ownershipCPath = Join-Path $outputDir "xlir_value_initializer_native_ownership_generated.c"
$nativeNullCPath = Join-Path $outputDir "xlir_value_initializer_native_null_generated.c"
$containerCPath = Join-Path $outputDir "xlir_value_initializer_containers_generated.c"
$packageExportCPath = Join-Path $outputDir "xlir_value_initializer_package_exports_generated.c"
$scalarCPath = Join-Path $outputDir "xlir_value_initializer_scalar_generated.c"
$callableCPath = Join-Path $outputDir "xlir_value_initializer_callable_generated.c"
$defaultExePath = Join-Path $outputDir "xlir_value_initializer_default_host.exe"
$classDefaultExePath = Join-Path $outputDir "xlir_value_initializer_class_default_host.exe"
$classNullExePath = Join-Path $outputDir "xlir_value_initializer_class_null_host.exe"
$ownershipExePath = Join-Path $outputDir "xlir_value_initializer_native_ownership_host.exe"
$nativeNullExePath = Join-Path $outputDir "xlir_value_initializer_native_null_host.exe"
$containerExePath = Join-Path $outputDir "xlir_value_initializer_containers_host.exe"
$packageExportExePath = Join-Path $outputDir "xlir_value_initializer_package_exports_host.exe"
$scalarExePath = Join-Path $outputDir "xlir_value_initializer_scalar_host.exe"
$callableExePath = Join-Path $outputDir "xlir_value_initializer_callable_host.exe"

foreach ( $path in @($xl, $defaultSource, $classDefaultSource, $classNullSource, $ownershipSource, $nativeNullSource,
                      $containerSource, $packageExportSource, $scalarSource, $callableSource,
                      $hostSource, $genericHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR ValueInitializer gate input is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Read-Json([string]$Path) {
    return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

& $xl ir $classDefaultSource --format json --emit $classDefaultTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-default ValueInitializer Typed IR failed" }
$classDefaultTyped = Read-Json $classDefaultTypedPath
$classInitializers = @($classDefaultTyped.valueInitializers)
$classFields = @($classDefaultTyped.classFields)
if ( $classDefaultTyped.valueInitializerCount -ne 1 -or
     $classInitializers[0].abiName -ne 'class-default' -or
     @($classFields | Where-Object { $_.initializerAbiName -eq 'scalar-zero' }).Count -ne 2 -or
     @($classFields | Where-Object { $_.initializerAbiName -eq 'string-empty' }).Count -ne 1 -or
     @($classFields | Where-Object { $_.initializerAbiName -eq 'union-zero' }).Count -ne 1 ) {
    throw "Typed XLIR does not fix class-default and ordered scalar/string/time/union field initializers"
}

& $xl ir $classNullSource --format json --emit $classNullTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-null ValueInitializer Typed IR failed" }
$classNullTyped = Read-Json $classNullTypedPath
$classNullInitializers = @($classNullTyped.valueInitializers)
if ( $classNullTyped.valueInitializerCount -ne 1 -or
     @($classNullInitializers | Where-Object {
         $_.abiName -eq 'class-null' -and $_.expression -eq 0 -and $_.runtimeOpName -eq 'none'
     }).Count -ne 1 -or
     @($classNullTyped.classLayouts | Where-Object { $_.hasExplicitCreate }).Count -ne 1 ) {
    throw "Typed XLIR does not distinguish explicit-constructor class null defaults from construction"
}

& $xl ir $defaultSource --format json --emit $defaultTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR default ValueInitializer Typed IR failed" }
$defaultTyped = Read-Json $defaultTypedPath
$defaultInitializers = @($defaultTyped.valueInitializers)
if ( $defaultTyped.valueInitializerCount -ne 5 -or
     @($defaultInitializers | Where-Object { $_.abiName -eq 'scalar-zero' }).Count -ne 3 -or
     @($defaultInitializers | Where-Object { $_.abiName -eq 'string-empty' }).Count -ne 1 -or
     @($defaultInitializers | Where-Object { $_.abiName -eq 'null-sentinel' }).Count -ne 1 -or
     @($defaultInitializers | Where-Object { $_.expression -ne 0 }).Count -ne 0 ) {
    throw "Typed XLIR does not fix scalar-zero, string-empty, and point null-sentinel defaults"
}

& $xl ir $ownershipSource --format json --emit $ownershipTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native ownership ValueInitializer Typed IR failed" }
$ownershipTyped = Read-Json $ownershipTypedPath
$ownershipInitializers = @($ownershipTyped.valueInitializers)
if ( $ownershipTyped.valueInitializerCount -ne 7 -or
     @($ownershipInitializers | Where-Object { $_.abiName -eq 'explicit' -and $_.expression -ne 0 }).Count -ne 6 -or
     @($ownershipInitializers | Where-Object { $_.abiName -eq 'class-default' -and $_.expression -eq 0 }).Count -ne 1 ) {
    throw "Typed XLIR does not separate explicit values from managed native-class defaults"
}

& $xl ir $nativeNullSource --format json --emit $nativeNullTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owning native null ValueInitializer Typed IR failed" }
$nativeNullTyped = Read-Json $nativeNullTypedPath
$nativeNullInitializers = @($nativeNullTyped.valueInitializers)
if ( $nativeNullTyped.valueInitializerCount -ne 1 -or
     $nativeNullInitializers[0].abiName -ne 'class-default' -or
     $nativeNullInitializers[0].expression -ne 0 ) {
    throw "Typed XLIR does not give an uninitialized owning native class a default object"
}

& $xl ir $containerSource --format json --emit $containerTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR empty-container ValueInitializer Typed IR failed" }
$containerTyped = Read-Json $containerTypedPath
$containerInitializers = @($containerTyped.valueInitializers)
$expectedContainerOps = @('xvalue-array-new', 'xvalue-int-map-new', 'xvalue-object-new', 'xvalue-set-new')
if ( $containerTyped.valueInitializerCount -ne 4 -or
     @($containerInitializers | Where-Object { $_.abiName -eq 'xvalue-container-empty' }).Count -ne 4 -or
     @($containerInitializers | Where-Object { $_.expression -ne 0 }).Count -ne 0 ) {
    throw "Typed XLIR does not give every erased container an owned empty initializer ABI"
}
foreach ( $runtimeOp in $expectedContainerOps ) {
    if ( @($containerInitializers | Where-Object { $_.runtimeOpName -eq $runtimeOp }).Count -ne 1 ) {
        throw "Typed XLIR empty-container initializer lost runtime operation: $runtimeOp"
    }
}

& $xl ir $packageExportSource --format json --emit $packageExportTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR package-export ValueInitializer Typed IR failed" }
$packageExportTyped = Read-Json $packageExportTypedPath
$packageExportInitializers = @($packageExportTyped.valueInitializers)
if ( $packageExportTyped.valueInitializerCount -ne 3 -or
     @($packageExportInitializers | Where-Object {
         $_.abiName -eq 'explicit' -and $_.expression -ne 0
     }).Count -ne 3 -or
     $packageExportTyped.globalExportCount -ne 0 ) {
    throw "Typed XLIR does not separate module package exports from explicit C GlobalExport storage"
}

& $xl ir $scalarSource --format json --emit $scalarTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR scalar ValueInitializer Typed IR failed" }
$scalarTyped = Read-Json $scalarTypedPath
$scalarInitializers = @($scalarTyped.valueInitializers)
foreach ( $family in @('integer-to-integer', 'float-to-float', 'bool-to-integer', 'bool-to-float') ) {
    if ( @($scalarInitializers | Where-Object {
             $_.abiName -eq 'explicit' -and $_.scalarConversionAbiName -eq $family
         }).Count -lt 1 ) {
        throw "Typed XLIR ValueInitializer lost explicit scalar adaptation: $family"
    }
}
if ( @($scalarInitializers | Where-Object {
         $_.abiName -ne 'explicit' -and $_.scalarConversionAbiName -ne 'invalid'
     }).Count -ne 0 ) {
    throw "Typed XLIR default ValueInitializer carries an ambiguous scalar adaptation"
}

& $xl ir $defaultSource --lowered --format json --emit $defaultLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR default ValueInitializer lowering failed" }
$defaultLowered = Read-Json $defaultLoweredPath
if ( @($defaultLowered.instructions | Where-Object { $_.kindName -eq 'const' }).Count -lt 3 -or
     @($defaultLowered.instructions | Where-Object { $_.kindName -eq 'null' }).Count -lt 1 -or
     @($defaultLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'string-dup'
     }).Count -lt 1 ) {
    throw "Lowered XLIR does not materialize typed scalar/string/pointer defaults"
}

& $xl ir $classDefaultSource --lowered --format json --emit $classDefaultLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-default ValueInitializer lowering failed" }
$classDefaultLowered = Read-Json $classDefaultLoweredPath
if ( @($classDefaultLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-class-new'
     }).Count -lt 1 -or
     @($classDefaultLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'string-dup'
     }).Count -lt 1 -or
     @($classDefaultLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-time'
     }).Count -ne 1 -or
     @($classDefaultLowered.instructions | Where-Object {
         $_.kindName -eq 'union-zero'
     }).Count -ne 1 -or
     @($classDefaultLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-union-box'
     }).Count -ne 1 -or
     @($classDefaultLowered.instructions | Where-Object { $_.kindName -eq 'release' }).Count -lt 1 ) {
    throw "Lowered XLIR class default has no object allocation, typed time/union zero, string owner, or explicit release"
}

& $xl ir $classNullSource --lowered --format json --emit $classNullLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-null ValueInitializer lowering failed" }
$classNullLowered = Read-Json $classNullLoweredPath
$classNullOrigin = $classNullInitializers[0].declaration
if ( @($classNullLowered.instructions | Where-Object {
         $_.origin -eq $classNullOrigin -and
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-null'
     }).Count -ne 1 -or
     @($classNullLowered.instructions | Where-Object {
         $_.origin -eq $classNullOrigin -and
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-class-new'
     }).Count -ne 0 ) {
    throw "Lowered XLIR class null default is not an explicit null materialization without construction"
}

& $xl ir $ownershipSource --lowered --format json --emit $ownershipLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native ownership ValueInitializer lowering failed" }
$ownershipLowered = Read-Json $ownershipLoweredPath
$ownershipDefault = @($ownershipLowered.valueInitializers | Where-Object {
    $_.abiName -eq 'class-default'
})
$ownershipDefaultField = @($ownershipLowered.classFields | Where-Object {
    $ownershipDefault.Count -eq 1 -and
    $_.ownerType -eq $ownershipDefault[0].type -and
    $_.initializerAbiName -eq 'null-sentinel'
})
if ( $ownershipDefault.Count -ne 1 -or $ownershipDefaultField.Count -ne 1 -or
     @($ownershipLowered.instructions | Where-Object {
         $_.origin -eq $ownershipDefault[0].declaration -and
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-class-new'
     }).Count -ne 1 -or
     @($ownershipLowered.instructions | Where-Object {
         $_.origin -eq $ownershipDefaultField[0].declaration -and
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-null'
     }).Count -ne 1 ) {
    throw "Lowered XLIR loses the borrowed native class-default object or null field ABI"
}

& $xl ir $nativeNullSource --lowered --format json --emit $nativeNullLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owning native null ValueInitializer lowering failed" }
$nativeNullLowered = Read-Json $nativeNullLoweredPath
$destroyTarget = @($nativeNullLowered.callTargets | Where-Object { $_.cName -eq 'borrowed_box_close' })
$nativeNullDefault = @($nativeNullLowered.valueInitializers | Where-Object {
    $_.abiName -eq 'class-default'
})
$nativeNullField = @($nativeNullLowered.classFields | Where-Object {
    $nativeNullDefault.Count -eq 1 -and
    $_.ownerType -eq $nativeNullDefault[0].type -and
    $_.initializerAbiName -eq 'null-sentinel'
})
if ( $destroyTarget.Count -ne 1 -or $nativeNullDefault.Count -ne 1 -or
     $nativeNullField.Count -ne 1 -or
     $nativeNullField[0].nativeDestroySymbol -ne $destroyTarget[0].symbol -or
     @($nativeNullLowered.instructions | Where-Object {
         $_.origin -eq $nativeNullDefault[0].declaration -and
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-class-new'
     }).Count -ne 1 -or
     @($nativeNullLowered.instructions | Where-Object {
         $_.origin -eq $nativeNullField[0].declaration -and
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-null'
     }).Count -ne 1 -or
     @($nativeNullLowered.instructions | Where-Object {
         $_.kindName -eq 'release' -and $_.type -eq $nativeNullDefault[0].type -and
         $_.targetSymbol -in @($nativeNullLowered.storages | Where-Object {
             $_.kindName -eq 'local' -and $_.declaration -eq $nativeNullDefault[0].declaration
         } | ForEach-Object { $_.symbol })
     }).Count -lt 1 ) {
    throw "Lowered XLIR owning native class default has no null field and destroy-aware release ABI"
}

& $xl ir $containerSource --lowered --format json --emit $containerLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR empty-container ValueInitializer lowering failed" }
$containerLowered = Read-Json $containerLoweredPath
foreach ( $runtimeOp in $expectedContainerOps ) {
    if ( @($containerLowered.instructions | Where-Object {
             $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq $runtimeOp
         }).Count -ne 1 ) {
        throw "Lowered XLIR empty-container initializer lost runtime operation: $runtimeOp"
    }
}


& $xl ir $packageExportSource --lowered --format json --emit $packageExportLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR package-export ValueInitializer lowering failed" }
$packageExportLowered = Read-Json $packageExportLoweredPath
if ( @($packageExportLowered.storages | Where-Object { $_.kindName -eq 'global' }).Count -ne 3 -or
     @($packageExportLowered.instructions | Where-Object {
         $_.kindName -eq 'store' -and
         $_.targetSymbol -in @($packageExportLowered.storages | Where-Object kindName -eq 'global' | ForEach-Object { $_.symbol }) -and
         $packageExportInitializers.declaration -contains $_.origin
     }).Count -ne 3 -or
     @($packageExportLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-string-copy'
     }).Count -lt 1 -or
     $packageExportLowered.globalExportCount -ne 0 ) {
    throw "Lowered XLIR package exports have no explicit module-global initialization/ownership ABI"
}

& $xl ir $scalarSource --lowered --format json --emit $scalarLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR scalar ValueInitializer lowering failed" }
$scalarLowered = Read-Json $scalarLoweredPath
foreach ( $family in @('integer-to-integer', 'float-to-float', 'bool-to-integer', 'bool-to-float') ) {
    if ( @($scalarLowered.instructions | Where-Object {
             $_.kindName -eq 'scalar-convert' -and
             $_.scalarConversionAbiName -eq $family
         }).Count -lt 1 ) {
        throw "Lowered XLIR ValueInitializer lost explicit scalar adaptation: $family"
    }
}

& $xl ir $callableSource --typed --format json --emit $callableTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable ValueInitializer Typed IR failed" }
$callableTyped = Read-Json $callableTypedPath
$callableInitializers = @($callableTyped.valueInitializers)
if ( $callableInitializers.Count -ne 5 -or
     @($callableInitializers | Where-Object { $_.abiName -eq 'xvalue-null' }).Count -ne 1 -or
     @($callableInitializers | Where-Object { $_.abiName -eq 'null-sentinel' }).Count -ne 4 -or
     @($callableInitializers | Where-Object { $_.errorAbiName -ne 'no-raise' }).Count -ne 0 ) {
    throw "Typed XLIR does not distinguish open-xvalue and fixed-closure callable null defaults"
}
$openCallableType = @($callableTyped.types | Where-Object {
    $_.kind -eq 4 -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    $_.callableErasureAbiName -eq 'open'
})
if ( $openCallableType.Count -ne 1 -or
     @($callableInitializers | Where-Object {
         $_.type -eq $openCallableType[0].id -and $_.abiName -eq 'xvalue-null'
     }).Count -ne 1 ) {
    throw "Typed XLIR open callable default has no exact xvalue-null identity"
}

& $xl ir $callableSource --lowered --format json --emit $callableLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable ValueInitializer lowering failed" }
$callableLowered = Read-Json $callableLoweredPath
if ( @($callableLowered.instructions | Where-Object {
         $_.kindName -eq 'runtime-call' -and $_.runtimeOpName -eq 'xvalue-null'
     }).Count -ne 1 -or
     @($callableLowered.instructions | Where-Object {
         $_.kindName -eq 'null' -and $_.type -ne $openCallableType[0].id
     }).Count -ne 4 ) {
    throw "Lowered XLIR callable defaults lost their representation-specific null operations"
}

& $xl cgen $defaultSource --backend xlir --emit $defaultCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR default ValueInitializer Cgen failed" }
& $xl cgen $classDefaultSource --backend xlir --emit $classDefaultCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-default ValueInitializer Cgen failed" }
& $xl cgen $classNullSource --backend xlir --emit $classNullCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-null ValueInitializer Cgen failed" }
& $xl cgen $ownershipSource --backend xlir --emit $ownershipCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native ownership ValueInitializer Cgen failed" }
& $xl cgen $nativeNullSource --backend xlir --emit $nativeNullCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owning native null ValueInitializer Cgen failed" }
& $xl cgen $containerSource --backend xlir --emit $containerCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR empty-container ValueInitializer Cgen failed" }
& $xl cgen $packageExportSource --backend xlir --emit $packageExportCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR package-export ValueInitializer Cgen failed" }
& $xl cgen $scalarSource --backend xlir --emit $scalarCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR scalar ValueInitializer Cgen failed" }
& $xl cgen $callableSource --backend xlir --emit $callableCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable ValueInitializer Cgen failed" }

$defaultCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $defaultExePath && $defaultExePath"
& $env:ComSpec /d /v:on /c $defaultCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR default ValueInitializer generated-C runtime failed" }
$classDefaultCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_VALUE_INITIALIZER_CLASS_DEFAULT $hostSource !XL6_CGEN_LINKS! -o $classDefaultExePath && $classDefaultExePath"
& $env:ComSpec /d /v:on /c $classDefaultCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-default ValueInitializer generated-C runtime failed" }
$classNullCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $classNullCPath $genericHostSource !XL6_CGEN_LINKS! -o $classNullExePath && $classNullExePath"
& $env:ComSpec /d /v:on /c $classNullCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-null ValueInitializer generated-C runtime failed" }
$ownershipCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_VALUE_INITIALIZER_NATIVE_OWNERSHIP $hostSource !XL6_CGEN_LINKS! -o $ownershipExePath && $ownershipExePath"
& $env:ComSpec /d /v:on /c $ownershipCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native ownership ValueInitializer generated-C runtime failed" }
$nativeNullCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_VALUE_INITIALIZER_NATIVE_NULL $hostSource !XL6_CGEN_LINKS! -o $nativeNullExePath && $nativeNullExePath"
& $env:ComSpec /d /v:on /c $nativeNullCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owning native null ValueInitializer generated-C runtime failed" }
$containerCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $containerCPath $genericHostSource !XL6_CGEN_LINKS! -o $containerExePath && $containerExePath"
& $env:ComSpec /d /v:on /c $containerCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR empty-container ValueInitializer generated-C runtime failed" }
$packageExportCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $packageExportCPath test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o $packageExportExePath && $packageExportExePath"
& $env:ComSpec /d /v:on /c $packageExportCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR package-export ValueInitializer generated-C runtime failed" }
$scalarCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $scalarCPath test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o $scalarExePath && $scalarExePath"
& $env:ComSpec /d /v:on /c $scalarCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR scalar ValueInitializer generated-C runtime failed" }
$callableCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $callableCPath $genericHostSource !XL6_CGEN_LINKS! -o $callableExePath && $callableExePath"
& $env:ComSpec /d /v:on /c $callableCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable ValueInitializer generated-C runtime failed" }

Write-Output "XLIR ValueInitializer gate ok: Typed explicit/default ABI, explicit scalar initializer adaptations, scalar/time/union zero, string-empty, representation-specific callable nulls, explicit empty-container runtime operations, package-export module-global ownership, class-default construction, explicit-constructor class-null semantics, managed native fields and destroy-aware release, strict C, and runtimes."
exit 0
