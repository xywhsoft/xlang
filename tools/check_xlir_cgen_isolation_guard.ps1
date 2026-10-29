param([string]$Root='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$file=Join-Path $Root 'tools/check_xlir_contract.ps1'
$tokens=$null; $parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($file,[ref]$tokens,[ref]$parseErrors)
if ($parseErrors.Count) { throw 'Cgen isolation guard has invalid PowerShell syntax' }
$refusal=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.ThrowStatementAst] -and
    $node.Extent.Text.Contains('XLIR C emitter must not consume AST/HIR/semantic analysis or call legacy Cgen')},$true))
if ($refusal.Count -ne 1) { throw 'Cgen isolation must have one exact production guard' }
$condition=$refusal[0].Parent
while ($condition -and $condition -isnot [Management.Automation.Language.IfStatementAst]) { $condition=$condition.Parent }
if (!$condition) { throw 'Cgen isolation refusal has no independent condition' }
$block=[scriptblock]::Create($condition.Extent.Text)
$allowed=@('xl6SemCreateV1','xl6SemCreateDefaultV1','xl6SemWaitV1','xl6SemTryWaitV1',
    'xl6SemWaitTimeoutV1','xl6SemPostV1','xl6SemPostMultipleV1','xl6CgenEmitBufferRuntime')
$rejected=@('xl6SemCheckModule','xl6SemInferType','xl6SemUnknownFrontendOperation',
    'XL6_AstNode *','XL6_HirModule','xl6CgenUnknownLegacyOperation',
    'xl6CgenEmitNetTcpComponent','xl6CgenEmitNetDatagramComponent','xl6CgenEmitTlsRuntime')
foreach($text in $allowed+$rejected) {
    $cEmitter=$text; $failed=$false
    try { & $block } catch { $failed=$true }
    if ($failed -ne ($text -in $rejected)) { throw "Cgen isolation guard misclassified: $text" }
}
Write-Output 'Cgen isolation guard: 8 native calls accepted, 9 frontend/legacy shapes rejected'
