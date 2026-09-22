param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function Read-Utf8([string]$path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Assert-Deterministic([string]$first, [string]$second)
{
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($first),
            [byte[]][System.IO.File]::ReadAllBytes($second)
        ) ) {
        throw "XLIR nullable container output is not deterministic: $first"
    }
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\typed_nullable_container_matrix.xl"
$hostSource = Join-Path $Root "test\cases\xlir_typed_nullable_container_matrix_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "typed_nullable_container_matrix.typed.json"
$typedRepeatPath = Join-Path $outputDir "typed_nullable_container_matrix.typed.repeat.json"
$loweredPath = Join-Path $outputDir "typed_nullable_container_matrix.lowered.json"
$loweredRepeatPath = Join-Path $outputDir "typed_nullable_container_matrix.lowered.repeat.json"
$xlirCPath = Join-Path $outputDir "typed_nullable_container_matrix.xlir.c"
$xlirCRepeatPath = Join-Path $outputDir "typed_nullable_container_matrix.xlir.repeat.c"
$xlirExe = Join-Path $outputDir "typed_nullable_container_matrix.xlir.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nullable container gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable container typed JSON failed" }
& $xl ir $source --typed --format json --emit $typedRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated XLIR nullable container typed JSON failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable container lowering failed" }
& $xl ir $source --lowered --format json --emit $loweredRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated XLIR nullable container lowering failed" }
& $xl cgen $source --backend xlir --emit $xlirCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable container Cgen failed" }
& $xl cgen $source --backend xlir --emit $xlirCRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated XLIR nullable container Cgen failed" }

Assert-Deterministic $typedPath $typedRepeatPath
Assert-Deterministic $loweredPath $loweredRepeatPath
Assert-Deterministic $xlirCPath $xlirCRepeatPath

$typed = Read-Utf8 $typedPath | ConvertFrom-Json
$lowered = Read-Utf8 $loweredPath | ConvertFrom-Json
$instructions = @($lowered.instructions)
$definitions = @{}
foreach ( $instruction in $instructions ) {
    if ( $instruction.result -gt 0 ) {
        $definitions[[string]$instruction.result] = $instruction
    }
}

# The fixture deliberately covers every physical nullable family admitted at
# an erased collection boundary: tagged scalar, sentinel string, sentinel
# closure and nullable protocol xvalue.
$nullableTypes = @($typed.types | Where-Object { $_.kind -eq 8 })
$nullableLayouts = @($nullableTypes | ForEach-Object {
    "$($_.cAbi):$($_.ownershipAbi):$($_.nullAbi)"
} | Select-Object -Unique)
foreach ( $layout in @("36:1:3", "25:2:1", "31:2:1", "26:2:2") ) {
    if ( $layout -notin $nullableLayouts ) {
        throw "Typed XLIR lost a required nullable physical layout: $layout"
    }
}

