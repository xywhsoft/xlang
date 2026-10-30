param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = [IO.Path]::GetFullPath((Join-Path $testRoot ('imagehash_guard_' + [guid]::NewGuid().ToString('N'))))
$inputs = @('release/extlibs/imagehash', 'release/extlibs/stb_image',
    'release/extlibs/stb_image_resize2', 'release/include') | ForEach-Object {
    Get-ChildItem (Join-Path $root $_) -Recurse -File | ForEach-Object FullName
}
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'tools/run_extlib_c.ps1', 'tools/extlib_smoke_process.ps1',
    'tools/xlir_native_process.ps1', 'test/cases/extlib_imagehash.xl',
    'release/examples/extlibs/imagehash.xl', 'release/examples/extlibs/assets/tiny.png',
    'release/examples/games/assets/player.png') | ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
foreach ($path in @($gcc,$python,$shell)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expected = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    if ($program.EndsWith('.ps1')) {
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;
        directory=$root;expectedExit=$expected;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expected) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[imagehash] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $null = Run 'strict GCC compile' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_ONCE',
        '-DXRT_MODULE_THREAD_KEY', '-Ilib/xrt', '-Irelease/extlibs/imagehash',
        '-Irelease/extlibs/imagehash/include', '-fsyntax-only',
        'release/extlibs/imagehash/imagehash_native.c')

    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
        '-Irelease/extlibs/imagehash/include')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/imagehash/test/runtime.c', '-o', $runtime))
    $objects = @($runtime)
    foreach ($name in @('imagehash', 'stb_image', 'stb_image_resize2')) {
        $object = Join-Path $work ($name + '.o')
        $null = Run "$name GCC object" $gcc ($flags + @('-c',
            "release/extlibs/$name/${name}_native.c", '-o', $object))
        $objects += $object
    }
    $links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
    $cold = Join-Path $work 'cold_state.exe'
    $coldObjects = @($objects | Where-Object { [IO.Path]::GetFileName($_) -ne 'imagehash.o' })
    $null = Run 'GCC cold state build' $gcc ($flags + @(
        'release/extlibs/imagehash/test/test_cold_state.c') + $coldObjects + $links + @('-o',$cold))
    foreach ($kind in 0..5) {
        $complete = $false
        foreach ($budget in 0..63) {
            $output = Run "C cold $kind/$budget" $cold @("$kind","$budget")
            if ($output -match 'injected=0 PASS$') { $complete = $true; break }
        }
        if (!$complete) { throw "C cold $kind did not complete" }
        $null = Run "C cold JIT $kind" $xl @('run',
            'release/extlibs/imagehash/test/test_cold_state.c','--',"$kind",'0')
    }
    $vector = Join-Path $work 'hash_vectors.exe'
    $null = Run 'GCC vector build' $gcc ($flags + @(
        'release/extlibs/imagehash/test/hash_vectors.c') + $objects + $links + @('-o',$vector))
    $vectorJson = Join-Path $work 'vectors.jsonl'
    foreach ($engine in @('GCC','JIT','AOT')) {
        if ($SkipAot -and $engine -eq 'AOT') { continue }
        $path = Join-Path $work ($engine + '.pgm')
        if ($engine -eq 'GCC') { $output = Run 'GCC image vectors' $vector @($path) }
        elseif ($engine -eq 'JIT') { $output = Run 'TCC JIT image vectors' $xl @('run',
            'release/extlibs/imagehash/test/hash_vectors.c','--',$path) }
        else {
            $target = Join-Path $work 'vectors_aot.exe'
            $null = Run 'TCC AOT vectors build' $xl @('build',
                'release/extlibs/imagehash/test/hash_vectors.c','-o',$target)
            $output = Run 'TCC AOT image vectors' $target @($path)
        }
        [IO.File]::WriteAllText($vectorJson,$output + "`n",[Text.UTF8Encoding]::new($false))
        $null = Run "$engine independent hash definitions" $python @('-I',
            'release/extlibs/imagehash/test/check_hash_vectors.py',$vectorJson)
    }
    $coldScript = 'release/extlibs/imagehash/test/cold_start.xl'
    $coldAot = Join-Path $work 'cold_start.exe'
    if (!$SkipAot) { $null = Run 'XLang cold AOT build' $xl @('build',$coldScript,'-o',$coldAot) }
    foreach ($engine in @('JIT','AOT')) {
        if ($SkipAot -and $engine -eq 'AOT') { continue }
        foreach ($kind in 0..2) {
            $complete = $false
            foreach ($budget in 0..63) {
                if ($engine -eq 'JIT') { $output = Run "XLang cold $engine $kind/$budget" $xl @('run',$coldScript,'--',"$kind","$budget") }
                else { $output = Run "XLang cold $engine $kind/$budget" $coldAot @("$kind","$budget") }
                if ($output -match 'injected=\s*false\s+PASS$') { $complete = $true; break }
            }
            if (!$complete) { throw "XLang cold $engine $kind did not complete" }
        }
    }
    $binary = Join-Path $work 'test_imagehash.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/imagehash/test/test_imagehash.c') + $objects +
        @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $binary))
    $null = Run 'GCC C boundaries' $binary @()
    $budgetExe = Join-Path $work 'test_owned_text.exe'
    $null = Run 'GCC C result budgets build' $gcc ($flags + @(
        'release/extlibs/imagehash/test/test_owned_text.c') + $objects +
        @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $budgetExe))
    $null = Run 'GCC C result budgets' $budgetExe @()
    $null = Run 'C result budgets JIT' $xl @('run', 'release/extlibs/imagehash/test/test_owned_text.c')
    foreach ($name in @('test_imagehash', 'test_error_threads')) {
        $null = Run $name $runner @('imagehash', "release/extlibs/imagehash/test/$name.c")
    }
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_imagehash.xl')
    $null = Run 'XLang allocations' $xl @('run', 'release/extlibs/imagehash/test/allocations.xl')
    $null = Run 'XLang result budgets' $xl @('run',
        'release/extlibs/imagehash/test/owned_results.xl', '--', '15')

    $generated = Join-Path $work 'owned_probe.c'
    $null = Run 'whole-call ownership cgen' $xl @('cgen',
        'release/extlibs/imagehash/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    $prototypes = [regex]::Matches($generatedText,'(?m)^extern [^\r\n;]*\bxihash_\w+\([^;\r\n]*\);')
    if ($prototypes.Count -ne 8) { throw 'imagehash cgen must declare all eight C functions' }
    $abi = Join-Path $work 'generated_abi.c'
    $abiText = '#include "imagehash_native.h"' + "`n" + (($prototypes | ForEach-Object Value) -join "`n")
    [IO.File]::WriteAllText($abi,$abiText,[Text.UTF8Encoding]::new($false))
    $null = Run 'eight actual generated prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    [IO.File]::WriteAllText($abi,($abiText -replace 'char\s*\*\s*xihash_ahash\(','bool xihash_ahash('),[Text.UTF8Encoding]::new($false))
    $null = Run 'wrong owned return rejected' $gcc ($flags + @('-fsyntax-only',$abi)) 1
    foreach ($name in @('xihash_ahash', 'xihash_dhash', 'xihash_phash')) {
        if ([regex]::Matches($generatedText, "\b\w+\.Data\s*=\s*$name\s*\(").Count -ne 1) {
            throw "$name must adopt the native C string exactly once in the probe"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'imagehash copied adopted result text in the complete public return chain'
    }

    $expected = (Get-Content -LiteralPath 'release/extlibs/imagehash/examples/imagehash.expected.txt' -Raw).
        Replace("`r`n", "`n").TrimEnd("`n")
    $cOutput = Run 'C example' $runner @('imagehash', 'release/extlibs/imagehash/examples/imagehash.c')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/imagehash.xl')
    $directSource = 'release/extlibs/imagehash/examples/imagehash.c'
    $directOutput = Run 'C example direct JIT' $xl @('run', $directSource)
    if ($cOutput -cne $expected -or $xlOutput -cne $expected -or $directOutput -cne $expected) {
        throw 'C/XLang examples differ from the fixed imagehash result'
    }
    $directExe = Join-Path $work 'imagehash_c.exe'
    $null = Run 'C example direct AOT build' $xl @('build', $directSource, '-o', $directExe)
    $builtOutput = Run 'C example direct AOT' $directExe @()
    if ($builtOutput -cne $expected) { throw 'C AOT imagehash result differs from fixed output' }

    $similar = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/imagehash/examples/similarity.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $same = "pHash distance: 0`nwithin threshold 4: true"
    foreach ($language in @('C','XLang')) {
        $source = if ($language -eq 'C') {'release/extlibs/imagehash/examples/similarity_c.c'} else {'release/extlibs/imagehash/examples/similarity.xl'}
        foreach ($engine in @('GCC','JIT','AOT')) {
            if (($engine -eq 'GCC' -and $language -ne 'C') -or ($SkipAot -and $engine -eq 'AOT')) { continue }
            $target = Join-Path $work ("similarity_${language}_${engine}.exe")
            if ($engine -eq 'GCC') { $null = Run 'GCC similarity build' $gcc ($flags + @($source) + $objects + $links + @('-o',$target)) }
            elseif ($engine -eq 'AOT') { $null = Run "$language similarity AOT build" $xl @('build',$source,'-o',$target) }
            foreach ($self in @($false,$true)) {
                $argsForExample = if ($self) {@('release/examples/extlibs/assets/tiny.png','release/examples/extlibs/assets/tiny.png')} else {@()}
                if ($engine -eq 'JIT') { $output = Run "$language similarity $engine self=$self" $xl (@('run',$source,'--') + $argsForExample) }
                else { $output = Run "$language similarity $engine self=$self" $target $argsForExample }
                if ($output -cne $(if ($self) {$same} else {$similar})) { throw 'Similarity example differs from golden output' }
            }
            $missingArgs = @((Join-Path $work 'missing-image.png'),'release/examples/extlibs/assets/tiny.png')
            if ($engine -eq 'JIT') { $output = Run "$language similarity $engine missing first image" $xl (@('run',$source,'--') + $missingArgs) 1 }
            else { $output = Run "$language similarity $engine missing first image" $target $missingArgs 1 }
            if ($output -notmatch 'cannot read or decode image') { throw 'Similarity example lost the first image error' }
        }
    }

    if (-not $SkipAot) {
        $budgetAot = Join-Path $work 'owned_c.exe'
        $null = Run 'C result budgets AOT build' $xl @('build',
            'release/extlibs/imagehash/test/test_owned_text.c', '-o', $budgetAot)
        $null = Run 'C result budgets AOT' $budgetAot @()
        foreach ($entry in @(
            @{Name='core'; Source='release/extlibs/imagehash/test/aot_core.xl'; Args=@()},
            @{Name='central'; Source='test/cases/extlib_imagehash.xl'; Args=@()},
            @{Name='allocations'; Source='release/extlibs/imagehash/test/allocations.xl'; Args=@()},
            @{Name='owned'; Source='release/extlibs/imagehash/test/owned_results.xl'; Args=@('15')}
        )) {
            $exe = Join-Path $work ($entry.Name + '.exe')
            $null = Run "$($entry.Name) AOT build" $xl @('build', $entry.Source, '-o', $exe)
            $null = Run "$($entry.Name) AOT" $exe $entry.Args
        }
        $exampleExe = Join-Path $work 'imagehash_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/imagehash.xl', '-o', $exampleExe)
        $aotOutput = Run 'XLang example AOT' $exampleExe @()
        if ($aotOutput -cne $expected) { throw 'XLang AOT imagehash result differs from fixed output' }
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "imagehash validation input changed: $path"
        }
    }
    $passed = $true
    if ($SkipAot) { Write-Output 'imagehash Windows focused gate: PASS' }
    else { Write-Output 'imagehash Windows complete AOT gate: PASS' }
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    if (-not $work.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe imagehash cleanup path: $work"
    }
    if (Test-Path -LiteralPath $work) {
        [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
            Set-Content ($work + '.report.json') -Encoding UTF8
        if ($passed) { Remove-Item -LiteralPath $work -Recurse -Force }
        else { Write-Warning "imagehash failure artifacts retained: $work" }
    }
}
