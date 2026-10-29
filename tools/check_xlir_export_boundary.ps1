param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

function Implementation-Prototype([string]$Public) {
    $opening = $Public.IndexOf('(')
    $parameters = $Public.Substring($opening + 1, $Public.Length - $opening - 2).Split(',')
    for ($p = 0; $p -lt $parameters.Count; ++$p) {
        $parameters[$p] = [regex]::Replace($parameters[$p].Trim(), '[A-Za-z_][A-Za-z_0-9]*$', "xlir_input_$p")
    }
    return $Public.Substring(0, $opening + 1) + ($parameters -join ', ') + ')'
}

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$classSource = Join-Path $Root "test\cases\export_class.xl"
$overloadSource = Join-Path $Root "test\cases\xlir_export_nominal_overload.xl"
$hostSource = Join-Path $Root "test\cases\xlir_export_class_boundary_host.c"
$methodSource = Join-Path $Root "test\cases\export_class_method.xl"
$methodHostSource = Join-Path $Root "test\cases\xlir_export_class_method_host.c"
$outputDir = Join-Path $Root "build\test"
$classIr = Join-Path $outputDir "xlir_export_class_boundary.lowered.xlir"
$classJson = Join-Path $outputDir "xlir_export_class_boundary.lowered.json"
$classC = Join-Path $outputDir "xlir_export_class_boundary_generated.c"
$classHeader = Join-Path $outputDir "xlir_export_class_boundary_generated.h"
$classExe = Join-Path $outputDir "xlir_export_class_boundary_host.exe"
$methodIr = Join-Path $outputDir "xlir_export_class_method.lowered.xlir"
$methodJson = Join-Path $outputDir "xlir_export_class_method.lowered.json"
$methodC = Join-Path $outputDir "xlir_export_class_method_generated.c"
$methodHeader = Join-Path $outputDir "xlir_export_class_method_generated.h"
$methodExe = Join-Path $outputDir "xlir_export_class_method_host.exe"
$overloadIr = Join-Path $outputDir "xlir_export_nominal_overload.lowered.xlir"
$overloadC = Join-Path $outputDir "xlir_export_nominal_overload_generated.c"

foreach ( $path in @($xl, $classSource, $overloadSource, $hostSource,
        $methodSource, $methodHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR export-boundary gate input is missing: $path"
    }
}

& $xl ir $classSource --lowered --format text --emit $classIr
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed export lowered IR failed" }
$text = [System.IO.File]::ReadAllText($classIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "cname=xl6_point_make", "return=t4/owned/type",
    "cname=xl6_point_sum", "params=[point:t4/borrow/type]"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "XLIR managed export public ABI is missing from text IR: $required"
    }
}

