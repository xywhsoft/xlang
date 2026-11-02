param([switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$arguments = @('-B', '-X', 'utf8', (Join-Path $PSScriptRoot 'check.py'))
if ($SkipAot) { $arguments += '--skip-aot' }
& python @arguments
exit $LASTEXITCODE
