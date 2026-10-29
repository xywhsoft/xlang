param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/module-product-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior module product evidence' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source module product compiler build failed' }
    }
    $public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Full current-source compiler/runtime required' }
    $cases = @('xlir_module_product_initializer','xlir_module_product_globals','xlir_module_product_closure',
        'xlir_module_product_entry','cgen_module_globals','cgen_module_global_closure','cgen_generic_multi_return',
        'stdlib_buffer_value','stdlib_buffer_snapshot')
    $negative = @('sem_module_product_duplicate','sem_module_product_conflict','sem_module_product_forward_initializer',
        'bad_xlir_print_no_raise','bad_xlir_print_input')
    $gcc = (Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $python = (Get-Command python -CommandType Application | Select-Object -First 1).Source
    $files = @($cases + $negative | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @(Get-Item -LiteralPath @($PSCommandPath,$gcc,$python,'test/cases/xlir_module_product_oom_host.c',
            'test/support/module_product_memory.c','test/support/module_product_memory.h',
            'tools/check_xlir_module_product_ir.py','tools/check_xlir_artifact_runtime.ps1'))
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
        @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Stale module product input $($record.Path)" }
    }
    $rows = [Collections.Generic.List[object]]::new()
    function Run([string]$Name, [string]$File, [string[]]$Arguments, [bool]$Reject = $false) {
        $process = [Diagnostics.Process]::new()
        $process.StartInfo.FileName = $File
        $process.StartInfo.Arguments = Join-XlirWindowsCommandLine $Arguments
        $process.StartInfo.WorkingDirectory = $rootPath
        $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true
        $process.StartInfo.RedirectStandardOutput = $true
        $process.StartInfo.RedirectStandardError = $true
        $process.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
        $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
        try {
            [void]$process.Start()
            $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
            if (!$process.WaitForExit(45000)) { Stop-XlirProcessTree $process; throw "Timeout $Name" }
            $code = $process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log", $stdout.Result + $stderr.Result, [Text.UTF8Encoding]::new($false))
        } finally { $process.Dispose() }
        $ok = if ($Reject) { $code -ne 0 } else { $code -eq 0 }
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Complete=$ok})
        if (!$ok) { Get-Content "$outPath/$Name.log" -Tail 25; throw "$Name failed: $code" }
    }
    & "$PSScriptRoot/check_xlir_artifact_runtime.ps1" -PublicDirectory $PublicDirectory -Directory "$outPath/artifact" -Cases $cases *> "$outPath/artifact.log"
    if (!$?) { throw 'Module product actual artifact acceptance failed' }
    $artifact = Get-Content "$outPath/artifact/results.json" -Raw | ConvertFrom-Json
    if (!$artifact.Complete -or !$artifact.InputsUnchanged) { throw 'Incomplete module product artifact evidence' }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outPath/xl.dll"
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation','-I.','-Irelease','-Irelease/include',
        "-I$PublicDirectory/include")
    Run 'debug-host-build' $gcc ($strict + @('-c','test/support/module_product_memory.c','-o',"$outPath/debug-host.o"))
    $sweeps = @()
    foreach ($case in $cases[0..3]) {
        $prefix = "$outPath/$case"
        Run "$case.emit" $public.Driver @("test/cases/$case.xl",'emit',$prefix)
        Run "$case.ir" $python @('tools/check_xlir_module_product_ir.py',$case,"$prefix.json")
        $flags = if ($case -eq 'xlir_module_product_entry') { @('-DXL6_PRODUCT_USER_ENTRY') } else { @() }
        Run "$case.host-build" $gcc ($strict + $flags + @('-include',"$prefix.c",'test/cases/xlir_module_product_oom_host.c',
            "$outPath/debug-host.o") +
            $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$prefix.host.exe"))
        Run "$case.host" "$prefix.host.exe" @()
        for ($point=0; $point -le 4096; ++$point) {
            $name = "$case.oom-$point"
            Run $name "$prefix.host.exe" @("$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
        }
        if ($point -gt 4096) { throw "Incomplete module product prefix: $case" }
        $sweeps += [pscustomobject]@{Fixture=$case;FirstNoHit=$point}
    }
    foreach ($case in $negative) {
        $path = "$outPath/$case.c"
        Run "$case.reject" $public.Driver @("test/cases/$case.xl",'cgen-default',$path) $true
        if (Test-Path -LiteralPath $path) { throw 'Rejected product declaration emitted C' }
        $marker = if ($case -eq 'sem_module_product_forward_initializer') { "undeclared identifier 'second'" }
            elseif ($case -like 'bad_xlir_print_*') { 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI' }
            else { '重复|already|duplicate|conflict|redeclar' }
        if ((Get-Content "$outPath/$case.reject.log" -Raw) -notmatch $marker) { throw 'Wrong declaration refusal reason' }
    }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Checks=$rows.Count;Inputs=$inputs;
        PositiveFixtures=$cases;NegativeFixtures=$negative;Sweeps=$sweeps;Results=$rows;
        Scope='Windows canonical module product scope/capture/cell construction: real C/JIT/AOT/pruned artifact plan and separate full-runtime logical OOM harness, not all 53 modules.'}
    [IO.File]::WriteAllText("$outPath/results.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (!$unchanged) { throw 'Module product inputs changed during acceptance' }
    Write-Output "Module product globals: $($rows.Count) checks, $($cases.Count) fixtures, unchanged inputs"
} finally { Pop-Location }
