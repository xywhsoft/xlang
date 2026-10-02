param(
    [string]$Root = '',
    [string]$BaselineParser = '',
    [string]$Baseline = 'tools/baselines/xlir_parser_corpus.json',
    [string]$BaselineGeneratedParser = '',
    [string]$Directory = ''
)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($Directory) {
        $Directory = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
        if (Test-Path -LiteralPath $Directory) { throw 'Preserve prior parser corpus evidence' }
        New-Item -ItemType Directory -Path $Directory | Out-Null
    } else { $Directory = 'build/test' }
    if ($BaselineParser -and !$BaselineGeneratedParser) {
        throw 'Source differential requires the baseline generated parser, not just its API adapter'
    }
    Copy-Item release/xl.dll "$Directory/xl.dll" -Force
    $inputs = @('test/cases/xlir_parser_corpus.c',
        'src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c','src/lexer/xl6_token.c','src/diag/xl6_diag.c',
        'src/ast/xl6_ast.c','src/common/xl6_output.c','src/common/xl6_console.c')
    $paths = @(Get-ChildItem test/cases -File -Filter '*.xl' | Sort-Object Name | ForEach-Object FullName)
    $outputs = @{}
    $passes = if ($BaselineParser) { @('baseline','current') } else { @('current') }
    foreach ($pass in $passes) {
        $parser = if ($pass -eq 'baseline') { $BaselineParser } else { 'src/parser/xl6_parser_api.c' }
        $generatedParser = if ($pass -eq 'baseline') { $BaselineGeneratedParser } else { 'src/parser/xl6_parser.c' }
        $prefix = "$Directory/xlir_parser_corpus.$pass"
        & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -O2 -I. @inputs $generatedParser $parser release/xl.dll -o "$prefix.exe"
        if ($LASTEXITCODE -ne 0) { throw "$pass parser corpus build failed" }
        $outputs[$pass] = @($paths | & "$prefix.exe")
        if ($LASTEXITCODE -ne 0 -or $outputs[$pass].Count -ne $paths.Count) { throw "$pass parser corpus incomplete" }
        [IO.File]::WriteAllLines([IO.Path]::GetFullPath("$prefix.log"), [string[]]$outputs[$pass], [Text.UTF8Encoding]::new($false))
    }

    $rootPath = (Resolve-Path -LiteralPath '.').Path.TrimEnd('\')
    $canonical = @()
    $accepted = 0
    $rejected = 0
    for ($i = 0; $i -lt $paths.Count; ++$i) {
        $current = $outputs.current[$i].Split('|', 3)
        $observedPath = $current[2].TrimStart([char]0xFEFF)
        if ($observedPath -ne $paths[$i]) { throw 'Current corpus identity/order changed' }
        if (!$observedPath.StartsWith($rootPath + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Corpus path escaped the project root: $observedPath"
        }
        $relative = $observedPath.Substring($rootPath.Length + 1).Replace('\', '/')
        $canonical += "$($current[0])|$($current[1])|$relative"
        if ($current[0] -eq '1') { ++$accepted } else { ++$rejected }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $resultSha256 = [BitConverter]::ToString($sha.ComputeHash(
            [Text.Encoding]::UTF8.GetBytes(($canonical -join "`n")))).Replace('-', '')
    } finally { $sha.Dispose() }

    $regressions = @(); $newlyAccepted = @(); $baselineSha256 = $null; $baselineMatches = $false
    if ($BaselineParser) {
        for ($i = 0; $i -lt $paths.Count; ++$i) {
            $old = $outputs.baseline[$i].Split('|', 3)
            $new = $outputs.current[$i].Split('|', 3)
            if ($old[2].TrimStart([char]0xFEFF) -ne $paths[$i]) { throw 'Baseline corpus identity/order changed' }
            if ($old[0] -eq '1' -and ($new[0] -ne '1' -or $old[1] -ne $new[1])) { $regressions += $paths[$i] }
            elseif ($old[0] -eq '0' -and $new[0] -eq '1') { $newlyAccepted += $paths[$i] }
        }
        $baselineSha256 = (Get-FileHash $BaselineParser).Hash
        $baselineMatches = $regressions.Count -eq 0
    } else {
        $baselinePath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Baseline))
        if (!(Test-Path -LiteralPath $baselinePath -PathType Leaf)) { throw "Parser corpus baseline missing: $Baseline" }
        $golden = Get-Content -LiteralPath $baselinePath -Raw | ConvertFrom-Json
        if ($golden.SchemaVersion -ne 1 -or $golden.Count -lt 1 -or !$golden.ResultSha256) {
            throw 'Parser corpus baseline schema is invalid'
        }
        $baselineSha256 = (Get-FileHash -LiteralPath $baselinePath).Hash
        $baselineMatches = $golden.Count -eq $paths.Count -and
            $golden.Accepted -eq $accepted -and $golden.Rejected -eq $rejected -and
            $golden.ResultSha256 -eq $resultSha256
    }
    $report = [pscustomobject]@{Count=$paths.Count;Accepted=$accepted;Rejected=$rejected;
        ResultSha256=$resultSha256;BaselineMatches=$baselineMatches;Regressions=$regressions;NewlyAccepted=$newlyAccepted;
        BaselineMode=if ($BaselineParser) {'source-differential'} else {'versioned-golden'};
        BaselineSha256=$baselineSha256;CurrentParserSha256=(Get-FileHash src/parser/xl6_parser_api.c).Hash;
        Scope='Parse-only corpus comparison, preserving accepted status and anonymous-function AST count; not semantic or runtime coverage.'}
    $report | Add-Member -NotePropertyName BaselineGeneratedParserSha256 -NotePropertyValue $(if($BaselineGeneratedParser){(Get-FileHash $BaselineGeneratedParser).Hash}else{$null})
    [IO.File]::WriteAllText([IO.Path]::GetFullPath("$Directory/xlir_parser_corpus.results.json"), ($report | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
    Write-Output ($report | ConvertTo-Json -Depth 4)
    if (!$baselineMatches) { throw 'Parser corpus differs from its reviewed baseline' }
} finally { Pop-Location }
