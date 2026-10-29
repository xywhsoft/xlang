param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$work = Join-Path $Root ('build/test/extlib_audit_' + [guid]::NewGuid().ToString('N'))
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Fixture([string]$Relative, [string]$Content) {
    $path = Join-Path $work $Relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $path)) | Out-Null
    [IO.File]::WriteAllText($path, $Content, $utf8)
}
function Require([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function New-Package([string]$Name, [string[]]$Dependencies = @()) {
    $manifest = [ordered]@{
        format='xlang-package-1'; name=$Name; version='1.0.0'
        artifact=@{ kind='source'; cSources=@('native.c'); xlSources=@('src/public.xl') }
        dependencies=@($Dependencies | ForEach-Object { @{ name=$_ } })
    }
    Write-Fixture "release/extlibs/$Name/xlang.package.json" ($manifest | ConvertTo-Json -Depth 6)
    Write-Fixture "release/extlibs/$Name/native.c" '#include <stdbool.h>
/* TODO documentation; unsupported input is a legitimate negative path. */
bool valid(int count) { if (count < 0) return false; return true; }
'
    Write-Fixture "release/extlibs/$Name/src/public.xl" "export func publicApi() as int`n    return 1`nend`n"
    # 未声明的旧副本与测试中的导出不能污染正式 API 和实现计数。
    Write-Fixture "release/extlibs/$Name/test/helper.xl" "export func testOnly()`nend`n"
    Write-Fixture "release/extlibs/$Name/unused.c" "extern void xrtRetired(void);`n"
}

foreach ($name in @('guard','registered','broken','root_dep','middle_dep','bad_json','wrong_name','onnxruntime','mysql','gpio','_zlib','abi_hint')) {
    New-Package $name
}
New-Package 'middle_dep' @('not_present')
New-Package 'root_dep' @('middle_dep')
Write-Fixture 'release/extlibs/broken/xlang.package.json' '{"format":"xlang-package-1","name":"broken","artifact":{"kind":"source","cSources":["absent.c"]}}'
Write-Fixture 'release/extlibs/bad_json/xlang.package.json' '{broken'
Write-Fixture 'release/extlibs/wrong_name/xlang.package.json' '{"format":"xlang-package-1","name":"other","artifact":{"kind":"source","cSources":["native.c"]}}'
Write-Fixture 'release/extlibs/abi_hint/native.c' 'extern void xrtExample(void);'
Write-Fixture 'test/featured_extlibs.json' '{"packages":[{"name":"registered"}]}'
Write-Fixture 'test/phase1_extlibs.json' '{"packages":[{"name":"registered"},{"name":"broken"},{"name":"onnxruntime"}]}'
Write-Fixture 'test/platform_extlibs.json' '{"packages":[]}'
[IO.Directory]::CreateDirectory((Join-Path $work 'docs')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $work 'test/cases')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $work 'release/examples/extlibs')) | Out-Null
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'audit_extlib_ecosystem.ps1') -Root $work
if ($LASTEXITCODE -ne 0) { throw 'Fixture audit failed' }
$report = [IO.File]::ReadAllText((Join-Path $work 'test/extlib_ecosystem.json')) | ConvertFrom-Json
$packages = @{}
foreach ($item in $report.packages) { $packages[$item.name] = $item }
Require ($report.format -eq 'xlang-extlib-ecosystem-2') 'Schema must describe evidence-free inventory'
Require ($packages.guard.status -eq 'source-review') 'Defensive returns or one C file cannot prove placeholder status'
Require ($packages.guard.cSources -eq 1 -and $packages.guard.xlangSources -eq 1) 'Only manifest sources count'
Require (@($packages.guard.api).Count -eq 1 -and $packages.guard.api[0] -eq 'func publicApi') 'Test helpers polluted export inventory'
Require (!$packages.guard.redeclaredXrtAbi) 'Unreferenced C source polluted ABI review'
Require ($packages.registered.status -eq 'registered' -and @($packages.registered.gates).Count -eq 2) 'Registration lost or promoted to verification'
Require ($packages.registered.verification -eq 'not-evaluated-by-inventory') 'Inventory must not fabricate execution evidence'
foreach ($name in @('broken','bad_json','wrong_name')) {
    Require ($packages[$name].status -eq 'invalid-package' -and !$packages[$name].manifestValid) "Invalid manifest accepted: $name"
}
Require ($packages.middle_dep.status -eq 'blocked-dependency' -and $packages.root_dep.status -eq 'blocked-dependency') 'Missing dependency did not propagate'
Require ($packages.onnxruntime.budgetScope -eq 'not-applicable-existing-implementation') 'Existing implementation charged against placeholder budget'
Require ($packages.mysql.status -eq 'budget-review' -and $packages.gpio.status -eq 'environment-review') 'Historical names were treated as completed deferral decisions'
Require ($packages._zlib.visibility -eq 'internal') 'Internal visibility lost'
Require ($packages.abi_hint.status -eq 'needs-abi-review') 'Textual ABI hint was not explicitly separated from a proven failure'
Require (@($report.packages | Where-Object { $_.status -match 'verified|^featured-rc$|^placeholder$' }).Count -eq 0) 'Unjustified readiness label generated'
Write-Host 'Extension inventory contract: PASS (12 fixture packages)'
