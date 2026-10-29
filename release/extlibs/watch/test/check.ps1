param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/watch_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
$case = Join-Path $work 'case'
[IO.Directory]::CreateDirectory($work) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $case 'build/test/watch_guard')) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/watch')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$hashes = [ordered]@{}
$failure = ''
$passed = $false

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/watch') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/examples/extlibs/watch.xl','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[watch] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_DIR','-DXRT_MODULE_PATH',
        '-DXRT_MODULE_HASH64','-fsyntax-only','release/extlibs/watch/watch_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/watch/test/xrt_runtime.c', '-o', $runtime))
    $test = Join-Path $work 'watch_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/watch/watch_native.c',
        'release/extlibs/watch/test/test_watch.c', $runtime) +
        $links + @('-o', $test))
    Push-Location $case
    try {
        $null = Run 'C test' $test @()
    } finally {
        Pop-Location
    }
    $digestSource = 'release/extlibs/watch/test/directory_digest_c.c'
    $digest = Join-Path $work 'digest.exe'
    $null = Run 'directory digest compile' $gcc ($flags + @('-DNDEBUG',$digestSource,$runtime) + $links + @('-o',$digest))
    $null = Run 'directory digest' $digest @()
    $null = Run 'directory digest direct xl run' $xl @('run',$digestSource)
    $race = Join-Path $work 'watch_race_test.exe'
    $null = Run 'directory race compile' $gcc ($flags + @(
        'release/extlibs/watch/watch_native.c',
        'release/extlibs/watch/test/test_directory_race.c', $runtime,
        '-Wl,--wrap=xrtDirOpen') + $links + @('-o', $race))
    Push-Location $case
    try {
        $null = Run 'directory race' $race @()
    } finally {
        Pop-Location
    }
    $example = Join-Path $work 'watch_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/watch/examples/watch_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $expected = @(
        'count: 1',
        'exists: true',
        'path matches: true',
        'after remove: 0',
        'slot reused: true',
        'created: true',
        'modified: true',
        'deleted: true'
    ) -join "`n"
    if ($cOutput -cne $expected) {
        throw "C watch example differs from expected output:`n$cOutput"
    }
    $directC = Run 'C example xl run' $xl @('run',
        'release/extlibs/watch/examples/watch_c.c')
    if ($directC -cne $cOutput) {
        throw 'direct C watch example differs from GCC output'
    }
    $directorySource = 'release/extlibs/watch/examples/directory_c.c'
    $directoryExample = Join-Path $work 'directory-c.exe'
    $null = Run 'C directory compile' $gcc ($flags + @($directorySource,$runtime) + $links + @('-o',$directoryExample))
    $directoryC = Run 'C directory example' $directoryExample @()
    $directoryDirect = Run 'C directory direct xl run' $xl @('run',$directorySource)
    $directoryXL = Run 'XLang directory example' $xl @('run','release/extlibs/watch/examples/directory.xl')
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/watch/examples/directory.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($directoryC -cne $golden -or $directoryDirect -cne $golden -or $directoryXL -cne $golden) { throw 'Directory example golden output differs' }

    $boundarySource = Join-Path $root 'release/extlibs/watch/test/boundaries.xl'
    $cwdSource = Join-Path $root 'release/extlibs/watch/test/cwd_snapshot.xl'
    $allocationSource = Join-Path $root 'release/extlibs/watch/test/allocations.xl'
    $ownedSource = Join-Path $root 'release/extlibs/watch/test/owned_events.xl'
    $privateSource = Join-Path $root 'release/extlibs/watch/test/private_native.xl'
    Push-Location $case
    try {
        $null = Run 'XLang boundaries' $xl @('run', $boundarySource)
        $null = Run 'XLang cwd snapshot' $xl @('run', $cwdSource)
        $null = Run 'XLang allocation failures' $xl @('run', $allocationSource)
        $null = Run 'XLang owned events' $xl @('run', $ownedSource)
        $private = Invoke-ExtlibSmokeProcess $xl @('check',$privateSource) (Get-Location).Path 90 $environment
        $checks.Add([ordered]@{name='private Native rejection';result=$private;expectedFailure=$true})
        $privateDiagnostic = $private.stdout + $private.stderr
        if ($private.status -ne 'fail' -or $private.exitCode -ne 1 -or -not $privateDiagnostic.Contains('addNative') -or
            -not $privateDiagnostic.Contains('access')) {
            throw "watch Native member access check failed: $privateDiagnostic"
        }
    } finally {
        Pop-Location
    }
    $generated = Join-Path $work 'owned_event.c'
    $null = Run 'direct-event cgen' $xl @('cgen', $boundarySource,
        '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxwatch_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $expectedNames = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/watch/src/watch.xl')),'extern "(xwatch_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actualNames = @([regex]::Matches(($decls -join "`n"),'\bxwatch_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 8 -or @(Compare-Object $expectedNames $actualNames).Count) { throw 'Incomplete watch generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"watch_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated eight prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $adoptions = [regex]::Matches($generatedText,
        '\b\w+\.Data\s*=\s*xwatch_poll\s*\(').Count
    if ($adoptions -ne 1 -or $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText.Contains('xl6_xrt_take_cstr')) {
        throw 'watch poll must transfer the C event allocation without copying'
    }
    Write-Host '[watch] direct string adoption: PASS'
    if (-not $SkipAot) {
        $target = Join-Path $work 'watch_xl_test.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            $boundarySource, '-o', $target)
        Push-Location $case
        try {
            $null = Run 'XLang boundaries AOT' $target @()
        } finally {
            Pop-Location
        }
        foreach ($entry in @(
            @{ Name = 'cwd snapshot'; Source = $cwdSource },
            @{ Name = 'allocations'; Source = $allocationSource },
            @{ Name = 'owned events'; Source = $ownedSource }
        )) {
            $target = Join-Path $work ('watch_' + $entry.Name.Replace(' ', '_') + '.exe')
            $null = Run "XLang $($entry.Name) build" $xl @('build',
                $entry.Source, '-o', $target)
            Push-Location $case
            try {
                $null = Run "XLang $($entry.Name) AOT" $target @()
            } finally {
                Pop-Location
            }
        }
    }

    Push-Location (Join-Path $root 'release')
    try {
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/watch/examples/watch.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang watch example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[watch] C/XLang parity: PASS'
        $publicOutput = Run 'Public XLang example' $xl @('run', 'examples/extlibs/watch.xl')
        if (-not $SkipAot) {
            $exampleAot = Join-Path $work 'watch_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/watch/examples/watch.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT watch example differs from C'
            }
            $publicAot = Join-Path $work 'watch_public_example.exe'
            $null = Run 'Public XLang example build' $xl @('build',
                'examples/extlibs/watch.xl', '-o', $publicAot)
            $publicAotOutput = Run 'Public XLang example AOT' $publicAot @()
            if ($publicAotOutput -cne $publicOutput) {
                throw 'public watch example AOT differs from JIT'
            }
            $cExampleAot = Join-Path $work 'watch_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/watch/examples/watch_c.c', '-o', $cExampleAot)
            Push-Location $root
            try {
                $cAotOutput = Run 'C example AOT' $cExampleAot @()
            } finally {
                Pop-Location
            }
            if ($cAotOutput -cne $cOutput) {
                throw 'C watch example AOT differs from GCC'
            }
        }
    } finally {
        Pop-Location
    }
    if (-not $SkipAot) {
        foreach ($source in @($digestSource,'release/extlibs/watch/examples/directory.xl',$directorySource)) {
            $target = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$target)
            $output = Run "$source AOT" $target @()
            if ($source -like '*/examples/*' -and $output -cne $golden) { throw 'Directory AOT golden output differs' }
        }
    }
    $globalSource = 'release/extlibs/watch/test/top_level.xl'
    $null = Run 'native global JIT' $xl @('run',$globalSource)
    if (-not $SkipAot) {
        $global = Join-Path $work 'global.exe'
        $null = Run 'native global build' $xl @('build',$globalSource,'-o',$global)
        $null = Run 'native global AOT retirement' $global @()
    }
    $passed = $true
    Write-Host 'watch Windows package gate: PASS'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    $outcome = if ($passed) {'passed'} else {'failed'}
    if ($changed.Count) { $passed = $false; $outcome = 'invalid-input-change' }
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[watch] report: $work.json"
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "watch temporary work path escaped build/test: $workPath"
    }
    if ($passed) {
        Remove-Item -LiteralPath $workPath -Recurse -Force
    } else {
        Write-Warning "watch failure artifacts retained: $workPath"
    }
    if ($changed.Count) { throw 'Inputs changed during watch gate' }
}
