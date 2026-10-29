param([string]$Root = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workRoot = Join-Path $Root 'build/test/argon2_bloom_guard'
$runName = 'run_' + [guid]::NewGuid().ToString('N')
$work = Join-Path $workRoot $runName
$reportPath = Join-Path $workRoot "$runName.results.json"
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory($work) | Out-Null
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$sources = @{}

# 固定宿主与库输入；分配故障只注入独立 C 进程，不改变 xl.dll 的内存策略。
$paths = @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
    'release/include/xrt_decl.h','test/support/extlib_scalar_runtime.c',
    'test/cases/extlib_argon2_bloom_alloc.c','tools/check_extlib_argon2_bloom.ps1',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')
foreach ($name in @('argon2','bloom')) {
    $package = "release/extlibs/$name"
    $manifest = Get-Content (Join-Path $Root "$package/xlang.package.json") -Raw -Encoding UTF8 | ConvertFrom-Json
    $sources[$name] = @($manifest.artifact.cSources | ForEach-Object { "$package/$_" })
    $paths += Get-ChildItem (Join-Path $Root $package) -Recurse -File |
        Where-Object Extension -in @('.c','.h','.xl','.json') |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
    $paths += @("test/cases/extlib_$name.xl","release/examples/extlibs/$name.xl",
        "release/examples/extlibs/${name}_basic.xl","release/examples/extlibs/${name}_c.c",
        "release/examples/extlibs/${name}_basic.expected.txt")
}
$paths += 'release/extlibs/bloom/test/nul_keys.xl'
foreach ($path in @($paths | Sort-Object -Unique)) {
    $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash
}

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 90) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[argon2/bloom] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/include')
$runtime = Join-Path $work 'runtime.o'
$links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
$objects = @{}
$outcome = 'failed'
try {
    $null = Check 'runtime' $gcc ($flags + @('-c','test/support/extlib_scalar_runtime.c','-o',$runtime))
    foreach ($name in @('argon2','bloom')) {
        $objects[$name] = @()
        $index = 0
        foreach ($source in $sources[$name]) {
            $object = Join-Path $work "$name-$index.o"
            # 上游参考库保留零下界检查；只对 vendor 的已知常量范围警告做局部处理。
            $vendorFlags = if ($source -match '/vendor/') { @('-Wno-type-limits') } else { @() }
            $null = Check "$name-native-$index" $gcc ($flags + $vendorFlags + @('-c',$source,'-o',$object))
            $objects[$name] += $object
            ++$index
        }
        $test = "release/extlibs/$name/test/test_$name.c"
        $exe = Join-Path $work "$name-c.exe"
        $testFlags = if ($name -eq 'argon2') { @('-DNDEBUG') } else { @() }
        $null = Check "$name-c-build" $gcc ($flags + $testFlags + @($test) + $objects[$name] + $links + @('-o',$exe))
        $null = Check "$name-c" $exe @()
        $null = Check "$name-c-jit" $xl (@('run',$test) + $sources[$name])

        $golden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/${name}_basic.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $example = "release/examples/extlibs/${name}_c.c"
        $text = Check "$name-example-c" $xl @('run',$example)
        if ($text -cne $golden) { throw "$name C example differs from golden output" }
        $binary = Join-Path $work "$name-example-c.exe"
        $null = Check "$name-example-c-build" $xl @('build',$example,'-o',$binary)
        $text = Check "$name-example-c-aot" $binary @()
        if ($text -cne $golden) { throw "$name C AOT example differs from golden output" }
        $exampleExe = Join-Path $work "$name-example-gcc.exe"
        $null = Check "$name-example-gcc-build" $gcc ($flags + @($example) + $links + @('-o',$exampleExe))
        $text = Check "$name-example-gcc" $exampleExe @()
        if ($text -cne $golden) { throw "$name GCC example differs from golden output" }
    }
    $alloc = Join-Path $work 'allocation.exe'
    $null = Check 'allocation-build' $gcc ($flags + @('test/cases/extlib_argon2_bloom_alloc.c') + $objects.argon2 + $objects.bloom + $links + @('-o',$alloc))
    $text = Check 'allocation' $alloc @()
    if ($text -notmatch '^argon2/bloom allocation: rejected=[1-9][0-9]* completed=[1-9][0-9]*$') { throw 'Allocation coverage marker missing' }

    # XLang 声明必须全部生成 C 原型；头文件可额外公开仅供 C 使用的入口。
    foreach ($name in @('argon2','bloom')) {
        $case = "test/cases/extlib_$name.xl"
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        if ($name -eq 'bloom') {
            $generatedText = [IO.File]::ReadAllText($module)
            foreach ($symbol in @('xbloom_serialize_owned', 'xcounting_bloom_serialize_owned')) {
                if ($generatedText -cnotmatch "\bv\d+\.Data\s*=\s*$symbol\s*\(") {
                    throw "$symbol must adopt the native C string directly"
                }
            }
            if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
                $generatedText.Contains('xl6CStringCopyV1')) {
                throw 'bloom serialization must not copy or re-adopt native text'
            }
        }
        $header = if ($name -eq 'argon2') { 'argon2_native.h' } else { 'bloom.h' }
        $prefix = if ($name -eq 'argon2') { 'xargon2_' } else { '(?:xbloom_|xcounting_bloom_)' }
        $decls = @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match "^extern .*\b${prefix}[a-z_]+\(" })
        $text = Get-Content (Join-Path $Root "release/extlibs/$name/include/$header") -Raw -Encoding UTF8
        $symbols = @([regex]::Matches($text,"\b${prefix}[a-z_]+(?=\()") | ForEach-Object Value | Sort-Object -Unique)
        $binding = Get-Content (Join-Path $Root "release/extlibs/$name/src/$name.xl") -Raw -Encoding UTF8
        $declared = @([regex]::Matches($binding,('extern\s+"(?<symbol>' + $prefix + '[a-z_]+)"')) |
            ForEach-Object { $_.Groups['symbol'].Value } | Sort-Object -Unique)
        $emitted = @([regex]::Matches(($decls -join "`n"),"\b${prefix}[a-z_]+(?=\()") | ForEach-Object Value | Sort-Object -Unique)
        $missingHeader = @($declared | Where-Object { $symbols -cnotcontains $_ })
        if ($missingHeader.Count -ne 0 -or @(Compare-Object $declared $emitted).Count -ne 0 -or
            $decls.Count -ne $declared.Count) {
            throw "$name generated ABI coverage incomplete: declared $($declared.Count), emitted $($decls.Count), missing C header: $($missingHeader -join ', ')"
        }
        $abi = Join-Path $work "$name-abi.c"
        [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"$header`"`n" + ($decls -join "`n"),$utf8)
        $null = Check "$name-generated-prototypes" $gcc ($flags + @("-Irelease/extlibs/$name/include",'-fsyntax-only',$abi))
        $null = Check "$name-xlang-jit" $xl @('run',$case)
        $aot = Join-Path $work "$name-xlang.exe"
        $null = Check "$name-xlang-build" $xl @('build',$case,'-o',$aot)
        $null = Check "$name-xlang-aot" $aot @()

        $golden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/${name}_basic.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $example = "release/examples/extlibs/${name}_basic.xl"
        $text = Check "$name-example-xl" $xl @('run',$example)
        if ($text -cne $golden) { throw "$name XLang example differs from golden output" }
        $binary = Join-Path $work "$name-example-xl.exe"
        $null = Check "$name-example-xl-build" $xl @('build',$example,'-o',$binary)
        $text = Check "$name-example-xl-aot" $binary @()
        if ($text -cne $golden) { throw "$name XLang AOT example differs from golden output" }
    }
    foreach ($name in @('argon2','bloom')) {
        $null = Check "$name-tutorial" $xl @('run',"release/examples/extlibs/$name.xl") 180
    }
    $nulKeys = 'release/extlibs/bloom/test/nul_keys.xl'
    $null = Check 'bloom-nul-keys-jit' $xl @('run',$nulKeys)
    $nulBinary = Join-Path $work 'bloom-nul-keys.exe'
    $null = Check 'bloom-nul-keys-build' $xl @('build',$nulKeys,'-o',$nulBinary)
    $null = Check 'bloom-nul-keys-aot' $nulBinary @()
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" }
    }
    $outcome = 'passed'
} finally {
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope='Windows x64 strict C ABI, boundary/allocation, JIT/AOT, paired examples';inputs=$hashes;checks=@($checks.ToArray())}
    try {
        [IO.File]::WriteAllText($reportPath,($report | ConvertTo-Json -Depth 7),$utf8)
        Write-Host "[argon2/bloom] report: $reportPath"
    } finally {
        $parent = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
        $target = [IO.Path]::GetFullPath($work)
        if (!$target.StartsWith($parent, [StringComparison]::OrdinalIgnoreCase)) {
            throw "argon2/bloom temporary work path escaped build/test: $target"
        }
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
