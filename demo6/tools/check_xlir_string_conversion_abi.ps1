param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_string_conversion_abi.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$typedPath = Join-Path $Root "build\test\xlir_string_conversion_abi.typed.json"
$loweredPath = Join-Path $Root "build\test\xlir_string_conversion_abi.lowered.json"
$generatedPath = Join-Path $Root "build\test\xlir_string_conversion_abi.gate.c"
$repeatPath = Join-Path $Root "build\test\xlir_string_conversion_abi.gate.repeat.c"
$legacyPath = Join-Path $Root "build\test\xlir_string_conversion_abi.gate.legacy.c"
$xlirExe = Join-Path $Root "build\test\xlir_string_conversion_abi.gate.exe"
$legacyExe = Join-Path $Root "build\test\xlir_string_conversion_abi.gate.legacy.exe"

foreach ( $path in @($xl, $source, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR string conversion ABI gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl ir $source --typed --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string conversion typed IR failed" }
    & $xl ir $source --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string conversion lowered IR failed" }

    $typed = Get-Content -LiteralPath $typedPath -Raw | ConvertFrom-Json
    $lowered = Get-Content -LiteralPath $loweredPath -Raw | ConvertFrom-Json
    $recordType = @($typed.types | Where-Object { $_.name -eq "StringRecord" })
    $customType = @($typed.types | Where-Object { $_.name -eq "CustomText" })
    if ( $recordType.Count -ne 1 -or $customType.Count -ne 1 ) {
        throw "XLIR string conversion fixture has no unique nominal TypeIds"
    }
    $recordLayout = @($typed.classLayouts | Where-Object { $_.type -eq $recordType[0].id })
    $customLayout = @($typed.classLayouts | Where-Object { $_.type -eq $customType[0].id })
    if ( $recordLayout.Count -ne 1 -or
         $recordLayout[0].stringAbiName -ne "runtime-derived" -or
         $recordLayout[0].stringTarget -ne 0 ) {
        throw "XLIR derived class has no closed runtime string layout ABI"
    }
    if ( $customLayout.Count -ne 1 -or
         $customLayout[0].stringAbiName -ne "internal-call" -or
         $customLayout[0].stringTarget -eq 0 ) {
        throw "XLIR explicit class string protocol has no exact internal target"
    }
    $recordFields = @($typed.classFields |
        Select-Object -Skip $recordLayout[0].firstField -First $recordLayout[0].fieldCount)
    if ( $recordFields.Count -ne 3 -or
         $recordFields[0].stringDeriveIncluded -ne 1 -or
         $recordFields[1].stringDeriveIncluded -ne 1 -or
         $recordFields[2].stringDeriveIncluded -ne 0 ) {
        throw "XLIR derived class has no exact @string(skip) field partition"
    }

    $stringIntrinsics = @($typed.intrinsicCalls |
        Where-Object { $_.kindName -eq "string-convert" })
    if ( @($stringIntrinsics | Where-Object {
            $_.stringConversionAbiName -eq "class-derived" -and
            $_.stringClassType -eq $recordType[0].id }).Count -ne 2 -or
         @($stringIntrinsics | Where-Object {
            $_.stringConversionAbiName -eq "class-protocol-call" -and
            $_.stringClassType -eq $customType[0].id }).Count -ne 2 -or
         @($stringIntrinsics | Where-Object {
            $_.stringConversionAbiName -eq "type-id-name" -and
            $_.stringClassType -eq 0 }).Count -ne 1 ) {
        throw "XLIR typed string intrinsics do not preserve the three exact conversion families"
    }

    $derivedOps = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "class-derived-to-string" })
    $typeNameOps = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "type-id-to-string" })
    $protocolCalls = @($lowered.instructions | Where-Object {
        $_.kindName -eq "call" -and
        $_.targetSymbol -eq $customLayout[0].stringTarget })
    if ( $derivedOps.Count -ne 2 -or
         @($derivedOps | Where-Object {
            $_.payloadType -ne $recordType[0].id }).Count -ne 0 -or
         $typeNameOps.Count -ne 1 -or $protocolCalls.Count -ne 2 ) {
        throw "XLIR lowering did not preserve derived, TypeId-name and exact protocol-call string ABIs"
    }

    & $xl cgen $source --backend xlir --emit $generatedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string conversion Cgen failed" }
    & $xl cgen $source --backend xlir --emit $repeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string conversion repeat Cgen failed" }
    $generatedBytes = [System.IO.File]::ReadAllBytes($generatedPath)
    $repeatBytes = [System.IO.File]::ReadAllBytes($repeatPath)
    if ( ! [System.Linq.Enumerable]::SequenceEqual($generatedBytes, $repeatBytes) ) {
        throw "XLIR string conversion generated C is not byte-for-byte deterministic"
    }
    $generatedText = [System.IO.File]::ReadAllText(
        $generatedPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "xlir_type_id_to_string", "xlir_class_derive_to_string_t",
        'xrtStrDup("StringRecord")', 'xrtStrView("id")',
        'xrtStrView("name")'
    ) ) {
        if ( ! $generatedText.Contains($required) ) {
            throw "XLIR string conversion generated C lacks exact runtime data: $required"
        }
    }
    $helperStart = $generatedText.IndexOf("static char* xlir_class_derive_to_string_t")
    $helperEnd = if ( $helperStart -ge 0 ) {
        $generatedText.IndexOf("static ", $helperStart + 20)
    } else { -1 }
    if ( $helperStart -lt 0 -or $helperEnd -le $helperStart ) {
        throw "XLIR derived class string helper boundary is missing"
    }
    $helperText = $generatedText.Substring($helperStart, $helperEnd - $helperStart)
    if ( $helperText.Contains('xrtStrView("hidden")') ) {
        throw "XLIR derived class string helper ignored the typed skip partition"
    }

    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $runtimeHost !XL6_CGEN_LINKS! -o $xlirExe && $xlirExe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR string conversion strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR string conversion legacy differential Cgen failed"
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath !XL6_CGEN_XLANG_LINKS! -o $legacyExe && $legacyExe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR string conversion legacy differential gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR TypeId, derived class and explicit class-protocol string conversion ABI ok."
