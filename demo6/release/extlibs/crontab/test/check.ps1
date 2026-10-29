param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test/crontab_guard'
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
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/crontab','-Irelease/extlibs/crontab/include')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
$timezone = [TimeZoneInfo]::Local.ToSerializedString()

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/crontab') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($item in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'test/cases/extlib_crontab.xl','test/cases/extlib_crontab_boundaries.xl',
        'release/examples/extlibs/crontab.xl','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {
        Join-Path $root $item
    }
    if ($gccCommand) { $gcc }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[crontab] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public header' $gcc ($flags + @('-x','c','-fsyntax-only','release/extlibs/crontab/include/crontab_native.h'))
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_TIME','-DXRT_MODULE_TIME_LOCAL',
        '-fsyntax-only','release/extlibs/crontab/crontab_native.c'))
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $null = Run 'selected XRT compile' $gcc ($flags + @('-c','release/extlibs/crontab/test/xrt_runtime.c','-o',$runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c','release/extlibs/crontab/crontab_native.c','-o',$native))
    foreach ($name in @('test_crontab','test_long_horizon','input_boundaries_c')) {
        $binary = Join-Path $work ($name+'.exe')
        $null = Run "$name compile" $gcc ($flags + @('-DNDEBUG',"release/extlibs/crontab/test/$name.c",$native,$runtime) + $links + @('-o',$binary))
        $null = Run $name $binary @()
    }
    $null = Run 'input boundaries direct C' $xl @('run','release/extlibs/crontab/test/input_boundaries_c.c')
    $sources = @('test/cases/extlib_crontab.xl','test/cases/extlib_crontab_boundaries.xl',
        'release/extlibs/crontab/test/allocations.xl','release/extlibs/crontab/test/owned_probe.xl',
        'release/extlibs/crontab/test/aot_core.xl')
    foreach ($source in $sources) { $null = Run "$source JIT" $xl @('run',$source) }
    $probe = Join-Path $work 'probe.c'
    $null = Run 'complete return cgen' $xl @('cgen','release/extlibs/crontab/test/owned_probe.xl','--emit',$probe)
    $text = [IO.File]::ReadAllText($probe)
    if ($text -match '=\s*xlir_string_copy\s*\(' -or
        [regex]::Matches($text,'\b\w+\.Data\s*=\s*xcron_describe\s*\(').Count -ne 1) {
        throw 'Description result is not adopted directly'
    }
    $module = Join-Path $work 'example.c'
    $null = Run 'full API cgen' $xl @('cgen','release/examples/extlibs/crontab.xl','--emit',$module)
    $text = [IO.File]::ReadAllText($module)
    $decls = @($text -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxcron_[a-z_]+\(' } | Sort-Object -Unique)
    $expected = @('xcron_describe','xcron_is_valid','xcron_matches','xcron_next')
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxcron_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 4 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete generated C ABI' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/crontab_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'four actual prototypes' $gcc ($flags + @('-fsyntax-only',$abi))
    $example = Join-Path $work 'c_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('-DNDEBUG','release/extlibs/crontab/examples/crontab.c',$native,$runtime) + $links + @('-o',$example))
    $golden = Run 'C example' $example @()
    $direct = Run 'C example direct' $xl @('run','release/extlibs/crontab/examples/crontab.c')
    $public = Run 'XLang example' $xl @('run','release/examples/extlibs/crontab.xl')
    if ($golden -cne $direct -or $golden -cne $public) { throw 'C/XLang example output differs' }
    if (-not $SkipAot) {
        foreach ($source in ($sources + @('release/extlibs/crontab/test/input_boundaries_c.c',
            'release/extlibs/crontab/examples/crontab.c','release/examples/extlibs/crontab.xl'))) {
            $binary = Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null = Run "$source build" $xl @('build',$source,'-o',$binary)
            $output = Run "$source AOT" $binary @()
            if ($source -in @('release/extlibs/crontab/examples/crontab.c','release/examples/extlibs/crontab.xl') -and $output -cne $golden) {
                throw 'AOT example differs from C'
            }
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
    [TimeZoneInfo]::ClearCachedData()
    $zoneChanged = [TimeZoneInfo]::Local.ToSerializedString() -cne $timezone
    if ($changed.Count -or $zoneChanged) { $outcome = 'invalid-input-change' }
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;timezone=$timezone;timezoneChanged=$zoneChanged;inputs=$hashes;
        changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[crontab] report: $work.json"
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Crontab work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count -or $zoneChanged) { throw 'Crontab inputs/timezone changed during gate' }
}
