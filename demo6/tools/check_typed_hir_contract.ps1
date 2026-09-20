param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure([string]$message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8([string]$relative)
{
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "missing typed HIR contract source: $relative"
        return ""
    }
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

$cgenDir = Join-Path $Root "src\cgen"
$cgenFiles = @(Get-ChildItem -LiteralPath $cgenDir -Filter "*.c" -File)
$directAstTypeReads = @($cgenFiles | Select-String -Pattern '(?:->|\.)inferredType')
if ( $directAstTypeReads.Count -ne 0 ) {
    foreach ( $match in $directAstTypeReads ) {
        Add-Failure "cgen bypasses typed HIR through AST inferredType: $($match.Path):$($match.LineNumber)"
    }
}

$rawTypeCalls = @($cgenFiles | Select-String -SimpleMatch "xl6CgenCloneExprTypeForInferenceRaw(")
$rawTypeFiles = @(
    (Join-Path $cgenDir "xl6_cgen_type_hir.c"),
    (Join-Path $cgenDir "xl6_cgen_type_infer_expr.c")
)
$rawCallers = @($rawTypeCalls | Where-Object { $rawTypeFiles -notcontains $_.Path })
if ( $rawCallers.Count -ne 0 ) {
    foreach ( $match in $rawCallers ) {
        Add-Failure "raw cgen type inference escaped the typed HIR gateway: $($match.Path):$($match.LineNumber)"
    }
}
if ( $rawTypeCalls.Count -ne 2 ) {
    Add-Failure "raw cgen type inference wiring changed: expected one definition and one typed HIR gateway call, actual $($rawTypeCalls.Count)"
}

$typeGateway = Read-Utf8 "src\cgen\xl6_cgen_type_hir.c"
foreach ( $required in @(
    "typed HIR source expression",
    "xl6HirTypeCloneAst",
    "if ( hirNode != NULL )",
    "xl6CgenCloneExprTypeForInferenceRaw(ctx, expr)"
) ) {
    if ( ! $typeGateway.Contains($required) ) {
        Add-Failure "typed HIR cgen gateway is missing invariant: $required"
    }
}

$nullableType = Read-Utf8 "src\cgen\xl6_cgen_type_nullable.c"
$nullableIdStart = $nullableType.IndexOf("static unsigned long long xl6CgenNullableTypeId")
$nullableIdEnd = $nullableType.IndexOf("static void xl6CgenNullableCTypeName", $nullableIdStart)
if ( $nullableIdStart -lt 0 -or $nullableIdEnd -lt 0 ) {
    Add-Failure "nullable runtime type identity helper boundary is missing"
} elseif ( [regex]::IsMatch(
    $nullableType.Substring($nullableIdStart, $nullableIdEnd - $nullableIdStart),
    'if\s*\([^\)]*semanticTypeAbiId') ) {
    Add-Failure "nullable internal symbol identity depends on optional semantic metadata"
}

$hir = Read-Utf8 "src\ir\xl6_hir.c"
foreach ( $required in @(
    "typed HIR value node has no canonical type",
    "typed HIR value node has no canonical storage type",
    "typed HIR source index is inconsistent",
    "typed HIR target symbol index is inconsistent",
    "xl6HirVerifyModule(module, diags)"
) ) {
    if ( ! $hir.Contains($required) ) {
        Add-Failure "typed HIR verifier is missing invariant: $required"
    }
}

$compileUnit = Read-Utf8 "src\compile\xl6_compile_unit.c"
if ( ([regex]::Matches($compileUnit, 'xl6IrBuildModuleFromAst\(&unit->ir')).Count -lt 2 ) {
    Add-Failure "file/source compile-unit paths do not both build verified typed XLIR"
}
if ( $compileUnit.Contains('xl6HirBuildModule(&unit->hir') ) {
    Add-Failure "compile-unit loading eagerly rebuilds the retired legacy HIR"
}

$api = Read-Utf8 "src\api\xl_api.c"
foreach ( $required in @(
    "HIR is no longer a compile-unit prerequisite",
    "xlEnsureUnitLegacyHir", 'unit->phase = "legacy-hir"',
    "xl6HirBuildModule(&unit->hir"
) ) {
    if ( ! $api.Contains($required) ) {
        Add-Failure "legacy Cgen no longer isolates lazy typed HIR construction: $required"
    }
}

$sem = Read-Utf8 "src\sem\xl6_sem.c"
if ( ([regex]::Matches($sem, 'xl6SemInvalidateFlowMutations\(')).Count -lt 6 ) {
    Add-Failure "control-flow merge paths no longer share flow-mutation invalidation"
}

$xrt = Read-Utf8 "lib\xrt\xrt.h"
if ( $xrt.Contains("to_string)(const ptr pObj") ) {
    Add-Failure "XRT to_string callback regressed to const-pointer instead of pointer-to-const"
}

if ( $failures.Count -ne 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Typed IR contract ok: compile units build XLIR eagerly; retired HIR is isolated behind the explicit legacy Cgen boundary."
exit 0
