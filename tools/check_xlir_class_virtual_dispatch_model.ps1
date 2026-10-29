param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$case = Join-Path $Root "test\cases\xlir_class_virtual_dispatch.xl"
$fullCase = Join-Path $Root "test\cases\cgen_class_polymorphism.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$tmp = Join-Path $Root "build\tmp"
$ir = Join-Path $tmp "xlir_class_virtual_dispatch_model.ir.txt"
$c = Join-Path $tmp "xlir_class_virtual_dispatch_model.c"
$exe = Join-Path $tmp "xlir_class_virtual_dispatch_model.exe"
$fullC = Join-Path $tmp "xlir_class_polymorphism_model.c"
$fullIr = Join-Path $tmp "xlir_class_polymorphism_model.ir.json"
$fullExe = Join-Path $tmp "xlir_class_polymorphism_model.exe"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) -or
     ! (Test-Path -LiteralPath $case -PathType Leaf) -or
     ! (Test-Path -LiteralPath $fullCase -PathType Leaf) -or
     ! (Test-Path -LiteralPath $hostSource -PathType Leaf) ) {
    throw "XLIR virtual-dispatch model gate requires the built compiler, polymorphism fixture and runtime host"
}

New-Item -ItemType Directory -Force -Path $tmp | Out-Null
Remove-Item -LiteralPath $ir,$c,$exe,$fullC,$fullIr,$fullExe -Force -ErrorAction SilentlyContinue

& $xl ir $case --format text --emit $ir
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $ir -PathType Leaf) ) {
    throw "XLIR virtual-dispatch model did not produce typed IR"
}

$dump = [System.IO.File]::ReadAllText($ir, [System.Text.Encoding]::UTF8)
if ( $dump -notmatch 'classDispatchSlots=[1-9][0-9]*' -or
     $dump -notmatch 'class-dispatch-slot d[0-9]+ owner=t[0-9]+/n[0-9]+ .*name=value' -or
     $dump -notmatch 'classDispatchImpls=[2-9][0-9]*' -or
     ([regex]::Matches($dump, 'class-dispatch-impl d[0-9]+ owner=t[0-9]+/n[0-9]+ implementation=n[0-9]+ symbol=[0-9]+').Count -lt 2) ) {
    throw "XLIR virtual-dispatch metadata lacks a verified slot and concrete base/derived bindings"
}

Push-Location $Root
try {
    & $xl cgen $case --backend xlir --emit $c
    if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $c -PathType Leaf) ) {
        throw "XLIR virtual-dispatch Cgen did not emit C"
    }
    $generated = [System.IO.File]::ReadAllText($c, [System.Text.Encoding]::UTF8)
    foreach ( $required in @('xlir_vdispatch_', '$xlir.class.runtime-type', 'switch (xlir_xvalue_object_get_literal_i64', 'xrtValueArrayAppendNew') ) {
        if ( !$generated.Contains($required) ) {
            throw "XLIR virtual-dispatch C lacks required verified ABI: $required"
        }
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $c $hostSource !XL6_CGEN_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR virtual-dispatch generated-C runtime failed" }

    & $xl ir $fullCase --format json --emit $fullIr
    if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $fullIr -PathType Leaf) ) {
        throw "XLIR full polymorphism model did not produce typed IR"
    }
    $fullModel = [System.IO.File]::ReadAllText(
        $fullIr, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $baseTypes = @($fullModel.types | Where-Object {
        $_.kind -eq 2 -and $_.name -eq "RefBase"
    })
    $derivedTypes = @($fullModel.types | Where-Object {
        $_.kind -eq 2 -and $_.name -eq "RefDerived"
    })
    if ( $baseTypes.Count -ne 1 -or $derivedTypes.Count -ne 1 ) {
        throw "XLIR full polymorphism nominal types are not unique"
    }
    $baseIdentitySets = @($fullModel.nominalIdentitySets | Where-Object {
        $_.targetType -eq $baseTypes[0].id
    })
    $derivedIdentitySets = @($fullModel.nominalIdentitySets | Where-Object {
        $_.targetType -eq $derivedTypes[0].id
    })
    if ( $baseIdentitySets.Count -ne 1 -or
         @($baseIdentitySets[0].identities).Count -ne 2 -or
         $derivedIdentitySets.Count -ne 1 -or
         @($derivedIdentitySets[0].identities).Count -ne 1 -or
         !(@($baseIdentitySets[0].identities) -contains
           [uint64]$derivedIdentitySets[0].identities[0]) ) {
        throw "XLIR full polymorphism nominal identity sets do not encode base/derived membership"
    }
    $derivedIdentity = [uint64]$derivedIdentitySets[0].identities[0]

    & $xl cgen $fullCase --backend xlir --emit $fullC
    if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $fullC -PathType Leaf) ) {
        throw "XLIR full polymorphism Cgen did not emit C"
    }
    $fullGenerated = [System.IO.File]::ReadAllText($fullC, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        'xrtValueArrayAppendNew',
        'xlir_vdispatch_',
        'static bool xlir_nominal_set_',
        ('case 0x{0:X16}ULL: return true;' -f $derivedIdentity),
        'if (xlir_nominal_set_'
    ) ) {
        if ( !$fullGenerated.Contains($required) ) {
            throw "XLIR full polymorphism C lacks required verified ABI: $required"
        }
    }
    $fullCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $fullC $hostSource !XL6_CGEN_LINKS! -o $fullExe && $fullExe"
    & cmd.exe /d /v:on /c $fullCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR full polymorphism generated-C runtime failed" }
} finally {
    Pop-Location
}
Write-Output "XLIR virtual class dispatch and full polymorphism runtime ABI ok."
