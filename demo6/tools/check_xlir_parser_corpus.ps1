param([string]$Root = '', [Parameter(Mandatory=$true)][string]$BaselineParser)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $inputs = @('test/cases/xlir_parser_corpus.c','src/parser/xl6_parser.c',
        'src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c','src/lexer/xl6_token.c','src/diag/xl6_diag.c',
        'src/ast/xl6_ast.c','src/common/xl6_output.c','src/common/xl6_console.c')
    $paths = @(Get-ChildItem test/cases -File -Filter '*.xl' | Sort-Object Name | ForEach-Object FullName)
    $outputs = @{}
    foreach ($pass in @('baseline','current')) {
        $parser = if ($pass -eq 'baseline') { $BaselineParser } else { 'src/parser/xl6_parser_api.c' }
        $prefix = "build/test/xlir_parser_corpus.$pass"
        & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -O2 -I. @inputs $parser release/xl.dll -o "$prefix.exe"
        if ($LASTEXITCODE -ne 0) { throw "$pass parser corpus build failed" }
        $outputs[$pass] = @($paths | & "$prefix.exe")
        if ($LASTEXITCODE -ne 0 -or $outputs[$pass].Count -ne $paths.Count) { throw "$pass parser corpus incomplete" }
        [IO.File]::WriteAllLines((Join-Path (Get-Location) "$prefix.log"), [string[]]$outputs[$pass], [Text.UTF8Encoding]::new($false))
    }
    $regressions = @(); $newlyAccepted = @(); $unchangedAccepted = 0; $unchangedRejected = 0
    for ($i = 0; $i -lt $paths.Count; ++$i) {
        $old = $outputs.baseline[$i].Split('|',3); $new = $outputs.current[$i].Split('|',3)
        if ($old[2] -ne $paths[$i] -or $new[2] -ne $paths[$i]) { throw 'Corpus identity/order changed' }
        if ($old[0] -eq '1' -and ($new[0] -ne '1' -or $old[1] -ne $new[1])) { $regressions += $paths[$i] }
        elseif ($old[0] -eq '0' -and $new[0] -eq '1') { $newlyAccepted += $paths[$i] }
        elseif ($new[0] -eq '1') { ++$unchangedAccepted }
        else { ++$unchangedRejected }
    }
    $report = [pscustomobject]@{Count=$paths.Count;UnchangedAccepted=$unchangedAccepted;UnchangedRejected=$unchangedRejected;
        Regressions=$regressions;NewlyAccepted=$newlyAccepted;BaselineParserSha256=(Get-FileHash $BaselineParser).Hash;
        CurrentParserSha256=(Get-FileHash src/parser/xl6_parser_api.c).Hash;
        Scope='Parse-only corpus comparison, preserving accepted status and anonymous-function AST count; not semantic or runtime coverage.'}
    [IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_parser_corpus.results.json'), ($report | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
    Write-Output ($report | ConvertTo-Json -Depth 4)
    if ($regressions.Count) { throw 'All parser corpus regressions collected' }
} finally { Pop-Location }
