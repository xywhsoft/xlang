$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/zstd'
[void][IO.Directory]::CreateDirectory($workParent)
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
$binary = Join-Path $work 'interop.exe'
[void][IO.Directory]::CreateDirectory($work)
$passed = $false
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$checks = [Collections.Generic.List[object]]::new()
$hashes = @{}
$environment = @{PYTHONDONTWRITEBYTECODE='1'}
$oracle = Join-Path $workParent 'python_oracle'
if (Test-Path -LiteralPath (Join-Path $oracle 'zstandard')) {
    $environment.PYTHONPATH = $oracle + [IO.Path]::PathSeparator + $env:PYTHONPATH
}
function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $work 'report.json') -Encoding UTF8
    if ($result.status -ne 'pass' -or $result.exitCode -ne 0 -or $result.stderr) {
        throw "[$name] $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    return $result.stdout.Trim()
}

Push-Location $root
try {
    $inputs = @($gcc,$python,(Join-Path $root 'lib/xrt/xrt.h'),(Join-Path $root 'lib/xruntime/xruntime.h'),
        (Join-Path $root 'test/support/extlib_codec_runtime.c'),(Join-Path $root 'tools/extlib_smoke_process.ps1'))
    $inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/zstd') -File -Recurse | ForEach-Object FullName)
    foreach ($path in $inputs) {$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
    $cc1 = Run 'GCC cc1' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1]=(Get-FileHash -LiteralPath $cc1).Hash
    $probe = Run 'oracle version and path' $python @('-c',
        'import json,zstandard; print(json.dumps(dict(version=zstandard.__version__,path=zstandard.__path__[0])))')
    $module = $probe | ConvertFrom-Json
    if ($module.version -cne '0.25.0') {throw 'zstandard 0.25.0 is required'}
    foreach ($file in (Get-ChildItem -LiteralPath $module.path -File -Recurse)) {
        $hashes[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash
    }
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $vendor = Join-Path $work 'vendor.o'
    $null = Run 'vendor build' $gcc @('-std=c11','-O2','-c','release/extlibs/zstd/third_party/zstd.c','-o',$vendor)
    $null = Run 'C interop build' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-Ilib/xrt','-Ilib/xruntime','-Irelease/extlibs/zstd/include',
        'test/support/extlib_codec_runtime.c','release/extlibs/zstd/xrt_zstd.c',
        'release/extlibs/zstd/test/interop.c',$vendor,'-lws2_32','-ladvapi32','-liphlpapi','-lm','-o',$binary)
    Write-Output (Run 'independent frame interop' $python @('release/extlibs/zstd/test/interop.py',$binary))
    $passed = $true
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $changed = @()
    foreach ($path in $hashes.Keys) {
        if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {$changed += $path}
    }
    if ($changed.Count) {$passed = $false}
    [ordered]@{passed=$passed;changed=$changed;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 10 | Set-Content ($work+'.report.json') -Encoding UTF8
    if ($changed.Count) {throw "zstd interop inputs changed: $($changed -join ', ')"}
    $safeParent = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($safeParent, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($safeWork).StartsWith('run_')) {
        throw 'refusing to remove a directory outside the Zstd test root'
    }
    if ($passed) {Remove-Item -LiteralPath $safeWork -Recurse -Force}
    else {Write-Warning "zstd interop failure artifacts: $safeWork"}
}
