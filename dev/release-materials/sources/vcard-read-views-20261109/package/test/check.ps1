param([switch]$SkipAot)
$ErrorActionPreference = 'Stop'
$runtime = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
$root = (Resolve-Path -LiteralPath (Join-Path $runtime '../..')).Path
$arguments = @((Join-Path $root 'tools/check_vcard_read_views.py'), '--runtime', $runtime)
if ($SkipAot) { $arguments += '--skip-aot' }
& python @arguments
exit $LASTEXITCODE
