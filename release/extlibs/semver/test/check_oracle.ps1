param(
    [Parameter(Mandatory = $true)][string]$OracleDir,
    [string]$NodeExe = 'node'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$oracle = (Resolve-Path -LiteralPath $OracleDir).Path
$oracleManifest = Join-Path $oracle 'package.json'
if (-not (Test-Path -LiteralPath $oracleManifest -PathType Leaf)) {
    throw "node-semver package.json not found in $oracle"
}
$oraclePackage = Get-Content -LiteralPath $oracleManifest -Raw | ConvertFrom-Json
if ($oraclePackage.name -cne 'semver') {
    throw "expected node-semver package in $oracle"
}
if ($oraclePackage.version -cne '7.8.5') {
    throw "expected node-semver 7.8.5, got $($oraclePackage.version)"
}
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('semver_oracle_' + [guid]::NewGuid().ToString('N'))
$runtime = Join-Path $work 'xrt_runtime.o'
$cli = Join-Path $work 'semver_oracle_cli.exe'

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
    & $gcc -std=c11 -O2 -Ilib/xrt -c `
        release/extlibs/semver/test/xrt_runtime.c -o $runtime
    if ($LASTEXITCODE -ne 0) { throw 'XRT compile failed' }
    & $gcc -std=c11 -O2 -Wall -Wextra -Werror -Ilib/xrt `
        -Irelease/extlibs/semver release/extlibs/semver/semver_native.c `
        release/extlibs/semver/test/semver_oracle_cli.c $runtime `
        -lWs2_32 -lAdvapi32 -lIphlpapi -lm -o $cli
    if ($LASTEXITCODE -ne 0) { throw 'semver oracle CLI compile failed' }
    & $NodeExe release/extlibs/semver/test/compare_node_semver.js $oracle $cli
    if ($LASTEXITCODE -ne 0) { throw 'semver oracle mismatch' }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
