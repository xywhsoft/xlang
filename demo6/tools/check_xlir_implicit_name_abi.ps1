param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$fixtures = @(
    @{ Name = "cgen_base_method_static_dispatch"; BaseCount = 2; RequireVirtual = $true },
    @{ Name = "cgen_base_method_xlir"; BaseCount = 2 },
    @{ Name = "cgen_stdlib_namespace_shadow"; FieldCount = 1 }
)

foreach ( $path in @($xl, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR implicit-name gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Push-Location $Root
try {
    foreach ( $fixture in $fixtures ) {
        $name = $fixture.Name
        $source = Join-Path $Root "test\cases\$name.xl"
        $typedPath = Join-Path $outputDir "$name.implicit_name.typed.json"
        $loweredPath = Join-Path $outputDir "$name.implicit_name.lowered.json"
        $cPath = Join-Path $outputDir "$name.implicit_name.xlir.c"
        $cRepeatPath = Join-Path $outputDir "$name.implicit_name.repeat.xlir.c"
        $xlirExe = Join-Path $outputDir "$name.implicit_name.xlir.exe"

        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR implicit-name source is missing: $source"
        }
        & $xl ir $source --format json --emit $typedPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name Typed XLIR failed" }
        & $xl ir $source --lowered --format json --emit $loweredPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name Lowered XLIR failed" }
        & $xl cgen $source --backend xlir --emit $cPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name XLIR Cgen failed" }
        & $xl cgen $source --backend xlir --emit $cRepeatPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name repeated XLIR Cgen failed" }
        if ( ! [System.Linq.Enumerable]::SequenceEqual(
                [byte[]][System.IO.File]::ReadAllBytes($cPath),
                [byte[]][System.IO.File]::ReadAllBytes($cRepeatPath)) ) {
            throw "$name XLIR Cgen is not byte deterministic"
        }

        $typed = [System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $lowered = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $baseNames = @($typed.nodes | Where-Object {
            $_.implicitNameAbiName -eq "base-receiver-view"
        })
        $fieldNames = @($typed.nodes | Where-Object {
            $_.implicitNameAbiName -eq "class-field"
        })
        $expectedBase = if ( $null -ne $fixture.BaseCount ) {
            $fixture.BaseCount
        } else { 0 }
        $expectedField = if ( $null -ne $fixture.FieldCount ) {
            $fixture.FieldCount
        } else { 0 }
        if ( $baseNames.Count -ne $expectedBase -or
             $fieldNames.Count -ne $expectedField ) {
            throw "$name has an unexpected closed implicit-name ABI population"
        }

        foreach ( $base in $baseNames ) {
            $sites = @($typed.callSites | Where-Object {
                $_.receiver -eq $base.id
            })
            if ( $base.implicitReceiverType -eq 0 -or
                 $base.storageType -eq 0 -or
                 $base.implicitReceiverType -eq $base.storageType -or
                 $sites.Count -ne 1 -or $sites[0].dispatchSlot -ne 0 ) {
                throw "$name base receiver is not an exact direct-dispatch CallSite"
            }
            $direct = @($lowered.instructions | Where-Object {
                $_.origin -eq $sites[0].origin -and $_.kindName -eq "call" -and
                $_.targetSymbol -eq $sites[0].targetSymbol -and $_.dispatchSlot -eq 0
            })
            $virtual = @($lowered.instructions | Where-Object {
                $_.origin -eq $sites[0].origin -and $_.kindName -eq "virtual-call"
            })
            if ( $direct.Count -ne 1 -or $virtual.Count -ne 0 ) {
                throw "$name base receiver was not lowered to one explicit direct call"
            }
        }
        if ( $fixture.RequireVirtual ) {
            $virtualSites = @($typed.callSites | Where-Object {
                $_.dispatchSlot -gt 0
            })
            $virtualCalls = @($lowered.instructions | Where-Object {
                $_.kindName -eq "virtual-call" -and $_.dispatchSlot -gt 0
            })
            if ( $virtualSites.Count -lt 1 -or $virtualCalls.Count -lt 1 ) {
                throw "$name lost ordinary virtual dispatch while closing base dispatch"
            }
        }
        foreach ( $field in $fieldNames ) {
            $projection = @($lowered.instructions | Where-Object {
                $_.origin -eq $field.id -and
                $_.kindName -eq "runtime-call" -and
                $_.runtimeOpName -eq "xvalue-object-get-literal"
            })
            if ( $field.implicitReceiverType -eq 0 -or
                 $field.implicitFieldDeclaration -eq 0 -or
                 $field.memberAbiName -ne "class-field-xvalue-borrow" -or
                 $projection.Count -ne 1 ) {
                throw "$name implicit field has no exact declaration-driven projection ABI"
            }
        }

        $generated = [System.IO.File]::ReadAllText(
            $cPath, [System.Text.Encoding]::UTF8)
        foreach ( $forbidden in @(
            "base-receiver-view", "implicitNameAbi", "implicitFieldDeclaration",
            "xl6IrLower", "xl6Cgen"
        ) ) {
            if ( $generated.Contains($forbidden) ) {
                throw "$name generated C leaked frontend or backend inference: $forbidden"
            }
        }

        $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $cPath $hostSource !XL6_CGEN_LINKS! -o $xlirExe && $xlirExe"
        & $env:ComSpec /d /v:on /c $xlirCommand
        if ( $LASTEXITCODE -ne 0 ) { throw "$name strict XLIR runtime failed" }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR implicit-name ABI gate ok: base direct dispatch, ordinary virtual dispatch, class-field identity, deterministic Cgen, and strict runtime."
exit 0
