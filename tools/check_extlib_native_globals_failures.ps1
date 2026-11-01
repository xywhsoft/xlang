param([string]$Shell = '')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$Shell) { $Shell = (Get-Process -Id $PID).Path }
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$work = Join-Path $root ('build/test/native_global_report_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()

# 真实调用生产门禁；只在子进程中替换哈希命令，不改待测文件或运行库。
foreach ($mode in @('first','partial','drift')) {
    $script = @'
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Import-Module Microsoft.PowerShell.Utility
$global:ownedHashCalls = 0
$global:ownedHashSeen = @{}
function global:Get-FileHash {
    param([string]$LiteralPath)
    $global:ownedHashCalls++
    if ('MODE' -eq 'first' -or ('MODE' -eq 'partial' -and $global:ownedHashCalls -eq 2)) {
        throw 'fixture: input hash unavailable'
    }
    $result = Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
    if ('MODE' -eq 'drift' -and $global:ownedHashSeen.ContainsKey($LiteralPath)) {
        $result.Hash = ('0' * 64)
    }
    $global:ownedHashSeen[$LiteralPath] = $true
    return $result
}
try {
    & 'GATE' -Root 'ROOT' -Package progress -SkipAot
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 37
}
'@
    $script = $script.Replace('MODE',$mode).Replace('GATE',(Join-Path $PSScriptRoot 'check_extlib_native_globals.ps1').Replace("'","''")).Replace('ROOT',$root.Replace("'","''"))
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($script))
    $result = Invoke-ExtlibSmokeProcess $Shell @('-NoProfile','-OutputFormat','Text','-EncodedCommand',$encoded) $root 90 @{}
    [IO.File]::WriteAllText((Join-Path $work ($mode+'.json')),($result | ConvertTo-Json -Depth 5),$utf8)
    $match = [regex]::Match($result.stdout,'\[native-globals\] report: ([^\r\n]+)')
    if ($result.exitCode -ne 37 -or !$match.Success) { throw "Missing $mode failure report" }
    $reportPath = Join-Path $match.Groups[1].Value 'results.json'
    $report = Get-Content -LiteralPath $reportPath -Encoding UTF8 -Raw | ConvertFrom-Json
    $count = @($report.inputs.PSObject.Properties).Count
    if ($mode -eq 'drift') {
        if (!$report.inputSnapshotComplete -or $report.outcome -ne 'invalid-input-change' -or
            !$report.changedInputs.Count -or $report.checks.Count -ne 2) { throw 'Completed baseline drift was not detected' }
    } else {
        $expected = if ($mode -eq 'first') { 0 } else { 1 }
        if ($report.inputSnapshotComplete -or $report.outcome -ne 'failed' -or $count -ne $expected -or
            $report.changedInputs.Count -or $report.checks.Count -or $report.failure -ne 'fixture: input hash unavailable' -or
            $result.stderr -notmatch 'fixture: input hash unavailable') { throw "$mode snapshot failure lost its first cause" }
    }
    $checks.Add([ordered]@{mode=$mode;report=$reportPath;result=$result;inputSnapshotComplete=$report.inputSnapshotComplete;
        inputCount=$count;changedInputs=$report.changedInputs;failure=$report.failure})
    Write-Host "[native-global-report] ${mode}: PASS"
}
$record = [ordered]@{status='passed';shell=$Shell;checks=@($checks.ToArray());
    toolSha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'check_extlib_native_globals.ps1')).Hash;
    selftestSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash}
[IO.File]::WriteAllText((Join-Path $work 'results.json'),($record | ConvertTo-Json -Depth 8),$utf8)
Write-Host "Native global report controls: $($checks.Count) passed; $work"
