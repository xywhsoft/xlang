param([string]$Root = '', [string]$Report = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
# Focused regression fixtures, not another parameterized-worker inventory.
$workers = @('aggregate_strict_source', 'callable_contextual', 'capture_ownership',
    'code_finalizer', 'frame_ownership', 'future_aggregate_ownership',
    'future_ownership', 'generator_ownership', 'generator_public_source', 'terminal_source_runtime')
$paths = @($PSCommandPath) + @($workers | ForEach-Object { Join-Path $Root "tools/check_xlir_$_.ps1" })
$inputs = @($paths | ForEach-Object {
    [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}
})
$crossDrive = if ([IO.Path]::GetPathRoot($Root) -eq 'F:\') { 'E:\' } else { 'F:\' }
$unicode = -join ([char]0x6D4B, [char]0x8BD5)
$cases = @(('build/test/path spaces ' + $unicode), 'build/test/../path spaces', '.',
    (Join-Path $Root ('build/test/path spaces ' + $unicode)), ($crossDrive + 'path spaces\output'), '\\localhost\not-accessed\output')
$rootPath = $Root
$rows = @(); $failures = @(); $previous = Get-Location
try {
    Set-Location $Root
    foreach ($worker in $workers) {
        $path = Join-Path $Root "tools/check_xlir_$worker.ps1"
        $tokens=$null; $errors=$null
        $ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
        if ($errors.Count) { throw "Invalid worker syntax: $worker" }
        $assignments = @($ast.FindAll({param($node)
            $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -eq '$outputPath'
        }, $true) | Sort-Object { $_.Extent.StartOffset })
        if (!$assignments.Count) { throw "Missing output-path assignment: $worker" }
        $actualAssignment = [scriptblock]::Create($assignments[0].Extent.Text)
        foreach ($Directory in $cases) {
            $expected = [IO.Path]::GetFullPath([IO.Path]::Combine($Root, $Directory))
            $outputPath = $null; $errorText = $null
            try { . $actualAssignment } catch { $errorText = $_.Exception.Message }
            $passed = !$errorText -and $outputPath -ceq $expected
            $row = [ordered]@{Worker=$worker;Directory=$Directory;Expected=$expected;Actual=$outputPath;Error=$errorText;Passed=$passed}
            $rows += $row
            if (!$passed) { $failures += $row }
        }
        $Directory = 'invalid' + [char]0 + 'path'; $rejected = $false
        try { . $actualAssignment } catch { $rejected = $true }
        $row = [ordered]@{Worker=$worker;InvalidNul=$true;Passed=$rejected}
        $rows += $row
        if (!$rejected) { $failures += $row }
    }
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -cne $record.Sha256) { throw "Changed path input: $($record.Path)" }
    }
} finally { Set-Location $previous }
$result = [ordered]@{Complete=$failures.Count -eq 0;Edition=$PSVersionTable.PSEdition;Inputs=$inputs;
    Workers=$workers.Count;Checks=$rows.Count;Failures=$failures;Rows=$rows;
    WorkerExecutionEvaluated=$false;FullParameterizedAccepted=$false;FullGoalComplete=$false;
    Scope='Evaluate only each actual first output-path assignment; no directory creation, worker execution, compiler, or UNC access.'}
if ($Report) {
    if (Test-Path -LiteralPath $Report) { throw 'Preserve prior path evidence' }
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($Report), ($result | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
}
if ($failures.Count) { throw "Output-path regressions: $($failures.Count)" }
Write-Output "Worker output paths: $($workers.Count) actual assignments, $($rows.Count) checks; no worker execution claim"
