param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_container_index_compound_assign.xl"
$stringSource = Join-Path $Root "test\cases\cgen_string_value_ownership.xl"
$dynamicSource = Join-Path $Root "test\cases\index_ownership_top_level_typed_mix.xl"
$generated = Join-Path $Root "build\test\xlir_index_compound_generated.c"
$generatedRepeat = Join-Path $Root "build\test\xlir_index_compound_generated_repeat.c"
$stringGenerated = Join-Path $Root "build\test\xlir_string_field_compound_generated.c"
$stringGeneratedRepeat = Join-Path $Root "build\test\xlir_string_field_compound_generated_repeat.c"
$dynamicGenerated = Join-Path $Root "build\test\xlir_index_compound_dynamic_rhs_generated.c"
$dynamicGeneratedRepeat = Join-Path $Root "build\test\xlir_index_compound_dynamic_rhs_generated_repeat.c"
$lowered = Join-Path $Root "build\test\xlir_index_compound_lowered.json"

foreach ( $path in @($xl, $source, $stringSource, $dynamicSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR index compound gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $generated) | Out-Null

& $xl cgen $source --backend xlir --emit $generated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR index compound Cgen failed" }
& $xl cgen $source --backend xlir --emit $generatedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated index compound Cgen failed" }
& $xl cgen $stringSource --backend xlir --emit $stringGenerated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed-string field compound Cgen failed" }
& $xl cgen $stringSource --backend xlir --emit $stringGeneratedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated managed-string field compound Cgen failed" }
& $xl cgen $dynamicSource --backend xlir --emit $dynamicGenerated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-RHS index compound Cgen failed" }
& $xl cgen $dynamicSource --backend xlir --emit $dynamicGeneratedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated dynamic-RHS index compound Cgen failed" }

if ( [IO.File]::ReadAllText($generated) -cne
     [IO.File]::ReadAllText($generatedRepeat) ) {
    throw "XLIR index compound Cgen is not deterministic"
}
if ( [IO.File]::ReadAllText($stringGenerated) -cne
     [IO.File]::ReadAllText($stringGeneratedRepeat) ) {
    throw "XLIR managed-string field compound Cgen is not deterministic"
}
if ( [IO.File]::ReadAllText($dynamicGenerated) -cne
     [IO.File]::ReadAllText($dynamicGeneratedRepeat) ) {
    throw "XLIR dynamic-RHS index compound Cgen is not deterministic"
}

& $xl ir $source --lowered --format json --emit $lowered
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR index compound lowering failed" }
$ir = Get-Content -LiteralPath $lowered -Raw | ConvertFrom-Json
$nodes = @{}
foreach ( $node in $ir.nodes ) { $nodes[[int]$node.id] = $node }
$readOps = @(
    "xvalue-array-at-i64-integer",
    "xvalue-array-at-i64-u64",
    "xvalue-array-at-i64-f64",
    "xvalue-int-map-at-i64-integer",
    "xvalue-int-map-at-i64-u64",
    "xvalue-int-map-at-i64-f64",
    "xvalue-dict-get-literal-i64",
    "xvalue-dict-get-literal-u64",
    "xvalue-dict-get-literal-f64",
    "xvalue-dict-get-literal-string-copy"
)
$writeOps = @(
    "xvalue-array-set-i64-new",
    "xvalue-int-map-set-i64-new",
    "xvalue-object-set-literal-new"
)
$compoundAssignments = @($ir.nodes | Where-Object {
    $_.kindName -eq "assign" -and $_.opcodeName -ne "none"
})
if ( $compoundAssignments.Count -ne 9 ) {
    throw "expected 9 typed index compound assignments, got $($compoundAssignments.Count)"
}
foreach ( $assignment in $compoundAssignments ) {
    $lhsId = [int]$ir.nodeChildren[[int]$assignment.firstChild]
    $lhs = $nodes[$lhsId]
    if ( $null -eq $lhs -or $lhs.kindName -ne "index" ) {
        throw "compound assignment $($assignment.id) did not retain its typed INDEX origin"
    }
    $read = @($ir.instructions | Where-Object {
        [int]$_.origin -eq $lhsId -and $_.runtimeOpName -in $readOps
    })
    $write = @($ir.instructions | Where-Object {
        [int]$_.origin -eq [int]$assignment.id -and $_.runtimeOpName -in $writeOps
    })
    if ( $read.Count -ne 1 -or $write.Count -ne 1 ) {
        throw "compound assignment $($assignment.id) lacks one exact INDEX read and ASSIGN write"
    }
}
$stringConcat = @($ir.instructions | Where-Object {
    $_.runtimeOpName -eq "string-concat"
})
if ( $stringConcat.Count -lt 1 ) {
    throw "managed-string index compound assignment lacks STRING_CONCAT"
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && " +
        "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
        "-include build\test\xlir_index_compound_generated.c " +
        "test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! " +
        "-o build\test\xlir_index_compound.exe && " +
        "build\test\xlir_index_compound.exe && " +
        "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
        "-include build\test\xlir_string_field_compound_generated.c " +
        "test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! " +
        "-o build\test\xlir_string_field_compound.exe && " +
        "build\test\xlir_string_field_compound.exe && " +
        "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
        "-DXLIR_GENERATED_MODULE_PREINCLUDED " +
        "-include build\test\xlir_index_compound_dynamic_rhs_generated.c " +
        "test\cases\cgen_class_host.c " +
        "test\cases\xlir_generated_module_diagnostic_host.c " +
        "!XL6_CGEN_LINKS! -o build\test\xlir_index_compound_dynamic_rhs.exe && " +
        "build\test\xlir_index_compound_dynamic_rhs.exe"
    & cmd.exe /v:on /d /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR index/class-field compound generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR compound assignment gate ok: every typed index mutation has one INDEX-origin read and ASSIGN-origin write, dynamic RHS values project exactly once, managed strings use explicit owned CONCAT lifetimes, generated C is deterministic, -Werror clean, and runtime semantics pass."
