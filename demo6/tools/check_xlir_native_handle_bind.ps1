param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_native_class_bind_point.xl"
$rvalueSource = Join-Path $Root "test\cases\cgen_native_class_bind_point_basic.xl"
$hostSource = Join-Path $Root "test\cases\xlir_native_handle_bind_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_native_handle_bind.typed.json"
$loweredPath = Join-Path $outputDir "xlir_native_handle_bind.lowered.json"
$rvalueLoweredPath = Join-Path $outputDir "xlir_native_handle_bind_rvalue.lowered.json"
$cPath = Join-Path $outputDir "xlir_native_handle_bind_generated.c"
$rvalueCPath = Join-Path $outputDir "xlir_native_handle_bind_rvalue_generated.c"
$exePath = Join-Path $outputDir "xlir_native_handle_bind_host.exe"
$rvalueExePath = Join-Path $outputDir "xlir_native_handle_bind_rvalue_host.exe"

foreach ( $path in @($xl, $source, $rvalueSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR native handle bind gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native handle bind Typed IR failed" }
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$typedBinds = @($typed.nativeHandleBinds)
if ( $typed.nativeHandleBindCount -ne 3 -or $typedBinds.Count -ne 3 -or
     @($typedBinds | Where-Object {
         $_.abiName -ne 'class-field-take' -or $_.sourceSymbol -eq 0 -or $_.destroySymbol -eq 0 -or
         $_.targetType -eq 0 -or $_.sourceType -eq 0
     }).Count -ne 0 ) {
    throw "Typed XLIR has no exact lvalue TAKE, source-clear, and destroy-target bind contracts"
}

& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native handle bind lowering failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$bindOrigins = @($lowered.nativeHandleBinds | ForEach-Object { $_.origin })
$bindInstructions = @($lowered.instructions | Where-Object {
    $_.kindName -eq 'runtime-call' -and
    $_.runtimeOpName -eq 'xvalue-native-handle-take' -and
    $bindOrigins -contains $_.origin
})
if ( $bindInstructions.Count -ne 3 -or
     @($lowered.nativeHandleBinds | Where-Object {
         $origin = $_.origin
         @($bindInstructions | Where-Object { $_.origin -eq $origin }).Count -ne 1
     }).Count -ne 0 ) {
    throw "Lowered XLIR does not contain one explicit managed-field handle TAKE per Typed bind"
}
foreach ( $bind in @($lowered.nativeHandleBinds) ) {
    $nulls = @($lowered.instructions | Where-Object {
        $_.origin -eq $bind.origin -and $_.kindName -eq 'null' -and $_.type -eq $bind.sourceType
    })
    $clears = @($lowered.instructions | Where-Object {
        $_.origin -eq $bind.origin -and $_.kindName -eq 'store' -and
        $_.targetSymbol -eq $bind.sourceSymbol -and $_.type -eq $bind.sourceType
    })
    if ( $nulls.Count -ne 1 -or $clears.Count -ne 1 ) {
        throw "Lowered XLIR TAKE does not explicitly clear its raw-pointer source storage"
    }
}
$targetType = [int]$lowered.nativeHandleBinds[0].targetType
$destroySymbol = [int]$lowered.nativeHandleBinds[0].destroySymbol
$borrowedInternal = @($lowered.callTargets | Where-Object {
    $_.kind -eq 1 -and $_.cName -eq '' -and $_.parameterCount -eq 1 -and
    @($_.parameterTypes)[0] -eq $targetType -and @($_.parameterAbis)[0] -eq 2
})
$nativeTemps = @($lowered.storages | Where-Object {
    $_.kindName -eq 'temporary' -and $_.type -eq $targetType
})
if ( $borrowedInternal.Count -ne 1 -or
     @($lowered.nativeHandleBinds | Where-Object {
         $bind = $_
         @($nativeTemps | Where-Object { $_.declaration -eq $bind.origin }).Count -ne 1
     }).Count -ne 0 -or
     @($bindInstructions | Where-Object {
         $_.targetSymbol -ne $destroySymbol
     }).Count -ne 0 ) {
    throw "Lowered XLIR has no borrowed managed-resource parameter, per-bind temporary owner, or destroy-directed TAKE"
}

& $xl ir $rvalueSource --lowered --format json --emit $rvalueLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native rvalue bind lowering failed" }
$rvalue = [System.IO.File]::ReadAllText($rvalueLoweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$rvalueBinds = @($rvalue.nativeHandleBinds)
if ( $rvalueBinds.Count -ne 1 -or $rvalueBinds[0].abiName -ne 'class-field-take' -or
     $rvalueBinds[0].sourceSymbol -ne 0 -or $rvalueBinds[0].destroySymbol -eq 0 -or
     $rvalueBinds[0].fieldDeclaration -eq 0 -or
     @($rvalue.instructions | Where-Object {
         $_.origin -eq $rvalueBinds[0].origin -and
         $_.kindName -eq 'runtime-call' -and
         $_.runtimeOpName -eq 'xvalue-native-handle-take' -and
         $_.targetSymbol -eq $rvalueBinds[0].destroySymbol
     }).Count -ne 1 ) {
    throw "XLIR native rvalue bind does not transfer directly without inventing source storage"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native handle bind Cgen failed" }
& $xl cgen $rvalueSource --backend xlir --emit $rvalueCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native rvalue bind Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    'extern void* make_borrowed_box(int64_t);',
    'extern void borrowed_box_close(void*);',
    'extern int64_t borrowed_box_value(void*);',
    "xrtValueHandleTake(handle != NULL ? &owned : NULL, &xlir_native_handle_ops_s$destroySymbol",
    "xlir_native_handle_take_s$destroySymbol(&",
    '= NULL;'
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR native handle bind generated C is missing: $required"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native lvalue bind generated-C runtime failed" }
$rvalueCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_NATIVE_HANDLE_BIND_RVALUE $hostSource !XL6_CGEN_LINKS! -o $rvalueExePath && $rvalueExePath"
& $env:ComSpec /d /v:on /c $rvalueCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native rvalue bind generated-C runtime failed" }

Write-Output "XLIR native handle bind gate ok: Typed direct/managed-field TAKE ABI, source clearing, managed-resource parameter borrow, destroy-directed ownership, strict C, and runtimes."
exit 0
