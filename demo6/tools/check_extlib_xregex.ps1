param([string]$Root = '', [string]$RuntimeDirectory = '', [string]$WorkDirectory = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$work = if ($WorkDirectory) { [IO.Path]::GetFullPath($WorkDirectory) } else { Join-Path $Root 'build/test/xregex_guard' }
[IO.Directory]::CreateDirectory($work) | Out-Null
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$pkg = 'release/extlibs/xregex'
$manifest = Get-Content (Join-Path $Root "$pkg/xlang.package.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "$pkg/$_" })
$coreSources = @($sources | Where-Object { $_ -ne "$pkg/xregex_native.c" })
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
# The language artifact selects resident regex roots and suppresses the local
# engine through the SDK marker. The standalone test deliberately links a base
# runtime without regex: compile the package engine, not feature-only stubs.
$standaloneModules = @($manifest.artifact.xrtModules | Where-Object { $_ -notmatch '^XRT_MODULE_REGEX(?:_|$)' })
$packageFlags = $flags + @($standaloneModules | ForEach-Object { "-D$_" })
$checks = [Collections.Generic.List[object]]::new()
$inputs = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$paths = @($xl,(Join-Path $RuntimeDirectory 'xl.dll'),(Join-Path $RuntimeDirectory 'xlc.dll'),$gcc,$PSCommandPath)
$paths += @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_xregex_runtime.c',
    'test/cases/extlib_xregex.xl','release/examples/extlibs/xregex.xl',
    'release/examples/extlibs/xregex_basic.xl','release/examples/extlibs/xregex_basic.expected.txt',
    'tools/check_extlib_xregex.sh','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'tools/run_extlib_c.ps1') | ForEach-Object { Join-Path $Root $_ }
$paths += Get-ChildItem (Join-Path $Root $pkg) -Recurse -File | ForEach-Object FullName
$runtimeInclude = Join-Path $RuntimeDirectory 'include'
if (Test-Path -LiteralPath $runtimeInclude) {
    $paths += Get-ChildItem -LiteralPath $runtimeInclude -Recurse -File | ForEach-Object FullName
}
foreach ($path in @($paths | Sort-Object -Unique)) { $inputs[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Check([string]$Name, [string]$Program, [string[]]$Arguments) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root 120 @{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$r})
    Write-Host "[xregex] ${Name}: $($r.status)"
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$outcome = 'failed'
$decls = @()
try {
    $runtime = Join-Path $work 'runtime.o'
    $null = Check 'runtime-build' $gcc ($flags + @('-c','test/support/extlib_xregex_runtime.c','-o',$runtime))
    $objects = @()
    foreach ($source in $coreSources) {
        $object = Join-Path $work "native-$($objects.Count).o"
        $null = Check "native-$($objects.Count)" $gcc ($packageFlags + @('-c',$source,'-o',$object))
        $objects += $object
    }
    $links = $objects + @($runtime,'-lws2_32','-ladvapi32','-lm')
    $binary = Join-Path $work 'native-test.exe'
    $null = Check 'native-test-build' $gcc ($packageFlags + @("$pkg/test/test_xregex_native.c") + $links + @('-o',$binary))
    $null = Check 'native-ownership-oom' $binary @()

    $cases = @('test/cases/extlib_xregex.xl',"$pkg/test/xregex_test.xl", "$pkg/test/xregex_lifetime_test.xl",
        'release/examples/extlibs/xregex.xl','release/examples/extlibs/xregex_basic.xl')
    foreach ($case in $cases) {
        $name = [IO.Path]::GetFileNameWithoutExtension($case)
        $text = Check "$name-jit" $xl @('run',$case)
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
        $aot = Check "$name-aot" $binary @()
        if ($aot -cne $text) { throw "$name JIT/AOT output differs" }
        if ($name -eq 'xregex_basic') { $xlOutput = $text }
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @([regex]::Matches([IO.File]::ReadAllText($module),
            '(?m)^extern[^\r\n]*\bxregex_\w+\([^\r\n]*;') | ForEach-Object Value)
    }

    # 以真实生成声明和 C 实现共同编译，捕获 LLP64/LP64、bool/int、const 等 ABI 漂移。
    # Inventory by ABI symbol, rejecting conflicting prototypes before sorting.
    # Repeated declarations across generated modules count as one public symbol.
    $prototypes = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    foreach ($declaration in $decls) {
        $name = [regex]::Match($declaration, '\bxregex_\w+(?=\()').Value
        $prototype = $declaration.Trim()
        if (!$name) { throw "Invalid generated xregex declaration: $prototype" }
        if ($prototypes.ContainsKey($name) -and $prototypes[$name] -cne $prototype) {
            throw "Conflicting generated ABI for ${name}: $($prototypes[$name]) / $prototype"
        }
        $prototypes[$name] = $prototype
    }
    $decls = @($prototypes.Values | Sort-Object)
    $binding = Get-Content (Join-Path $Root "$pkg/src/xlang/xregex.xl") -Raw -Encoding UTF8
    $expected = @([regex]::Matches($binding,'extern "(xregex_\w+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxregex_\w+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) {
        throw "Generated ABI coverage incomplete: expected $($expected.Count), got $($decls.Count)"
    }
    [IO.File]::WriteAllText((Join-Path $work 'declarations.inc'),($decls -join "`n")+"`n",$utf8)
    $nativePath = (Join-Path $Root "$pkg/xregex_native.c").Replace('\','/')
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"$nativePath`"`n#include `"declarations.inc`"`n",$utf8)
    $null = Check 'generated-prototypes' $gcc ($packageFlags + @('-fsyntax-only',$abi))

    $example = "$pkg/examples/basic.c"
    $binary = Join-Path $work 'c-example.exe'
    $null = Check 'c-example-build' $gcc ($packageFlags + @($example) + $links + @('-o',$binary))
    $cOutput = Check 'c-example' $binary @()
    $options = @('--inc-path','lib/xrt','--inc-path',"$pkg/include")
    $jit = Check 'c-example-jit' $xl (@('run',$example) + $sources + $options)
    $binary = Join-Path $work 'c-example-tcc.exe'
    $null = Check 'c-example-aot-build' $xl (@('build',$example) + $sources + $options + @('-o',$binary))
    $aot = Check 'c-example-aot' $binary @()
    $helper = Check 'c-example-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1',
        '-Package','xregex','-Source',$example,'-RuntimeDirectory',$RuntimeDirectory)
    $golden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/xregex_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($text in @($cOutput,$jit,$aot,$helper,$xlOutput)) {
        if ($text -cne $golden) { throw 'C/xlang example output differs from common golden' }
    }
    $outcome = 'passed'
} finally {
    $changed = @($inputs.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $inputs[$_] })
    $changed += @(Get-ChildItem (Join-Path $Root $pkg) -Recurse -File |
        Where-Object { !$inputs.Contains($_.FullName) } | ForEach-Object FullName)
    if (Test-Path -LiteralPath $runtimeInclude) {
        $changed += @(Get-ChildItem -LiteralPath $runtimeInclude -Recurse -File |
            Where-Object { !$inputs.Contains($_.FullName) } | ForEach-Object FullName)
    }
    $changed = @($changed | Sort-Object -Unique)
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=Get-Date -Format o;outcome=$outcome;runtimeDirectory=$RuntimeDirectory;
        scope='Windows native ownership/OOM, xlang and C JIT/AOT, all generated binding prototypes, paired examples';
        generatedPrototypes=$decls.Count;inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 7),$utf8)
}
if ($outcome -ne 'passed') { throw "xregex gate failed; see $work/results.json" }
