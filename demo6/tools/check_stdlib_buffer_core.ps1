param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/buffer-cores-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior Buffer core evidence' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source core compiler build failed' }
    }
    $public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Full current-source compiler/runtime required' }
    $cases = @('stdlib_buffer_core','stdlib_buffer_core_only','xlir_buffer_core_oom',
        'stdlib_buffer_value','stdlib_buffer_snapshot')
    $negative = @('bad_xlir_buffer_core_no_raise','bad_xlir_buffer_copy_owner','bad_xlir_buffer_byte_width')
    $gcc = (Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $python = (Get-Command python -CommandType Application | Select-Object -First 1).Source
    $files = @($cases + $negative | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @(Get-Item -LiteralPath @($PSCommandPath,$gcc,$python,'release/xl.dll','test/cases/stdlib_buffer_core_host.c',
            'test/support/buffer_core_native.c','test/cases/xlir_module_product_oom_host.c',
            'test/support/module_product_memory.c','test/support/module_product_memory.h',
            'tools/check_xlir_buffer_core_ir.py','test/cases/xlir_text_value_contract.c','tools/check_xlir_artifact_runtime.ps1'))
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
        @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Stale core input $($record.Path)" }
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
    if (!$?) { throw 'core actual artifact acceptance failed' }
    $artifact = Get-Content "$outPath/artifact/results.json" -Raw | ConvertFrom-Json
    if (!$artifact.Complete -or !$artifact.InputsUnchanged) { throw 'Incomplete core artifact evidence' }
    foreach ($case in $cases[0..1]) {
        $plan = $artifact.NativeLinkage | Where-Object Fixture -eq $case
        if ('src/runtime/xl6_buffer_core.c' -notin $plan.Sources) { throw 'Buffer core closure missing' }
        $query = 'src/runtime/xl6_buffer_query.c' -in $plan.Sources
        if ($query -ne ($case -eq 'stdlib_buffer_core')) { throw 'Buffer query pruning mismatch' }
    }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outPath/xl.dll"
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
        '-I.','-Irelease','-Irelease/include',"-I$PublicDirectory/include")
    Run 'native-build' $gcc ($strict + @('test/cases/stdlib_buffer_core_host.c','test/support/buffer_core_native.c',
        'src/runtime/xl6_buffer_core.c','src/runtime/xl6_buffer_query.c','-o',"$outPath/native.exe"))
    Run 'native-vectors' "$outPath/native.exe" @()
    $sweeps = @()
    foreach ($operation in @('create','create-reserve','reserve','append-byte','append-raw','append-alias','insert-alias','copy-gap')) {
        for ($point=0; $point -le 64; ++$point) {
            $name = "native.$operation.oom-$point"
            Run $name "$outPath/native.exe" @($operation,"$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
        }
        if ($point -gt 64) { throw "Incomplete core native prefix: $operation" }
        $sweeps += [pscustomobject]@{Fixture=$operation;FirstNoHit=$point}
    }
    Run 'contract-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c') +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/contract.exe"))
    foreach ($case in $cases[0..2]) {
        $prefix = "$outPath/$case"
        Run "$case.emit" $public.Driver @("test/cases/$case.xl",'emit',$prefix)
        Run "$case.ir" $python @('tools/check_xlir_buffer_core_ir.py',$case,"$prefix.json")
        Run "$case.contract" "$outPath/contract.exe" @("test/cases/$case.xl","$prefix.contract.c")
        if ((Get-Content "$prefix.c" -Raw) -cne (Get-Content "$prefix.contract.c" -Raw)) { throw 'Noncanonical core C output' }
        if ((Get-Content "$prefix.c" -Raw) -match 'xl6_xrt_buffer_(create|destroy|len|copy_within|fill|byte_at)\s*\(') { throw 'Retired raw core bridge emitted' }
    }
    Run 'debug-host-build' $gcc ($strict + @('-c','test/support/module_product_memory.c','-o',"$outPath/debug-host.o"))
    $prefix = "$outPath/xlir_buffer_core_oom"
    Run 'factory-oom-build' $gcc ($strict + @('-include',"$prefix.c",'test/cases/xlir_module_product_oom_host.c',"$outPath/debug-host.o") +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$prefix.host.exe"))
    Run 'factory-oom-normal' "$prefix.host.exe" @()
    for ($point=0; $point -le 4096; ++$point) {
        $name = "factory.oom-$point"
        Run $name "$prefix.host.exe" @("$point")
        if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
    }
    if ($point -gt 4096) { throw 'Incomplete managed factory allocation prefix' }
    $sweeps += [pscustomobject]@{Fixture='xlir_buffer_core_oom';FirstNoHit=$point}
    foreach ($case in $negative) {
        $path = "$outPath/$case.c"
        Run "$case.reject" $public.Driver @("test/cases/$case.xl",'cgen-default',$path) $true
        if ((Test-Path -LiteralPath $path) -or (Get-Content "$outPath/$case.reject.log" -Raw) -notmatch
            'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI') { throw 'Wrong core ABI refusal' }
    }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Checks=$rows.Count;Inputs=$inputs;
        PositiveFixtures=$cases;NegativeFixtures=$negative;Sweeps=$sweeps;Results=$rows;
        Scope='Windows public Buffer core and byte queries, actual C/JIT/AOT/pruned artifact and native/full-runtime OOM. Trusted C pointer escapes and legacy file/network conversions remain outside this safety contract. Not all containers or all 53 modules.'}
    [IO.File]::WriteAllText("$outPath/results.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (!$unchanged) { throw 'core inputs changed during acceptance' }
    Write-Output "Buffer core acceptance: $($rows.Count) checks, unchanged inputs"
} finally { Pop-Location }
