[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$directoryPath=[IO.Path]::GetFullPath($Directory)
$batch=[IO.File]::ReadAllText((Join-Path $root 'build_dll.bat'))
# The explicit compile commands are the source of truth. Never glob a build
# directory: stale objects from earlier configurations must not enter a DLL.
$matches=[regex]::Matches($batch, '(?m)^"%XL6_RELEASE_CC%"[^\r\n]* -c [^\r\n]* -o "%XL6_BUILD_TMP%\\([A-Za-z0-9_]+\.o)"\s*$')
$objects=@($matches | ForEach-Object { $_.Groups[1].Value })
if ($objects.Count -lt 30 -or @($objects | Select-Object -Unique).Count -ne $objects.Count) {
    throw 'Missing or duplicate explicit DLL object commands.'
}
$lines=@($objects | ForEach-Object {
    $path=Join-Path $directoryPath $_
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing compiled DLL object: $path" }
    if ($path.Contains('"') -or $path.Contains("`r") -or $path.Contains("`n")) { throw 'Invalid response-file path.' }
    '"'+$path.Replace('\','/')+'"'
})
$output=Join-Path $directoryPath 'dll_objects.rsp'
[IO.File]::WriteAllLines($output,$lines,(New-Object Text.UTF8Encoding $false))
Write-Output "DLL response contains $($objects.Count) explicit compiled objects"
