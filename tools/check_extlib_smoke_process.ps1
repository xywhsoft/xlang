param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$shell = (Get-Command powershell -CommandType Application).Source
$work = Join-Path $Root ('build/test/smoke process ' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($true)
$fixture = Join-Path $work 'child.ps1'
[IO.File]::WriteAllText($fixture, @'
param([string]$Mode, [string]$Value)
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
if ($Mode -eq 'echo') {
    [Console]::Write($Value)
    [Console]::Error.Write('diagnostic')
    exit 7
}
if ($Mode -eq 'sleep') {
    Start-Sleep -Seconds 60
    exit 0
}
if ($Mode -eq 'tree') {
    $child = Start-Process -FilePath (Get-Command powershell).Source -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile -Command Start-Sleep -Seconds 60'
    [Console]::WriteLine($child.Id)
    Start-Sleep -Seconds 60
}
'@, $utf8)

# 空格、引号、尾随反斜线与 UTF-8 不能被参数拼接或输出读取破坏。
$value = '中文 "quoted" path\'
$result = Invoke-ExtlibSmokeProcess $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'echo',$value) $work 5
if ($result.status -ne 'fail' -or $result.exitCode -ne 7 -or $result.stdout -cne $value -or $result.stderr -cne 'diagnostic') {
    throw "Native capture/argument contract failed: $($result | ConvertTo-Json -Compress)"
}
$result = Invoke-ExtlibSmokeProcess $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'sleep') $work 1
if ($result.status -ne 'timeout' -or $result.elapsedMs -gt 10000) { throw 'Timeout was not enforced' }
$result = Invoke-ExtlibSmokeProcess $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File',$fixture,'tree') $work 3
if ($result.status -ne 'timeout' -or $result.stdout.Trim() -notmatch '^\d+$') { throw 'Process tree timeout was not exercised' }
$childId = [int]$result.stdout.Trim()
if ($null -ne (Get-Process -Id $childId -ErrorAction SilentlyContinue)) { throw "Owned child leaked: $childId" }
$result = Invoke-ExtlibSmokeProcess (Join-Path $work 'missing.exe') @() $work 1
if ($result.status -ne 'runner-error' -or $null -ne $result.exitCode) { throw 'Missing executable treated as test success' }
Write-Host 'Extension smoke process contract: PASS (capture, arguments, timeout, child cleanup, launch error)'
