param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_container_objects.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$runtimeSupport = Join-Path $Root "test\cases\cgen_class_host.c"
$typedPath = Join-Path $Root "build\test\xlir_native_class_object_identity.typed.json"
$loweredPath = Join-Path $Root "build\test\xlir_native_class_object_identity.lowered.json"
$generatedPath = Join-Path $Root "build\test\xlir_native_class_object_identity.gate.c"
$repeatPath = Join-Path $Root "build\test\xlir_native_class_object_identity.gate.repeat.c"
$legacyPath = Join-Path $Root "build\test\xlir_native_class_object_identity.gate.legacy.c"
$xlirExe = Join-Path $Root "build\test\xlir_native_class_object_identity.gate.exe"
$artifactExe = Join-Path $Root "build\test\xlir_native_class_object_identity.artifact.exe"
$legacyExe = Join-Path $Root "build\test\xlir_native_class_object_identity.gate.legacy.exe"

foreach ( $path in @($xl, $source, $runtimeHost, $runtimeSupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR managed native class identity ABI gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl ir $source --typed --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity typed IR failed"
    }
    & $xl ir $source --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity lowered IR failed"
    }

    $typed = Get-Content -LiteralPath $typedPath -Raw | ConvertFrom-Json
    $lowered = Get-Content -LiteralPath $loweredPath -Raw | ConvertFrom-Json
    $nativeType = @($typed.types | Where-Object { $_.name -eq "Native" })
    $ordinaryType = @($typed.types | Where-Object { $_.name -eq "Pair" })
    if ( $nativeType.Count -ne 1 -or $ordinaryType.Count -ne 1 -or
         $nativeType[0].kind -ne $ordinaryType[0].kind -or
         $nativeType[0].cAbi -ne $ordinaryType[0].cAbi -or
         $nativeType[0].ownershipAbi -ne $ordinaryType[0].ownershipAbi ) {
        throw "XLIR field-only native-backed class is not using the ordinary managed class ABI"
    }
    $layout = @($typed.classLayouts |
        Where-Object { $_.type -eq $nativeType[0].id })
    if ( $layout.Count -ne 1 -or $layout[0].fieldCount -ne 1 -or
         $layout[0].hashAbiName -ne "runtime-reference" -or
         $layout[0].equalityAbiName -ne "runtime-reference" ) {
        throw "XLIR field-only native-backed class has no exact reference-identity ClassLayout"
    }
    $field = @($typed.classFields |
        Where-Object { $_.ownerType -eq $nativeType[0].id })
    if ( $field.Count -ne 1 ) {
        throw "XLIR field-only native-backed class has no unique native field"
    }
    $binds = @($typed.nativeHandleBinds |
        Where-Object { $_.targetType -eq $nativeType[0].id })
    if ( $binds.Count -ne 3 -or
         @($binds | Where-Object {
            $_.abiName -ne "class-field-borrow" -or
            $_.fieldDeclaration -ne $field[0].declaration -or
            $_.sourceSymbol -ne 0 -or $_.destroySymbol -ne 0
         }).Count -ne 0 ) {
        throw "XLIR managed native constructor has no exact class-field-borrow contract"
    }

    foreach ( $bind in $binds ) {
        $classNew = @($lowered.instructions | Where-Object {
            $_.origin -eq $bind.origin -and
            $_.runtimeOpName -eq "xvalue-class-new" -and
            $_.type -eq $nativeType[0].id
        })
        $identityBind = @($lowered.instructions | Where-Object {
            $_.origin -eq $bind.origin -and
            $_.runtimeOpName -eq "xvalue-class-identity-bind" -and
            $_.type -eq $nativeType[0].id -and
            $_.payloadType -eq $nativeType[0].id
        })
        $pointerBorrow = @($lowered.instructions | Where-Object {
            $_.origin -eq $bind.origin -and
            $_.runtimeOpName -eq "xvalue-raw-pointer-borrow"
        })
        $directBind = @($lowered.instructions | Where-Object {
            $_.origin -eq $bind.origin -and
            $_.kindName -eq "native-handle-bind"
        })
        if ( $classNew.Count -ne 1 -or $identityBind.Count -ne 1 -or
             $pointerBorrow.Count -ne 1 -or $directBind.Count -ne 0 ) {
            throw "XLIR managed native constructor did not lower to explicit object identity operations"
        }
    }
    $fieldStores = @($lowered.instructions | Where-Object {
        $_.origin -eq $field[0].declaration -and
        $_.runtimeOpName -eq "class-field-init-new"
    })
    if ( $fieldStores.Count -ne 3 -or
         @($fieldStores | Where-Object {
            [int]$_.instantiation -le 0 -or
            [int]$_.executionOrigin -notin @($binds.origin)
         }).Count -ne 0 ) {
        throw "XLIR managed native constructor did not initialize the exact native field"
    }
    foreach ( $bind in $binds ) {
        $fieldStore = @($fieldStores | Where-Object {
            [int]$_.executionOrigin -eq [int]$bind.origin
        })
        if ( $fieldStore.Count -ne 1 ) {
            throw "XLIR managed native constructor field commit is not unique at its execution site"
        }
        $instance = @($lowered.instantiations | Where-Object {
            [int]$_.id -eq [int]$fieldStore[0].instantiation
        })
        if ( $instance.Count -ne 1 -or
             [int]$instance[0].field - 1 -ne [int]$field[0].id -or
             [int]$instance[0].site -ne [int]$bind.origin -or
             [int]$instance[0].layoutType -ne [int]$nativeType[0].id -or
             [int]$instance[0].commitInstruction -ne [int]$fieldStore[0].index ) {
            throw "XLIR managed native constructor field instantiation record is inconsistent"
        }
    }

    & $xl cgen $source --backend xlir --emit $generatedPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity Cgen failed"
    }
    & $xl cgen $source --backend xlir --emit $repeatPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity repeat Cgen failed"
    }
    $generatedBytes = [System.IO.File]::ReadAllBytes($generatedPath)
    $repeatBytes = [System.IO.File]::ReadAllBytes($repeatPath)
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            $generatedBytes, $repeatBytes) ) {
        throw "XLIR managed native class identity generated C is not byte-for-byte deterministic"
    }
    $generatedText = [System.IO.File]::ReadAllText(
        $generatedPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "xl6ClassRuntimeV2Get()->BindIdentity", 'xrtStrView("handle")',
        "xlir_class_reference_hash", "xlir_class_reference_equal"
    ) ) {
        if ( ! $generatedText.Contains($required) ) {
            throw "XLIR managed native class identity generated C lacks exact runtime data: $required"
        }
    }

    # Resolve the resident language-runtime ABI from the shared runtime in the
    # strict-C lane. The standalone lane below separately verifies that the
    # artifact planner packages the same providers without the compiler DLL.
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $generatedPath -include $runtimeSupport $runtimeHost !XL6_CGEN_XLANG_LINKS! -o $xlirExe && $xlirExe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity strict-C/runtime gate failed"
    }

    & $xl build $source $runtimeSupport --backend xlir -o $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity standalone artifact build gate failed"
    }
    & $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity standalone artifact runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity legacy differential Cgen failed"
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $runtimeSupport !XL6_CGEN_XLANG_LINKS! -o $legacyExe && $legacyExe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native class identity legacy differential gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR field-only native-backed managed object identity ABI ok."
