[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments=$true)][string[]]$IdeArguments)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runtime=Join-Path $root 'release\win_x64'
$state=Join-Path $root 'dev\local-state\ide\win_x64'
[void](New-Item -ItemType Directory -Path $state -Force)
$settings=Join-Path $state 'settings.json'
if (-not (Test-Path -LiteralPath $settings)) {
    $value=@{format='xlang-ide-settings-1';toolchain=@{compiler=(Join-Path $runtime 'xl.exe')}}
    [IO.File]::WriteAllText($settings,($value|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
}
& (Join-Path $runtime 'xlang_ide.exe') --settings $settings --session-root (Join-Path $state 'sessions') --diagnostic-root (Join-Path $state 'diagnostics') @IdeArguments
