param([string]$Root = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path

$ruleDirectory = Join-Path $Root 'cgenrule'
$readmePath = Join-Path $ruleDirectory 'README.md'
$cgenDirectory = Join-Path $Root 'src\cgen'
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$Message) {
    $script:failures.Add($Message)
}

function Read-Utf8([string]$Path) {
    return [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
}

if (!(Test-Path -LiteralPath $readmePath -PathType Leaf)) {
    Write-Host "canonical C emitter boundary document is missing: $readmePath"
    exit 1
}

$documents = @(Get-ChildItem -LiteralPath $ruleDirectory -Filter '*.md' -File)
if ($documents.Count -ne 1 -or $documents[0].Name -ne 'README.md') {
    Add-Failure 'cgenrule must contain only the canonical README; retired AST Cgen rule tables may not return'
}

$readme = Read-Utf8 $readmePath
foreach ($required in @(
    'AST -> XLIR -> C',
    'src/cgen/xl6_ir_cgen.c',
    'src/cgen/xl6_cgen_shared_runtime.c',
    'docs/XLIR',
    'tools/check_xlir_contract.ps1',
    'AST/HIR'
)) {
    if (!$readme.Contains($required)) {
        Add-Failure "canonical C emitter boundary is missing: $required"
    }
}
foreach ($retired in @(
    'xl6_cgen_expr.c',
    'xl6_cgen_type_infer.c',
    'xl6_cgen_module.c',
    'XLC_BACKEND_LEGACY'
)) {
    if ($readme.Contains($retired)) {
        Add-Failure "canonical C emitter boundary names retired implementation: $retired"
    }
}

$requiredSources = @(
    'src/cgen/xl6_ir_cgen.c',
    'src/cgen/xl6_ir_cgen.h',
    'src/cgen/xl6_cgen_shared_runtime.c'
)
foreach ($relative in $requiredSources) {
    if (!(Test-Path -LiteralPath (Join-Path $Root $relative) -PathType Leaf)) {
        Add-Failure "canonical C emitter source is missing: $relative"
    }
}

$retiredSources = @(
    'src/cgen/xl6_cgen.c',
    'src/cgen/xl6_cgen.h',
    'src/cgen/xl6_cgen_expr.c',
    'src/cgen/xl6_cgen_type_infer.c',
    'src/cgen/xl6_cgen_module.c',
    'src/ir/xl6_hir.c',
    'src/ir/xl6_hir.h',
    'src/ir/xl6_hir_type.c',
    'src/ir/xl6_hir_type.h'
)
foreach ($relative in $retiredSources) {
    if (Test-Path -LiteralPath (Join-Path $Root $relative)) {
        Add-Failure "retired backend source returned: $relative"
    }
}

# Current normative documents must describe the unified pipeline, not the
# retired implementation that remains available only through Git history.
$documentDirectory = Join-Path $Root 'docs'
$abiDocumentCandidates = @(Get-ChildItem -LiteralPath $documentDirectory -Filter 'ABI*.md' -File |
    Where-Object { (Read-Utf8 $_.FullName).Contains('# xlang DEMO6 ABI') })
$rc0SummaryCandidates = @(Get-ChildItem -LiteralPath $documentDirectory -Filter 'RC0*.md' -File |
    Where-Object { (Read-Utf8 $_.FullName).Contains('### C emitter') })
if ($abiDocumentCandidates.Count -ne 1) {
    Add-Failure 'current ABI design document is missing'
} else {
    $abiDocument = Read-Utf8 $abiDocumentCandidates[0].FullName
    if (!$abiDocument.Contains('XL6_IrTypeId') -or
        $abiDocument.Contains('XL6_HirTypeId')) {
        Add-Failure 'current ABI design does not use the unified XLIR TypeId contract'
    }
}
if ($rc0SummaryCandidates.Count -ne 1) {
    Add-Failure 'current RC0 summary is missing'
} else {
    $rc0Summary = Read-Utf8 $rc0SummaryCandidates[0].FullName
    foreach ($required in @(
        'AST -> XLIR -> C',
        'src/cgen/xl6_ir_cgen.c',
        'src/cgen/xl6_cgen_shared_runtime.c'
    )) {
        if (!$rc0Summary.Contains($required)) {
            Add-Failure "current RC0 summary is missing the unified C emitter boundary: $required"
        }
    }
    foreach ($retired in @(
        'xl6_cgen_entry.*',
        'xl6_cgen_header.c',
        'xl6_cgen_runtime.*',
        'xl6_cgen_lifecycle.*'
    )) {
        if ($rc0Summary.Contains($retired)) {
            Add-Failure "current RC0 summary names a retired Cgen module: $retired"
        }
    }
}

$irRootPath = Join-Path $cgenDirectory 'xl6_ir_cgen.c'
if (Test-Path -LiteralPath $irRootPath -PathType Leaf) {
    $irRoot = Read-Utf8 $irRootPath
    $irShards = @(Get-ChildItem -LiteralPath $cgenDirectory -Filter 'xl6_ir_cgen_*.c' -File)
    foreach ($shard in $irShards) {
        $include = '#include "src/cgen/' + $shard.Name + '"'
        if (!$irRoot.Contains($include)) {
            Add-Failure "XLIR C emitter shard is not wired through its unity root: $($shard.Name)"
        }
    }
}

# Runtime component emitters form a separate mechanical source graph. Every
# component C file must be reachable from the one build root, including the
# local TLS/WebSocket subfragments included by their parent components.
$componentSources = @(Get-ChildItem -LiteralPath $cgenDirectory -Filter 'xl6_cgen_*.c' -File)
$componentByName = @{}
foreach ($source in $componentSources) { $componentByName[$source.Name] = $source.FullName }
$reachable = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$pending = [System.Collections.Generic.Queue[string]]::new()
$pending.Enqueue('xl6_cgen_shared_runtime.c')
while ($pending.Count -ne 0) {
    $name = $pending.Dequeue()
    if (!$reachable.Add($name) -or !$componentByName.ContainsKey($name)) { continue }
    $text = Read-Utf8 $componentByName[$name]
    foreach ($match in [regex]::Matches($text, '#include\s+"(?:src/cgen/)?([^"/]+\.c)"')) {
        $child = $match.Groups[1].Value
        if ($componentByName.ContainsKey($child)) { $pending.Enqueue($child) }
    }
}
foreach ($source in $componentSources) {
    if (!$reachable.Contains($source.Name)) {
        Add-Failure "runtime component emitter is outside the shared-runtime graph: $($source.Name)"
    }
    $text = Read-Utf8 $source.FullName
    if ($text -match 'XL6_Ast|XL6_Hir|->inferredType|xl6Sem') {
        Add-Failure "runtime component emitter reads a forbidden front-end model: $($source.Name)"
    }
}

$boundaryFiles = @(Get-ChildItem -Path @(
    (Join-Path $Root 'src\api'),
    (Join-Path $Root 'src\compile'),
    (Join-Path $Root 'src\driver'),
    (Join-Path $Root 'src\cgen'),
    (Join-Path $Root 'src\ir')
) -Include '*.c','*.h' -File -Recurse)
$boundaryText = [string]::Join("`n", @($boundaryFiles | ForEach-Object { Read-Utf8 $_.FullName }))
foreach ($retiredToken in @(
    'XLC_BACKEND_LEGACY',
    'xl6HirBuildModule',
    'xl6CgenEmitCEx',
    'xl6CgenEmitHeader('
)) {
    if ($boundaryText.Contains($retiredToken)) {
        Add-Failure "retired backend token remains in the live compiler boundary: $retiredToken"
    }
}

if ($failures.Count -ne 0) {
    Write-Host 'C emitter boundary documentation check failed.'
    $failures | Select-Object -First 80 | ForEach-Object { Write-Host "  $_" }
    if ($failures.Count -gt 80) {
        Write-Host "  ... $($failures.Count - 80) more failure(s)"
    }
    exit 1
}

$irFileCount = @(Get-ChildItem -LiteralPath $cgenDirectory -Filter 'xl6_ir_cgen*' -File).Count
Write-Host "C emitter boundary docs ok: one canonical document, $irFileCount XLIR emitter files, $($componentSources.Count) reachable runtime component sources."
exit 0