$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 5 -or
     @($loops | Where-Object {
        $_.loopErrorAbiName -ne "no-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy"
     }).Count -ne 0 -or
     @($loops | Where-Object {
        $_.loopKeyAbiName -eq "discard" -and
        $_.loopKeyLifetimeAbiName -eq "none"
     }).Count -ne 3 ) {
    throw "Typed XLIR nullable loop shape, discarded key or resource ABI is not exact"
}
foreach ( $entry in @{
    "tagged-nullable-copy" = 2
    "nullable-string-copy" = 1
    "nullable-closure-copy" = 1
    "xvalue-retain" = 1
}.GetEnumerator() ) {
    if ( @($loops | Where-Object {
            $_.loopItemAbiName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "Typed XLIR nullable loop item ABI count is not exact: $($entry.Key)"
    }
}

$expectedRuntimeOps = @{
    "xvalue-tagged-nullable-box" = 4
    "xvalue-project-tagged-nullable-copy" = 3
    "xvalue-nullable-string-box" = 4
    "xvalue-project-nullable-string-copy" = 7
    "string-nullable-copy" = 3
    "xvalue-nullable-closure-box" = 6
    "xvalue-project-nullable-closure-copy" = 6
    "xvalue-project-nullable-xvalue-retain" = 1
    "xvalue-int-map-at-i64-or-null" = 3
    "xvalue-iterator-item-borrow" = 4
    "xvalue-iterator-item-xvalue-retain" = 1
}
foreach ( $entry in $expectedRuntimeOps.GetEnumerator() ) {
    if ( @($instructions | Where-Object {
            $_.runtimeOpName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "Lowered XLIR nullable operation count is not exact: $($entry.Key)"
    }
}

# XVALUE_RETAIN is ownership-only.  Any nominal-to-dynamic widening must be a
# following VALUE instruction rather than a hidden TypeId change in retain.
$retains = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-retain"
})
foreach ( $retain in $retains ) {
    if ( @($retain.operands).Count -ne 1 ) {
        throw "XLIR xvalue retain v$($retain.result) has no exact unary ABI"
    }
    $sourceDefinition = $definitions[[string]$retain.operands[0]]
    if ( $null -eq $sourceDefinition -or $sourceDefinition.type -ne $retain.type ) {
        throw "XLIR xvalue retain v$($retain.result) changed its source TypeId"
    }
}
$explicitRebinds = @($instructions | Where-Object {
    if ( $_.kindName -ne "value" -or @($_.operands).Count -ne 1 ) {
        return $false
    }
    $sourceDefinition = $definitions[[string]$_.operands[0]]
    return $null -ne $sourceDefinition -and
           $sourceDefinition.runtimeOpName -eq "xvalue-retain" -and
           $sourceDefinition.type -ne $_.type
})
if ( $explicitRebinds.Count -ne 5 ) {
    throw "Nullable container lowering lost explicit retain-to-VALUE TypeId rebinds"
}

$generated = Read-Utf8 $xlirCPath
$renderedRetains = @([regex]::Matches(
    $generated, "v(\d+) = xrtValueRetain\(v(\d+)\);"))
# Reachability removes dormant IR retains, while exact nullable projection now
# owns its retain inside a TypeId-checking helper.  Validate every direct retain
# against IR below instead of treating an incidental C-text count as semantics.
foreach ( $renderedRetain in $renderedRetains ) {
    $result = [int]$renderedRetain.Groups[1].Value
    $source = [int]$renderedRetain.Groups[2].Value
    $matches = @($retains | Where-Object {
        $_.result -eq $result -and @($_.operands).Count -eq 1 -and
        $_.operands[0] -eq $source
    })
    if ( $matches.Count -ne 1 ) {
        throw "Generated C retain has no unique verified XLIR instruction: $($renderedRetain.Value)"
    }
}
foreach ( $rebind in $explicitRebinds ) {
    $rendered = "v$($rebind.result) = v$($rebind.operands[0]);"
    if ( ! $generated.Contains($rendered) ) {
        throw "Generated C did not mechanically render explicit static rebind: $rendered"
    }
}
foreach ( $safeRead in @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-int-map-at-i64-or-null"
}) ) {
    if ( ! $generated.Contains(
            "if (v$($safeRead.result) == NULL) v$($safeRead.result) = xrtValueNull();") ) {
        throw "Generated C lost canonical xvalue null stabilization for safe read"
    }
}
foreach ( $required in @(
    "static bool xlir_xvalue_project_nullable_string_copy(",
    "static bool xlir_xvalue_project_closure_try_copy_into(",
    "static xvalue* xlir_xvalue_project_nullable_retain(",
    "exactTypeId != 0",
    "static xlir_nullable_t7 xlir_xvalue_project_xlir_nullable_t7(",
    "enum xlir_xvalue_tag_abi {",
    "(int)xrtValueType(values) == (int)XLIR_XVALUE_ARRAY",
    "!= NULL ? xrtStrDup("
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "Generated C nullable runtime contract is incomplete: $required"
    }
}
foreach ( $forbidden in @(
    "xrtValueType(values) == 10",
    "loopItemAbi", "loopKeyAbi", "collectionAbiName",
    "XL6_HIR", "xl6Hir"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "Generated C leaked a magic tag or semantic/HIR fallback: $forbidden"
    }
}

$compileCommands = @(
    "call test\suites\common.bat",
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include "{0}" "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $xlirCPath, $hostSource, $xlirExe)
)
Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c ($compileCommands -join " && ")
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Nullable container strict XLIR compile failed"
    }
    $xlirOutput = @(& $xlirExe 2>&1)
    $xlirExit = $LASTEXITCODE
    $xlirText = $xlirOutput -join "`n"
    if ( $xlirExit -ne 0 -or $xlirText -ne "typed nullable container matrix ok" ) {
        throw "Nullable container runtime failed: XLIR=$xlirExit '$xlirText'"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR nullable container projection gate ok: tagged/string/closure/protocol layouts, array/list/set/dict indexing and iteration, safe missing reads, discarded keys, ownership-only exact-TypeId retain, explicit VALUE rebinds, named print tag ABI, deterministic mechanical C, strict compile, and heap-clean exit."
exit 0
