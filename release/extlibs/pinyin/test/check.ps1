param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test/pinyin_guard'
$work = Join-Path $workRoot ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$compiler = if ($env:CC) { $env:CC } else { 'gcc' }
$command = Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($command) { $command.Source } else { $compiler }
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/pinyin','-Irelease/extlibs/pinyin/include')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
function Normalize([string]$value) { $value.Replace("`r`n","`n").TrimEnd("`n") }
$golden = Normalize ([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'pinyin.expected.txt')))
$keysGolden = Normalize ([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../examples/search_keys.expected.txt')))

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/pinyin') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'test/cases/extlib_pinyin.xl','release/examples/extlibs/pinyin.xl','release/examples/extlibs/pinyin_c.c',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    if ($command) { $gcc }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[pinyin] ${name}: PASS"
    return Normalize $result.stdout
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/pinyin/include/pinyin_native.h'))
    $manifest = Get-Content -LiteralPath release/extlibs/pinyin/xlang.package.json -Raw | ConvertFrom-Json
    $modules = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest C syntax' $gcc ($flags + $modules + @('-fsyntax-only','release/extlibs/pinyin/pinyin_native.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/pinyin/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/pinyin/pinyin_native.c','-o',$native))
    foreach ($name in @('test_pinyin','readings_c')) {
        $source = "release/extlibs/pinyin/test/$name.c"
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @('-DNDEBUG',$source,$native,$runtime) + $links + @('-o',$binary))
        $output = Run $name $binary @()
        if ($name -eq 'readings_c' -and $output -cne 'pinyin readings: 55328 checks, 0 failures') { throw 'Incomplete reading traversal' }
    }
    $output = Run 'reading oracle direct C' $xl @('run','release/extlibs/pinyin/test/readings_c.c')
    if ($output -cne 'pinyin readings: 55328 checks, 0 failures') { throw 'TCC reading traversal differs' }
    $sources = @('test/cases/extlib_pinyin.xl','release/extlibs/pinyin/test/boundaries.xl',
        'release/extlibs/pinyin/test/allocations.xl','release/extlibs/pinyin/test/owned_probe.xl')
    foreach ($source in $sources) { $null = Run "$source JIT" $xl @('run',$source) }

    $probe = Join-Path $work 'probe.c'
    $null = Run 'complete return cgen' $xl @('cgen','release/extlibs/pinyin/test/owned_probe.xl','--emit',$probe)
    $text = [IO.File]::ReadAllText($probe)
    # 只约束四个返回函数；调用方比较已持有字符串的副本不冒充桥接开销。
    $functions = [regex]::Matches($text,'(?ms)^static xlir_result_string xlir_fn_\d+\([^;\r\n]*\)\r?\n\{.*?^\}')
    if ($functions.Count -ne 4) { throw "Incomplete complete-return probe: $($functions.Count) functions" }
    foreach ($fn in $functions) { if ($fn.Value -match '=\s*xlir_string_copy\s*\(') { throw 'Return wrapper copies text' } }
    foreach ($name in @('convert_n','initials_n')) {
        if ([regex]::Matches($text,'\b\w+\.Data\s*=\s*xpinyin_'+$name+'\s*\(').Count -ne 1) { throw "Result not adopted directly: $name" }
    }
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxpinyin_[a-z_]+\(' } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxpinyin_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 2 -or @(Compare-Object @('xpinyin_convert_n','xpinyin_initials_n') $actual).Count) { throw 'Incomplete generated ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/pinyin_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'two actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    foreach ($pair in @(@('release/examples/extlibs/pinyin_c.c','release/examples/extlibs/pinyin.xl',$golden),
        @('release/extlibs/pinyin/examples/search_keys_c.c','release/extlibs/pinyin/examples/search_keys.xl',$keysGolden))) {
        $binary = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($pair[0])+'.exe')
        $objects = @($runtime)
        # 旧C范例已内含原生源；新范例由GCC独立链接，TCC路径才内含。
        if ($pair[0] -match '/search_keys_c.c$') { $objects += $native }
        $null = Run "$($pair[0]) compile" $gcc ($flags + @('-DNDEBUG',$pair[0]) + $objects + $links + @('-o',$binary))
        $c = Run "$($pair[0]) GCC" $binary @()
        $direct = Run "$($pair[0]) direct" $xl @('run',$pair[0])
        $language = Run "$($pair[1]) JIT" $xl @('run',$pair[1])
        if ($c -cne $pair[2] -or $direct -cne $pair[2] -or $language -cne $pair[2]) { throw 'Bilingual example golden differs' }
    }
    if (-not $SkipAot) {
        foreach ($source in ($sources + @('release/extlibs/pinyin/test/readings_c.c','release/examples/extlibs/pinyin_c.c',
            'release/examples/extlibs/pinyin.xl','release/extlibs/pinyin/examples/search_keys.xl','release/extlibs/pinyin/examples/search_keys_c.c'))) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match 'search_keys' -and $output -cne $keysGolden) { throw 'Search keys AOT golden differs' }
            if ($source -in @('release/examples/extlibs/pinyin.xl','release/examples/extlibs/pinyin_c.c') -and $output -cne $golden) { throw 'Example AOT golden differs' }
            if ($source -match '/readings_c.c$' -and $output -cne 'pinyin readings: 55328 checks, 0 failures') { throw 'AOT reading traversal differs' }
        }
    }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[pinyin] report: $work.json"
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Pinyin work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Pinyin inputs changed during gate' }
}
