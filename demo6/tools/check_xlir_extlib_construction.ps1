param([string]$Root = '', [string]$RuntimeDirectory = '', [string]$WorkDirectory = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$work = if ($WorkDirectory) { [IO.Path]::GetFullPath($WorkDirectory) } else { Join-Path $Root 'build/test/extlib_construction_guard' }
[IO.Directory]::CreateDirectory($work) | Out-Null
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$checks = [Collections.Generic.List[object]]::new()
$inputs = [ordered]@{}
$cases = @(
    @{name='xlir_native_field_initializers';native='test/cases/xlir_native_field_initializers_native.c';output='native field initializers ok'},
    @{name='xlir_explicit_empty_collection_kind';native='';output='explicit empty collection kind ok'},
    @{name='xlir_extern_error_borrow';native='test/cases/xlir_extern_error_borrow_native.c';output='extern error borrow ok'}
)
$negative = @('sem_explicit_list_values_invalid','sem_explicit_set_pairs_invalid')
$paths = @($xl,$PSCommandPath) + @('src/ir/xl6_ir_lower.c','src/sem/xl6_sem.c',
    'test/cases/xlir_native_field_initializers_native.c','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'test/cases/xlir_extern_error_borrow_native.c',
    'lib/xrt/xrt.h','lib/xruntime/xruntime.h') | ForEach-Object {
    if ([IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $Root $_ }
}
$paths += @($cases.name + $negative | ForEach-Object { Join-Path $Root "test/cases/$_.xl" })
foreach ($path in $paths) { $inputs[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Check([string]$Name, [string]$Program, [string[]]$Arguments) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root 120
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$r})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[construction] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$outcome = 'failed'
try {
    foreach ($case in $cases) {
        $name = $case.name
        $source = "test/cases/$name.xl"
        $native = if ($case.native) { @($case.native) } else { @() }
        $jit = Check "$name-jit" $xl (@('run',$source) + $native)
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-build" $xl (@('build',$source) + $native + @('-o',$binary))
        $aot = Check "$name-aot" $binary @()
        if ($jit -cne $case.output -or $aot -cne $case.output) { throw "$name did not reach its final assertions" }
        $irPath = Join-Path $work "$name.lowered.json"
        $null = Check "$name-ir" $xl @('ir',$source,'--lowered','--format','json','--emit',$irPath)
        $ir = Get-Content $irPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($name -eq 'xlir_explicit_empty_collection_kind') {
            foreach ($op in @('xvalue-set-new','xvalue-int-map-new','xvalue-array-new','xvalue-object-new')) {
                if (@($ir.instructions | Where-Object runtimeOpName -eq $op).Count -eq 0) { throw "Missing concrete collection lowering: $op" }
            }
        }
        $c1 = Join-Path $work "$name.c"
        $c2 = Join-Path $work "$name.repeat.c"
        $null = Check "$name-cgen" $xl @('cgen',$source,'--emit',$c1)
        $null = Check "$name-cgen-repeat" $xl @('cgen',$source,'--emit',$c2)
        if ((Get-FileHash $c1).Hash -cne (Get-FileHash $c2).Hash) { throw "$name C output is not deterministic" }
    }
    foreach ($name in $negative) {
        $r = Invoke-ExtlibSmokeProcess $xl @('check',"test/cases/$name.xl") $Root 30
        $checks.Add([ordered]@{name=$name;expected='semantic rejection';result=$r})
        if ($r.status -ne 'fail' -or $r.exitCode -ne 1 -or $r.stderr -notmatch '\[sem\]' -or $r.stderr -notmatch '(set|list)') {
            throw "$name was not rejected in semantic checking"
        }
        Write-Host "[construction] ${name}: expected rejection"
    }
    $outcome = 'passed'
} finally {
    $changed = @($inputs.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $inputs[$_] })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=Get-Date -Format o;outcome=$outcome;runtimeDirectory=$RuntimeDirectory;
        scope='Native base/own field initialization, object independence, failed-initializer cleanup, explicit collection kinds; JIT/AOT/IR/C and negatives';
        inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
}
if ($outcome -ne 'passed') { throw "Construction gate failed; see $work/results.json" }
