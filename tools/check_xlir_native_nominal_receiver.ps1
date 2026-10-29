param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_native_class.xl"
$hostSource = Join-Path $Root "test\cases\xlir_native_nominal_receiver_host.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_native_nominal_receiver.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_native_nominal_receiver.lowered.json"
$cPath = Join-Path $outputDir "xlir_native_nominal_receiver_generated.c"
$exePath = Join-Path $outputDir "xlir_native_nominal_receiver_host.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR native nominal receiver gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native nominal receiver lowering failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
$nativeTypeMatch = [regex]::Match(
    $lowered,
    'type t(?<type>[0-9]+) kind=2 collection=none resource=none persistence=class-schema/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s(?<symbol>[1-9][0-9]*) typeValue=0x[0-9A-F]{16} name=NativeCounter display=NativeCounter integer=none params=\[\] error-result=xlir_result_xvalue')
if ( ! $nativeTypeMatch.Success ) {
    throw "XLIR native-backed class type has no nominal managed-object ABI"
}
$nativeType = [int]$nativeTypeMatch.Groups['type'].Value
if ( ! $lowered.Contains('cname=counter_open') -or
     ! $lowered.Contains('cname=counter_add') -or
     ! $lowered.Contains('cname=counter_close') ) {
    throw "XLIR native class CallTarget dump has no canonical C linkage"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native nominal receiver JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$factory = @($json.callTargets | Where-Object { $_.cName -eq 'counter_open' })
$add = @($json.callTargets | Where-Object { $_.cName -eq 'counter_add' })
$destroy = @($json.callTargets | Where-Object { $_.cName -eq 'counter_close' })
$pointType = @($json.types | Where-Object { $_.cAbi -eq 28 })
$layout = @($json.classLayouts | Where-Object { $_.type -eq $nativeType })
$field = @($json.classFields | Where-Object { $_.ownerType -eq $nativeType })
$wrapper = @($json.callTargets | Where-Object {
    $_.cName -eq '' -and $_.parameterCount -gt 0 -and
    @($_.parameterTypes)[0] -eq $nativeType
})
if ( $factory.Count -ne 1 -or $factory[0].kind -ne 3 -or
     $factory[0].returnType -ne $nativeType -or $factory[0].parameterCount -ne 1 -or
     $factory[0].resultPhysicalAbiName -ne 'native-class-field-take' -or
     $field.Count -ne 1 -or $field[0].nativeDestroySymbol -eq 0 -or
     $factory[0].resultPhysicalFieldDeclaration -ne $field[0].declaration -or
     $factory[0].resultPhysicalDestroySymbol -ne $field[0].nativeDestroySymbol -or
     $layout.Count -ne 1 -or $layout[0].copyAbiName -ne 'noncopyable-null' -or
     $layout[0].cloneAbiName -ne 'noncopyable-null' -or
     $add.Count -ne 1 -or $add[0].parameterCount -ne 2 -or
     @($add[0].parameterTypes)[0] -ne $nativeType -or @($add[0].parameterAbis)[0] -ne 2 -or
     @($add[0].parameterPhysicalAbis)[0] -ne 'native-class-handle-borrow' -or
     @($add[0].parameterPhysicalFieldDeclarations)[0] -ne $field[0].declaration -or
     $destroy.Count -ne 1 -or $destroy[0].parameterCount -ne 1 -or
     @($destroy[0].parameterTypes)[0] -ne $nativeType -or @($destroy[0].parameterAbis)[0] -ne 2 -or
     $destroy[0].symbol -ne $field[0].nativeDestroySymbol -or
     @($destroy[0].parameterPhysicalFieldDeclarations)[0] -ne $field[0].declaration -or
     $wrapper.Count -ne 2 -or
     @($wrapper | Where-Object { @($_.parameterAbis)[0] -ne 2 }).Count -ne 0 -or
     $pointType.Count -ne 1 -or $json.nativeHandleProjectCount -ne 0 ) {
    throw "XLIR native factory/extern/wrapper receiver ABIs are incomplete"
}
$addCalls = @($json.instructions | Where-Object {
    $_.kindName -eq 'call' -and $_.targetSymbol -eq $add[0].symbol
})
if ( $addCalls.Count -ne 3 ) {
    throw "XLIR native wrapper/direct method call count is incomplete"
}
foreach ( $call in $addCalls ) {
    $projection = @($json.instructions | Where-Object {
        $_.result -eq @($call.operands)[0] -and
        $_.runtimeOpName -eq 'xvalue-object-get-literal-native-handle-borrow'
    })
    if ( $projection.Count -ne 1 -or $projection[0].type -ne $field[0].type ) {
        throw "XLIR native method CALL does not consume its exact owned-field borrow"
    }
}
$fieldMembers = @($json.nodes | Where-Object {
    $_.memberAbiName -eq 'class-field-owned-raw-pointer-borrow'
})
if ( $fieldMembers.Count -ne 1 -or
     @($json.instructions | Where-Object {
         $_.origin -eq $fieldMembers[0].id -and
         $_.runtimeOpName -eq 'xvalue-object-get-literal-native-handle-borrow'
     }).Count -ne 1 ) {
    throw "XLIR owned point field has no explicit handle-storage/raw-pointer-result MemberAbi"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native nominal receiver Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    'extern void* counter_open(int64_t);',
    'extern int64_t counter_add(void*, int64_t);',
    'extern void counter_close(void*);',
    'xlir_xvalue_object_get_literal_native_handle('
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR native nominal receiver generated C is missing: $required"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native nominal receiver generated-C runtime failed" }

Write-Output "XLIR native nominal receiver gate ok: managed nominal identity, factory field TAKE, extern/wrapper handle borrow, owned-point MemberAbi, strict C, lifecycle, and runtime."
exit 0
