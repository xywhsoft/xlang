param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/stack-ownership-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior Stack ownership evidence' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current source ownership compiler build failed' }
    }
    $public = Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Current full source compiler/runtime required' }
    $gcc = (Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $files = @(Get-Item -LiteralPath $PSCommandPath,$gcc,'release/xl.dll','release/xlc.dll',
        'test/cases/stdlib_stack_ownership_host.c','test/cases/stdlib_stack_ownership_public.c',
        'test/cases/stdlib_stack_ownership.xl','test/cases/stdlib_stack_ownership_artifact.c',
        'test/cases/stdlib_stack_ownership_tcc_host.c','test/cases/xlir_one_shot_tcc_host.c',
        'test/cases/xlir_async_module_lifetime_host.c','test/support/stack_native.c')
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) + @($files | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    foreach ($inputRecord in $inputs) {
        if ((Get-FileHash -LiteralPath $inputRecord.Path).Hash -ne $inputRecord.Sha256) { throw "Stale Stack input $($inputRecord.Path)" }
    }
    New-Item -ItemType Directory -Path "$outPath/__xl_runtime" | Out-Null
    Copy-Item -LiteralPath release/xl.dll,release/xlc.dll -Destination $outPath
    foreach ($header in $public.Headers | Where-Object Path -match '[/\\]__xl_runtime[/\\]') {
        Copy-Item -LiteralPath $header.Path -Destination (Join-Path "$outPath/__xl_runtime" ([IO.Path]::GetFileName($header.Path)))
    }
    $rows = [Collections.Generic.List[object]]::new()
    function Run([string]$Name, [string]$File, [string[]]$Arguments) {
        $process = [Diagnostics.Process]::new()
        $process.StartInfo.FileName = $File; $process.StartInfo.Arguments = Join-XlirWindowsCommandLine $Arguments
        $process.StartInfo.WorkingDirectory = $rootPath; $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true; $process.StartInfo.RedirectStandardOutput = $true
        $process.StartInfo.RedirectStandardError = $true
        $process.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8; $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
        try {
            [void]$process.Start(); $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
            if (!$process.WaitForExit(60000)) { Stop-XlirProcessTree $process; throw "Timeout $Name" }
            $code = $process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log",$stdout.Result+$stderr.Result,[Text.UTF8Encoding]::new($false))
        } finally { $process.Dispose() }
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Complete=$code -eq 0})
        if ($code) { Get-Content "$outPath/$Name.log" -Tail 30; throw "$Name failed: $code" }
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers','-I.','-Irelease',"-I$outPath",'-Ilib/tcc')
    $sweeps = @()
    foreach ($level in @(0,2)) {
        $name = "native-O$level"; $binary = "$outPath/$name.exe"
        Run "$name.build" $gcc ($strict + @("-O$level",'test/cases/stdlib_stack_ownership_host.c',
            'test/support/stack_native.c','src/runtime/xl6_stack.c','src/runtime/xl6_ownership_plan.c','-o',$binary))
        Run "$name.vectors" $binary @()
        foreach ($stage in @('create','refresh','commit')) {
            for ($point=0; $point -le 64; ++$point) {
                Run "$name.$stage.oom-$point" $binary @($stage,"$point")
                if ((Get-Content "$outPath/$name.$stage.oom-$point.log" -Raw) -match 'hit=0 balanced') { break }
            }
            if ($point -gt 64) { throw "Incomplete ownership OOM prefix $stage" }
            $sweeps += [pscustomobject]@{Lane=$name;Stage=$stage;FirstNoHit=$point}
        }
    }
    $prefix = "$outPath/stdlib_stack_ownership.generated"
    Run 'module.emit' $public.Driver @('test/cases/stdlib_stack_ownership.xl','emit',$prefix)
    Run 'module.repeat' $public.Driver @('test/cases/stdlib_stack_ownership.xl','emit',"$outPath/repeat")
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$outPath/repeat.$extension").Hash) { throw 'Noncanonical module emission' }
    }
    $excluded = @('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -notin $excluded } | ForEach-Object Path)
    $libraries = @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32')
    foreach ($level in @(0,2)) {
        $name = "module-O$level"; $binary = "$outPath/$name.exe"
        Run "$name.build" $gcc ($strict + @("-O$level",'-DJOINT_SOURCE_XRT','-DXL_ENABLE_EXE_COMPILER=1',
            (New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outPath.Replace('\','/')),
            'test/cases/stdlib_stack_ownership_tcc_host.c') + $objects + $libraries + @('-o',$binary))
        foreach ($mode in 0..3) { Run "$name.mode-$mode" $binary @("$mode") }
    }
    Run 'public.build' $gcc ($strict + @('test/cases/stdlib_stack_ownership_public.c','release/xlc.dll','-o',"$outPath/public.exe"))
    foreach ($mode in 0..3) {
        Run "public.mode-$mode" "$outPath/public.exe" @("$mode",$outPath,"$outPath/public-$mode.temps",
            "$rootPath/release/xl.dll","$rootPath/res/tcc/win64/x86_64")
    }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Checks=$rows.Count;Inputs=$inputs;
        Sweeps=$sweeps;Results=$rows;Scope='Stack physical graph/lifecycle prerequisite. Native O0/O2 real cycles, duplicate and shared-wrapper edges, weak quarantine/rooted abort, opaque refusal, callback tails, cross-thread freeze and OOM through no-hit. Current-source real TCC module O0/O2 and shared public compiler JIT lifetimes for MODULE-GLOBAL-REACHABLE class/Stack cycles, actual external roots/intact finalizers and Windows MEM_FREE. Detached code-lease islands remain a known pending discovery issue. Shared libraries are not sanitizer-instrumented. Owned return, char/bytes, batch/FixedStack and other 53-module work remain pending.'}
    [IO.File]::WriteAllText("$outPath/results.json",($report | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if (!$unchanged) { throw 'Ownership inputs changed during verification' }
    Write-Host "Stack ownership Windows: $($rows.Count) checks"
} finally { Pop-Location }
