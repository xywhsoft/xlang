param([Parameter(Mandatory = $true)][string]$ScopeFile)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if ([IO.Path]::DirectorySeparatorChar -ne '\') {
    throw 'this guard targets Windows extension test entry points'
}
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$scopePath = if ([IO.Path]::IsPathRooted($ScopeFile)) { $ScopeFile } else {
    Join-Path $root $ScopeFile
}

# 只读取正式普通库清单；格式或数量变化必须显式更新，不能静默缩小测试范围。
$groups = @{}
$group = -1
foreach ($line in Get-Content -LiteralPath $scopePath -Encoding UTF8) {
    if ($line -match '^### P([0-3])') { $group = [int]$Matches[1] }
    elseif ($group -ge 0 -and $line -match '^`') {
        if ($groups.ContainsKey($group)) { throw "duplicate priority group P$group" }
        $groups[$group] = @([regex]::Matches($line, '`([^`]+)`') |
            ForEach-Object { $_.Groups[1].Value })
        $group = -1
    }
}
$counts = @(13, 30, 24, 33)
for ($i = 0; $i -lt $counts.Count; ++$i) {
    if (@($groups[$i]).Count -ne $counts[$i]) { throw "invalid priority group P$i" }
}
$names = @(0..3 | ForEach-Object { $groups[$_] })
if (@($names | Sort-Object -Unique).Count -ne 100) { throw 'scope must contain 100 unique packages' }

# 用 PowerShell AST 只提取编译器绑定表达式，不执行包门禁的其他语句。
$bindings = @()
foreach ($name in $names) {
    if ($name -notmatch '^[A-Za-z][A-Za-z0-9_.]*$') { throw "invalid package name: $name" }
    $directory = Join-Path (Join-Path $root 'release/extlibs') "$name/test"
    if (-not (Test-Path -LiteralPath $directory)) { continue }
    foreach ($file in Get-ChildItem -LiteralPath $directory -Recurse -File -Filter '*.ps1') {
        $tokens = $null
        $parseErrors = $null
        $source = Get-Content -LiteralPath $file.FullName -Encoding UTF8 -Raw
        $ast = [Management.Automation.Language.Parser]::ParseInput(
            $source, [ref]$tokens, [ref]$parseErrors)
        if ($parseErrors.Count) { throw "PowerShell parse failed: $($file.FullName)" }
        foreach ($assignment in $ast.FindAll({ param($node)
            $node -is [Management.Automation.Language.AssignmentStatementAst] -and
            $node.Right.Extent.Text -match '\bGet-Command\s+gcc\b'
        }, $true)) {
            $bindings += [pscustomobject]@{
                File = $file.FullName
                Line = $assignment.Extent.StartLineNumber
                Expression = $assignment.Right.Extent.Text
            }
        }
    }
}
if ($bindings.Count -eq 0) { throw 'no compiler bindings found' }

function Assert-Bindings([string]$label, [string[]]$paths, [string]$expected) {
    $env:PATH = $paths -join [IO.Path]::PathSeparator
    foreach ($binding in $bindings) {
        $failure = $null
        $actual = @()
        try { $actual = @(& ([scriptblock]::Create($binding.Expression))) }
        catch { $failure = $_ }
        if (-not $expected) {
            if ($failure -and $failure.Exception -is [Management.Automation.CommandNotFoundException]) {
                continue
            }
            throw "missing compiler not rejected: $($binding.File):$($binding.Line)"
        }
        if ($failure -or $actual.Count -ne 1 -or $actual[0] -isnot [string] -or
            -not [StringComparer]::OrdinalIgnoreCase.Equals($actual[0], $expected)) {
            throw "${label}: invalid compiler binding $($binding.File):$($binding.Line): $($actual -join ', ') $failure"
        }
    }
    if ($expected) {
        & $expected
        if ($LASTEXITCODE -ne 37) { throw "${label}: selected executable did not return 37" }
    }
    Write-Host "[compiler-path] ${label}: PASS ($($bindings.Count) bindings)"
}

$base = Join-Path $root 'build/test'
$work = Join-Path $base ('compiler_path_' + [guid]::NewGuid().ToString('N'))
$originalPath = $env:PATH
$compiler = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
try {
    $first = Join-Path $work 'first compiler'
    $second = Join-Path $work 'second compiler'
    [IO.Directory]::CreateDirectory($first) | Out-Null
    [IO.Directory]::CreateDirectory($second) | Out-Null
    $firstExe = Join-Path $first 'gcc.exe'
    $secondExe = Join-Path $second 'gcc.exe'
    # 复用已有返回 37 的 C 夹具，不增加伪编译器实现或修改生产源码。
    & $compiler -std=c11 -Wall -Wextra -Werror (Join-Path $root 'test/cases/c_run_exit_code.c') -o $firstExe
    if ($LASTEXITCODE -ne 0) { throw 'compiler fixture build failed' }
    Copy-Item -LiteralPath $firstExe -Destination $secondExe
    Assert-Bindings 'one compiler' @($first) $firstExe
    Assert-Bindings 'multiple compilers' @($first, $second, $originalPath) $firstExe
    Assert-Bindings 'reversed priority' @($second, $first, $originalPath) $secondExe
    function script:gcc { throw 'compiler discovery selected a PowerShell function' }
    Assert-Bindings 'shadowed command' @($first, $originalPath) $firstExe
    Assert-Bindings 'missing compiler' @($work) ''
} finally {
    $env:PATH = $originalPath
    if (Test-Path -LiteralPath $work) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $resolvedBase = (Resolve-Path -LiteralPath $base).Path.TrimEnd('\', '/')
        if (-not $resolved.StartsWith($resolvedBase + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path -Leaf $resolved) -notmatch '^compiler_path_[0-9a-f]{32}$') {
            throw "unsafe compiler fixture cleanup path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
exit 0
