param(
    [string]$Root = '',
    [string]$RuntimeDirectory = '',
    [ValidateSet('lz4','bz2','lzma','miniz','snappy','xxhash','zstd','gzip','dotenv')]
    [string[]]$Package = @('lz4','bz2','lzma','miniz','snappy','xxhash','zstd','gzip','dotenv'),
    [switch]$SkipAot
)

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$Package = @($Package | Select-Object -Unique)
$work = Join-Path $Root ('build/test/native_globals_' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $utf8
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$environment = @{XLANG_PACKAGE_PATH = Join-Path $Root 'release/extlibs'}
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}

function InputPaths {
    foreach ($name in $Package) {
        Get-ChildItem (Join-Path $Root "release/extlibs/$name") -Recurse -File | ForEach-Object FullName
    }
    # BZ2 的固定私有算法依赖也是本次编译的输入，不只记录薄绑定。
    if ($Package -contains 'bz2') {
        Get-ChildItem (Join-Path $Root 'release/extlibs/_bzip2') -Recurse -File | ForEach-Object FullName
    }
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'tools/check_extlib_native_globals.ps1','tools/check_extlib_native_globals.sh',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $Root $path }
    foreach ($name in @('xl.exe','xl.dll','xlc.dll')) { Join-Path $RuntimeDirectory $name }
    Get-ChildItem (Join-Path $RuntimeDirectory 'include') -Recurse -File | ForEach-Object FullName
}

function Run([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds) {
    $result = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$result})
    Write-Host "[native-globals] ${Name}: $($result.status) exit=$($result.exitCode)"
    if ($result.stdout) { Write-Host $result.stdout.TrimEnd() }
    if ($result.stderr) { Write-Host $result.stderr.TrimEnd() }
    return $result
}

$outcome = 'failed'
$failure = ''
$changed = @()
try {
    foreach ($path in @(InputPaths | Sort-Object -Unique)) {
        $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash
    }
    $failed = $false
    foreach ($name in $Package) {
        $source = "release/extlibs/$name/test/top_level.xl"
        $expected = "$name top-level: PASS"
        $jit = Run "$name-jit" $xl @('run',$source) 90
        if ($jit.status -ne 'pass' -or $jit.stdout.Replace("`r`n","`n").TrimEnd("`n") -cne $expected) {
            $failed = $true
        }
        if ($SkipAot) { continue }
        $binary = Join-Path $work "$name.exe"
        $build = Run "$name-build" $xl @('build',$source,'-o',$binary) 180
        if ($build.status -ne 'pass') { $failed = $true; continue }
        $aot = Run "$name-aot" $binary @() 90
        if ($aot.status -ne 'pass' -or $aot.stdout.Replace("`r`n","`n").TrimEnd("`n") -cne $expected) {
            $failed = $true
        }
    }
    if ($failed) { throw 'Native global lifetime checks failed; see individual results' }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    # 失败也检查输入，并保留全部 EXE、原始诊断和真实退出码。
    foreach ($path in $hashes.Keys) {
        if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            $changed += $path
        }
    }
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        scope='Native-backed class global creation, validity and natural retirement; not algorithm recertification';
        aotSkipped=[bool]$SkipAot;packages=$Package;runtimeDirectory=$RuntimeDirectory;
        environment=$environment;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[native-globals] report: $work"
    if ($changed.Count) { throw 'Inputs changed during native global lifetime checks' }
}
