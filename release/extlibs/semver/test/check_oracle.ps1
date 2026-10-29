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
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$node = (Get-Command $NodeExe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$outcome = 'failed'
$failure = ''
$utf8 = [Text.UTF8Encoding]::new($false)
function InputPaths {
    Get-ChildItem -LiteralPath $oracle -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','release/extlibs/semver/semver_native.c',
        'release/extlibs/semver/semver_native.h','release/extlibs/semver/test/xrt_runtime.c',
        'release/extlibs/semver/test/semver_oracle_cli.c','release/extlibs/semver/test/compare_node_semver.js',
        'release/extlibs/semver/test/check_oracle.ps1','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {
        Join-Path $root $path
    }
    $gcc
    $node
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    Write-Host "[semver oracle] ${name}: $($result.status)"
    if ($result.stdout) { Write-Host $result.stdout.TrimEnd() }
    if ($result.stderr) { Write-Host $result.stderr.TrimEnd() }
    if ($result.status -ne 'pass') { throw "$name failed" }
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    Run 'compiler version' $gcc @('--version')
    Run 'node version' $node @('--version')
    Run 'XRT compile' $gcc @('-std=c11','-O2','-Ilib/xrt','-c',
        'release/extlibs/semver/test/xrt_runtime.c','-o',$runtime)
    Run 'oracle CLI compile' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',
        '-Irelease/extlibs/semver','release/extlibs/semver/semver_native.c',
        'release/extlibs/semver/test/semver_oracle_cli.c',$runtime,
        '-lWs2_32','-lAdvapi32','-lIphlpapi','-lm','-o',$cli)
    Run 'independent comparison' $node @('release/extlibs/semver/test/compare_node_semver.js',
        $oracle,$cli,(Join-Path $work 'comparison.json'))
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $comparison = $null
    if (Test-Path -LiteralPath (Join-Path $work 'comparison.json')) {
        $comparison = Get-Content -LiteralPath (Join-Path $work 'comparison.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    $report = Join-Path $testRoot (([IO.Path]::GetFileName($work)) + '.json')
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        oracleDirectory=$oracle;oracleVersion=$oraclePackage.version;inputs=$hashes;changedInputs=$changed;
        comparison=$comparison;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 10),$utf8)
    Write-Host "[semver oracle] report: $report"
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if ($outcome -eq 'passed' -and (Test-Path -LiteralPath $safeWork)) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
    if ($changed.Count) { throw 'Inputs changed during semver oracle gate' }
}
