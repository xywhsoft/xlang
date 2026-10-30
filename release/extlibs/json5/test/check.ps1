param([switch]$SkipAot, [string]$ReferenceDir = '')

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('json5_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/json5')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$sources = @('release/extlibs/json5/json5_native.c',
    'release/extlibs/json5/vendor/pdjson5.c')
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/cases/extlib_json5.xl', 'release/examples/extlibs/json5.xl',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1') |
    ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/json5') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes[$gcc] = (Get-FileHash $gcc).Hash
$shell = (Get-Process -Id $PID).Path
$hashes[$shell] = (Get-FileHash $shell).Hash

function Run([string]$name, [string]$program, [string[]]$arguments) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $work 'report.json') -Encoding UTF8
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "[$name] $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -ne 'pass' -or $result.exitCode -ne 0) {
        throw "[$name] $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[json5] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    if (!(Test-Path -LiteralPath $cc1 -PathType Leaf)) { throw "Missing GCC cc1: $cc1" }
    $hashes[(Resolve-Path $cc1).Path] = (Get-FileHash $cc1).Hash
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/json5/test/xrt_runtime.c', '-o', $runtime))
    $objects = @($runtime)
    for ($index = 0; $index -lt $sources.Count; ++$index) {
        $object = Join-Path $work "native_$index.o"
        $null = Run "C native $index compile" $gcc ($flags + @(
            '-c', $sources[$index], '-o', $object))
        $objects += $object
    }
    foreach ($name in @('boundaries', 'numbers', 'depth')) {
        $target = Join-Path $work "test_$name.exe"
        $null = Run "C $name compile" $gcc ($flags + @(
            "release/extlibs/json5/test/test_$name.c") + $objects +
            $links + @('-o', $target))
        $null = Run "C $name" $target @()
    }
    # 该用例直接包含解析器和桥接，只链接同一份 XRT 实现。
    $unicode = Join-Path $work 'unicode.exe'
    $null = Run 'C Unicode compile' $gcc ($flags + @('release/extlibs/json5/test/test_unicode_escapes.c',
        $runtime) + $links + @('-o',$unicode))
    $null = Run 'C Unicode' $unicode @()
    $null = Run 'TCC Unicode' $xl @('run','release/extlibs/json5/test/test_unicode_escapes.c')
    $null = Run 'XLang Unicode' $xl @('run','release/extlibs/json5/test/unicode_escapes.xl')
    $unicodeGolden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/json5/examples/unicode_config.expected.txt'),
        [Text.UTF8Encoding]::new($false)).Replace("`r`n","`n").TrimEnd("`n")
    $unicodeExample = Join-Path $work 'unicode-example.exe'
    $null = Run 'C Unicode example compile' $gcc ($flags + @(
        'release/extlibs/json5/examples/unicode_config_c.c',$runtime) + $links + @('-o',$unicodeExample))
    if ((Run 'C Unicode example' $unicodeExample @()) -cne $unicodeGolden) { throw 'C Unicode golden differs' }
    foreach ($source in @('unicode_config.xl','unicode_config_c.c')) {
        if ((Run "JIT $source" $xl @('run',"release/extlibs/json5/examples/$source")) -cne $unicodeGolden) {
            throw "Unicode golden differs: $source"
        }
    }
    if (-not $SkipAot) {
        foreach ($source in @('test/test_unicode_escapes.c','test/unicode_escapes.xl',
            'examples/unicode_config.xl','examples/unicode_config_c.c')) {
            $target = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            $null = Run "AOT $source build" $xl @('build',("release/extlibs/json5/" + $source),'-o',$target)
            $output = Run "AOT $source" $target @()
            if ($source.StartsWith('examples/') -and $output -cne $unicodeGolden) { throw "Unicode AOT golden differs: $source" }
        }
    }
    if ($ReferenceDir -ne '') {
        $reference = (Resolve-Path -LiteralPath $ReferenceDir -ErrorAction Stop).Path
        $node = (Get-Command node -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
        $hashes[$node] = (Get-FileHash $node).Hash
        foreach ($file in @(Get-ChildItem -LiteralPath $reference -File -Recurse)) {
            $hashes[$file.FullName] = (Get-FileHash $file.FullName).Hash
        }
        $probe = Join-Path $work 'reference_probe.exe'
        $null = Run 'C reference probe compile' $gcc ($flags + @(
            'release/extlibs/json5/test/reference_probe.c') + $objects +
            $links + @('-o', $probe))
        $null = Run 'JSON5 reference' $node @(
            'release/extlibs/json5/test/reference_probe.mjs', $probe, $reference)
    }
    $example = Join-Path $work 'json5_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/json5/examples/json5_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/json5/examples/json5_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }
    $generated = Join-Path $work 'json5_owned.c'
    $null = Run 'XLang direct string cgen' $xl @('cgen',
        'release/extlibs/json5/test/boundaries.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    if ($generatedText -notmatch '\.Data\s*=\s*xjson5_get_text_len\s*\(' -or
        $generatedText.Contains('xjson5_get_text_abi') -or
        $generatedText.Contains('xl6_xrt_take_cstr(')) {
        throw 'json5.get must adopt the original owned C string directly'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/json5/test/boundaries.xl')
        $null = Run 'XLang allocations' $xl @('run', 'extlibs/json5/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/json5/examples/json5.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang JSON5 examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[json5] C/XLang parity: PASS'
        $core = Join-Path $work 'json5_xl_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'extlibs/json5/test/aot_core.xl', '-o', $core)
        $null = Run 'XLang core AOT' $core @()
        $publicOutput = Run 'Public XLang example' $xl @('run', 'examples/extlibs/json5.xl')
        if (-not $SkipAot) {
            $testAot = Join-Path $work 'json5_xl_boundaries.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/json5/test/boundaries.xl', '-o', $testAot)
            $null = Run 'XLang boundaries AOT' $testAot @()
            $allocAot = Join-Path $work 'json5_xl_allocations.exe'
            $null = Run 'XLang allocations build' $xl @('build',
                'extlibs/json5/test/allocations.xl', '-o', $allocAot)
            $null = Run 'XLang allocations AOT' $allocAot @()
            $exampleAot = Join-Path $work 'json5_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/json5/examples/json5.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT JSON5 example differs from C'
            }
            $publicAot = Join-Path $work 'json5_public_example.exe'
            $null = Run 'Public XLang example build' $xl @('build',
                'examples/extlibs/json5.xl', '-o', $publicAot)
            $publicAotOutput = Run 'Public XLang example AOT' $publicAot @()
            if ($publicAotOutput -cne $publicOutput) {
                throw 'public JSON5 example AOT differs from JIT'
            }
            $cExampleAot = Join-Path $work 'json5_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/json5/examples/json5_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'C JSON5 example AOT differs from GCC'
            }
        }
        foreach ($path in $hashes.Keys) {
            if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
                throw "json5 input changed during gate: $path"
            }
        }
        # 正确的 var 数字消费必须单独签收，不以包内转换绕过主线错误。
        $null = Run 'XLang numeric consumer' $xl @('run', 'extlibs/json5/test/numeric_consumer.xl')
        if (-not $SkipAot) {
            $consumerAot = Join-Path $work 'json5_numeric_consumer.exe'
            $null = Run 'XLang numeric consumer build' $xl @('build',
                'extlibs/json5/test/numeric_consumer.xl', '-o', $consumerAot)
            $null = Run 'XLang numeric consumer AOT' $consumerAot @()
        }
    } finally {
        Pop-Location
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "json5 input changed during gate: $path"
        }
    }
    $passed = $true
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        if ($passed) {
            Get-ChildItem -LiteralPath $safeWork -File |
                ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
            Remove-Item -LiteralPath $safeWork
        } else {
            Write-Warning "json5 failure artifacts: $safeWork"
        }
    }
}
