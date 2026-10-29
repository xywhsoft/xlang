param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$classHost = Join-Path $Root "test\cases\cgen_class_host.c"
$nativeHost = Join-Path $Root "test\cases\cgen_class_native_field_host.c"
foreach ( $path in @($xl, $genericHost, $classHost, $nativeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR class reference-field gate input is missing: $path"
    }
}

function Invoke-XlirCase([string]$Name, [string]$SourceName) {
    $source = Join-Path $Root "test\cases\$SourceName"
    $lowered = Join-Path $Root "build\test\$Name.gate.lowered.xlir"
    $loweredJson = Join-Path $Root "build\test\$Name.gate.lowered.json"
    $generated = Join-Path $Root "build\test\$Name.gate.c"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR class reference-field source is missing: $source"
    }
    & $xl ir $source --lowered --format text --emit $lowered
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class reference-field lowering failed: $SourceName"
    }
    & $xl ir $source --lowered --format json --emit $loweredJson
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class reference-field JSON lowering failed: $SourceName"
    }
    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class reference-field Cgen failed: $SourceName"
    }
    return [pscustomobject]@{
        Name = $Name
        Source = $source
        Lowered = $lowered
        LoweredJson = $loweredJson
        Generated = $generated
        IrText = [System.IO.File]::ReadAllText(
            $lowered,
            [System.Text.Encoding]::UTF8
        )
        CText = [System.IO.File]::ReadAllText(
            $generated,
            [System.Text.Encoding]::UTF8
        )
        Ir = [System.IO.File]::ReadAllText(
            $loweredJson,
            [System.Text.Encoding]::UTF8
        ) | ConvertFrom-Json
    }
}

function Assert-TextContains(
    [string]$Text,
    [string]$Required,
    [string]$Description
) {
    if ( ! $Text.Contains($Required) ) {
        throw "XLIR class reference-field contract is missing $Description`: $Required"
    }
}

function Invoke-GeneratedRuntime($Case, [string]$SupportSource) {
    $exe = Join-Path $Root "build\test\$($Case.Name).gate.exe"
    $generatedForward = $Case.Generated -replace '\\', '/'
    $define = '-DXLIR_GENERATED_MODULE=\"' + $generatedForward + '\"'
    $command = 'call test\suites\common.bat && ' +
        '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror ' +
        $define + ' "' + $genericHost + '" "' + $SupportSource +
        '" !XL6_CGEN_LINKS! -o "' + $exe + '" && "' + $exe + '"'
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class reference-field generated-C runtime failed: $($Case.Name)"
    }
}

Push-Location $Root
try {
    $reference = Invoke-XlirCase `
        "xlir_class_reference_field_initializer" `
        "xlir_class_reference_field_initializer.xl"
    foreach ( $required in @(
        "name=child init=xvalue-null",
        "name=next init=xvalue-null",
        "runtime=xvalue-null",
        "runtime=xvalue-class-new"
    ) ) {
        Assert-TextContains $reference.IrText $required "reference-slot IR"
    }
    Assert-TextContains `
        $reference.CText `
        "= xrtValueObjectLifo();" `
        "reference-slot class runtime constructor"
    Invoke-GeneratedRuntime $reference $classHost

    $nested = Invoke-XlirCase `
        "cgen_nested_class_constructor" `
        "cgen_nested_class_constructor.xl"
    Assert-TextContains `
        $nested.IrText `
        "runtime=xvalue-class-new" `
        "nested class storage IR"
    Assert-TextContains `
        $nested.CText `
        "= xrtValueObjectLifo();" `
        "nested class runtime constructor"
    Invoke-GeneratedRuntime $nested $classHost

    $native = Invoke-XlirCase `
        "cgen_class_native_field" `
        "cgen_class_native_field.xl"
    $nativeBoxTypes = @($native.Ir.types | Where-Object {
        $_.kind -eq 2 -and $_.name -eq "NativeBox"
    })
    $holderTypes = @($native.Ir.types | Where-Object {
        $_.kind -eq 2 -and $_.name -eq "Holder"
    })
    if ( $nativeBoxTypes.Count -ne 1 -or $holderTypes.Count -ne 1 ) {
        throw "XLIR native class/holder types are not unique"
    }
    $nativeBoxType = [int]$nativeBoxTypes[0].id
    $holderType = [int]$holderTypes[0].id
    $nativeFields = @($native.Ir.classFields | Where-Object {
        $_.ownerType -eq $nativeBoxType
    })
    if ( $nativeFields.Count -ne 1 -or
         $nativeFields[0].nativeDestroySymbol -le 0 -or
         $nativeFields[0].initializerAbiName -ne "null-sentinel" ) {
        throw "XLIR native class field has no typed destroy symbol"
    }
    $destroyId = [int]$nativeFields[0].nativeDestroySymbol
    $holderFields = @($native.Ir.classFields | Where-Object {
        $_.ownerType -eq $holderType
    })
    if ( $holderFields.Count -ne 2 -or
         @($holderFields | Where-Object {
             $_.type -ne $nativeBoxType -or
             $_.nativeDestroySymbol -ne 0 -or
             $_.initializerAbiName -ne "xvalue-null"
         }).Count -ne 0 ) {
        throw "XLIR holder fields do not carry exact managed NativeBox ownership ABI"
    }
    $nativeTakes = @($native.Ir.instructions | Where-Object {
        $_.kindName -eq "runtime-call" -and
        $_.runtimeOpName -eq "xvalue-native-handle-take" -and
        $_.targetSymbol -eq $destroyId
    })
    if ( $nativeTakes.Count -lt 2 -or
         @($native.Ir.instructions | Where-Object {
             $_.kindName -eq "runtime-call" -and
             $_.runtimeOpName -eq "xvalue-class-new"
         }).Count -eq 0 ) {
        throw "XLIR native-field construction does not carry exact take/class-new IR"
    }
    foreach ( $required in @(
        "= xrtValueObjectLifo();",
        "xlir_native_handle_take_s$destroyId"
    ) ) {
        Assert-TextContains $native.CText $required "native-field generated C"
    }
    Invoke-GeneratedRuntime $native $nativeHost
} finally {
    Pop-Location
}

Write-Output "XLIR class reference-field gate ok: null slots, nested classes, native ownership transfer, LIFO field destruction, strict C, and runtime."