& $xl ir $classSource --lowered --format json --emit $classJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed export JSON IR failed" }
$json = [System.IO.File]::ReadAllText($classJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$make = $json.exports | Where-Object { $_.cName -eq "xl6_point_make" }
$sum = $json.exports | Where-Object { $_.cName -eq "xl6_point_sum" }
if ( $json.exportCount -ne 2 -or $make.returnAbiName -ne "owned" -or
     $make.returnValueAbi -ne "type" -or
     $sum.returnAbiName -ne "value" -or $sum.returnValueAbi -ne "type" -or
     @($sum.parameterAbis)[0] -ne "borrow" -or
     @($sum.parameterValueAbis)[0] -ne "type" -or
     @($sum.parameterNames)[0] -ne "point" ) {
    throw "XLIR managed export JSON has no exact public ownership ABI"
}

& $xl cgen $classSource --backend xlir --emit $classC --emit-header $classHeader
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed export Cgen failed" }
$generated = [System.IO.File]::ReadAllText($classC, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($classHeader, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xvalue* xl6_point_make(int64_t x, int64_t y)",
    "int64_t xl6_point_sum(xvalue* point)"
) ) {
    if ( ! $generated.Contains((Implementation-Prototype $required)) -or ! $header.Contains("$required;") ) {
        throw "XLIR managed export C/header ABI is incomplete: $required"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $classExe && $classExe"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed export generated-C runtime failed" }

& $xl ir $methodSource --lowered --format text --emit $methodIr
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR exported class-method lowering failed" }
$methodText = [System.IO.File]::ReadAllText($methodIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "cname=xl6_point_move return=t3/value/type error-abi=1 body-error-abi=2 params=[self:t4/borrow/type,dx:t3/value/type,dy:t3/value/type]",
    "cname=xl6_point_pair return=t13/value/type error-abi=1 body-error-abi=2 params=[self:t4/borrow/type]",
    "origin=n73 kind=7 target=3 result=t3 receiver=n75"
) ) {
    if ( ! $methodText.Contains($required) ) {
        throw "XLIR exported class-method text ABI is incomplete: $required"
    }
}

& $xl ir $methodSource --lowered --format json --emit $methodJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR exported class-method JSON lowering failed" }
$methodModule = [System.IO.File]::ReadAllText(
    $methodJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$move = @($methodModule.exports | Where-Object { $_.cName -eq "xl6_point_move" })
$pair = @($methodModule.exports | Where-Object { $_.cName -eq "xl6_point_pair" })
$moveCall = @($methodModule.callSites | Where-Object { $_.origin -eq 73 })
if ( $methodModule.exportCount -ne 4 -or $move.Count -ne 1 -or
     $pair.Count -ne 1 -or $moveCall.Count -ne 1 -or
     @($move[0].parameterNames).Count -ne 3 -or
     @($move[0].parameterNames)[0] -ne "self" -or
     @($move[0].parameterAbis)[0] -ne "borrow" -or
     @($pair[0].parameterNames).Count -ne 1 -or
     @($pair[0].parameterNames)[0] -ne "self" -or
     $moveCall[0].receiver -eq 0 ) {
    throw "XLIR exported class method has no exact receiver/call/export ABI"
}

& $xl cgen $methodSource --backend xlir --emit $methodC --emit-header $methodHeader
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR exported class-method Cgen failed" }
$methodGenerated = [System.IO.File]::ReadAllText($methodC, [System.Text.Encoding]::UTF8)
$methodPublic = [System.IO.File]::ReadAllText($methodHeader, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "int64_t xl6_point_move(xvalue* self, int64_t dx, int64_t dy)",
    "xlir_product_t13 xl6_point_pair(xvalue* self)"
) ) {
    if ( ! $methodGenerated.Contains((Implementation-Prototype $required)) -or
         ! $methodPublic.Contains("$required;") ) {
        throw "XLIR exported class-method C/header ABI is incomplete: $required"
    }
}
if ( ! $methodGenerated.Contains("return (xlir_product_t13){0};") ) {
    throw "XLIR exported product method has no valid initialization-failure value"
}
$methodCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $methodHostSource !XL6_CGEN_LINKS! -o $methodExe && $methodExe"
& $env:ComSpec /d /v:on /c $methodCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR exported class-method generated-C runtime failed" }

& $xl ir $overloadSource --lowered --format text --emit $overloadIr
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nominal export overload lowering failed" }
$text = [System.IO.File]::ReadAllText($overloadIr, [System.Text.Encoding]::UTF8)
$names = @([regex]::Matches($text, 'cname=(xl6_export_read__xvalue_[0-9a-f]{16})') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
if ( $names.Count -ne 2 ) {
    throw "XLIR nominal export overloads have no distinct stable Type-ABI names"
}
& $xl cgen $overloadSource --backend xlir --emit $overloadC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nominal export overload Cgen failed" }
$generated = [System.IO.File]::ReadAllText($overloadC, [System.Text.Encoding]::UTF8)
foreach ( $name in $names ) {
    if ( ! $generated.Contains("int64_t $name(xvalue* xlir_input_0)") ) {
        throw "XLIR nominal export overload C emitter ignored verified linkage: $name"
    }
}

Write-Output "XLIR export boundary gate ok: explicit public names/value representations, value/borrow parameters, value/owned returns, exported class receivers/products, IR-derived headers, and nominal overload names."
exit 0
