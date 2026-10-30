param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $workRoot ('xml_trees_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/xml.sax/include',
    '-Irelease/extlibs/xml.dom/include',
    '-Irelease/extlibs/xml.etree/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = @{}
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;expectedExit=0;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "[$name] $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -ne 'pass') {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[xml] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

function ExpectPrivate([string]$name, [string]$path,
    [string]$symbol, [string]$detail) {
    $result = Invoke-ExtlibSmokeProcess $xl @('check',$path) $root 90
    $checks.Add([ordered]@{name=$name;program=$xl;arguments=@('check',$path);
        directory=$root;expectedExit=1;result=$result})
    $diagnostic = $result.stdout + $result.stderr
    if ($result.status -ne 'fail' -or $result.exitCode -ne 1 -or -not $diagnostic.Contains($symbol) -or
        -not $diagnostic.Contains($detail)) {
        throw "${name}: expected private-access diagnostic`n$diagnostic"
    }
    Write-Host "[xml] ${name}: PASS"
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $inputs = @('release/xl.exe','release/xl.dll','release/xlc.dll',
        'lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'test/cases/extlib_xml_tree_interop.xl') | ForEach-Object { Join-Path $root $_ }
    foreach ($kind in @('sax','dom','etree')) {
        $inputs += @(Get-ChildItem (Join-Path $root "release/extlibs/xml.$kind") -Recurse -File | ForEach-Object FullName)
        $inputs += @("test/cases/extlib_xml_$kind.xl","release/examples/extlibs/xml_$kind.xl",
            "release/examples/extlibs/xml_${kind}_c.c","dev/extlib_blockers/xml_${kind}_$(if($kind -eq 'sax'){'reader'}elseif($kind -eq 'dom'){'node'}else{'element'})_type.xl") |
            ForEach-Object { Join-Path $root $_ }
    }
    $inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/_expat') -Recurse -File | ForEach-Object FullName)
    $inputs += $gcc, (Get-Process -Id $PID).Path
    foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/xml.sax/test/xrt_runtime.c', '-o', $runtime))
    $cold = Join-Path $work 'bridge_state.exe'
    $null = Run 'C bridge state compile' $gcc ($flags + @(
        'release/extlibs/xml.sax/test/test_bridge_state.c',$runtime) + $links + @('-o',$cold))
    $null = Run 'C bridge thread isolation' $cold @('threads')
    foreach ($kind in 0..14) {
        $complete = $false
        foreach ($budget in 0..127) {
            $output = Run "cold state $kind/$budget" $cold @("$kind","$budget",(Join-Path $work "state-$kind-$budget.xml"))
            if ($output -match 'injected=0 PASS$') { $complete = $true; break }
        }
        if (!$complete) { throw "Cold state $kind budget did not complete" }
    }
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/xml.sax/examples/catalog_stream.expected.txt'),
        [Text.UTF8Encoding]::new($false)).Replace("`r`n","`n").TrimEnd("`n")
    $catalogC = Join-Path $work 'catalog_c.exe'
    $null = Run 'C catalog compile' $gcc ($flags + @('release/extlibs/xml.sax/examples/catalog_stream_c.c',$runtime) +
        $links + @('-o',$catalogC))
    if ((Run 'C catalog golden' $catalogC @()) -cne $golden) { throw 'C catalog golden differs' }
    foreach ($source in @('catalog_stream.xl','catalog_stream_c.c')) {
        if ((Run "catalog JIT $source" $xl @('run',"release/extlibs/xml.sax/examples/$source")) -cne $golden) {
            throw "Catalog JIT golden differs: $source"
        }
        if (!$SkipAot) {
            $target = Join-Path $work ($source + '.exe')
            $null = Run "catalog AOT build $source" $xl @('build',"release/extlibs/xml.sax/examples/$source",'-o',$target)
            if ((Run "catalog AOT $source" $target @()) -cne $golden) { throw "Catalog AOT golden differs: $source" }
        }
    }
    $null = Run 'bridge state TCC' $xl @('run','release/extlibs/xml.sax/test/test_bridge_state.c')
    $null = Run 'bridge threads TCC' $xl @('run','release/extlibs/xml.sax/test/test_bridge_state.c','--','threads')
    if (!$SkipAot) {
        $target = Join-Path $work 'bridge_state_aot.exe'
        $null = Run 'bridge state AOT build' $xl @('build','release/extlibs/xml.sax/test/test_bridge_state.c','-o',$target)
        $null = Run 'bridge state AOT' $target @()
        $null = Run 'bridge threads AOT' $target @('threads')
    }
    foreach ($kind in @('sax', 'dom', 'etree')) {
        $target = Join-Path $work "test_xml_$kind.exe"
        $null = Run "$kind C compile" $gcc ($flags + @(
            "release/extlibs/xml.$kind/test/test_xml_$kind.c", $runtime) +
            $links + @('-o', $target))
        $null = Run "$kind C boundaries" $target @()
        $null = Run "$kind XLang central" $xl @('run',
            "test/cases/extlib_xml_$kind.xl")
        $null = Run "$kind XLang boundaries" $xl @('run',
            "release/extlibs/xml.$kind/test/boundaries.xl")
        $null = Run "$kind XLang allocations" $xl @('run',
            "release/extlibs/xml.$kind/test/allocations.xl")
    }
    $null = Run 'DOM/ElementTree roundtrip' $xl @('run',
        'test/cases/extlib_xml_tree_interop.xl')

    ExpectPrivate 'DOM native entry' `
        'release/extlibs/xml.dom/test/private_native_negative.xl' `
        'createElementNative' 'access'
    ExpectPrivate 'DOM pointer entry' `
        'release/extlibs/xml.dom/test/private_pointer_negative.xl' `
        'appendChildNative' 'access'
    ExpectPrivate 'ElementTree constructor' `
        'release/extlibs/xml.etree/test/constructor_access_negative.xl' `
        'no such exported symbol' '[import]'
    ExpectPrivate 'ElementTree native entry' `
        'release/extlibs/xml.etree/test/private_native_negative.xl' `
        'setTextNative' 'access'
    ExpectPrivate 'ElementTree pointer entry' `
        'release/extlibs/xml.etree/test/private_pointer_negative.xl' `
        'nameNative' 'access'

    $goldenByKind = @{}
    foreach ($kind in @('sax', 'dom', 'etree')) {
        $goldenPath = Join-Path $root "release/extlibs/xml.sax/test/xml_$kind.expected.txt"
        $golden = [IO.File]::ReadAllText($goldenPath,
            [Text.UTF8Encoding]::new($false)).Replace("`r`n", "`n").TrimEnd("`n")
        $goldenByKind[$kind] = $golden
        $cOutput = Run "$kind C example" $xl @('run',
            "release/examples/extlibs/xml_${kind}_c.c")
        $xlOutput = Run "$kind XLang example" $xl @('run',
            "release/examples/extlibs/xml_$kind.xl")
        if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
            throw "xml.$kind examples differ from fixed output:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host "[xml] $kind fixed C/XLang output: PASS"
    }
    if (-not $SkipAot) {
        foreach ($kind in @('sax', 'dom', 'etree')) {
            foreach ($case in @('boundaries', 'allocations')) {
                $caseTarget = Join-Path $work "xml_${kind}_${case}_aot.exe"
                $null = Run "$kind $case AOT build" $xl @('build',
                    "release/extlibs/xml.$kind/test/$case.xl", '-o', $caseTarget)
                $caseOutput = Run "$kind $case AOT" $caseTarget @()
                $expected = if ($case -eq 'boundaries') {
                    "xml.$kind boundaries: ok"
                } else {
                    "xml.$kind allocation: PASS"
                }
                if ($caseOutput -cne $expected) {
                    throw "xml.$kind $case AOT output differs from fixed output"
                }
            }
            $target = Join-Path $work "xml_${kind}_aot.exe"
            $null = Run "$kind XLang AOT build" $xl @('build',
                "release/examples/extlibs/xml_$kind.xl", '-o', $target)
            $aotOutput = Run "$kind XLang AOT" $target @()
            if ($aotOutput -cne $goldenByKind[$kind]) {
                throw "xml.$kind AOT output differs from fixed output"
            }
            $centralTarget = Join-Path $work "xml_${kind}_central_aot.exe"
            $null = Run "$kind central AOT build" $xl @('build',
                "test/cases/extlib_xml_$kind.xl", '-o', $centralTarget)
            $null = Run "$kind central AOT" $centralTarget @()
            $cTarget = Join-Path $work "xml_${kind}_c_aot.exe"
            $null = Run "$kind C example AOT build" $xl @('build',
                "release/examples/extlibs/xml_${kind}_c.c", '-o', $cTarget)
            $cAotOutput = Run "$kind C example AOT" $cTarget @()
            if ($cAotOutput -cne $goldenByKind[$kind]) {
                throw "xml.$kind C example AOT output differs from fixed output"
            }
        }
        $interopTarget = Join-Path $work 'xml_tree_interop_aot.exe'
        $null = Run 'DOM/ElementTree roundtrip AOT build' $xl @('build',
            'test/cases/extlib_xml_tree_interop.xl', '-o', $interopTarget)
        $null = Run 'DOM/ElementTree roundtrip AOT' $interopTarget @()
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) { throw "XML input changed: $path" }
    }
    # 这是应当通过的公开类型正例；未导出不能被既有工厂路径的通过掩盖。
    $null = Run 'SAX public Reader type' $xl @('check','dev/extlib_blockers/xml_sax_reader_type.xl')
    $passed = $true
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($workRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe XML trees work directory: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
            Set-Content ($work + '.report.json') -Encoding UTF8
        if ($passed) { Remove-Item -LiteralPath $safeWork -Recurse -Force }
        else { Write-Warning "XML failure artifacts: $safeWork" }
    }
}
