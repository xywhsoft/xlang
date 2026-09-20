param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$matrixSource = Join-Path $Root "test\cases\enum_match_matrix.xl"
$numericSource = Join-Path $Root "test\cases\enum_numeric_runtime.xl"
$genericSource = Join-Path $Root "test\cases\cgen_generic_enum_match_top_level.xl"
$nestedSource = Join-Path $Root "test\cases\enum_nested_generic.xl"
$recursiveSource = Join-Path $Root "test\cases\enum_recursive_container.xl"
$lifecycleSource = Join-Path $Root "test\cases\enum_tagged_lifecycle.xl"
$asyncSource = Join-Path $Root "test\cases\enum_async_closure.xl"
$programHost = Join-Path $Root "test\cases\xlir_program_entry_host.c"
$moduleHost = Join-Path $Root "test\cases\xlir_module_init_host.c"
$outputDir = Join-Path $Root "build\test"

foreach ( $path in @($xl, $matrixSource, $numericSource, $genericSource,
                      $nestedSource, $recursiveSource, $lifecycleSource,
                      $asyncSource, $programHost, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR enum ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Read-Json([string]$path)
{
    return [System.IO.File]::ReadAllText(
        $path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Invoke-StrictRuntime(
    [string]$generatedC,
    [string]$hostSource,
    [string]$executable,
    [string]$label)
{
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$generatedC`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 `"$hostSource`" !XL6_CGEN_LINKS! -o `"$executable`" && `"$executable`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR enum $label strict-C/runtime gate failed"
    }
}

$matrixTypedPath = Join-Path $outputDir "xlir_enum_match_matrix.typed.json"
$matrixLoweredPath = Join-Path $outputDir "xlir_enum_match_matrix.lowered.json"
$matrixCPath = Join-Path $outputDir "xlir_enum_match_matrix.generated.c"
$matrixExePath = Join-Path $outputDir "xlir_enum_match_matrix.exe"

& $xl ir $matrixSource --format json --emit $matrixTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR enum matrix Typed IR failed" }
& $xl ir $matrixSource --lowered --format json --emit $matrixLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR enum matrix lowering failed" }
$typed = Read-Json $matrixTypedPath
$lowered = Read-Json $matrixLoweredPath

if ( $typed.enumLayoutCount -ne 2 -or
     $typed.enumVariantCount -ne 6 -or
     $typed.enumPayloadCount -ne 3 -or
     @($typed.enumLayouts).Count -ne $typed.enumLayoutCount -or
     @($typed.enumVariants).Count -ne $typed.enumVariantCount -or
     @($typed.enumPayloads).Count -ne $typed.enumPayloadCount ) {
    throw "Typed XLIR enum arenas do not describe the complete matrix"
}

$numericLayout = @($typed.enumLayouts | Where-Object {
    $enumType = $_.type
    @($typed.types | Where-Object {
        $_.id -eq $enumType -and $_.kind -eq 11 -and
        $_.integerTargetName -eq 'u16' -and $_.ownershipAbi -eq 1
    }).Count -eq 1
})
$taggedLayout = @($typed.enumLayouts | Where-Object {
    $enumType = $_.type
    @($typed.types | Where-Object {
        $_.id -eq $enumType -and $_.kind -eq 12 -and
        $_.ownershipAbi -eq 2 -and $_.typeValueId -ne '0x0000000000000000'
    }).Count -eq 1
})
if ( $numericLayout.Count -ne 1 -or $taggedLayout.Count -ne 1 ) {
    throw "Typed XLIR does not distinguish exact numeric and tagged enum ABIs"
}

foreach ( $layout in @($typed.enumLayouts) ) {
    $variants = @($typed.enumVariants | Select-Object -Skip $layout.firstVariant |
        Select-Object -First $layout.variantCount)
    if ( $variants.Count -ne $layout.variantCount -or
         @($variants | Where-Object { $_.ownerType -ne $layout.type }).Count -ne 0 -or
         @($variants.tag | Sort-Object -Unique).Count -ne $layout.variantCount -or
         @($variants | Where-Object { $_.tag -le 0 }).Count -ne 0 ) {
        throw "Typed XLIR enum layout has an invalid closed variant slice"
    }
    foreach ( $variant in $variants ) {
        $payloads = @($typed.enumPayloads | Select-Object -Skip $variant.firstPayload |
            Select-Object -First $variant.payloadCount)
        if ( $payloads.Count -ne $variant.payloadCount -or
             @($payloads | Where-Object {
                 $_.ownerType -ne $layout.type -or $_.abiName -eq 'invalid'
             }).Count -ne 0 ) {
            throw "Typed XLIR enum variant has an invalid payload slice"
        }
    }
}

$taggedOps = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -like 'tagged-enum-*'
})
foreach ( $required in @('tagged-enum-new', 'tagged-enum-payload-set-new',
                          'tagged-enum-tag-equal',
                          'tagged-enum-payload-project') ) {
    if ( @($taggedOps | Where-Object { $_.runtimeOpName -eq $required }).Count -lt 1 ) {
        throw "Lowered XLIR enum matrix omitted explicit operation: $required"
    }
}
foreach ( $instruction in $taggedOps ) {
    if ( $instruction.enumVariant -lt 0 -or
         $instruction.enumVariant -ge $lowered.enumVariantCount ) {
        throw "Lowered XLIR tagged enum operation has no exact EnumVariant row"
    }
    if ( $instruction.runtimeOpName -like 'tagged-enum-payload-*' ) {
        if ( $instruction.enumPayload -lt 0 -or
             $instruction.enumPayload -ge $lowered.enumPayloadCount ) {
            throw "Lowered XLIR tagged enum payload operation has no exact EnumPayload row"
        }
        $variant = $lowered.enumVariants[$instruction.enumVariant]
        if ( $instruction.enumPayload -lt $variant.firstPayload -or
             $instruction.enumPayload -ge
                 ($variant.firstPayload + $variant.payloadCount) ) {
            throw "Lowered XLIR tagged enum payload escapes its variant slice"
        }
    } elseif ( $instruction.enumPayload -ne -1 ) {
        throw "Lowered XLIR tagged enum non-payload operation carries an ambiguous payload row"
    }
}

& $xl cgen $matrixSource --backend xlir --emit $matrixCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR enum matrix Cgen failed" }
$matrixGenerated = [System.IO.File]::ReadAllText(
    $matrixCPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @('$xlir.enum.variant', '$xlir.enum.payload.0',
                          'xrtValueObjectSetNew', 'xlir_xvalue_type_bind') ) {
    if ( ! $matrixGenerated.Contains($required) ) {
        throw "XLIR enum Cgen omitted mechanical verified-IR emission: $required"
    }
}
Invoke-StrictRuntime $matrixCPath $programHost $matrixExePath "matrix"

$numericTypedPath = Join-Path $outputDir "xlir_enum_numeric.typed.json"
$numericLoweredPath = Join-Path $outputDir "xlir_enum_numeric.lowered.json"
$numericCPath = Join-Path $outputDir "xlir_enum_numeric.generated.c"
$numericExePath = Join-Path $outputDir "xlir_enum_numeric.exe"
& $xl ir $numericSource --format json --emit $numericTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR numeric enum Typed IR failed" }
& $xl ir $numericSource --lowered --format json --emit $numericLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR numeric enum lowering failed" }
$numericTyped = Read-Json $numericTypedPath
$numericLowered = Read-Json $numericLoweredPath
if ( $numericTyped.enumLayoutCount -ne 1 ) {
    throw "Typed XLIR numeric enum has no exact EnumLayout"
}
$numericTypeId = $numericTyped.enumLayouts[0].type
$numericType = @($numericTyped.types | Where-Object { $_.id -eq $numericTypeId })
if ( $numericType.Count -ne 1 -or $numericType[0].kind -ne 11 -or
     $numericType[0].integerTargetName -ne 'i32' -or
     $numericType[0].ownershipAbi -ne 1 ) {
    throw "Typed XLIR numeric enum does not preserve its exact int32 scalar ABI"
}
if ( @($numericLowered.instructions | Where-Object {
         $_.runtimeOpName -eq 'xvalue-type-bind' -and
         $_.runtimeIdentityType -eq $numericTypeId
     }).Count -lt 1 -or
     @($numericLowered.instructions | Where-Object {
         $_.runtimeOpName -eq 'xvalue-cast-checked' -and
         $_.castAbiName -eq 'xvalue-to-i64-checked'
     }).Count -lt 1 ) {
    throw "Lowered XLIR numeric enum erasure lost nominal type identity or checked projection"
}
& $xl cgen $numericSource --backend xlir --emit $numericCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR numeric enum Cgen failed" }
Invoke-StrictRuntime $numericCPath $programHost $numericExePath "numeric"

$genericCPath = Join-Path $outputDir "xlir_enum_generic_top_level.generated.c"
$genericExePath = Join-Path $outputDir "xlir_enum_generic_top_level.exe"
& $xl cgen $genericSource --backend xlir --emit $genericCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic enum top-level Cgen failed" }
Invoke-StrictRuntime $genericCPath $moduleHost $genericExePath "generic top-level"

$nestedTypedPath = Join-Path $outputDir "xlir_enum_nested_generic.typed.json"
$nestedCPath = Join-Path $outputDir "xlir_enum_nested_generic.generated.c"
$nestedExePath = Join-Path $outputDir "xlir_enum_nested_generic.exe"
& $xl ir $nestedSource --format json --emit $nestedTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nested generic enum Typed IR failed" }
$nestedTyped = Read-Json $nestedTypedPath
if ( $nestedTyped.enumLayoutCount -ne 2 -or
     @($nestedTyped.enumLayouts.declaration | Sort-Object -Unique).Count -ne 1 -or
     @($nestedTyped.enumLayouts.type | Sort-Object -Unique).Count -ne 2 ) {
    throw "Typed XLIR does not model generic enum specializations as distinct concrete layouts sharing one declaration"
}
& $xl cgen $nestedSource --backend xlir --emit $nestedCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nested generic enum Cgen failed" }
Invoke-StrictRuntime $nestedCPath $programHost $nestedExePath "nested generic"

$recursiveCPath = Join-Path $outputDir "xlir_enum_recursive_container.generated.c"
$recursiveExePath = Join-Path $outputDir "xlir_enum_recursive_container.exe"
& $xl cgen $recursiveSource --backend xlir --emit $recursiveCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR recursive enum container Cgen failed" }
Invoke-StrictRuntime $recursiveCPath $programHost $recursiveExePath "recursive container"

$lifecycleCPath = Join-Path $outputDir "xlir_enum_tagged_lifecycle.generated.c"
$lifecycleExePath = Join-Path $outputDir "xlir_enum_tagged_lifecycle.exe"
& $xl cgen $lifecycleSource --backend xlir --emit $lifecycleCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR tagged enum lifecycle Cgen failed" }
Invoke-StrictRuntime $lifecycleCPath $programHost $lifecycleExePath "tagged lifecycle"

$asyncTypedPath = Join-Path $outputDir "xlir_enum_async_closure.typed.json"
$asyncLoweredPath = Join-Path $outputDir "xlir_enum_async_closure.lowered.json"
$asyncCPath = Join-Path $outputDir "xlir_enum_async_closure.generated.c"
$asyncExePath = Join-Path $outputDir "xlir_enum_async_closure.exe"
& $xl ir $asyncSource --format json --emit $asyncTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async enum Typed IR failed" }
& $xl ir $asyncSource --lowered --format json --emit $asyncLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async enum lowering failed" }
$asyncTyped = Read-Json $asyncTypedPath
$asyncLowered = Read-Json $asyncLoweredPath
$canonicalInts = @($asyncTyped.types | Where-Object {
    $_.name -eq 'int' -and $_.integerTargetName -eq 'i64'
})
$canonicalIntTypeId = if ( $canonicalInts.Count -eq 1 ) {
    $canonicalInts[0].id
} else { -1 }
$asyncIntTypes = @($asyncTyped.types | Where-Object {
    $_.name -eq 'AsyncResult' -and @($_.params).Count -eq 1 -and
    $_.params[0] -eq $canonicalIntTypeId
})
if ( $canonicalInts.Count -ne 1 -or $asyncIntTypes.Count -ne 1 ) {
    throw "Typed XLIR type pool did not canonicalize repeated integer and generic enum identities"
}
if ( $asyncLowered.asyncMachineCount -lt 1 -or
     @($asyncLowered.asyncMachineFrameSlots | Where-Object {
         $_.destroyAbiName -eq 'release-xvalue'
     }).Count -lt 1 -or
     @($asyncLowered.instructions | Where-Object {
         $_.runtimeOpName -like 'tagged-enum-*'
     }).Count -lt 1 ) {
    throw "Lowered XLIR async enum flow has no generic state machine, retained xvalue frame, or exact enum operations"
}
& $xl cgen $asyncSource --backend xlir --emit $asyncCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async enum/closure Cgen failed" }
Invoke-StrictRuntime $asyncCPath $moduleHost $asyncExePath "async closure"

Write-Output "XLIR enum ABI gate ok: closed numeric/tagged layouts, exact variant/payload rows, exhaustive match CFG, concrete generic specialization, recursive containers, managed lifecycle, nominal erasure, async/closure state machines, strict C, and runtime."
exit 0
