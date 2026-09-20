param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_xvalue_membership.xl"
$hostSource = Join-Path $Root "test\cases\xlir_xvalue_membership_host.c"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_xvalue_membership.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_xvalue_membership.lowered.json"
$cPath = Join-Path $outputDir "xlir_xvalue_membership_generated.c"
$exePath = Join-Path $outputDir "xlir_xvalue_membership_host.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR xvalue membership gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue membership text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($text, "runtime=xvalue-contains")).Count -ne 19 -or
     ([regex]::Matches($text, "runtime=xvalue-set-has")).Count -ne 2 ) {
    throw "XLIR xvalue membership has no exact operation partition"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue membership JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$containsOps = @($json.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-contains"
})
$setOps = @($json.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-set-has"
})
$temporarySymbols = @{}
foreach ( $storage in @($json.storages | Where-Object { $_.kindName -eq "temporary" }) ) {
    $temporarySymbols[[string]$storage.symbol] = $storage
}
$allReleases = @($json.instructions | Where-Object { $_.kindName -eq "release" })
$releases = @($allReleases | Where-Object { $temporarySymbols.ContainsKey([string]$_.targetSymbol) })
if ( $containsOps.Count -ne 19 -or $setOps.Count -ne 2 -or
     @($containsOps | Where-Object {
         $_.kindName -ne "runtime-call" -or $_.operandCount -ne 2 -or
         $_.hasImmediate -ne 0 -or $_.targetSymbol -ne 0 -or
         $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
         $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
     }).Count -ne 0 -or
     $temporarySymbols.Count -lt 23 -or $releases.Count -ne $temporarySymbols.Count -or
     @($releases | Where-Object {
         $_.operandCount -ne 0 -or ! $temporarySymbols.ContainsKey([string]$_.targetSymbol)
     }).Count -ne 0 ) {
    throw "XLIR xvalue membership JSON has no exact two-borrow bool and temporary ownership ABI"
}
foreach ( $entry in $temporarySymbols.GetEnumerator() ) {
    $matching = @($releases | Where-Object { [string]$_.targetSymbol -eq $entry.Key })
    if ( $matching.Count -ne 1 -or $matching[0].type -ne $entry.Value.type ) {
        throw "Each membership and boxing temporary must release its exact type once"
    }
}
$stringTypes = @($json.types | Where-Object { $_.name -eq "string" } | ForEach-Object { $_.id })
$stringReleases = @($releases | Where-Object { $_.type -in $stringTypes })
if ( $stringReleases.Count -lt 1 ) {
    throw "String boxing must close the owned source copied into dynamic storage"
}

# Persistent containers alias the same underlying objects in this fixture.
# Their owners close in the module finalizer, separately from expression temps.
$globals = @($json.storages | Where-Object { $_.kindName -eq "global" })
$finalizers = @($json.functions | Where-Object { $_.kindName -eq "module-fini" })
if ( $globals.Count -ne 9 -or $finalizers.Count -ne 1 -or
     $json.moduleFinalizerFunction -ne $finalizers[0].id -or
     $json.moduleFinalizerCleanupCount -ne $globals.Count -or
     $allReleases.Count -ne ($releases.Count + $globals.Count) ) {
    throw "Membership owners must partition into exact expression and module lifetimes"
}
$finalizerReleases = @($json.instructions | Select-Object `
    -Skip $finalizers[0].firstInstruction -First $finalizers[0].instructionCount |
    Where-Object { $_.kindName -eq "release" })
if ( $finalizerReleases.Count -ne $globals.Count ) {
    throw "Membership globals must be released only by the module finalizer"
}
for ( $i = 0; $i -lt $globals.Count; ++$i ) {
    $storage = $globals[$globals.Count - 1 - $i]
    $release = $finalizerReleases[$i]
    if ( $release.targetSymbol -ne $storage.symbol -or $release.type -ne $storage.type -or
         $release.operandCount -ne 0 -or @($allReleases | Where-Object {
             $_.targetSymbol -eq $storage.symbol
         }).Count -ne 1 ) {
        throw "Membership globals must close their exact owners once in reverse order"
    }
}
$allStringReleases = @($allReleases | Where-Object { $_.type -in $stringTypes })

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue membership Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_xvalue_contains(const xvalue* values, const xvalue* item)",
    "if (valuesType == 11) return xrtValueSetHas(values, item);",
    "if (valuesType == 12) {",
    "if (itemType != 4 || !xrtValueGetString(item, &itemText)) return false;",
    "return xrtValueObjectHas(values, itemText);",
    "if (valuesType == 4) {",
    "return xrtStrContains(valuesText, itemText);",
    "if (valuesType != 9 && valuesType != 10) return false;",
    "iterator = xrtValueIterCreate(values);",
    "while ((current = xrtValueIterNext(iterator, NULL)) != NULL) {",
    "if (xrtValueEqual(current, item)) { found = true; break; }",
    "xrtValueIterDestroy(iterator);"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR xvalue membership C rendering is incomplete: $required"
    }
}
if ( ([regex]::Matches($generated, "xlir_xvalue_contains\(v[0-9]+, v[0-9]+\)")).Count -ne 19 -or
     ([regex]::Matches($generated, "xrtValueRelease\(s[0-9]+\)")).Count -ne ($allReleases.Count - $allStringReleases.Count) -or
     ([regex]::Matches($generated, "xrtFree\(s[0-9]+\)")).Count -ne $allStringReleases.Count ) {
    throw "XLIR xvalue membership C rendering did not preserve operation/release counts"
}
$objectTag = $generated.IndexOf("if (valuesType == 12) {")
$objectGetter = $generated.IndexOf("xrtValueGetString(item, &itemText)", $objectTag)
$stringTag = $generated.IndexOf("if (valuesType == 4) {")
$stringGetter = $generated.IndexOf("xrtValueGetString(values, &valuesText)", $stringTag)
if ( $objectTag -lt 0 -or $objectGetter -le $objectTag -or
     $stringTag -lt 0 -or $stringGetter -le $stringTag ) {
    throw "XLIR xvalue membership probes getters before validating runtime tags"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue membership strict generated-C runtime failed" }

Write-Output "XLIR xvalue membership gate ok: verified typed/dynamic array, list, set, dict/object and string tag dispatch, stable iteration, two-borrow ABI, explicit temporary releases, strict C and runtime."
exit 0
