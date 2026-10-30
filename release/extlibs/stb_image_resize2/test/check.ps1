param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('stb_resize2_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$failure = ''
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/cases/extlib_stb_image_resize2.xl', 'release/examples/extlibs/stb_image_resize2.xl',
    'release/examples/extlibs/stb_image_resize2_c.c', 'release/examples/extlibs/assets/tiny.png',
    'tools/extlib_smoke_process.ps1', 'tools/xlir_native_process.ps1', 'tools/run_extlib_c.ps1') |
    ForEach-Object { Join-Path $root $_ }
foreach ($package in @('stb_image_resize2', 'stb_image')) {
    $inputs += @(Get-ChildItem (Join-Path $root "release/extlibs/$package") -File -Recurse |
        ForEach-Object FullName)
}
$inputs += @($gcc, $shell)
$hashes = @{}

function Run([string]$label, [string]$program, [string[]]$arguments, [int]$expectedExit = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    if ([IO.Path]::GetExtension($program) -ieq '.ps1') {
        $arguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120
    $checks.Add([ordered]@{label=$label;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;expectedExit=$expectedExit;result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $work 'report.json') -Encoding UTF8
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "[$label] $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass', 'fail') -or $result.exitCode -ne $expectedExit) {
        throw "[$label] $($result.status) exit=$($result.exitCode), expected=$expectedExit`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[stb resize] $label : PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
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
        '-Ilib/xrt','release/extlibs/stb_image_resize2/stb_image_resize2_native.c',
        'release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c',
        'release/examples/extlibs/stb_image_resize2_c.c')
    $flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-DXRT_IMPLEMENTATION','-Ilib/xrt')
    $links = @('-lws2_32','-lbcrypt','-lshlwapi','-liphlpapi')
    foreach ($name in @('test_stride', 'test_workspace_context')) {
        $target = Join-Path $work ($name + '.exe')
        $null = Run "GCC $name build" $gcc ($flags + @("release/extlibs/stb_image_resize2/test/$name.c",'-o',$target) + $links)
        $null = Run "GCC $name" $target @()
    }
    $separate = Join-Path $work 'stride_separate.exe'
    $null = Run 'GCC separate allocation build' $gcc ($flags + @('-DSTBIR__SEPARATE_ALLOCATIONS',
        'release/extlibs/stb_image_resize2/test/test_stride.c','-o',$separate) + $links)
    $null = Run 'GCC separate allocation' $separate @()
    $paddedExe = Join-Path $work 'padded_gcc.exe'
    $null = Run 'GCC padded example build' $gcc ($flags + @(
        'release/extlibs/stb_image_resize2/examples/padded_rows_c.c','-o',$paddedExe) + $links)
    $paddedGolden = @('source bytes: 5','sRGB midpoint: 188','linear midpoint: 128',
        'short stride rejected','source unchanged: true','stb resize padded rows: OK') -join "`n"
    if ((Run 'GCC padded example' $paddedExe @()) -cne $paddedGolden) { throw 'GCC padded example differs from golden' }

    $null = Run 'C boundaries' $runner @('-Package','stb_image_resize2',
        '-Source','release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c')
    foreach ($source in @('test/cases/extlib_stb_image_resize2.xl',
        'release/extlibs/stb_image_resize2/test/boundaries.xl',
        'release/extlibs/stb_image_resize2/test/allocations.xl',
        'release/extlibs/stb_image_resize2/test/stride.xl',
        'release/extlibs/stb_image_resize2/test/test_stride.c',
        'release/extlibs/stb_image_resize2/test/test_workspace_context.c')) {
        $null = Run "JIT $source" $xl @('run',$source)
    }
    $direct = Run 'direct C image example' $xl @('run','release/examples/extlibs/stb_image_resize2_c.c')
    $runnerOutput = Run 'C runner image example' $runner @('-Package','stb_image_resize2,stb_image',
        '-Source','release/examples/extlibs/stb_image_resize2_c.c')
    $xlangOutput = Run 'XLang image example' $xl @('run','release/examples/extlibs/stb_image_resize2.xl')
    $expected = @('sRGB bytes:  64','linear bytes:  36','gray-alpha bytes:  2',
        'stb_image_resize2 C/XL example: PASS') -join "`n"
    if ($direct -cne $expected -or $runnerOutput -cne $expected -or $xlangOutput -cne $expected) {
        throw 'Original image example differs from golden'
    }
    foreach ($source in @('padded_rows.xl','padded_rows_c.c')) {
        if ((Run "JIT $source" $xl @('run',"release/extlibs/stb_image_resize2/examples/$source")) -cne $paddedGolden) {
            throw "Padded example differs from golden: $source"
        }
    }
    if (-not $SkipAot) {
        # 原图片路径保留；新内存例不替代解码依赖和原有素材路径。
        $asset = Join-Path $root 'release/examples/extlibs/assets/tiny.png'
        $cases = @(
            @{name='core';source='test/cases/extlib_stb_image_resize2.xl';args=@();golden=$null},
            @{name='image_xl';source='release/examples/extlibs/stb_image_resize2.xl';args=@($asset);golden=$expected},
            @{name='image_c';source='release/examples/extlibs/stb_image_resize2_c.c';args=@($asset);golden=$expected},
            @{name='stride_xl';source='release/extlibs/stb_image_resize2/test/stride.xl';args=@();golden=$null},
            @{name='stride_c';source='release/extlibs/stb_image_resize2/test/test_stride.c';args=@();golden=$null},
            @{name='context_c';source='release/extlibs/stb_image_resize2/test/test_workspace_context.c';args=@();golden=$null},
            @{name='padded_xl';source='release/extlibs/stb_image_resize2/examples/padded_rows.xl';args=@();golden=$paddedGolden},
            @{name='padded_c';source='release/extlibs/stb_image_resize2/examples/padded_rows_c.c';args=@();golden=$paddedGolden}
        )
        foreach ($case in $cases) {
            $target = Join-Path $work ($case.name + '.exe')
            $null = Run "AOT $($case.name) build" $xl @('build',$case.source,'-o',$target)
            $output = Run "AOT $($case.name)" $target $case.args
            if ($null -ne $case.golden -and $output -cne $case.golden) { throw "AOT golden differs: $($case.name)" }
        }
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash $path).Hash -ne $hashes[$path]) { throw "Input changed during gate: $path" }
    }
    $passed = $true
} catch {
    $failure = $_.Exception.Message
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$failure,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or
        ![IO.Path]::GetFileName($target).StartsWith('stb_resize2_guard_')) { throw "Unsafe cleanup path: $target" }
    [ordered]@{passed=$passed;failure=$failure;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 10 | Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) { Remove-Item -LiteralPath $target -Recurse -Force }
    else { Write-Warning "Resize failure artifacts: $target" }
}
