param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_object_graph_boundary'
    $inputs = @('test/cases/xlir_object_graph_boundary.c', 'lib/xruntime/xruntime.h', 'release/xl.dll')
    $before = @($inputs | ForEach-Object { [pscustomobject]@{Path=$_;Sha256=(Get-FileHash $_).Hash} })
    $results = @()
    foreach ($lane in @('snapshot','resident')) {
        $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O2','-I.')
        if ($lane -eq 'snapshot') { $flags += '-DXLIR_GRAPH_SNAPSHOT' }
        $link = @()
        if ($lane -eq 'resident') { $link += 'release/xl.dll' }
        $buildLog = "$prefix.$lane.build.log"
        & gcc @flags test/cases/xlir_object_graph_boundary.c @link -o "$prefix.$lane.exe" *> $buildLog
        $buildExit = $LASTEXITCODE
        $runExit = $null
        $log = "$prefix.$lane.log"
        if ($buildExit -eq 0) { & "$prefix.$lane.exe" *> $log; $runExit = $LASTEXITCODE }
        $results += [pscustomobject]@{Lane=$lane;BuildExit=$buildExit;Exit=$runExit;BuildLog=$buildLog;Log=$log}
        Write-Output "$lane build=$buildExit run=$runExit"
    }
    $unchanged = @($before | Where-Object { (Get-FileHash $_.Path).Hash -ne $_.Sha256 }).Count -eq 0
    $report = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$before;InputsUnchanged=$unchanged;Results=$results;
        Scope='Runtime capability/counterexample probe only. Does not pass, replace or waive check_xlir_object_escape. Quiescent host-resident descriptors only.'}
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.results.json"), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    if (!$unchanged -or @($results | Where-Object { $_.BuildExit -ne 0 -or $_.Exit -ne 0 }).Count) {
        throw 'Object graph boundary probe failed; inspect both lane logs'
    }
} finally { Pop-Location }
