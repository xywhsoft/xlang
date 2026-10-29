param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\nullable_expression_semantics.xl"
$typedPath = Join-Path $Root "build\tmp\nullable_expression_semantics.coalesce.typed.json"
$loweredPath = Join-Path $Root "build\tmp\nullable_expression_semantics.coalesce.lowered.json"
$generatedPath = Join-Path $Root "build\test\nullable_expression_semantics_coalesce_generated.c"
$repeatPath = Join-Path $Root "build\tmp\nullable_expression_semantics.coalesce.repeat.c"
$programHost = Join-Path $Root "test\cases\xlir_program_null_args_host.c"

foreach ( $path in @($xl, $source, $programHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nullable coalesce gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path `
    (Split-Path -Parent $typedPath),(Split-Path -Parent $generatedPath) | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable coalesce Typed dump failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$nested = @($typed.nodes | Where-Object {
    [int]$_.line -eq 58 -and $_.opcodeName -eq "null-coalesce"
} | Sort-Object { [int]$_.column })
if ( $nested.Count -ne 2 ) {
    throw "Typed XLIR lost the nested nullable-coalesce pair"
}
$inner = $nested[0]
$outer = $nested[1]
$innerType = @($typed.types | Where-Object {
    [int]$_.id -eq [int]$inner.storageType
})
if ( $innerType.Count -ne 1 -or [int]$innerType[0].kind -ne 8 -or
     [int]$innerType[0].cAbi -ne 36 -or [int]$innerType[0].nullAbi -ne 3 -or
     [int]$outer.storageType -eq [int]$inner.storageType ) {
    throw "Typed XLIR nested coalesce lost its exact T? then T result identities"
}

& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable coalesce lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$innerInstructions = @($lowered.instructions | Where-Object {
    [int]$_.origin -eq [int]$inner.id
} | Sort-Object { [int]$_.index })
$unwraps = @($innerInstructions | Where-Object { $_.kindName -eq "nullable-unwrap" })
$wraps = @($innerInstructions | Where-Object { $_.kindName -eq "nullable-wrap" })
$tests = @($innerInstructions | Where-Object { $_.kindName -eq "null-test" })
$branches = @($innerInstructions | Where-Object { $_.kindName -eq "branch" })
if ( $unwraps.Count -ne 1 -or $wraps.Count -ne 1 -or
     $tests.Count -ne 1 -or $branches.Count -ne 1 -or
     [int]$wraps[0].type -ne [int]$inner.storageType -or
     [int]$unwraps[0].index -ge [int]$wraps[0].index -or
     ! (@($wraps[0].operands) -contains [int]$unwraps[0].result) ) {
    throw "Lowered XLIR T? ?? T? is not an explicit unwrap-then-wrap CFG"
}
$outerInstructions = @($lowered.instructions | Where-Object {
    [int]$_.origin -eq [int]$outer.id
})
if ( @($outerInstructions | Where-Object {
        $_.kindName -eq "nullable-unwrap" -and
        [int]$_.type -eq [int]$outer.storageType
    }).Count -ne 1 ) {
    throw "Lowered XLIR outer coalesce lost its exact non-null projection"
}

& $xl cgen $source --backend xlir --emit $generatedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable coalesce Cgen failed" }
& $xl cgen $source --backend xlir --emit $repeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable coalesce repeat Cgen failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($repeatPath)
    ) ) {
    throw "XLIR nullable coalesce C output is not byte-for-byte deterministic"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(".hasValue", ".value", "xlir_module_init") ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR nullable coalesce mechanical C is incomplete: $required"
    }
}

Push-Location $Root
try {
    & $xl run $source
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nullable coalesce CLI runtime failed"
    }
    $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\nullable_expression_semantics_coalesce_generated.c test\cases\xlir_program_null_args_host.c !XL6_CGEN_LINKS! -o build\tmp\nullable_expression_semantics.coalesce.exe && build\tmp\nullable_expression_semantics.coalesce.exe'
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nullable coalesce strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR nullable coalesce ABI gate passed: nested T? ?? T? preserves its exact nullable TypeId through explicit unwrap/wrap CFG, outer projection remains non-null, fallback evaluation is lazy, and deterministic strict-C and CLI runtimes agree."
