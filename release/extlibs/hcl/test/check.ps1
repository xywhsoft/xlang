param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('hcl_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h',
    'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h',
    'test/cases/extlib_hcl.xl', 'release/examples/extlibs/hcl.xl',
    'release/examples/extlibs/hcl_c.c') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/hcl') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    foreach ($source in @(
        'release/extlibs/hcl/hcl_native.c',
        'release/extlibs/hcl/test/test_hcl.c',
        'release/extlibs/hcl/test/test_hcl_threads.c',
        'release/extlibs/hcl/test/test_expression_strings.c',
        'release/examples/extlibs/hcl_c.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/hcl/include $source
        if ($LASTEXITCODE -ne 0) { throw "hcl strict GCC check failed: $source" }
    }

    # 新矩阵直接包含本包实现，独立 GCC 编译时不重复链接原生单元。
    $expressionsC = Join-Path $work 'expressions_gcc.exe'
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -DXRT_IMPLEMENTATION -Ilib/xrt `
        release/extlibs/hcl/test/test_expression_strings.c -o $expressionsC `
        -lws2_32 -lbcrypt -lshlwapi -liphlpapi
    if ($LASTEXITCODE -ne 0) { throw 'hcl expression strict GCC build failed' }
    & $expressionsC
    if ($LASTEXITCODE -ne 0) { throw 'hcl expression strict GCC run failed' }
    $queryExe = Join-Path $work 'query_comments_gcc.exe'
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -DXRT_IMPLEMENTATION -Ilib/xrt `
        release/extlibs/hcl/examples/query_comments_c.c -o $queryExe `
        -lws2_32 -lbcrypt -lshlwapi -liphlpapi
    if ($LASTEXITCODE -ne 0) { throw 'hcl query comments GCC build failed' }
    $queryGolden = @('port: 8080', '[', '  "api.example", // note: "retired',
        '  "admin.example"', ']', 'HCL query comments: OK') -join "`n"
    $queryOutput = & $queryExe
    if ($LASTEXITCODE -ne 0 -or ($queryOutput -join "`n") -cne $queryGolden) {
        throw 'hcl query comments GCC output differs from golden'
    }
    foreach ($source in @('query_comments.xl', 'query_comments_c.c')) {
        $queryOutput = & $xl run "release/extlibs/hcl/examples/$source"
        if ($LASTEXITCODE -ne 0 -or ($queryOutput -join "`n") -cne $queryGolden) {
            throw "hcl $source direct run differs from golden"
        }
    }
    & $xl run release/extlibs/hcl/test/test_expression_strings.c
    if ($LASTEXITCODE -ne 0) { throw 'hcl C expression direct run failed' }

    foreach ($source in @(
        'release/extlibs/hcl/test/test_hcl.c',
        'release/extlibs/hcl/test/test_hcl_threads.c'
    )) {
        & $runner -Package hcl -Source $source
        if ($LASTEXITCODE -ne 0) { throw "hcl C test failed: $source" }
    }
    foreach ($source in @(
        'test/cases/extlib_hcl.xl',
        'release/extlibs/hcl/test/boundaries.xl',
        'release/extlibs/hcl/test/allocations.xl',
        'release/extlibs/hcl/test/expression_strings.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "hcl XLang test failed: $source" }
    }

    $generated = Join-Path $work 'owned_text.c'
    & $xl cgen release/extlibs/hcl/test/boundaries.xl --emit $generated
    if ($LASTEXITCODE -ne 0) { throw 'hcl direct-text cgen failed' }
    $generatedText = [IO.File]::ReadAllText($generated)
    # 使用实际生成原型对公开 C 头检查，而不只比较手写声明的拼写。
    $prototypes = [regex]::Matches($generatedText,
        '(?m)^extern [^\r\n;]*\bxhcl_\w+\([^;\r\n]*\);')
    if ($prototypes.Count -ne 12) { throw 'hcl cgen must declare all 12 C functions' }
    $abi = Join-Path $work 'generated_abi.c'
    $abiText = '#include "hcl_native.h"' + "`n" + (($prototypes | ForEach-Object Value) -join "`n")
    [IO.File]::WriteAllText($abi, $abiText, [Text.UTF8Encoding]::new($false))
    & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Irelease/extlibs/hcl/include $abi
    if ($LASTEXITCODE -ne 0) { throw 'hcl generated C ABI disagrees with public header' }
    foreach ($symbol in @('xhcl_get', 'xhcl_get_raw', 'xhcl_block_label',
            'xhcl_block_label_at')) {
        if ($generatedText -notmatch ('\.Data\s*=\s*' + $symbol + '\s*\(')) {
            throw "hcl $symbol must adopt the owned C result directly"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1')) {
        throw 'hcl text results must not use legacy transfer or copying'
    }
    Write-Output 'hcl direct string adoption: PASS'

    $cOutput = & $runner -Package hcl -Source release/examples/extlibs/hcl_c.c
    if ($LASTEXITCODE -ne 0) { throw 'hcl C example failed' }
    $directCOutput = & $xl run release/examples/extlibs/hcl_c.c
    if ($LASTEXITCODE -ne 0 -or ($cOutput -join "`n") -cne ($directCOutput -join "`n")) {
        throw 'hcl direct C example failed or output differs'
    }
    $xlOutput = & $xl run release/examples/extlibs/hcl.xl
    if ($LASTEXITCODE -ne 0) { throw 'hcl XLang example failed' }
    if (($cOutput -join "`n") -cne ($xlOutput -join "`n")) {
        throw 'hcl C/XLang example output differs'
    }

    if (-not $SkipAot) {
        $core = Join-Path $work 'core.exe'
        & $xl build release/extlibs/hcl/test/aot_core.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'hcl core AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'hcl core AOT run failed' }
        Write-Output 'hcl core AOT: PASS'

        $boundaries = Join-Path $work 'boundaries.exe'
        & $xl build release/extlibs/hcl/test/boundaries.xl -o $boundaries
        if ($LASTEXITCODE -ne 0) { throw 'hcl boundaries AOT build failed' }
        $boundariesOutput = & $boundaries
        if ($LASTEXITCODE -ne 0 -or ($boundariesOutput -join "`n") -cne 'hcl boundaries: ok') {
            throw 'hcl boundaries AOT failed or output differs'
        }
        Write-Output 'hcl boundaries AOT: PASS'

        $allocations = Join-Path $work 'allocations.exe'
        & $xl build release/extlibs/hcl/test/allocations.xl -o $allocations
        if ($LASTEXITCODE -ne 0) { throw 'hcl allocations AOT build failed' }
        $allocationsOutput = & $allocations
        if ($LASTEXITCODE -ne 0 -or ($allocationsOutput -join "`n") -cne 'hcl allocation: PASS') {
            throw 'hcl allocations AOT failed or output differs'
        }
        Write-Output 'hcl allocations AOT: PASS'

        foreach ($source in @('expression_strings.xl', 'test_expression_strings.c')) {
            $expressions = Join-Path $work ($source + '.exe')
            & $xl build "release/extlibs/hcl/test/$source" -o $expressions
            if ($LASTEXITCODE -ne 0) { throw "hcl expressions AOT build failed: $source" }
            $expressionOutput = & $expressions
            if ($LASTEXITCODE -ne 0 -or ($expressionOutput -join "`n") -cne
                'HCL string/expression boundaries: 77 cases PASS') {
                throw "hcl expressions AOT failed or output differs: $source"
            }
        }
        Write-Output 'hcl C/XLang expressions AOT: PASS'

        $example = Join-Path $work 'example.exe'
        & $xl build release/examples/extlibs/hcl.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'hcl example AOT build failed' }
        $aotOutput = & $example
        if ($LASTEXITCODE -ne 0 -or ($aotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'hcl AOT example failed or output differs from C'
        }
        Write-Output 'hcl AOT/C example parity: PASS'
        $cExample = Join-Path $work 'c_example.exe'
        & $xl build release/examples/extlibs/hcl_c.c -o $cExample
        if ($LASTEXITCODE -ne 0) { throw 'hcl C example AOT build failed' }
        $cAotOutput = & $cExample
        if ($LASTEXITCODE -ne 0 -or ($cAotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'hcl C example AOT failed or output differs from GCC'
        }
        Write-Output 'hcl C example AOT parity: PASS'
        foreach ($source in @('query_comments.xl', 'query_comments_c.c')) {
            $target = Join-Path $work ($source + '.exe')
            & $xl build "release/extlibs/hcl/examples/$source" -o $target
            if ($LASTEXITCODE -ne 0) { throw "hcl $source AOT build failed" }
            $queryOutput = & $target
            if ($LASTEXITCODE -ne 0 -or ($queryOutput -join "`n") -cne $queryGolden) {
                throw "hcl $source AOT differs from golden"
            }
        }
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "hcl input changed while testing: $path"
        }
    }
    $passed = $true
    Write-Output 'hcl Windows C/XLang gate: PASS'
} catch {
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $work 'failure.txt')
    throw
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('hcl_guard_')) {
        throw "HCL test directory escapes build/test: $resolvedWork"
    }
    if (-not $passed) {
        Write-Host "[hcl] failure artifacts: $resolvedWork"
    } elseif (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
