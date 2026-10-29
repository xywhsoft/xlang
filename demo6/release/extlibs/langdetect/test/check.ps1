param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test/langdetect_guard'
$work = Join-Path $workRoot ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$compiler = if ($env:CC) { $env:CC } else { 'gcc' }
$gccCommand = Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($gccCommand) { $gccCommand.Source } else { $compiler }
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/langdetect','-Irelease/extlibs/langdetect/include')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
$corpus = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'corpus.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
$queue = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../examples/review_queue.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/langdetect') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($item in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'test/cases/extlib_langdetect.xl','test/cases/extlib_langdetect_boundaries.xl',
        'release/examples/extlibs/langdetect.xl','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $item }
    if ($gccCommand) { $gcc }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[langdetect] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/langdetect/include/langdetect_native.h'))
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_UNICODE','-fsyntax-only','release/extlibs/langdetect/langdetect_native.c'))
    $null = Run 'C ABI contract' $gcc ($flags + @('-fsyntax-only','release/extlibs/langdetect/test/abi_contract.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/langdetect/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/langdetect/langdetect_native.c','-o',$native))
    foreach ($name in @('test_langdetect','scoring_reference_c','corpus_c')) {
        $source = "release/extlibs/langdetect/test/$name.c"
        $objects = @($runtime)
        if ($name -ne 'scoring_reference_c') { $objects += $native }
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @('-DNDEBUG',$source) + $objects + $links + @('-o',$binary))
        $output = Run $name $binary @()
        if ($name -eq 'corpus_c' -and $output -cne $corpus) { throw 'Public API corpus changed' }
    }
    foreach ($source in @('scoring_reference_c','corpus_c')) {
        $output = Run "$source direct C" $xl @('run',"release/extlibs/langdetect/test/$source.c")
        if ($source -eq 'corpus_c' -and $output -cne $corpus) { throw 'TCC corpus differs' }
    }
    $sources = @('test/cases/extlib_langdetect.xl','test/cases/extlib_langdetect_boundaries.xl',
        'release/extlibs/langdetect/test/analysis_boundaries.xl','release/extlibs/langdetect/test/score_boundaries.xl',
        'release/extlibs/langdetect/test/borrowed_probe.xl','release/extlibs/langdetect/test/aot_core.xl')
    foreach ($source in $sources) { $null = Run "$source JIT" $xl @('run',$source) }
    $probe = Join-Path $work 'probe.c'
    $null = Run 'complete borrowed return cgen' $xl @('cgen','release/extlibs/langdetect/test/borrowed_probe.xl','--emit',$probe)
    $text = [IO.File]::ReadAllText($probe)
    if ($text -match '=\s*xlir_string_copy\s*\(') { throw 'Export wrapper copies the owned result again' }
    foreach ($name in @('detect','code','script','last_error')) {
        $match = [regex]::Matches($text,'const char\* (borrowed_raw_\d+) = xlangdetect_'+$name+'\(')
        if ($match.Count -ne 1 -or [regex]::Matches($text,'xlir_string_from_cstr\('+$match[0].Groups[1].Value+'\)').Count -ne 1) {
            throw "Static borrowed result not copied exactly once: $name"
        }
    }
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxlangdetect_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $expected = @('xlangdetect_code','xlangdetect_confidence','xlangdetect_detect','xlangdetect_last_error','xlangdetect_script','xlangdetect_valid_utf8')
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxlangdetect_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 6 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete generated C ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/langdetect_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'six actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $golden = ''
    foreach ($pair in @(@('langdetect','release/examples/extlibs/langdetect.xl'),@('review_queue_c','release/extlibs/langdetect/examples/review_queue.xl'))) {
        $source = "release/extlibs/langdetect/examples/$($pair[0]).c"
        $binary = Join-Path $work ($pair[0]+'.exe')
        $null = Run "$($pair[0]) C compile" $gcc ($flags + @('-DNDEBUG',$source,$native,$runtime) + $links + @('-o',$binary))
        $c = Run "$($pair[0]) C" $binary @()
        $direct = Run "$($pair[0]) direct C" $xl @('run',$source)
        $language = Run "$($pair[0]) XLang" $xl @('run',$pair[1])
        if ($c -cne $direct -or $c -cne $language) { throw 'C/XLang example output differs' }
        if ($pair[0] -eq 'review_queue_c') { if ($c -cne $queue) { throw 'Review queue golden output differs' } }
        else { $golden = $c }
    }
    if (-not $SkipAot) {
        foreach ($source in ($sources + @('release/extlibs/langdetect/test/corpus_c.c',
            'release/extlibs/langdetect/test/scoring_reference_c.c','release/extlibs/langdetect/examples/langdetect.c',
            'release/examples/extlibs/langdetect.xl','release/extlibs/langdetect/examples/review_queue.xl',
            'release/extlibs/langdetect/examples/review_queue_c.c'))) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -match 'review_queue' -and $output -cne $queue) { throw 'Review queue AOT golden differs' }
            if ($source -in @('release/extlibs/langdetect/examples/langdetect.c','release/examples/extlibs/langdetect.xl') -and $output -cne $golden) {
                throw 'Example AOT differs from C'
            }
            if ($source -match '/corpus_c.c$' -and $output -cne $corpus) { throw 'AOT corpus changed' }
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
    Write-Host "[langdetect] report: $work.json"
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Langdetect work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Langdetect inputs changed during gate' }
}
