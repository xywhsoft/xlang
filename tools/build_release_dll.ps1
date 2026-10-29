[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet('runtime','compiler')][string]$Target)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if (-not $env:XL6_BUILD_TMP -or -not $env:XL6_RELEASE_CC) { throw 'Call this helper through build_dll.bat after toolchain initialization.' }
$template=Join-Path $PSScriptRoot ($Target+'_dll.rsp.in')
$text=[Environment]::ExpandEnvironmentVariables([IO.File]::ReadAllText($template,[Text.Encoding]::UTF8))
$text=$text.Replace('\','/')
if ($text -match '%[A-Za-z0-9_]+%') { throw 'Unresolved compiler environment variable in response template.' }
$response=Join-Path $env:XL6_BUILD_TMP ($Target+'_dll.rsp')
[IO.File]::WriteAllText($response,$text,(New-Object Text.UTF8Encoding $false))
Push-Location -LiteralPath $root
$code=1
try {
    # Windows PowerShell can turn native stderr into a terminating error before
    # recording the exit status. GCC owns diagnostics; preserve its full output.
    $ErrorActionPreference='Continue'
    # A launch error does not set LASTEXITCODE. Clear any inherited value and
    # keep failure unless a real native process supplied its exit status.
    $LASTEXITCODE=$null
    & $env:XL6_RELEASE_CC ('@'+$response)
    if ($null -ne $LASTEXITCODE) { $code=[int]$LASTEXITCODE }
} finally { Pop-Location }
exit $code
