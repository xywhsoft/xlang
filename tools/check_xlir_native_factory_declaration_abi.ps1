param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_native_factory_declaration.xl"
$prefix = Join-Path $Root "build\test\xlir_native_factory_declaration"
$artifact = "$prefix.artifact.exe"
Push-Location $Root
try {
    & $xl ir $source --lowered --format json --emit "$prefix.json"
    if ( $LASTEXITCODE -ne 0 ) { throw "Unused native factory IR generation failed" }
    $module = [System.IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $factories = @($module.callTargets | Where-Object { $_.cName -in @("xlir_unused_factory_default", "xlir_unused_factory_overload") })
    $destroy = @($module.callTargets | Where-Object { $_.cName -eq "xlir_unused_factory_destroy" })
    if ( $factories.Count -ne 2 -or $destroy.Count -ne 1 ) { throw "Unused native overloads disappeared from the canonical ABI arena" }
    foreach ( $factory in $factories ) {
        $type = @($module.types | Where-Object { $_.id -eq $factory.returnType })
        if ( $factory.kind -ne 3 -or $factory.reachability -ne "unreachable" -or
             $factory.resultPhysicalAbiName -ne "native-class-field-take" -or
             $type.Count -ne 1 -or $type[0].name -ne "UnusedNativeFactory" -or
             $type[0].cAbi -ne 26 -or $type[0].ownershipAbi -ne 2 -or $type[0].nullAbi -ne 2 -or
             $factory.resultPhysicalFieldDeclaration -eq 0 -or
             $factory.resultPhysicalDestroySymbol -ne $destroy[0].symbol -or
             $factory.returnType -ne $destroy[0].parameterTypes[0] -or
             $factory.resultPhysicalFieldDeclaration -ne $destroy[0].parameterPhysicalFieldDeclarations[0] ) {
            throw "Unused constructor still depends on a call site to obtain its owner/field ABI"
        }
    }
    if ( ($factories.parameterCount | Sort-Object) -join ',' -ne "0,1" ) { throw "Native constructors acquired synthetic self parameters" }
    & $xl cgen $source --emit "$prefix.c"
    if ( $LASTEXITCODE -ne 0 ) { throw "Unused native factory default Cgen failed" }
    & $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ( $LASTEXITCODE -ne 0 ) { throw "Unused native factory explicit Cgen failed" }
    $c = [System.IO.File]::ReadAllText("$prefix.c")
    if ( $c -cne [System.IO.File]::ReadAllText("$prefix.repeat.c") -or $c.Contains("xlir_unused_factory_")) {
        throw "Unreachable factories lost deterministic linkage pruning"
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$prefix.c`" -o `"$prefix.o`""
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) { throw "Unused native factory strict C compile failed" }
    & $xl run $source --backend xlir
    if ( $LASTEXITCODE -ne 0 ) { throw "Unreachable factories incorrectly require host linkage" }
    & $xl build $source --backend xlir -o $artifact
    if ( $LASTEXITCODE -ne 0 ) { throw "Unused native factory standalone artifact build failed" }
    & $artifact
    if ( $LASTEXITCODE -ne 0 ) { throw "Unused native factory standalone artifact runtime failed" }
} finally { Pop-Location }
Write-Output "XLIR native factory declaration ABI ok: unused overloads own exact class/field/destroy ABI without call-site inference; deterministic strict C and no host-link dependency."
exit 0
