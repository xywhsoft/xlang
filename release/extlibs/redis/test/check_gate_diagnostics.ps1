param([string]$GatePath = (Join-Path $PSScriptRoot 'check.ps1'))

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('redis_gate_diagnostics_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false

function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

# 直接取实际门禁的 Run，避免自测另写一份近似实现。
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path -LiteralPath $GatePath).Path, [ref]$tokens, [ref]$errors)
Require ($errors.Count -eq 0) 'Gate script has parser errors'
$functions = @($ast.FindAll({param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Run'
}, $true))
Require ($functions.Count -eq 1) 'Expected one gate Run function'
. ([scriptblock]::Create($functions[0].Extent.Text))

function Reject([string]$name, [string]$program, [string[]]$arguments,
    [int]$expectedExit = 0, [int]$timeoutSeconds = 120) {
    $count = $checks.Count
    $threw = $false
    try { $null = Run $name $program $arguments $expectedExit $timeoutSeconds } catch { $threw = $true }
    Require $threw "$name was accepted"
    Require ($checks.Count -eq $count + 1) "$name did not record its process result"
    return $checks[$checks.Count - 1].result
}

Push-Location $root
try {
    $LASTEXITCODE = 0
    $missing = Join-Path $work 'missing.exe'
    $result = Reject 'missing program' $missing @()
    Require ($result.status -eq 'runner-error' -and $null -eq $result.exitCode) 'Missing-program status changed'
    Require ($result.stderr.Length -gt 0) 'Missing-program diagnostic is empty'
    Require ($checks[0].program -ceq $missing) 'Missing-program path was not recorded'

    $gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $probe = Join-Path $work 'probe.exe'
    $null = Run 'probe compile' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror',
        (Join-Path $PSScriptRoot 'gate_probe_c.c'),'-o',$probe)
    $result = Reject 'silent nonzero' $probe @('exit37')
    Require ($result.status -eq 'fail' -and $result.exitCode -eq 37) 'Lost raw exit 37'
    Require ($result.stdout -ceq '' -and $result.stderr -ceq '') 'Silent child was not silent'
    $null = Run 'expected nonzero' $probe @('exit37') 37
    $null = Reject 'unexpected success' $probe @() 1
    $empty = Run 'null arguments' $probe $null
    Require ($empty -ceq 'empty arguments') 'Null argument list gained an empty argument'

    $streams = Run 'separate streams' $probe @('streams')
    $result = $checks[$checks.Count - 1].result
    Require ($result.stdout.Replace("`r`n","`n") -ceq "stdout`n") 'stdout was lost'
    Require ($result.stderr.Replace("`r`n","`n") -ceq "stderr`n") 'stderr was lost'
    Require ($streams -ceq "stdout`nstderr") 'Combined caller diagnostic changed'
    Require ((Run 'preserve bare CR' $probe @('bare-cr')) -ceq "a`rb") 'Output normalization removed an internal CR'

    [IO.File]::WriteAllText((Join-Path $work 'probe.txt'), 'relative cwd', [Text.UTF8Encoding]::new($false))
    Push-Location $work
    try { $text = Run 'relative cwd' $probe @('relative') } finally { Pop-Location }
    Require ($text -ceq 'relative cwd') 'Child did not inherit the current test directory'
    $result = Reject 'bounded child' $probe @('wait') 0 1
    Require ($result.status -eq 'timeout' -and $result.elapsedMs -lt 15000) 'Child timeout did not terminate promptly'

    # 本门禁会调用 .ps1 C 运行器，此路径也必须得到原始进程退出码。
    $script = Join-Path $work 'exit37.ps1'
    [IO.File]::WriteAllText($script, 'exit 37', [Text.UTF8Encoding]::new($false))
    $result = Reject 'PowerShell child' $script @()
    Require ($result.status -eq 'fail' -and $result.exitCode -eq 37) 'PowerShell child exit was lost'
    Require ($checks[$checks.Count - 1].arguments -contains $script) 'Effective shell command was not recorded'

    $saved = Get-Content -LiteralPath (Join-Path $work 'report.json') -Raw | ConvertFrom-Json
    Require ($saved.Count -eq $checks.Count) 'Structured report is incomplete'
    Require (([IO.File]::ReadAllText((Join-Path $work 'commands.log'))).Contains('exit=37')) 'Command log lost raw exit code'
    [ordered]@{passed=$true; gate=(Resolve-Path -LiteralPath $GatePath).Path; checks=$checks} |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath ($work + '.report.json') -Encoding UTF8
    $passed = $true
    Write-Host "Redis gate diagnostics: PASS ($($checks.Count) child results)"
} finally {
    Pop-Location
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $target = [IO.Path]::GetFullPath($work)
    if (-not $target.StartsWith($base, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($target).StartsWith('redis_gate_diagnostics_')) {
        throw "Unsafe diagnostic cleanup path: $target"
    }
    if ($passed) { Remove-Item -LiteralPath $target -Recurse -Force }
    else { Write-Warning "Redis diagnostic failure artifacts: $target" }
}
