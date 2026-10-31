[CmdletBinding()]
param([string]$Root='', [string]$OutputDirectory='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=(Resolve-Path -LiteralPath $Root).Path
$release=(Resolve-Path -LiteralPath (Join-Path $Root 'release')).Path
if (!$OutputDirectory) { $OutputDirectory=Join-Path $Root ('build\release-cleanup-'+(Get-Date -Format 'yyyyMMdd-HHmmss')) }
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
function Assert-Within([string]$Path,[string]$Boundary) {
    $absolute=[IO.Path]::GetFullPath($Path); $base=[IO.Path]::GetFullPath($Boundary).TrimEnd('\')
    if (!$absolute.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw "Path outside cleanup boundary: $absolute" }
    $probe=$absolute
    while ($probe.Length -ge $base.Length) {
        if (Test-Path -LiteralPath $probe) {
            $item=Get-Item -LiteralPath $probe -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse path refused: $probe" }
        }
        if ($probe -eq $base) { break }; $probe=Split-Path -Parent $probe
    }
    return $absolute
}
function Get-SafeFiles([string]$Path) {
    $stack=New-Object 'Collections.Generic.Stack[string]'; $stack.Push($Path)
    while ($stack.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $stack.Pop() -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse entry refused: $($item.FullName)" }
            if ($item.PSIsContainer) { $stack.Push($item.FullName) } else { $item }
        }
    }
}
function Get-Sha([string]$Path) {
    $stream=[IO.File]::OpenRead($Path); $hash=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose(); $hash.Dispose() }
}
function Relative([string]$Path) { return $Path.Substring($release.Length+1).Replace('\','/') }
[void](Assert-Within $OutputDirectory (Join-Path $Root 'build'))
[void](New-Item -ItemType Directory -Path $OutputDirectory -Force)
$before=@(Get-SafeFiles $release); $baseline=@{}; $beforeBytes=[long]0
$busyBaselineLocks=New-Object Collections.Generic.List[string]
foreach ($file in $before) {
    $name=Relative $file.FullName
    try { $baseline[$name]=Get-Sha $file.FullName }
    catch {
        if ($name -ne 'option/settings.json.lock') { throw }
        $baseline[$name]=$null; $busyBaselineLocks.Add($name)
    }
    $beforeBytes+=$file.Length
}
$baseline | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'before-sha256.json') -Encoding UTF8
$pinned=@{}; $receipt=Join-Path $release 'ide-install-receipt.json'
if (Test-Path -LiteralPath $receipt) {
    (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).installed.PSObject.Properties | ForEach-Object { $pinned[$_.Name]=$true }
}
$plan=New-Object Collections.Generic.List[object]; $keptCaches=New-Object Collections.Generic.List[string]
$tmp=Join-Path $release 'tmp'
if (Test-Path -LiteralPath $tmp -PathType Container) {
    foreach ($item in @(Get-ChildItem -LiteralPath $tmp -Force)) {
        $action=if ($item.Name -like 'xl6_build_*') {'remove'} else {'archive'}
        $plan.Add([pscustomobject]@{path=$item.FullName;action=$action;reason='completed validation or compiler temporary output'})
    }
}
$cacheDirectories=@($before | Where-Object { $_.FullName -match '[\\/]__pycache__[\\/]' } | ForEach-Object { $_.DirectoryName } | Sort-Object -Unique)
foreach ($directory in $cacheDirectories) {
    if ($directory.StartsWith($tmp+'\',[StringComparison]::OrdinalIgnoreCase)) { continue }
    $files=@(Get-SafeFiles $directory)
    if ((Relative $directory).StartsWith('sources/',[StringComparison]::OrdinalIgnoreCase) -or @($files | Where-Object { $pinned.ContainsKey((Relative $_.FullName)) }).Count) { $keptCaches.Add((Relative $directory)); continue }
    $plan.Add([pscustomobject]@{path=$directory;action='remove';reason='unreferenced Python bytecode cache'})
}
foreach ($file in @($before | Where-Object { $_.FullName -like "$release\projects\dev\*.exe" })) {
    $paired=(Test-Path -LiteralPath ([IO.Path]::ChangeExtension($file.FullName,'.xl'))) -or (Test-Path -LiteralPath ([IO.Path]::ChangeExtension($file.FullName,'.c')))
    $primesSmoke=(Relative $file.FullName) -eq 'projects/dev/primes/primes_smoke.exe' -and (Test-Path -LiteralPath (Join-Path $file.DirectoryName 'primes.xl'))
    if (!$paired -and !$primesSmoke) { throw "Unclassified development executable: $($file.FullName)" }
    $plan.Add([pscustomobject]@{path=$file.FullName;action='remove';reason='rebuildable development example executable'})
}
$sqlite=Join-Path $release 'examples\extlibs\sqlite_test.exe'
if (Test-Path -LiteralPath $sqlite) { $plan.Add([pscustomobject]@{path=$sqlite;action='remove';reason='SQLite test executable'}) }
$lock=Join-Path $release 'option\settings.json.lock'
if (Test-Path -LiteralPath $lock) { $plan.Add([pscustomobject]@{path=$lock;action='remove';reason='inactive preferences writer lock'}) }
$running=@(Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath })
$changed=@{}; $operations=New-Object Collections.Generic.List[object]; $blocked=New-Object Collections.Generic.List[string]
$plan.ToArray() | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'plan.json') -Encoding UTF8
foreach ($entry in $plan) {
    $source=Assert-Within $entry.path $release
    if (!(Test-Path -LiteralPath $source)) { continue }
    $item=Get-Item -LiteralPath $source -Force
    $files=if ($item.PSIsContainer) {@(Get-SafeFiles $source)} else {@($item)}
    $active=@($running | Where-Object { $_.ExecutablePath -eq $source -or ($item.PSIsContainer -and $_.ExecutablePath.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) })
    if ($active.Count) { $blocked.Add((Relative $source)); continue }
    if ($source -eq $lock) {
        try { $handle=[IO.File]::Open($lock,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None); $handle.Dispose() }
        catch { $blocked.Add((Relative $source)); continue }
    }
    $bytes=[long]0; foreach ($file in $files) { $bytes+=$file.Length }
    $destination=$null
    if ($entry.action -eq 'archive') {
        $destination=Assert-Within (Join-Path $OutputDirectory ('validation\'+$item.Name)) (Join-Path $Root 'build')
        if (Test-Path -LiteralPath $destination) { throw "Archive already exists: $destination" }
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force)
        Move-Item -LiteralPath $source -Destination $destination
        $moved=if ($item.PSIsContainer) {@(Get-SafeFiles $destination)} else {@(Get-Item -LiteralPath $destination)}
        foreach ($file in $moved) {
            $original=if ($item.PSIsContainer) {$source+$file.FullName.Substring($destination.Length)} else {$source}
            if ((Get-Sha $file.FullName) -ne $baseline[(Relative $original)]) { throw "Archive verification failed: $original" }
        }
    } else {
        if ($item.PSIsContainer) { Remove-Item -LiteralPath $source -Recurse -Force } else { Remove-Item -LiteralPath $source -Force }
    }
    foreach ($file in $files) { $changed[(Relative $file.FullName)]=$true }
    $operation=[pscustomobject]@{path=(Relative $source);action=$entry.action;reason=$entry.reason;destination=$destination;files=$files.Count;bytes=$bytes}
    $operations.Add($operation)
    $operation | ConvertTo-Json -Depth 5 -Compress | Add-Content -LiteralPath (Join-Path $OutputDirectory 'operations.jsonl') -Encoding UTF8
}
if ((Test-Path -LiteralPath $tmp) -and @(Get-ChildItem -LiteralPath $tmp -Force).Count -eq 0) { [void](Assert-Within $tmp $release); Remove-Item -LiteralPath $tmp -Force }
$after=@(Get-SafeFiles $release); $afterBytes=[long]0
foreach ($file in $after) {
    $name=Relative $file.FullName; $afterBytes+=$file.Length
    if (!$baseline.ContainsKey($name) -or ($null -ne $baseline[$name] -and (Get-Sha $file.FullName) -ne $baseline[$name])) { throw "Retained payload changed during cleanup: $name" }
}
$expected=$baseline.Count-$changed.Count
if ($after.Count -ne $expected) { throw "Unexpected payload file count: $($after.Count), expected $expected" }
$report=[ordered]@{schema=1;status='cleaned';createdUtc=[DateTime]::UtcNow.ToString('o');beforeFiles=$before.Count;beforeBytes=$beforeBytes;afterFiles=$after.Count;afterBytes=$afterBytes;releaseReductionBytes=($beforeBytes-$afterBytes);retainedBytesUnchanged=($busyBaselineLocks.Count -eq 0);unhashedBusyLocks=$busyBaselineLocks.ToArray();preservedPinnedCaches=$keptCaches.ToArray();blocked=$blocked.ToArray();operations=$operations.ToArray()}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'cleanup.json') -Encoding UTF8
Write-Output "Release cleanup: $($before.Count) -> $($after.Count) files; $([Math]::Round(($beforeBytes-$afterBytes)/1MB,2)) MiB removed from release; $($blocked.Count) busy entries retained."
