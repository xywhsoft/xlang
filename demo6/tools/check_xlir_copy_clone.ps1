param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\tmp"
$cases = @(
    @{ Name = "cgen_copy_clone"; Source = "test\cases\cgen_copy_clone.xl"; Layout = "copy=runtime-shallow/s0 clone=runtime-deep/s0"; Ir = @("runtime=xvalue-clone", "runtime=xvalue-deep-clone"); C = @("= xrtValueClone(", "= xrtValueDeepClone(") },
    @{ Name = "cgen_generic_class_copy_clone"; Source = "test\cases\cgen_generic_class_copy_clone.xl"; Layout = "copy=runtime-shallow/s0 clone=runtime-deep/s0"; Ir = @("generic-instance source=", "runtime=xvalue-clone", "runtime=xvalue-deep-clone"); C = @("= xrtValueClone(", "= xrtValueDeepClone(") },
    @{ Name = "cgen_generic_class_custom_copy_clone"; Source = "test\cases\cgen_generic_class_custom_copy_clone.xl"; Layout = "copy=internal-call/s"; Ir = @("clone=internal-call/s", "init=scalar-zero", "generic-instance source="); C = @(); ScalarZero = $true; DynamicTargets = $true },
    @{ Name = "cgen_class_copy"; Source = "test\cases\cgen_class_copy.xl"; Layout = "copy=runtime-shallow/s0 clone=runtime-shallow/s0"; Ir = @("runtime=xvalue-clone"); C = @("= xrtValueClone(") },
    @{ Name = "cgen_class_custom_copy_clone"; Source = "test\cases\cgen_class_custom_copy_clone.xl"; Layout = "copy=internal-call/s2 clone=internal-call/s3"; Ir = @("target=2", "target=3"); C = @("= xlir_fn_2(", "= xlir_fn_3(") }
)

foreach ( $path in @($xl, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR copy/clone gate input is missing: $path"
    }
}

foreach ( $case in $cases ) {
    $source = Join-Path $Root $case.Source
    $lowered = Join-Path $outputDir "$($case.Name).copy-clone.lowered.ir"
    $jsonPath = Join-Path $outputDir "$($case.Name).copy-clone.lowered.json"
    $generated = Join-Path $outputDir "$($case.Name).copy-clone.c"
    $executable = Join-Path $outputDir "$($case.Name).copy-clone.exe"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR copy/clone fixture is missing: $source"
    }

    & $xl ir $source --lowered --format text --emit $lowered
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR copy/clone lowered IR failed: $($case.Name)" }
    $irText = [System.IO.File]::ReadAllText($lowered, [System.Text.Encoding]::UTF8)
    foreach ( $required in @($case.Layout) + $case.Ir ) {
        if ( ! $irText.Contains($required) ) {
            throw "XLIR copy/clone IR ABI is incomplete for $($case.Name): $required"
        }
    }
    $copyTarget = $null
    $cloneTarget = $null
    if ( $case.DynamicTargets ) {
        $layoutMatch = [regex]::Match(
            $irText,
            'copy=internal-call/s([0-9]+) clone=internal-call/s([0-9]+)'
        )
        if ( ! $layoutMatch.Success ) {
            throw "XLIR generic custom copy/clone layout has no concrete callable symbols: $($case.Name)"
        }
        $copyTarget = $layoutMatch.Groups[1].Value
        $cloneTarget = $layoutMatch.Groups[2].Value
        if ( $copyTarget -eq "0" -or $cloneTarget -eq "0" -or $copyTarget -eq $cloneTarget ) {
            throw "XLIR generic custom copy/clone layout has invalid callable symbols: $($case.Name)"
        }
    }

    & $xl ir $source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR copy/clone JSON dump failed: $($case.Name)" }
    $json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    if ( $null -eq $json.classLayouts -or $json.classLayouts.Count -eq 0 -or
         $null -eq $json.classLayouts[0].copyAbiName -or
         $null -eq $json.classLayouts[0].cloneAbiName ) {
        throw "XLIR copy/clone JSON class-layout table is incomplete: $($case.Name)"
    }
    if ( $case.ScalarZero -and
         $null -eq ($json.classFields | Where-Object { $_.initializerAbiName -eq "scalar-zero" }) ) {
        throw "XLIR generic class scalar-zero field initializer ABI is absent from JSON: $($case.Name)"
    }
    if ( $case.DynamicTargets ) {
        $cleanupOwners = @($json.cleanupActions | ForEach-Object { $_.ownerDeclaration })
        if ( $cleanupOwners.Count -eq 0 -or $cleanupOwners -contains $null ) {
            throw "XLIR generic custom cleanup actions have no explicit callable owner: $($case.Name)"
        }
    }

    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR copy/clone Cgen failed: $($case.Name)" }
    $cText = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    foreach ( $required in $case.C ) {
        if ( ! $cText.Contains($required) ) {
            throw "XLIR copy/clone C emitter did not mechanically consume the selected ABI for $($case.Name): $required"
        }
    }
    if ( $case.DynamicTargets ) {
        foreach ( $required in @("= xlir_fn_$copyTarget(", "= xlir_fn_$cloneTarget(")) {
            if ( ! $cText.Contains($required) ) {
                throw "XLIR generic custom copy/clone C emitter did not consume concrete callable target: $required"
            }
        }
    }

    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $runtimeHost !XL6_CGEN_LINKS! -o $executable && $executable"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR copy/clone generated-C runtime failed: $($case.Name)"
    }
}

Write-Output "XLIR copy/clone ABI gate ok: derived shallow/deep xvalue operations, monomorphic custom protocol calls, scalar-zero fields, and explicit cleanup owners."
exit 0
