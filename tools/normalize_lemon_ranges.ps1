param([Parameter(Mandatory = $true)][string]$Path)
$ErrorActionPreference = 'Stop'
$resolved = (Resolve-Path -LiteralPath $Path).Path
$encoding = [System.Text.UTF8Encoding]::new($false)
$text = [System.IO.File]::ReadAllText($resolved, $encoding)
if ($text.Contains('/* XL6 source-range stack v1 */')) { exit 0 }
$grammar = [System.IO.File]::ReadAllText([System.IO.Path]::ChangeExtension($resolved, '.y'), $encoding)
if (![regex]::IsMatch($grammar, '%default_type\s*\{\s*XL6_AstNode\*\s*\}')) { throw 'Unexpected Lemon default semantic type' }
$types = @{}
foreach ($m in [regex]::Matches($grammar, '(?m)^%type\s+(\w+)\s*\{([^}]+)\}')) { $types[$m.Groups[1].Value] = $m.Groups[2].Value.Trim() }
$ast = @{}
foreach ($m in [regex]::Matches($grammar, '(?m)^(\w+)(?:\((\w+)\))?\s*::=')) {
    $name = $m.Groups[1].Value
    if ($name -in @('program', 'opt_seps', 'need_sep', 'if_sep')) { continue }
    if (!$m.Groups[2].Success) { throw "Uninitialized semantic nonterminal: $name" }
    if (!$types.ContainsKey($name) -or $types[$name] -eq 'XL6_AstNode*') { $ast[$name] = $true }
}
$member = [regex]::Matches($text, '(?m)^\s*XL6_AstNode\* (yy\d+);\r?$')
if ($member.Count -ne 1) { throw 'Cannot identify the AST union member' }
$member = $member[0].Groups[1].Value
$symbols = @{}
$lhsTable = [regex]::Match($text, 'static const YYCODETYPE yyRuleInfoLhs\[\] = \{([\s\S]*?)\n\};')
if (!$lhsTable.Success) { throw 'Missing Lemon LHS table' }
foreach ($m in [regex]::Matches($lhsTable.Groups[1].Value, '(?m)^\s*(\d+),\s*/\* \(\d+\) (\w+) ::=')) { $symbols[$m.Groups[2].Value] = [int]$m.Groups[1].Value }
if (!$symbols.ContainsKey('program')) { throw 'Missing Lemon rule information' }
$cases = @()
foreach ($name in ($ast.Keys | Sort-Object)) {
    if (!$symbols.ContainsKey($name)) { throw "Missing rule for $name" }
    $cases += "    case $($symbols[$name]): /* $name */"
}
$cases += "      if (yymsp->minor.$member) yymsp->minor.$member->range = yySourceRange;"
$cases += '      break;'
$cases += "    case $($symbols['program']):"
$cases += '      if (ctx && ctx->root) ctx->root->range = yySourceRange;'
$cases += '      break;'
$cases += '    default: break;'
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
function Insert-After([string]$pattern, [string]$addition) {
    $matches = [regex]::Matches($script:text, $pattern)
    if ($matches.Count -ne 1) { throw "Unexpected Lemon template anchor: $pattern ($($matches.Count))" }
    $m = $matches[0]
    $script:text = $script:text.Insert($m.Index + $m.Length, $addition.Replace("`n", $nl))
}
$stackAnchor = [regex]::Matches($text, '(?m)^  YYMINORTYPE minor;')
if ($stackAnchor.Count -ne 1) { throw 'Missing Lemon stack member' }
$text = $text.Insert($stackAnchor[0].Index, "  XL6_AstRange sourceRange; /* XL6 source-range stack v1 */" + $nl)
Insert-After '(?m)^  yypParser->yystack\[0\].major = 0;\r?\n' "  memset(&yypParser->yystack[0].sourceRange, 0, sizeof(XL6_AstRange));`n"
Insert-After '(?m)^  yytos->minor.yy0 = yyMinor;\r?\n' "  yytos->sourceRange = xl6AstRangeFromToken(&yyMinor);`n"
Insert-After '(?m)^  int yysize;[^\r\n]*\r?\n' "  XL6_AstRange yySourceRange;`n  int yySourceIndex;`n"
$beforeActions = "  /* Capture spans before grammar actions reuse or destroy semantic values. */`n" +
    "  memset(&yySourceRange, 0, sizeof(yySourceRange));`n" +
    "  for (yySourceIndex = yyRuleInfoNRhs[yyruleno] + 1; yySourceIndex <= 0; ++yySourceIndex) {`n" +
    "    xl6AstRangeInclude(&yySourceRange, &yymsp[yySourceIndex].sourceRange);`n  }`n"
Insert-After '(?m)^  yymsp = yypParser->yytos;\r?\n' $beforeActions
$tail = "  yymsp->sourceRange = yySourceRange;`n  switch (yygoto) {`n" + ($cases -join "`n") + "`n  }`n"
Insert-After '(?m)^  yymsp->major = (?:\(YYCODETYPE\))?yygoto;\r?\n' $tail
[System.IO.File]::WriteAllText($resolved, $text, $encoding)
