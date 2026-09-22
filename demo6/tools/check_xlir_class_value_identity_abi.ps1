param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_derive_string_hash_equals.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$typedPath = Join-Path $Root "build\test\xlir_class_value_identity.typed.json"
$loweredPath = Join-Path $Root "build\test\xlir_class_value_identity.lowered.json"
$generatedPath = Join-Path $Root "build\test\xlir_class_value_identity.gate.c"
$repeatPath = Join-Path $Root "build\test\xlir_class_value_identity.gate.repeat.c"
$xlirExe = Join-Path $Root "build\test\xlir_class_value_identity.gate.exe"
$artifactExe = Join-Path $Root "build\test\xlir_class_value_identity.artifact.exe"

foreach ( $path in @($xl, $source, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR class value identity ABI gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl ir $source --typed --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class value identity typed IR failed" }
    & $xl ir $source --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class value identity lowered IR failed" }

    $typed = Get-Content -LiteralPath $typedPath -Raw | ConvertFrom-Json
    $lowered = Get-Content -LiteralPath $loweredPath -Raw | ConvertFrom-Json
    $recordType = @($typed.types | Where-Object { $_.name -eq "DeriveRecord" })
    $referenceType = @($typed.types |
        Where-Object { $_.name -eq "ReferenceRecord" })
    if ( $recordType.Count -ne 1 -or $referenceType.Count -ne 1 ) {
        throw "XLIR class value identity fixture has no unique nominal TypeIds"
    }
    $layout = @($typed.classLayouts |
        Where-Object { $_.type -eq $recordType[0].id })
    if ( $layout.Count -ne 1 -or
         $layout[0].hashAbiName -ne "runtime-derived" -or
         $layout[0].hashTarget -ne 0 -or
         $layout[0].equalityAbiName -ne "runtime-derived" -or
         $layout[0].equalityTarget -ne 0 ) {
        throw "XLIR class layout has no closed structural hash/equality identity ABI"
    }
    $referenceLayout = @($typed.classLayouts |
        Where-Object { $_.type -eq $referenceType[0].id })
    if ( $referenceLayout.Count -ne 1 -or
         $referenceLayout[0].hashAbiName -ne "runtime-reference" -or
         $referenceLayout[0].hashTarget -ne 0 -or
         $referenceLayout[0].equalityAbiName -ne "runtime-reference" -or
         $referenceLayout[0].equalityTarget -ne 0 ) {
        throw "XLIR plain class layout has no closed reference identity ABI"
    }
    $fields = @($typed.classFields |
        Select-Object -Skip $layout[0].firstField -First $layout[0].fieldCount)
    if ( $fields.Count -ne 3 -or
         $fields[0].compareDeriveIncluded -ne 1 -or
         $fields[1].compareDeriveIncluded -ne 1 -or
         $fields[2].compareDeriveIncluded -ne 0 ) {
        throw "XLIR class identity has no exact @compare(skip) field partition"
    }

    $identityBinds = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-class-identity-bind"
    })
    $derivedEquals = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "class-derived-equal"
    })
    if ( @($identityBinds | Where-Object {
             [int]$_.payloadType -ne [int]$_.type
         }).Count -ne 0 -or
         @($identityBinds | Where-Object {
            $_.payloadType -eq $recordType[0].id
         }).Count -ne 3 -or
         @($identityBinds | Where-Object {
            $_.payloadType -eq $referenceType[0].id
         }).Count -ne 2 -or
         $derivedEquals.Count -ne 3 -or
         @($derivedEquals | Where-Object {
            $_.payloadType -ne $recordType[0].id
         }).Count -ne 0 ) {
        throw "XLIR lowering did not preserve exact class identity bind and equality operations"
    }

    & $xl cgen $source --backend xlir --emit $generatedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class value identity Cgen failed" }
    & $xl cgen $source --backend xlir --emit $repeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class value identity repeat Cgen failed" }
    $generatedBytes = [System.IO.File]::ReadAllBytes($generatedPath)
    $repeatBytes = [System.IO.File]::ReadAllBytes($repeatPath)
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            $generatedBytes, $repeatBytes) ) {
        throw "XLIR class value identity generated C is not byte-for-byte deterministic"
    }
    $generatedText = [System.IO.File]::ReadAllText(
        $generatedPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "xlir_class_derive_equal_t", "xlir_class_derive_hash_t",
        "xlir_class_reference_hash_t", "xlir_class_reference_equal_t",
        "xlir_class_identity_bind_t", "xl6ClassRuntimeV2Get()->BindIdentity",
        'xrtStrView("id")', 'xrtStrView("name")'
    ) ) {
        if ( ! $generatedText.Contains($required) ) {
            throw "XLIR class value identity generated C lacks exact runtime data: $required"
        }
    }
    $helperStart = $generatedText.IndexOf(
        "static bool xlir_class_derive_equal_t")
    $helperEnd = if ( $helperStart -ge 0 ) {
        $generatedText.IndexOf(
            "static char* xlir_string_append_take", $helperStart)
    } else { -1 }
    if ( $helperStart -lt 0 -or $helperEnd -le $helperStart ) {
        throw "XLIR class value identity helper boundary is missing"
    }
    $helperText = $generatedText.Substring(
        $helperStart, $helperEnd - $helperStart)
    if ( $helperText.Contains('xrtStrView("transient")') ) {
        throw "XLIR class value identity helper ignored the typed compare partition"
    }

    # The strict-C lane consumes the exported resident ABI instead of silently
    # supplying test-local implementations of generated-code dependencies.
    # The following standalone lane independently exercises the production
    # artifact planner and its private source/runtime closure.
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $runtimeHost !XL6_CGEN_XLANG_LINKS! -o $xlirExe && $xlirExe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class value identity strict-C/runtime gate failed"
    }

    & $xl build $source --backend xlir -o $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class value identity standalone artifact build gate failed"
    }
    & $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class value identity standalone artifact runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR class structural hash/equality identity ABI ok."
