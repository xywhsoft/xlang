param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('stb_write_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = @{}
$passed = $false
$failure = ''
$inputs = @('release/xl.exe','release/xl.dll','release/xlc.dll',
    'lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h',
    'release/include/xruntime_decl.h','release/include/xl_extlib_abi.h',
    'test/cases/extlib_stb_image_write.xl','release/examples/extlibs/stb_image_write.xl',
    'release/examples/extlibs/stb_image_write_c.c','release/examples/extlibs/assets/tiny.png',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1') |
    ForEach-Object { Join-Path $root $_ }
foreach ($package in @('stb_image_write','stb_image')) {
    $inputs += @(Get-ChildItem (Join-Path $root "release/extlibs/$package") -File -Recurse | ForEach-Object FullName)
}
$inputs += @($gcc,$shell)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    if ($null -eq $arguments) { $arguments = @() }
    if ([IO.Path]::GetExtension($program) -ieq '.ps1') {
        $arguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120
    $checks.Add([ordered]@{label=$label;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $work 'report.json') -Encoding UTF8
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "[$label] $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -ne 'pass' -or $result.exitCode -ne 0) {
        throw "[$label] $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[stb write] $label : PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash $path).Hash }
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    if (!(Test-Path -LiteralPath $cc1 -PathType Leaf)) { throw "Missing GCC cc1: $cc1" }
    $hashes[(Resolve-Path $cc1).Path] = (Get-FileHash $cc1).Hash
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $null = Run 'strict C syntax' $gcc @('-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only',
        '-Ilib/xrt','-Irelease/extlibs/stb_image_write/include','-Irelease/extlibs/stb_image/include',
        'release/extlibs/stb_image_write/stb_image_write_native.c',
        'release/extlibs/stb_image_write/test/test_stb_image_write.c',
        'release/examples/extlibs/stb_image_write_c.c')
    $flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-DXRT_IMPLEMENTATION','-Ilib/xrt')
    $links = @('-lws2_32','-lbcrypt','-lshlwapi','-liphlpapi')
    $budget = Join-Path $work 'bmp_budget.exe'
    $null = Run 'GCC BMP budgets build' $gcc ($flags + @(
        'release/extlibs/stb_image_write/test/test_bmp_budget.c','-o',$budget) + $links)
    $null = Run 'GCC BMP budgets' $budget @()
    $layout = Join-Path $work 'bmp_layout.exe'
    $null = Run 'GCC BMP example build' $gcc ($flags + @(
        'release/extlibs/stb_image_write/examples/bmp_layout_c.c','-o',$layout) + $links)
    $golden = @('gray BMP: 58 bytes, pixels at 54','RGB BMP: 70 bytes, pixels at 54',
        'RGBA BMP: 126 bytes, pixels at 122','source unchanged: true','stb write BMP layout: OK') -join "`n"
    if ((Run 'GCC BMP example' $layout @()) -cne $golden) { throw 'GCC BMP example differs from golden' }
    $null = Run 'C boundaries' $runner @('-Package','stb_image_write,stb_image',
        '-Source','release/extlibs/stb_image_write/test/test_stb_image_write.c')
    foreach ($source in @(
        'test/cases/extlib_stb_image_write.xl',
        'release/extlibs/stb_image_write/test/boundaries.xl',
        'release/extlibs/stb_image_write/test/allocations.xl',
        'release/extlibs/stb_image_write/test/output_limit.xl',
        'release/extlibs/stb_image_write/test/bmp_headers.xl',
        'release/extlibs/stb_image_write/test/test_bmp_budget.c'
    )) {
        $null = Run "JIT $source" $xl @('run',$source)
    }
    foreach ($source in @('bmp_layout.xl','bmp_layout_c.c')) {
        if ((Run "JIT $source" $xl @('run',"release/extlibs/stb_image_write/examples/$source")) -cne $golden) {
            throw "BMP example differs from golden: $source"
        }
    }
    $cDir = Join-Path $work 'c'
    $xlDir = Join-Path $work 'xl'
    New-Item -ItemType Directory -Path $cDir, $xlDir | Out-Null
    $direct = Run 'direct C image example' $xl @('run','release/examples/extlibs/stb_image_write_c.c','--',$cDir)
    $runnerOutput = Run 'C runner image example' $runner @('-Package','stb_image_write,stb_image',
        '-Source','release/examples/extlibs/stb_image_write_c.c')
    $xlangOutput = Run 'XLang image example' $xl @('run','release/examples/extlibs/stb_image_write.xl','--',$xlDir)
    $expected = 'stb_image_write C/XL example: PASS'
    if (($xlangOutput -join "`n") -cne $expected -or
        ($direct -join "`n") -cne $expected -or
        ($runnerOutput -join "`n") -cne $expected) {
        throw 'stb_image_write C/XLang example output differs'
    }
    foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
        $cFile = Join-Path $cDir $name
        $xlFile = Join-Path $xlDir $name
        if (-not (Test-Path -LiteralPath $cFile) -or -not (Test-Path -LiteralPath $xlFile)) {
            throw "stb_image_write missing example output: $name"
        }
        if ((Get-FileHash -LiteralPath $cFile -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $xlFile -Algorithm SHA256).Hash) {
            throw "stb_image_write C/XLang image differs: $name"
        }
    }

    if (-not $SkipAot) {
        $core = Join-Path $work 'stbiw_core.exe'
        $example = Join-Path $work 'stbiw_example.exe'
        $cExample = Join-Path $work 'stbiw_c_example.exe'
        $null = Run 'core AOT build' $xl @('build','test/cases/extlib_stb_image_write.xl','-o',$core)
        $null = Run 'core AOT' $core @()
        $null = Run 'XL image AOT build' $xl @('build','release/examples/extlibs/stb_image_write.xl','-o',$example)
        $aotDir = Join-Path $work 'aot'
        New-Item -ItemType Directory -Path $aotDir | Out-Null
        $asset = Join-Path $root 'release/examples/extlibs/assets/tiny.png'
        $aotOutput = Run 'XL image AOT' $example @($aotDir,$asset)
        if (($aotOutput -join "`n") -cne $expected) {
            throw 'stb_image_write example AOT output differs'
        }
        foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
            $aotFile = Join-Path $aotDir $name
            $xlFile = Join-Path $xlDir $name
            if (-not (Test-Path -LiteralPath $aotFile) -or
                (Get-FileHash -LiteralPath $aotFile -Algorithm SHA256).Hash -cne
                (Get-FileHash -LiteralPath $xlFile -Algorithm SHA256).Hash) {
                throw "stb_image_write AOT image differs: $name"
            }
        }
        $null = Run 'C image AOT build' $xl @('build','release/examples/extlibs/stb_image_write_c.c','-o',$cExample)
        $cAotDir = Join-Path $work 'c_aot'
        New-Item -ItemType Directory -Path $cAotDir | Out-Null
        $cAotOutput = Run 'C image AOT' $cExample @($cAotDir,$asset)
        if ($cAotOutput -cne $expected) {
            throw 'stb_image_write C example AOT failed'
        }
        foreach ($name in @('tiny.png', 'tiny.jpg', 'tiny.bmp', 'tiny.tga', 'tiny.hdr')) {
            if ((Get-FileHash -LiteralPath (Join-Path $cAotDir $name) -Algorithm SHA256).Hash -cne
                (Get-FileHash -LiteralPath (Join-Path $xlDir $name) -Algorithm SHA256).Hash) {
                throw "stb_image_write C AOT image differs: $name"
            }
        }
        $cases = @(
            @{name='headers';source='test/bmp_headers.xl';golden=$null},
            @{name='budgets';source='test/test_bmp_budget.c';golden=$null},
            @{name='limit';source='test/output_limit.xl';golden=$null},
            @{name='layout_xl';source='examples/bmp_layout.xl';golden=$golden},
            @{name='layout_c';source='examples/bmp_layout_c.c';golden=$golden}
        )
        foreach ($case in $cases) {
            $target = Join-Path $work ($case.name + '.exe')
            $null = Run "AOT $($case.name) build" $xl @('build',("release/extlibs/stb_image_write/" + $case.source),'-o',$target)
            $output = Run "AOT $($case.name)" $target @()
            if ($null -ne $case.golden -and $output -cne $case.golden) { throw "AOT golden differs: $($case.name)" }
        }
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash $path).Hash -ne $hashes[$path]) { throw "Input changed during gate: $path" }
    }
    $passed = $true
    Write-Output 'stb_image_write Windows C/XLang gate: PASS'
} catch {
    $failure = $_.Exception.Message
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$failure,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or
        ![IO.Path]::GetFileName($target).StartsWith('stb_write_guard_')) { throw "Unsafe cleanup path: $target" }
    [ordered]@{passed=$passed;failure=$failure;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 10 | Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) { Remove-Item -LiteralPath $target -Recurse -Force }
    else { Write-Warning "stb_image_write failure artifacts: $target" }
}
