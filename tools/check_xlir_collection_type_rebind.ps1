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
        throw "XLIR collection TypeId rebind output is not deterministic: $first"
    }
}

function Assert-RebindLowering(
    [string]$name,
    [object]$lowered,
    [int]$expectedCheckedRebinds,
    [int]$expectedCheckedCasts,
    [int]$expectedClones)
{
    $instructions = @($lowered.instructions)
    $checked = @($instructions | Where-Object {
        $_.runtimeOpName -in @(
            "xvalue-collection-checked-cast",
            "xvalue-collection-checked-rebind"
        )
    })
    $ownedRebinds = @($instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-type-rebind-owned"
    })
    $clones = @($instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-clone"
    })

    if ( @($checked | Where-Object {
            $_.runtimeOpName -eq "xvalue-collection-checked-rebind"
         }).Count -ne $expectedCheckedRebinds -or
         @($checked | Where-Object {
            $_.runtimeOpName -eq "xvalue-collection-checked-cast"
         }).Count -ne $expectedCheckedCasts -or
         $ownedRebinds.Count -ne $checked.Count -or
         $clones.Count -ne $expectedClones ) {
        throw "XLIR $name collection checked/rebind/clone instruction counts are not exact"
    }

    foreach ( $operation in $checked ) {
        $errorTests = @($instructions | Where-Object {
            $_.kindName -eq "error-test" -and
            @($_.operands).Count -eq 1 -and
            @($_.operands)[0] -eq $operation.result
        })
        $identityRebinds = @($ownedRebinds | Where-Object {
            @($_.operands).Count -eq 1 -and
            @($_.operands)[0] -eq $operation.result
        })
        if ( $errorTests.Count -ne 1 -or $identityRebinds.Count -ne 1 ) {
            throw "XLIR $name checked collection result v$($operation.result) has no unique error-test and owned TypeId rebind"
        }
        $identityRebind = $identityRebinds[0]
        if ( $identityRebind.type -ne $operation.type -or
             $identityRebind.runtimeIdentityType -ne $identityRebind.type -or
             $identityRebind.runtimeIdentityType -le 0 ) {
            throw "XLIR $name owned TypeId rebind does not preserve the verified checked-result identity"
        }
    }
}

$xl = Join-Path $Root "release\xl.exe"
$projectionSource = Join-Path $Root "test\cases\xlir_dynamic_typed_collection_projection.xl"
$projectionHost = Join-Path $Root "test\cases\xlir_dynamic_typed_collection_projection_host.c"
$completeSource = Join-Path $Root "test\cases\cgen_dynamic_to_typed_containers.xl"
$completeHost = Join-Path $Root "test\cases\xlir_dynamic_to_typed_containers_host.c"
$outputDir = Join-Path $Root "build\test"

$cases = @(
    @{
        Name = "projection"
        Source = $projectionSource
        Lowered = Join-Path $outputDir "xlir_collection_type_rebind_projection.lowered.json"
        LoweredRepeat = Join-Path $outputDir "xlir_collection_type_rebind_projection.lowered.repeat.json"
        C = Join-Path $outputDir "xlir_dynamic_typed_collection_projection_generated.c"
        CRepeat = Join-Path $outputDir "xlir_collection_type_rebind_projection.repeat.c"
        CheckedRebinds = 2
        CheckedCasts = 0
        Clones = 0
    },
    @{
        Name = "complete"
        Source = $completeSource
        Lowered = Join-Path $outputDir "xlir_collection_type_rebind_complete.lowered.json"
        LoweredRepeat = Join-Path $outputDir "xlir_collection_type_rebind_complete.lowered.repeat.json"
        C = Join-Path $outputDir "xlir_collection_type_rebind_complete.c"
        CRepeat = Join-Path $outputDir "xlir_collection_type_rebind_complete.repeat.c"
        CheckedRebinds = 12
        CheckedCasts = 0
        Clones = 1
    }
)

foreach ( $path in @(
    $xl,
    $projectionSource,
    $projectionHost,
    $completeSource,
    $completeHost
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR collection TypeId rebind gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

foreach ( $case in $cases ) {
    & $xl ir $case.Source --lowered --format json --emit $case.Lowered
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR $($case.Name) collection TypeId rebind lowering failed"
    }
    & $xl ir $case.Source --lowered --format json --emit $case.LoweredRepeat
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR repeated $($case.Name) collection TypeId rebind lowering failed"
    }
    & $xl cgen $case.Source --backend xlir --emit $case.C
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR $($case.Name) collection TypeId rebind Cgen failed"
    }
    & $xl cgen $case.Source --backend xlir --emit $case.CRepeat
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR repeated $($case.Name) collection TypeId rebind Cgen failed"
    }

    Assert-Deterministic $case.Lowered $case.LoweredRepeat
    Assert-Deterministic $case.C $case.CRepeat

    $lowered = Read-Utf8 $case.Lowered | ConvertFrom-Json
    Assert-RebindLowering `
        $case.Name `
        $lowered `
        $case.CheckedRebinds `
        $case.CheckedCasts `
        $case.Clones

    $generated = Read-Utf8 $case.C
    foreach ( $required in @(
        "extern bool xrtValueTypeIdRebind(xvalue*, uint64_t);",
        "static xvalue* xlir_xvalue_type_rebind_owned(xvalue* value, uint64_t typeId)",
        "xrtValueTypeIdRebind(value, typeId)",
        "extern xvalue* xl6CollectionCheckedTakeV1(xvalue*, int, unsigned int, bool, uint64_t);"
    ) ) {
        if ( ! $generated.Contains($required) ) {
            throw "XLIR $($case.Name) collection TypeId rebind C emission is incomplete: $required"
        }
    }
    foreach ( $forbidden in @(
        "collectionAbiName",
        "runtimeIdentityType",
        "typeValueId",
        "XL6_HIR",
        "xl6Hir"
        "xlir_xvalue_collection_checked_take"
    ) ) {
        if ( $generated.Contains($forbidden) ) {
            throw "XLIR $($case.Name) generated C leaked semantic metadata or HIR tracing: $forbidden"
        }
    }
    $rebindCalls = ([regex]::Matches(
        $generated,
        "=\s*xlir_xvalue_type_rebind_owned\(v\d+,\s*0x[0-9A-F]{16}ULL\);"
    )).Count
    if ( $rebindCalls -ne ($case.CheckedRebinds + $case.CheckedCasts) ) {
        throw "XLIR $($case.Name) generated C does not mechanically render every verified owned TypeId rebind"
    }
}

$projectionExe = Join-Path $outputDir "xlir_collection_type_rebind_projection.exe"
$completeExe = Join-Path $outputDir "xlir_collection_type_rebind_complete.exe"
$compileCommands = @(
    "call test\suites\common.bat",
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Irelease -Werror "{0}" !XL6_CGEN_LINKS! -o "{1}"' -f $projectionHost, $projectionExe),
    ('"{0}"' -f $projectionExe),
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Irelease -Werror "{0}" "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $cases[1].C, $completeHost, $completeExe),
    ('"{0}"' -f $completeExe)
)
Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c ($compileCommands -join " && ")
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR collection TypeId rebind strict compile or runtime contract failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR collection TypeId rebind gate ok: checked failure edges, unique-owner identity changes, borrowed-source cloning, exact TypeIds, deterministic mechanical C, strict compile, and runtime semantics."
exit 0
