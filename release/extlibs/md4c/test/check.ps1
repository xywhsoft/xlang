param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test'
$work = Join-Path $workParent ('md4c_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/include', '-Irelease/extlibs/md4c')
$native = @('release/extlibs/md4c/md4c_native.c',
    'release/extlibs/md4c/third_party/md4c/md4c.c',
    'release/extlibs/md4c/third_party/md4c/md4c-html.c',
    'release/extlibs/md4c/third_party/md4c/entity.c')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$outcome = 'failed'
$failure = ''

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/md4c') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/examples/extlibs/md4c.xl','test/cases/extlib_md4c.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[md4c] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_FILE_WHOLE', '-DXRT_MODULE_UNICODE',
        '-fsyntax-only', $native[0]))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/md4c/test/xrt_runtime.c', '-o', $runtime))
    $test = Join-Path $work 'md4c_test.exe'
    $null = Run 'C test compile' $gcc ($flags + $native +
        @('release/extlibs/md4c/test/test_md4c.c', $runtime) + $links + @('-o', $test))
    $null = Run 'C test' $test @()
    foreach ($name in @('parser_allocations_c','render_options_c')) {
        $source = "release/extlibs/md4c/test/$name.c"
        $target = Join-Path $work "$name.exe"
        $null = Run "$name compile" $gcc ($flags + @('-DNDEBUG',$source,$runtime) + $links + @('-o',$target))
        $null = Run $name $target @()
        $null = Run "$name direct xl run" $xl @('run',$source)
    }

    $example = Join-Path $work 'md4c_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/md4c/examples/md4c_c.c', $runtime) + $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run','release/extlibs/md4c/examples/md4c_c.c')
    if ($directCOutput -cne $cOutput) { throw 'direct C example output differs' }

    $articleSource = 'release/extlibs/md4c/examples/article_c.c'
    $articleExe = Join-Path $work 'article-c.exe'
    $null = Run 'C article compile' $gcc ($flags + @($articleSource,$runtime) + $links + @('-o',$articleExe))
    $articleC = Run 'C article' $articleExe @()
    $articleDirect = Run 'C article direct xl run' $xl @('run',$articleSource)
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/md4c/examples/article.expected.html')).Replace("`r`n","`n").TrimEnd("`n")
    if ($articleC -cne $golden -or $articleDirect -cne $golden) { throw 'C article golden output differs' }

    foreach ($name in @('boundaries','allocations','render_options')) {
        $null = Run "XLang $name" $xl @('run',"release/extlibs/md4c/test/$name.xl")
    }
    $null = Run 'central XLang test' $xl @('run','test/cases/extlib_md4c.xl')
    $xlOutput = Run 'XLang example' $xl @('run','release/extlibs/md4c/examples/md4c.xl')
    if ($cOutput -cne $xlOutput) { throw 'C/XLang md4c example outputs differ' }
    $articleXL = Run 'XLang article' $xl @('run','release/extlibs/md4c/examples/article.xl')
    if ($articleXL -cne $golden) { throw 'XLang article golden output differs' }
    $publicOutput = Run 'public XLang example' $xl @('run','release/examples/extlibs/md4c.xl')

    # 编译器实际生成的十二个 C 原型须与公开头文件完全相容。
    $generated = Join-Path $work 'module.c'
    $null = Run 'generated ABI source' $xl @('cgen','release/extlibs/md4c/test/render_options.xl','--emit',$generated)
    $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxmd4c_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/md4c/src/md4c.xl')),'extern "(xmd4c_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxmd4c_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 12 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete md4c generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"md4c_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated twelve prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($symbol in @('xmd4c_html','xmd4c_html_flags','xmd4c_file_html')) {
        if ($generatedText -notmatch ('\.Data\s*=\s*' + $symbol + '\s*\(')) { throw "$symbol must directly adopt the owned buffer" }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or $generatedText.Contains('xl6CStringCopyV1')) { throw 'md4c results must not add a transfer wrapper or copy' }

    if (-not $SkipAot) {
        foreach ($source in @('test/boundaries.xl','test/allocations.xl','test/render_options.xl',
            'test/parser_allocations_c.c','test/render_options_c.c',
            'examples/md4c.xl','examples/md4c_c.c','examples/article.xl','examples/article_c.c')) {
            $target = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.exe')
            $null = Run "$source build" $xl @('build',"release/extlibs/md4c/$source",'-o',$target)
            $output = Run "$source AOT" $target @()
            if ($source -like 'examples/md4c*' -and $output -cne $cOutput) { throw 'md4c AOT example output differs' }
            if ($source -like 'examples/article*' -and $output -cne $golden) { throw 'article AOT golden output differs' }
        }
        $publicAot = Join-Path $work 'public-example.exe'
        $null = Run 'public XLang example build' $xl @('build','release/examples/extlibs/md4c.xl','-o',$publicAot)
        $output = Run 'public XLang example AOT' $publicAot @()
        if ($output -cne $publicOutput) { throw 'public AOT output differs' }
        $centralAot = Join-Path $work 'central.exe'
        $null = Run 'central XLang test build' $xl @('build','test/cases/extlib_md4c.xl','-o',$centralAot)
        $null = Run 'central XLang test AOT' $centralAot @()
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
    $report = $work + '.json'
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[md4c] report: $report"
    $base = [IO.Path]::GetFullPath($workParent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'md4c work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Inputs changed during md4c gate' }
}
