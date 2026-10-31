[CmdletBinding()]
param([string]$Root='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=(Resolve-Path -LiteralPath $Root).Path
$cleaner=Join-Path $Root 'tools\clean_release.ps1'
$testRoot=Join-Path $Root ('build\release-cleanup-safety-'+[Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $testRoot)
function Put([string]$Name,[string]$Text) {
    $path=Join-Path $testRoot $Name
    [void](New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force)
    [IO.File]::WriteAllText($path,$Text)
}
function Check([bool]$Condition,[string]$Message) { if (!$Condition) { throw $Message } }
function Run-Cleanup([string]$Output,[bool]$ExpectSuccess=$true) {
    $savedPreference=$ErrorActionPreference
    try {
        $ErrorActionPreference='Continue'
        $log=& powershell -NoProfile -ExecutionPolicy Bypass -File $cleaner -Root $testRoot -OutputDirectory $Output 2>&1
        $code=$LASTEXITCODE
    } finally { $ErrorActionPreference=$savedPreference }
    Check (($code -eq 0) -eq $ExpectSuccess) ("Unexpected cleanup exit $code : "+($log -join "`n"))
}
Put 'release\tmp\proof\result.json' '{"passed":true}'
Put 'release\tmp\xl6_build_old\cache.o' 'temporary object'
Put 'release\projects\dev\toy\toy.xl' 'print(42)'
Put 'release\projects\dev\toy\toy.exe' 'generated example'
Put 'release\projects\xs\xs.exe' 'required by start.bat'
Put 'release\examples\extlibs\sqlite_test.exe' 'test executable'
Put 'release\extlibs\toy\__pycache__\temporary.pyc' 'unreferenced cache'
Put 'release\sources\xlang\tools\__pycache__\pinned.pyc' 'fixed source input'
Put 'release\option\settings.json' '{"user":"preserved"}'
Put 'release\option\settings.json.lock' ''
Put 'release\projects\dev\fixture\original.xl.bak' 'intentional fixture'
Put 'release\ide-install-receipt.json' '{"installed":{"sources/xlang/tools/__pycache__/pinned.pyc":"pinned"}}'
$settingsHash=(Get-FileHash -LiteralPath (Join-Path $testRoot 'release\option\settings.json')).Hash
$first=Join-Path $testRoot 'build\first'
Run-Cleanup $first
$report=Get-Content -LiteralPath (Join-Path $first 'cleanup.json') -Raw | ConvertFrom-Json
Check ($report.beforeFiles -eq 12 -and $report.afterFiles -eq 6) 'Unexpected cleanup file counts'
Check ($report.operations.Count -eq 6) 'Expected six classified operations'
Check (@($report.blocked).Count -eq 0) 'Unexpected blocked entries'
Check ((Get-Content -LiteralPath (Join-Path $first 'validation\proof\result.json') -Raw) -eq '{"passed":true}') 'Archived proof changed'
Check ((Get-FileHash -LiteralPath (Join-Path $testRoot 'release\option\settings.json')).Hash -eq $settingsHash) 'User settings changed'
Check (Test-Path -LiteralPath (Join-Path $testRoot 'release\sources\xlang\tools\__pycache__\pinned.pyc')) 'Pinned source input removed'
Check (Test-Path -LiteralPath (Join-Path $testRoot 'release\projects\xs\xs.exe')) 'Required application removed'
Check (Test-Path -LiteralPath (Join-Path $testRoot 'release\projects\dev\fixture\original.xl.bak')) 'Intentional fixture removed'
Run-Cleanup (Join-Path $testRoot 'build\second')
$second=Get-Content -LiteralPath (Join-Path $testRoot 'build\second\cleanup.json') -Raw | ConvertFrom-Json
Check ($second.releaseReductionBytes -eq 0 -and @($second.operations).Count -eq 0) 'Second cleanup must be a no-op'
Run-Cleanup (Join-Path $testRoot 'release\forbidden-output') $false
Check (!(Test-Path -LiteralPath (Join-Path $testRoot 'release\forbidden-output'))) 'Output boundary was not enforced'
Put 'release\option\settings.json.lock' ''
$lock=Join-Path $testRoot 'release\option\settings.json.lock'
$handle=[IO.File]::Open($lock,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
    Run-Cleanup (Join-Path $testRoot 'build\busy')
    $busy=Get-Content -LiteralPath (Join-Path $testRoot 'build\busy\cleanup.json') -Raw | ConvertFrom-Json
    Check (@($busy.blocked).Count -eq 1) 'Active settings lock must be retained'
    Check (Test-Path -LiteralPath $lock) 'Active settings lock was removed'
} finally { $handle.Dispose() }
Run-Cleanup (Join-Path $testRoot 'build\unlocked')
Check (!(Test-Path -LiteralPath $lock)) 'Inactive lock was not removed'
Put 'release\projects\dev\unknown.exe' 'unclassified executable'
Run-Cleanup (Join-Path $testRoot 'build\unknown') $false
Check (Test-Path -LiteralPath (Join-Path $testRoot 'release\projects\dev\unknown.exe')) 'Unclassified executable was removed'
# Move the unknown fixture out of release so the following test isolates link refusal.
$unknown=Join-Path $testRoot 'release\projects\dev\unknown.exe'
$retained=Join-Path $testRoot 'build\unknown-fixture.exe'
Check ([IO.Path]::GetFullPath($unknown).StartsWith($testRoot+'\')) 'Source outside test workspace'
Check ([IO.Path]::GetFullPath($retained).StartsWith($testRoot+'\')) 'Destination outside test workspace'
Move-Item -LiteralPath $unknown -Destination $retained
Put 'build\outside\sentinel.txt' 'must survive'
$junction=Join-Path $testRoot 'release\tmp\junction'
[void](New-Item -ItemType Directory -Path (Split-Path -Parent $junction) -Force)
[void](New-Item -ItemType Junction -Path $junction -Target (Join-Path $testRoot 'build\outside'))
Run-Cleanup (Join-Path $testRoot 'build\reparse') $false
Check ((Get-Content -LiteralPath (Join-Path $testRoot 'build\outside\sentinel.txt') -Raw) -eq 'must survive') 'Reparse target changed'
# Delete only the verified junction, without recursive traversal of its target.
Check ([IO.Path]::GetFullPath($junction).StartsWith($testRoot+'\release\')) 'Junction outside test workspace'
Check ([bool]((Get-Item -LiteralPath $junction).Attributes -band [IO.FileAttributes]::ReparsePoint)) 'Expected a junction'
[IO.Directory]::Delete($junction)
[ordered]@{status='passed';testRoot=$testRoot;checks=@('classified removal and proof archive','retained hashes','pinned source inputs','required application and intentional fixture','idempotency','output boundary','active settings lock','unknown executable refusal','reparse refusal')} | ConvertTo-Json -Depth 4
