param(
    [string]$Root = "",
    [string]$StartAt = "",
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 30,
    [string[]]$Package = @(),
    [switch]$FocusedOnly
)

$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
if (!(Test-Path -LiteralPath $xl -PathType Leaf)) { throw "xl.exe missing: $xl" }
$utf8 = [Text.UTF8Encoding]::new($false)
$cases = @{}
$skipped = [Collections.Generic.List[object]]::new()
$platformNames = @()
$packageRoot = Join-Path $Root 'release/extlibs'
$platform = [IO.File]::ReadAllText((Join-Path $Root 'test/platform_extlibs.json')) | ConvertFrom-Json
$platformNames = @($platform.packages.name)
foreach ($name in $Package) {
    if (!(Test-Path -LiteralPath (Join-Path $packageRoot "$name/xlang.package.json"))) { throw "Unknown package: $name" }
}

function Add-SmokeCase([string]$Relative, [string]$Owner, [string]$Origin) {
    if ($Package.Count -ne 0 -and $Owner -notin $Package) { return }
    $Relative = $Relative.Replace('\', '/')
    if ($Package.Count -eq 0 -and $Owner -in $platformNames) {
        $skipped.Add([pscustomobject]@{ case=$Relative; reason='requires explicit platform selection' })
        return
    }
    if (!$cases.ContainsKey($Relative)) {
        $cases[$Relative] = [pscustomobject]@{
            case=$Relative; packages=[Collections.Generic.List[string]]::new()
            origins=[Collections.Generic.List[string]]::new()
        }
    }
    if ($Owner -ne '' -and !$cases[$Relative].packages.Contains($Owner)) { $cases[$Relative].packages.Add($Owner) }
    if (!$cases[$Relative].origins.Contains($Origin)) { $cases[$Relative].origins.Add($Origin) }
}

# 默认使用已登记的无设备测试与包内 coverage.tests。平台套件需明确选择，不能盲跑 GUI/设备。
foreach ($suite in @('featured', 'phase1', 'platform')) {
    $file = "test/${suite}_extlibs.json"
    $registry = [IO.File]::ReadAllText((Join-Path $Root $file)) | ConvertFrom-Json
    foreach ($item in @($registry.packages)) {
        if (![string]::IsNullOrWhiteSpace([string]$item.case)) {
            Add-SmokeCase "test/cases/$($item.case)" $item.name $file
        }
    }
    foreach ($shared in @($registry.sharedCases)) {
        if ($shared) { Add-SmokeCase "test/cases/$shared" '' "$file sharedCases" }
    }
}
if (!$FocusedOnly) {
    foreach ($directory in Get-ChildItem -LiteralPath $packageRoot -Directory | Sort-Object Name) {
        $path = Join-Path $directory.FullName 'xlang.package.json'
        if (!(Test-Path -LiteralPath $path)) { continue }
        $manifest = [IO.File]::ReadAllText($path) | ConvertFrom-Json
        foreach ($case in @($manifest.coverage.tests)) {
            if ($case) { Add-SmokeCase "release/extlibs/$($directory.Name)/$case" $directory.Name 'package coverage.tests' }
        }
    }
}
$selected = @($cases.Values | Sort-Object case)
if ($StartAt -ne '') {
    $matches = @($selected | Where-Object { $_.case -eq $StartAt -or [IO.Path]::GetFileName($_.case) -eq $StartAt })
    if ($matches.Count -ne 1) { throw 'StartAt must identify exactly one registered case' }
    $index = [array]::IndexOf($selected, $matches[0])
    $selected = @($selected | Select-Object -Skip $index)
}
if ($selected.Count -eq 0) { throw 'No registered cases match the selection' }

# 捕获源码、依赖包与宿主产物；来源改变时，单项输出保留但整轮证据不再有效。
function Get-SmokeInputPaths {
    foreach ($file in Get-ChildItem -LiteralPath $packageRoot -Recurse -File) {
        if ($file.Extension -in @('.c', '.h', '.xl', '.json')) { $file.FullName }
    }
    foreach ($case in $selected) {
        $path = Join-Path $Root $case.case
        if (Test-Path -LiteralPath $path -PathType Leaf) { $path }
    }
    foreach ($relative in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'release/include/xrt_decl.h','release/include/xruntime_decl.h','release/include/xl_extlib_abi.h',
        'test/featured_extlibs.json','test/phase1_extlibs.json','test/platform_extlibs.json',
        'tools/smoke_extlib_catalog.ps1','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {
        Join-Path $Root $relative
    }
}
$hashes = [ordered]@{}
foreach ($path in @(Get-SmokeInputPaths | Sort-Object -Unique)) {
    $relative = (Get-XlirContainedRelativePath $Root $path).Replace('\', '/')
    $hashes[$relative] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}
$results = [Collections.Generic.List[object]]::new()
$oldPackagePath = [Environment]::GetEnvironmentVariable('XLANG_PACKAGE_PATH', 'Process')
try {
    [Environment]::SetEnvironmentVariable('XLANG_PACKAGE_PATH', (Join-Path $Root 'release/extlibs'), 'Process')
    foreach ($case in $selected) {
        Write-Host "[catalog] $($case.case)"
        $path = Join-Path $Root $case.case
        $result = if (Test-Path -LiteralPath $path -PathType Leaf) {
            Invoke-ExtlibSmokeProcess $xl @('run', $path) $Root $TimeoutSeconds
        } else {
            [pscustomobject]@{ status='missing'; exitCode=$null; stdout=''; stderr='Registered test file missing'; elapsedMs=0 }
        }
        $results.Add([pscustomobject]@{
            case=$case.case; packages=@($case.packages.ToArray()); origins=@($case.origins.ToArray())
            status=$result.status; exitCode=$result.exitCode; stdout=$result.stdout; stderr=$result.stderr; elapsedMs=$result.elapsedMs
        })
    }
} finally {
    [Environment]::SetEnvironmentVariable('XLANG_PACKAGE_PATH', $oldPackagePath, 'Process')
}
$changed = @($hashes.Keys | Where-Object {
    $path = Join-Path $Root $_
    !(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hashes[$_]
})
# 新增文件同样改变本轮输入，不能只检查开始时已存在的文件。
$changed += @(Get-SmokeInputPaths | ForEach-Object {
    (Get-XlirContainedRelativePath $Root $_).Replace('\', '/')
} | Where-Object { !$hashes.Contains($_) })
$changed = @($changed | Sort-Object -Unique)
$unselected = @(Get-ChildItem -LiteralPath (Join-Path $Root 'test/cases') -File -Filter 'extlib_*.xl' |
    ForEach-Object { 'test/cases/' + $_.Name } | Where-Object { $_ -notin @($selected.case) })
$report = [ordered]@{
    format='xlang-extlib-catalog-smoke-2'; generatedAt=(Get-Date).ToUniversalTime().ToString('o')
    scope='Registered focused and package-declared JIT cases only; not C/AOT, examples, complete API coverage or release admission'
    timeoutSeconds=$TimeoutSeconds; selection=$Package; focusedOnly=[bool]$FocusedOnly
    evidenceValid=($changed.Count -eq 0); changedInputs=$changed; inputs=$hashes
    skipped=@($skipped.ToArray()); unselectedFocusedFiles=$unselected; results=@($results.ToArray())
}
$reportSuffix = if ($Package.Count -ne 0 -or $StartAt -ne '') { '.targeted' } else { '' }
$json = Join-Path $Root "test/extlib_smoke$reportSuffix.generated.json"
[IO.File]::WriteAllText($json, ($report | ConvertTo-Json -Depth 8), $utf8)
$lines = [Collections.Generic.List[string]]::new()
$lines.Add('# Extension Library Catalog Smoke')
$lines.Add('')
$lines.Add($report.scope)
$lines.Add("Evidence valid for recorded inputs: $($report.evidenceValid). A nonzero result is a reproduction requiring diagnosis, not automatically a package bug.")
$lines.Add('')
$lines.Add('| Status | Count |')
$lines.Add('| --- | ---: |')
foreach ($group in @($results | Group-Object status | Sort-Object Name)) { $lines.Add("| $($group.Name) | $($group.Count) |") }
$lines.Add('')
$lines.Add('| Case | Status | Exit | Milliseconds | Diagnostic |')
$lines.Add('| --- | --- | ---: | ---: | --- |')
foreach ($result in $results) {
    $summary = ($result.stderr + ' ' + $result.stdout).Replace("`r", ' ').Replace("`n", ' ')
    if ($summary.Length -gt 240) { $summary = $summary.Substring(0, 240) + '...' }
    $lines.Add("| $($result.case) | $($result.status) | $($result.exitCode) | $($result.elapsedMs) | $($summary.Replace('|', '\|')) |")
}
[IO.File]::WriteAllText((Join-Path $Root "docs/extlib_smoke$reportSuffix.generated.md"),
    ($lines -join "`n") + "`n", $utf8)
$passed = @($results | Where-Object status -eq 'pass').Count
$failed = $results.Count - $passed
Write-Host "Extension catalog smoke: $passed passed, $failed not passed; fixed inputs: $($report.evidenceValid)."
exit $(if ($failed -eq 0 -and $report.evidenceValid) { 0 } else { 1 })
